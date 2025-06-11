#include <upower.h>

#include "../queue.h"
#include "../util.h"

typedef struct upower_t {
	TAILQ_ENTRY(upower_t) entry;
	const char *device;
	char *model;
	gdouble percentage;
	UpDeviceKind kind;
	UpDeviceState state;
} upower_t;

TAILQ_HEAD(upower_q, upower_t);

struct upower_q upower_queue;
UpClient *upower_client = NULL;

void
upower_init(void)
{
	TAILQ_INIT(&upower_queue);

	upower_client = up_client_new();
}

void
upower_tick(void)
{
	while (g_main_context_iteration(NULL, FALSE));
}

void
upower_free(void)
{
	upower_t *upower;

	while ((upower = TAILQ_FIRST(&upower_queue))) {
		TAILQ_REMOVE(&upower_queue, upower, entry);

		if (upower->model) {
			g_free(upower->model);
			upower->model = NULL;
		}

		free(upower);
	}

	if (upower_client) {
		g_object_unref(upower_client);
		upower_client = NULL;
	}
}

void
upower_device_callback(UpDevice *device, GParamSpec *pspec, gpointer user_data)
{
	const gchar *name;
	upower_t *upower;

	name = g_param_spec_get_name(pspec);
	upower = (upower_t *)user_data;

	if (!strcmp(name, "percentage")) {
		 g_object_get(device, "percentage", &upower->percentage, NULL);
	} else if (!strcmp(name, "state")) {
		g_object_get(device, "state", &upower->state, NULL);
	}
}

void
upower_update_from_device(upower_t *upower, UpDevice *device)
{
	if (upower->model)
		g_free(upower->model);

	g_object_get(
			device,
			"kind",
			&upower->kind,
			"model",
			&upower->model,
			"percentage",
			&upower->percentage,
			"state",
			&upower->state,
			NULL);
}

upower_t *
upower_find(const char *device)
{
	GPtrArray *devices;
	gchar *native_path;
	guint i;
	int check;
	UpDevice *gdevice;
	upower_t *upower;

	TAILQ_FOREACH(upower, &upower_queue, entry)
		if (!strcmp(upower->device, device))
			return upower;

	devices = up_client_get_devices2(upower_client);
	for (i = 0; i < devices->len; i++) {
		gdevice = g_ptr_array_index(devices, i);
		g_object_get(gdevice, "native-path", &native_path, NULL);
		check = strcmp(native_path, device);
		g_free(native_path);

		if (!check) {
			upower = calloc(1, sizeof(upower_t));
			upower->device = device;

			upower_update_from_device(upower, gdevice);

			TAILQ_INSERT_TAIL(&upower_queue, upower, entry);

			g_signal_connect(G_OBJECT(gdevice), "notify", G_CALLBACK(upower_device_callback), upower);

			return upower;
		}
	}

	g_ptr_array_unref(devices);

	return NULL;
}

const char *
upower_line(const char *device)
{
	char *icon;
	upower_t *upower;

	upower = upower_find(device);
	if (!upower)
		return NULL;

	if (upower->state == UP_DEVICE_STATE_DISCHARGING) {
		if (upower->percentage < 5)
			icon = "󰂎";
		else if (upower->percentage < 15)
			icon = "󰁺";
		else if (upower->percentage < 25)
			icon = "󰁻";
		else if (upower->percentage < 35)
			icon = "󰁼";
		else if (upower->percentage < 45)
			icon = "󰁽";
		else if (upower->percentage < 55)
			icon = "󰁾";
		else if (upower->percentage < 65)
			icon = "󰁿";
		else if (upower->percentage < 75)
			icon = "󰂀";
		else if (upower->percentage < 85)
			icon = "󰂁";
		else if (upower->percentage < 95)
			icon = "󰂂";
		else
			icon = "󰁹";
	} else {
		if (upower->percentage < 5)
			icon = "󰢟";
		else if (upower->percentage < 15)
			icon = "󰢜";
		else if (upower->percentage < 25)
			icon = "󰂆";
		else if (upower->percentage < 35)
			icon = "󰂇";
		else if (upower->percentage < 45)
			icon = "󰂈";
		else if (upower->percentage < 55)
			icon = "󰢝";
		else if (upower->percentage < 65)
			icon = "󰂉";
		else if (upower->percentage < 75)
			icon = "󰢞";
		else if (upower->percentage < 85)
			icon = "󰂊";
		else if (upower->percentage < 95)
			icon = "󰂋";
		else
			icon = "󰂅";
	}

	return bprintf("%s %d%%", icon, (int)upower->percentage);
}

const char *
upower_perc(const char *device)
{
	upower_t *upower = upower_find(device);
	return upower ? bprintf("%d", (int)upower->percentage) : NULL;
}

const char *
upower_state(const char *device)
{
	upower_t *upower = upower_find(device);
	return upower ? up_device_state_to_string(upower->state) : NULL;
}
