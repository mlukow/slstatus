/* See LICENSE file for copyright and license details. */
#include <fcntl.h>
#include <libudev.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../queue.h"
#include "../slstatus.h"
#include "../util.h"

#define BACKLIGHT_PATH "/sys/class/backlight/%s"

typedef struct backlight_t {
	TAILQ_ENTRY(backlight_t) entry;
	const char *name;
	int brightness;
	struct udev_monitor *monitor;
} backlight_t;

TAILQ_HEAD(backlight_q, backlight_t);

struct backlight_q backlight_queue;
struct udev *udev = NULL;
struct udev_monitor *udev_monitor = NULL;
pthread_t backlight_thread;

backlight_t *
backlight_find(const char *name, int create)
{
	backlight_t *backlight;
	char path[PATH_MAX];
	struct udev_device *device;

	TAILQ_FOREACH(backlight, &backlight_queue, entry)
		if (!strcmp(backlight->name, name)) {
			return backlight;
		}

	if (!create)
		return NULL;
	
	if (esnprintf(path, sizeof(path), BACKLIGHT_PATH, name) < 0)
		return NULL;

	device = udev_device_new_from_syspath(udev, path);
	if (!device)
		return NULL;

	backlight = calloc(1, sizeof(backlight_t));
	backlight->name = strdup(name);
	backlight->brightness = (int)(100.0 * (float)atoi(udev_device_get_sysattr_value(device, "brightness")) / atoi(udev_device_get_sysattr_value(device, "max_brightness")));

	TAILQ_INSERT_TAIL(&backlight_queue, backlight, entry);

	return backlight;
}

void *
backlight_loop(void *arg)
{
	backlight_t *backlight;
	const char *name;
	int brightness, fd, flags;
	struct udev_device *device;

	udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "backlight", NULL);
	udev_monitor_enable_receiving(udev_monitor);

	fd = udev_monitor_get_fd(udev_monitor);
	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

	while (1) {
		device = udev_monitor_receive_device(udev_monitor);
		if (device) {
			name = udev_device_get_sysname(device);
			backlight = backlight_find(name, 0);
			if (backlight) {
				brightness = (int)(100.0 * (float)atoi(udev_device_get_sysattr_value(device, "brightness")) / atoi(udev_device_get_sysattr_value(device, "max_brightness")));
				if (brightness != backlight->brightness) {
					backlight->brightness = brightness;
					kill(getpid(), SIGRTMIN + BACKLIGHT_SIGNAL);
				}
			}
			udev_device_unref(device);
		}
	}

	return NULL;
}

void
backlight_init(void)
{

	udev = udev_new();
	if (!udev)
		return;

	TAILQ_INIT(&backlight_queue);

	pthread_create(&backlight_thread, NULL, backlight_loop, NULL);
}

void
backlight_tick(void)
{
}

void
backlight_free(void)
{
	pthread_kill(backlight_thread, SIGTERM);

	if (udev_monitor) {
		udev_monitor_unref(udev_monitor);
		udev_monitor = NULL;
	}

	if (udev) {
		udev_unref(udev);
		udev = NULL;
	}
}

const char *
backlight_line(const char *arg)
{
	/*
	char *icon;
	const char *str_perc;
	unsigned long perc;

	if (!(str_perc = backlight_perc(arg)))
		return NULL;

	perc = strtoul(str_perc, NULL, 10);

	if (perc < 25)
		icon = "󰃞";
	else if (perc < 50)
		icon = "󰃟";
	else if (perc < 75)
		icon = "󰃝";
	else
		icon = "󰃠";

	return bprintf("%s %d%%", icon, perc);
	*/
	backlight_t *backlight;
	char *icon;

	backlight = backlight_find(arg, 1);
	if (!backlight)
		return NULL;

	if (backlight->brightness < 25)
		icon = "󰃞";
	else if (backlight->brightness < 50)
		icon = "󰃟";
	else if (backlight->brightness < 75)
		icon = "󰃝";
	else
		icon = "󰃠";

	return bprintf("%s %d%%", icon, backlight->brightness);
}

const char *
backlight_icon(const char *arg)
{
	unsigned long perc;
	const char *str_perc;

	if (!(str_perc = backlight_perc(arg)))
		return NULL;

	perc = strtoul(str_perc, NULL, 10);

	if (perc < 25)
		return "󰃞";
	else if (perc < 50)
		return "󰃟";
	else if (perc < 75)
		return "󰃝";
	else
		return "󰃠";
}

#if defined(__linux__)
	#include <limits.h>

	#define BACKLIGHT_BRIGHTNESS "/sys/class/backlight/%s/brightness"
	#define BACKLIGHT_MAX_BRIGHTNESS "/sys/class/backlight/%s/max_brightness"

	const char *
	backlight_perc(const char *arg)
	{
		int brightness, max_brightness;
		char path[PATH_MAX];
	
		if (esnprintf(path, sizeof(path), BACKLIGHT_BRIGHTNESS, arg) < 0)
			return NULL;
		if (pscanf(path, "%d", &brightness) != 1)
			return NULL;
	
		if (esnprintf(path, sizeof(path), BACKLIGHT_MAX_BRIGHTNESS, arg) < 0)
			return NULL;
		if (pscanf(path, "%d", &max_brightness) != 1)
			return NULL;
	
		 return bprintf("%d", (int)(100 * (double)brightness / max_brightness));
	}
#endif

