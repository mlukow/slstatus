#include <string.h>
#include <unistd.h>

#include <pulse/pulseaudio.h>

#include "../slstatus.h"
#include "../queue.h"
#include "../util.h"

typedef struct pa_t {
	TAILQ_ENTRY(pa_t) entry;
	char *sink;
	char *name;
	char *description;
	uint32_t index;
	int volume;
	int mute;
} pa_t;

TAILQ_HEAD(pa_q, pa_t);

struct pa_q pa_queue;
pa_context *pa_ctx;
pa_threaded_mainloop *pa_loop;
int pa_state_callback_signal = 0;
char *pa_default_sink;

pa_t *
pa_find_by_index(uint32_t index)
{
	pa_t *pa;

	TAILQ_FOREACH(pa, &pa_queue, entry)
		if (pa->index == index)
			return pa;

	return NULL;
}

void
pa_wait(pa_operation *op)
{
	while (pa_operation_get_state(op) != PA_OPERATION_DONE)
		pa_threaded_mainloop_wait(pa_loop);

	pa_operation_unref(op);
}

void
pa_sink_info_callback(pa_context *ctx, const pa_sink_info *info, int eol, void *userdata)
{
	int signal = 0;
	int volume;
	pa_t *pa = (pa_t *)userdata;

	if (!eol && info) {
		if (info->index != pa->index) {
			pa->index = info->index;
			signal++;
		}

		volume = (int)(pa_cvolume_max(&info->volume) * 100.0f / PA_VOLUME_NORM + 0.5f);
		if (volume != pa->volume) {
			pa->volume = volume;
			signal++;
		}

		if (info->mute != pa->mute) {
			pa->mute = info->mute;
			signal++;
		}

		if (pa->name)
			free(pa->name);
		pa->name = strdup(info->name);

		if (pa->description)
			free(pa->description);
		pa->description = strdup(info->description);
	}

	if (signal)
		kill(getpid(), SIGRTMIN + PA_SIGNAL);

	pa_threaded_mainloop_signal(pa_loop, 0);
}

pa_t *
pa_find_by_sink(const char *sink)
{
	pa_operation *op;
	pa_t *pa;

	TAILQ_FOREACH(pa, &pa_queue, entry)
		if ((!sink && !pa->sink) || (sink && !strcmp(pa->sink, sink)))
			return pa;

	pa = calloc(1, sizeof(pa_t));

	if (!sink || !strcmp(sink, "@DEFAULT_SINK@") || !strcmp(sink, "@DEFAULT_AUDIO_SINK@"))
		pa->sink = NULL;
	else
		pa->sink = strdup(sink);

	op = pa_context_get_sink_info_by_name(pa_ctx, pa->sink, pa_sink_info_callback, pa);
	pa_operation_unref(op);

	TAILQ_INSERT_TAIL(&pa_queue, pa, entry);

	return pa;
}

void
server_info_callback(pa_context *ctx, const pa_server_info *info, void *userdata)
{
	pa_operation *op;
	pa_t *pa;

	if (pa_default_sink && !strcmp(info->default_sink_name, pa_default_sink))
		return;

	if (pa_default_sink)
		free(pa_default_sink);
	pa_default_sink = strdup(info->default_sink_name);

	pa = pa_find_by_sink(NULL);
	if (pa) {
		op = pa_context_get_sink_info_by_name(ctx, pa_default_sink, pa_sink_info_callback, pa);
		pa_operation_unref(op);
	}
}

