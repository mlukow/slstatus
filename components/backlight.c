/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../slstatus.h"
#include "../util.h"

const char *
backlight_line(const char *arg)
{
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

