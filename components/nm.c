#include <NetworkManager.h>

#include "../slstatus.h"
#include "../queue.h"
#include "../util.h"

typedef struct nm_t {
	TAILQ_ENTRY(nm_t) entry;
	char *iface;
	char *ipv4;
	char *ipv6;
	NMDevice *device;
	NMDeviceState state;
	int state_id;

	NMAccessPoint *ap;
	char *essid;
	int ss;
	int ap_id;
	int ss_id;

	GDBusProxy *proxy;
} nm_t;

TAILQ_HEAD(nm_q, nm_t);

struct nm_q nm_queue;
NMClient *nm_client;

void
nm_init(void)
{
	GError *error = NULL;

	TAILQ_INIT(&nm_queue);

	nm_client = nm_client_new(NULL, &error);
}

void
nm_tick(void)
{
	while (g_main_context_iteration(NULL, FALSE));
}

void
nm_free(void)
{
	nm_t *nm;

	while ((nm = TAILQ_FIRST(&nm_queue))) {
		TAILQ_REMOVE(&nm_queue, nm, entry);

		if (nm->ipv4) {
			free(nm->ipv4);
			nm->ipv4 = NULL;
		}

		if (nm->ipv6) {
			free(nm->ipv6);
			nm->ipv6 = NULL;
		}

		if (nm->essid) {
			free(nm->essid);
			nm->essid = NULL;
		}

		if (nm->state_id) {
			g_signal_handler_disconnect(nm->device, nm->state_id);
			nm->state_id = 0;
		}

		if (nm->proxy) {
			g_object_unref(nm->proxy);
			nm->proxy = NULL;
		}
	}

	if (nm_client) {
		g_object_unref(nm_client);
		nm_client = NULL;
	}

	free(nm);
}

void
nm_update(nm_t *nm)
{
	GPtrArray *addresses;
	NMIPConfig *config;

	nm->state = nm_device_get_state(nm->device);

	if (nm->state == NM_DEVICE_STATE_ACTIVATED) {
		config = nm_device_get_ip4_config(nm->device);
		if (config) {
			addresses = nm_ip_config_get_addresses(config);
			if (addresses && addresses->len > 0) {
				if (nm->ipv4)
					free(nm->ipv4);
				nm->ipv4 = strdup(nm_ip_address_get_address(g_ptr_array_index(addresses, 0)));
				kill(getpid(), SIGRTMIN + NM_SIGNAL);
			}
		}

		config = nm_device_get_ip6_config(nm->device);
		if (config) {
			addresses = nm_ip_config_get_addresses(config);
			if (addresses && addresses->len > 0) {
				if (nm->ipv6)
					free(nm->ipv6);
				nm->ipv6 = strdup(nm_ip_address_get_address(g_ptr_array_index(addresses, 0)));
				kill(getpid(), SIGRTMIN + NM_SIGNAL);
			}
		}
	} else {
		if (nm->ipv4) {
			free(nm->ipv4);
			nm->ipv4 = NULL;
		}

		if (nm->ipv6) {
			free(nm->ipv6);
			nm->ipv6 = NULL;
		}

		kill(getpid(), SIGRTMIN + NM_SIGNAL);
	}
}


static void
nm_signal_strength_callback(NMAccessPoint *ap, GParamSpec *pspec, gpointer user_data)
{
	int ss;
	nm_t *nm = (nm_t *)user_data;

	ss = nm_access_point_get_strength(ap);
	if (ss != nm->ss) {
		nm->ss = ss;
		kill(getpid(), SIGRTMIN + NM_SIGNAL);
	}
}

