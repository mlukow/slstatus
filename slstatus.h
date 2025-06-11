/* See LICENSE file for copyright and license details. */

/* backlight */
const char *backlight_line(const char *);
const char *backlight_icon(const char *);
const char *backlight_perc(const char *);

/* battery */
const char *battery_perc(const char *);
const char *battery_remaining(const char *);
const char *battery_state(const char *);

/* cat */
const char *cat(const char *path);

/* cpu */
const char *cpu_freq(const char *unused);
const char *cpu_perc(const char *unused);

/* datetime */
const char *datetime(const char *fmt);

/* disk */
const char *disk_free(const char *path);
const char *disk_perc(const char *path);
const char *disk_total(const char *path);
const char *disk_used(const char *path);

/* entropy */
const char *entropy(const char *unused);

/* hostname */
const char *hostname(const char *unused);

/* ip */
const char *ipv4(const char *interface);
const char *ipv6(const char *interface);
const char *up(const char *interface);

/* kernel_release */
const char *kernel_release(const char *unused);

/* keyboard_indicators */
const char *keyboard_indicators(const char *fmt);

/* keymap */
const char *keymap(const char *unused);

/* load_avg */
const char *load_avg(const char *unused);

/* mm */
void mm_init(void);
void mm_tick(void);
void mm_free(void);
const char *mm_line(const char *iface);
const char *mm_perc(const char *iface);

/* netspeeds */
const char *netspeed_rx(const char *interface);
const char *netspeed_tx(const char *interface);

/* nm */
void nm_init(void);
void nm_free(void);
void nm_tick(void);
const char *nm_line(const char *interface);
const char *nm_ip4(const char *interface);
const char *nm_ip6(const char *interface);
const char *nm_mac(const char *interface);
const char *nm_vpn(const char *name);

/* num_files */
const char *num_files(const char *path);

/* ram */
const char *ram_free(const char *unused);
const char *ram_perc(const char *unused);
const char *ram_total(const char *unused);
const char *ram_used(const char *unused);

/* run_command */
const char *run_command(const char *cmd);

 /* pa */
void pa_init(void);
void pa_tick(void);
void pa_free(void);
const char *pa_line(const char *sink);
const char *pa_description(const char *sink);
const char *pa_mute(const char *sink);
const char *pa_name(const char *sink);
const char *pa_perc(const char *sink);

/* ppd */
void ppd_init(void);
void ppd_tick(void);
void ppd_free(void);
const char *ppd_line(const char *unused);
const char *ppd_active(const char *unused);

/* swap */
const char *swap_free(const char *unused);
const char *swap_perc(const char *unused);
const char *swap_total(const char *unused);
const char *swap_used(const char *unused);

/* temperature */
const char *temp(const char *);

/* upower */
void upower_init(void);
void upower_tick(void);
void upower_free(void);
const char *upower_line(const char *device);
const char *upower_perc(const char *device);
const char *upower_perc(const char *device);
const char *upower_state(const char *device);

/* uptime */
const char *uptime(const char *unused);

/* user */
const char *gid(const char *unused);
const char *uid(const char *unused);
const char *username(const char *unused);

/* volume */
const char *vol_perc(const char *card);

/* wifi */
const char *wifi_essid(const char *interface);
const char *wifi_perc(const char *interface);
