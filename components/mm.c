#include <stdio.h>
#include <libmm-glib.h>

#include "../slstatus.h"
#include "../queue.h"
#include "../util.h"

typedef struct mm_t {
	TAILQ_ENTRY(mm_t) entry;
	char *iface;

	MMModem *modem;
	MMModemState state;
	guint sq;
	int sq_id;
} mm_t;

TAILQ_HEAD(mm_q, mm_t);

struct mm_q mm_queue;
MMManager *mm_manager;

void
mm_device_added_callback(MMObject *object, GParamSpec *pspec, gpointer user_data)
{
	printf("##### OBJECT ADDED\n");
}

void
mm_device_removed_callback(MMObject *object, GParamSpec *pspec, gpointer user_data)
{
	printf("##### OBJECT REMOVED\n");
}

void
mm_init(void)
{
	TAILQ_INIT(&mm_queue);

	GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
	if (!bus)
		return;

	mm_manager = mm_manager_new_sync(bus, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE, NULL, NULL);
	if (!mm_manager)
		return;

	/*
	g_signal_connect(mm_manager, "object-added", G_CALLBACK(mm_device_added_callback), NULL);
	g_signal_connect(mm_manager, "object-removed", G_CALLBACK(mm_device_removed_callback), NULL);
	*/
}

void
mm_modem_callback(MMModem *modem, GParamSpec *pspec, gpointer user_data)
{
	const gchar *name;
	guint sq;
	mm_t *mm;
	MMModemState state;

	mm = (mm_t *)user_data;
	name = g_param_spec_get_name(pspec);

	if (!strcmp(name, "state")) {
		g_object_get(modem, "state", &state, NULL);
		if (state != mm->state) {
			mm->state = state;
			kill(getpid(), SIGRTMIN + MM_SIGNAL);
		}
	} else if (!strcmp(name, "signal-quality")) {
		g_object_get(modem, "signal-quality", &sq, NULL);
		if (sq != mm->sq) {
			mm->sq = sq;
			kill(getpid(), SIGRTMIN + MM_SIGNAL);
		}
	}
}

mm_t *
mm_find(const char *iface)
{
	mm_t *mm;

	TAILQ_FOREACH(mm, &mm_queue, entry)
		if (!strcmp(mm->iface, iface))
			return mm;

	GList *devices, *iter;
	MMModem *modem;
	const MMModemPortInfo *ports;
	guint len;

	devices = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mm_manager));
	for (iter = devices; iter; iter = iter->next) {
		modem = mm_object_get_modem(iter->data);
		mm_modem_peek_ports(modem, &ports, &len);

		for (guint i = 0; i < len; i++) {
			if (!strcmp(ports[i].name, iface)) {
				mm = calloc(1, sizeof(mm_t));
				mm->iface = strdup(iface);
				mm->modem = modem;
				mm->state = mm_modem_get_state(modem);
				mm->sq = mm_modem_get_signal_quality(modem, NULL);

				TAILQ_INSERT_TAIL(&mm_queue, mm, entry);

				mm->sq_id = g_signal_connect(modem, "notify", G_CALLBACK(mm_modem_callback), mm);

				return mm;
			}
		}
	}

	return NULL;
}

void
mm_tick(void)
{
	while (g_main_context_iteration(NULL, FALSE));
}

void
mm_free(void)
{
	mm_t *mm;

	while ((mm = TAILQ_FIRST(&mm_queue))) {
		TAILQ_REMOVE(&mm_queue, mm, entry);
	}
}

const char *
mm_line(const char *iface)
{
	char *icon;
	mm_t *mm;

	mm = mm_find(iface);
	if (!mm)
		return NULL;

	if (mm->state == MM_MODEM_STATE_CONNECTED) {
		if (mm->sq < 25)
			icon = "󰢿";
		else if (mm->sq < 50)
			icon = "󰢼";
		else if (mm->sq < 70)
			icon = "󰢽";
		else
			icon = "󰢾";

		return icon;
	}

	return NULL;
}

const char *
mm_perc(const char *iface)
{
	mm_t *mm = mm_find(iface);
	return mm ? bprintf("%u", mm->sq) : NULL;
}