void
pa_subscribe_callback(pa_context *ctx, pa_subscription_event_type_t type, uint32_t idx, void *userdata)
{
	pa_operation *op;
	pa_t *pa;

	if ((unsigned int)(type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK) {
		pa = pa_find_by_index(idx);
		if (pa) {
			op = pa_context_get_sink_info_by_index(ctx, idx, pa_sink_info_callback, pa);
			pa_operation_unref(op);
		} else
			pa_context_get_server_info(ctx, server_info_callback, userdata);
	}
}

void
pa_state_callback(pa_context *ctx, void *userdata)
{
	switch (pa_context_get_state(ctx)) {
		case PA_CONTEXT_READY:
			pa_context_set_subscribe_callback(ctx, pa_subscribe_callback, userdata);
			pa_context_subscribe(ctx, PA_SUBSCRIPTION_MASK_SINK, NULL, NULL);
			pa_context_get_server_info(ctx, server_info_callback, userdata);
			__attribute__ ((fallthrough));
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
			pa_state_callback_signal = 1;
			pa_threaded_mainloop_signal(pa_loop, 0);
			break;
		default:
			break;
	}
}

void
pa_init(void)
{
	TAILQ_INIT(&pa_queue);

	pa_loop = pa_threaded_mainloop_new();
	if (!pa_loop)
		return;

	pa_threaded_mainloop_lock(pa_loop);

	pa_ctx = pa_context_new(pa_threaded_mainloop_get_api(pa_loop), "slstatus");
	if (!pa_ctx) {
		pa_threaded_mainloop_unlock(pa_loop);
		pa_threaded_mainloop_free(pa_loop);
		pa_loop = NULL;

		return;
	}

	pa_context_set_state_callback(pa_ctx, pa_state_callback, NULL);

	if (pa_context_connect(pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
		pa_context_disconnect(pa_ctx);
		pa_context_unref(pa_ctx);
		pa_ctx = NULL;

		pa_threaded_mainloop_unlock(pa_loop);
		pa_threaded_mainloop_free(pa_loop);
		pa_loop = NULL;

		return;
	}

	if (pa_threaded_mainloop_start(pa_loop) < 0) {
		pa_context_disconnect(pa_ctx);
		pa_context_unref(pa_ctx);
		pa_ctx = NULL;

		pa_threaded_mainloop_unlock(pa_loop);
		pa_threaded_mainloop_free(pa_loop);
		pa_loop = NULL;

		return;
	}

	while (!pa_state_callback_signal)
		pa_threaded_mainloop_wait(pa_loop);

	if (pa_context_get_state(pa_ctx) != PA_CONTEXT_READY) {
		pa_context_disconnect(pa_ctx);
		pa_context_unref(pa_ctx);
		pa_ctx = NULL;

		pa_threaded_mainloop_unlock(pa_loop);
		pa_threaded_mainloop_stop(pa_loop);
		pa_threaded_mainloop_free(pa_loop);
		pa_loop = NULL;

		return;
	}

	pa_threaded_mainloop_unlock(pa_loop);
}

void
pa_tick(void)
{
}

void
pa_free(void)
{
	pa_t *pa;

	while ((pa = TAILQ_FIRST(&pa_queue))) {
		TAILQ_REMOVE(&pa_queue, pa, entry);

		if (pa->sink) {
			free(pa->sink);
			pa->sink = NULL;
		}

		if (pa->name) {
			free(pa->name);
			pa->name = NULL;
		}

		if (pa->description) {
			free(pa->description);
			pa->description = NULL;
		}

		free(pa);
	}

	if (pa_loop) {
		pa_threaded_mainloop_stop(pa_loop);
		pa_threaded_mainloop_free(pa_loop);
	}

	if (pa_default_sink) {
		free(pa_default_sink);
		pa_default_sink = NULL;
	}
}

const char *
pa_line(const char *sink)
{
	char *icon;
	pa_t *pa;

	pa = pa_find_by_sink(sink);
	if (!pa)
		return NULL;

	if (pa->mute)
		icon = "󰖁";
	else if (pa->volume < 34)
		icon = "󰕿";
	else if (pa->volume < 67)
		icon = "󰖀";
	else
		icon = "󰕾";

	return bprintf("%s %d%%", icon, pa->volume);
}

const char *
pa_description(const char *sink)
{
	pa_t *pa = pa_find_by_sink(sink);
	return pa ? pa->description : NULL;
}

const char *
pa_mute(const char *sink)
{
	pa_t *pa = pa_find_by_sink(sink);
	return pa ? (pa->mute ? "+" : "-") : NULL;
}

const char *
pa_name(const char *sink)
{
	pa_t *pa = pa_find_by_sink(sink);
	return pa ? pa->name : NULL;
}

const char *
pa_perc(const char *sink)
{
	pa_t *pa = pa_find_by_sink(sink);
	return pa ? bprintf("%d", pa->volume) : NULL;
}