static void
nm_access_point_callback(NMDeviceWifi *device, GParamSpec *pspec, gpointer user_data)
{
	const guint8 *essid;
	GBytes *ssid;
	gsize size;
	NMAccessPoint *ap;
	nm_t *nm;

	nm = (nm_t *)user_data;
	ap = nm_device_wifi_get_active_access_point(device);

	if (ap) {
		nm->ap = ap;

		ssid = nm_access_point_get_ssid(ap);
		if (ssid) {
			essid = g_bytes_get_data(ssid, &size);
			nm->essid = strdup(nm_utils_ssid_to_utf8(essid, size));
		}

		nm->ss = nm_access_point_get_strength(ap);
		nm->ss_id = g_signal_connect(
				ap,
				"notify::" NM_ACCESS_POINT_STRENGTH,
				G_CALLBACK(nm_signal_strength_callback),
				nm);

		kill(getpid(), SIGRTMIN + NM_SIGNAL);
	} else if (nm->ap) {
		g_signal_handler_disconnect(nm->ap, nm->ss_id);
		if (nm->essid) {
			free(nm->essid);
			nm->essid = NULL;
		}

		nm->ap = NULL;
		nm->ss = 0;
		nm->ss_id = 0;

		kill(getpid(), SIGRTMIN + NM_SIGNAL);
	}
}

static void
nm_state_callback(NMDevice *device, GParamSpec *pspec, gpointer user_data)
{
	nm_t *nm = (nm_t *)user_data;

	nm_update(nm);
}

nm_t *
nm_find(const char *iface)
{
	const guint8 *essid;
	GBytes *ssid;
	gsize size;
	NMDevice *device;
	nm_t *nm;

	TAILQ_FOREACH(nm, &nm_queue, entry)
		if (!strcmp(nm->iface, iface))
			return nm;

	device = nm_client_get_device_by_iface(nm_client, iface);
	if (!device)
		return NULL;

	nm = calloc(1, sizeof(nm_t));
	nm->iface = strdup(iface);
	nm->device = device;
	nm->state = nm_device_get_state(device);
	nm->ap = NULL;
	nm->essid = NULL;
	nm->ap_id = 0;
	nm->ss_id = 0;
	nm->ss = 0;

	nm_update(nm);

	TAILQ_INSERT_TAIL(&nm_queue, nm, entry);

	if (NM_IS_DEVICE_WIFI(nm->device)) {
		nm->ap = nm_device_wifi_get_active_access_point((NMDeviceWifi *)nm->device);
		nm->ap_id = g_signal_connect(
				nm->device,
				"notify::" NM_DEVICE_WIFI_ACTIVE_ACCESS_POINT,
				G_CALLBACK(nm_access_point_callback),
				nm);

		if (nm->ap) {
			ssid = nm_access_point_get_ssid(nm->ap);
			if (ssid) {
				essid = g_bytes_get_data(ssid, &size);
				nm->essid = strdup(nm_utils_ssid_to_utf8(essid, size));
			}

			nm->ss = nm_access_point_get_strength(nm->ap);
			nm->ss_id = g_signal_connect(
					nm->ap,
					"notify::" NM_ACCESS_POINT_STRENGTH,
					G_CALLBACK(nm_signal_strength_callback),
					nm);
		}
	}

	nm->state_id = g_signal_connect(nm->device, "notify::" NM_DEVICE_STATE, G_CALLBACK(nm_state_callback), nm);

	return nm;
}

const char *
nm_line(const char *iface)
{
	char *icon;
	nm_t *nm;

	nm = nm_find(iface);
	if (!nm)
		return NULL;

	if (nm->state != NM_DEVICE_STATE_ACTIVATED)
		return NULL;

	if (NM_IS_DEVICE_ETHERNET(nm->device))
		return "󰈀";
	else if (NM_IS_DEVICE_WIFI(nm->device)) {
		if (nm->ss < 20)
			icon = "󰤯";
		else if (nm->ss < 40)
			icon = "󰤟";
		else if (nm->ss < 60)
			icon = "󰤢";
		else if (nm->ss < 80)
			icon = "󰤥";
		else
			icon = "󰤨";

		if (nm->essid)
			return bprintf("%s %s", icon, nm->essid);

		return icon;
	}

	return NULL;
}

const char *
nm_ip4(const char *iface)
{
	nm_t *nm = nm_find(iface);
	return nm ? nm->ipv4 : NULL;
}

const char *
nm_ip6(const char *iface)
{
	nm_t *nm = nm_find(iface);
	return nm ? nm->ipv6 : NULL;
}

const char *
nm_mac(const char *iface)
{
	nm_t *nm = nm_find(iface);
	return nm ? nm_device_get_hw_address(nm->device) : NULL;
}
