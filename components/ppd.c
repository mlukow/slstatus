#include <gio/gio.h>
#include <glib.h>

#include "../slstatus.h"

GDBusProxy *ppd_proxy = NULL;
gchar *ppd_profile = NULL;

void
ppd_update(GDBusProxy *proxy)
{
	gchar *profile = NULL;
	GVariant *tuple, *value;

	tuple = g_dbus_proxy_call_sync(
			proxy,
			"Get",
			g_variant_new("(ss)", "org.freedesktop.UPower.PowerProfiles", "ActiveProfile"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL);
	if (tuple) {
		g_variant_get(tuple, "(v)", &value);
		profile = g_variant_dup_string(value, NULL);
		g_variant_unref(value);
		g_variant_unref(tuple);

		if ((profile != ppd_profile) || (ppd_profile && strcmp(profile, ppd_profile))) {
			if (ppd_profile)
				free(ppd_profile);
			ppd_profile = profile;
			kill(getpid(), SIGRTMIN + PPD_SIGNAL);
		}
	}
}

void
ppd_callback(GDBusProxy *proxy, gchar *sender, gchar *signal, GVariant *parameters, gpointer user_data)
{
	ppd_update(proxy);
}

void
ppd_init(void)
{
	ppd_proxy = g_dbus_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			"org.freedesktop.UPower.PowerProfiles",
			"/org/freedesktop/UPower/PowerProfiles",
			"org.freedesktop.DBus.Properties",
			NULL,
			NULL);
	if (!ppd_proxy)
		return;

	ppd_update(ppd_proxy);
	g_signal_connect(ppd_proxy, "g-signal", G_CALLBACK(ppd_callback), NULL);
}

void
ppd_tick(void)
{
	while (g_main_context_iteration(NULL, FALSE));
}

void
ppd_free(void)
{
	if (ppd_proxy) {
		g_object_unref(ppd_proxy);
		ppd_proxy = NULL;
	}
}

const char *
ppd_line(const char *unused)
{
	if (!ppd_profile)
		return NULL;

	if (!strcmp(ppd_profile, "power-saver"))
		return "󰌪";
	else if (!strcmp(ppd_profile, "balanced"))
		return "󰗑";
	else
		return "󱐋";
}

const char *
ppd_active(const char *unused)
{
	return ppd_profile;
}
