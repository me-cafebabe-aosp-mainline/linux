// Minimal debug init for first mainline boot (no busybox needed).
// Logs to /dev/kmsg (visible on the framebuffer console), creates a USB
// ACM+ECM gadget and offers a tiny shell on /dev/ttyGS0.
#define _GNU_SOURCE
#include <dirent.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

static int kmsg = -1;

static void klog(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = snprintf(buf, sizeof(buf), "<4>init: ");
	n += vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
	va_end(ap);
	if (n < (int)sizeof(buf) - 1)
		buf[n++] = '\n';
	if (kmsg >= 0)
		write(kmsg, buf, n);
}

static int wr(const char *path, const char *val)
{
	int fd = open(path, O_WRONLY);
	int r;

	if (fd < 0)
		return -errno;
	r = write(fd, val, strlen(val));
	r = r < 0 ? -errno : 0;
	close(fd);
	return r;
}

static void mk(const char *path)
{
	if (mkdir(path, 0755) && errno != EEXIST)
		klog("mkdir %s: %s", path, strerror(errno));
}

static void gadget(void)
{
	const char *g = "/sys/kernel/config/usb_gadget/g1";
	char p[256], t[300], udc[128] = "";
	DIR *d;
	struct dirent *e;

	if (mount("none", "/sys/kernel/config", "configfs", 0, NULL) && errno != EBUSY)
		klog("mount configfs: %s", strerror(errno));
	mk("/sys/kernel/config/usb_gadget");
	mk(g);
	snprintf(p, sizeof(p), "%s/idVendor", g);  wr(p, "0x1d6b");
	snprintf(p, sizeof(p), "%s/idProduct", g); wr(p, "0x0104");
	snprintf(p, sizeof(p), "%s/strings/0x409", g); mk(p);
	snprintf(p, sizeof(p), "%s/strings/0x409/serialnumber", g); wr(p, "mainline-j");
	snprintf(p, sizeof(p), "%s/strings/0x409/manufacturer", g); wr(p, "msm8916-mainline");
	snprintf(p, sizeof(p), "%s/strings/0x409/product", g); wr(p, "Samsung J debug");
	snprintf(p, sizeof(p), "%s/configs/c.1", g); mk(p);
	snprintf(p, sizeof(p), "%s/configs/c.1/strings/0x409", g); mk(p);
	snprintf(p, sizeof(p), "%s/configs/c.1/strings/0x409/configuration", g); wr(p, "serial+net");
	snprintf(p, sizeof(p), "%s/functions/acm.GS0", g); mk(p);
	snprintf(p, sizeof(p), "%s/functions/ecm.usb0", g); mk(p);
	/*
	 * configfs resolves the link target relative to the *process* cwd, not to the link,
	 * so use absolute targets.
	 */
	snprintf(p, sizeof(p), "%s/configs/c.1/acm.GS0", g);
	snprintf(t, sizeof(t), "%s/functions/acm.GS0", g);
	if (symlink(t, p))
		klog("link acm: %s", strerror(errno));
	snprintf(p, sizeof(p), "%s/configs/c.1/ecm.usb0", g);
	snprintf(t, sizeof(t), "%s/functions/ecm.usb0", g);
	if (symlink(t, p))
		klog("link ecm: %s", strerror(errno));

	d = opendir("/sys/class/udc");
	if (d) {
		while ((e = readdir(d)))
			if (e->d_name[0] != '.') {
				snprintf(udc, sizeof(udc), "%s", e->d_name);
				break;
			}
		closedir(d);
	}
	if (!udc[0]) {
		klog("no UDC found in /sys/class/udc");
		return;
	}
	snprintf(p, sizeof(p), "%s/UDC", g);
	klog("binding gadget to UDC %s: %d", udc, wr(p, udc));
}

static void out(int fd, const char *s)
{
	write(fd, s, strlen(s));
}

static void do_cat(int fd, const char *path)
{
	char buf[1024];
	int f = open(path, O_RDONLY | O_NONBLOCK), n;

	if (f < 0) {
		snprintf(buf, sizeof(buf), "cat: %s: %s\r\n", path, strerror(errno));
		out(fd, buf);
		return;
	}
	while ((n = read(f, buf, sizeof(buf))) > 0) {
		/* LF -> CRLF for the serial terminal */
		for (int i = 0; i < n; i++) {
			if (buf[i] == '\n')
				write(fd, "\r", 1);
			write(fd, &buf[i], 1);
		}
	}
	close(f);
	out(fd, "\r\n");
}

static void do_ls(int fd, const char *path)
{
	DIR *d = opendir(path[0] ? path : ".");
	struct dirent *e;
	char buf[300];

	if (!d) {
		snprintf(buf, sizeof(buf), "ls: %s: %s\r\n", path, strerror(errno));
		out(fd, buf);
		return;
	}
	while ((e = readdir(d))) {
		snprintf(buf, sizeof(buf), "%s%s\r\n", e->d_name, e->d_type == DT_DIR ? "/" : "");
		out(fd, buf);
	}
	closedir(d);
}

static void do_insmod(int fd, const char *path, const char *args)
{
	int f = open(path, O_RDONLY);
	char buf[200];
	long r;

	if (f < 0) {
		snprintf(buf, sizeof(buf), "insmod: %s: %s\r\n", path, strerror(errno));
		out(fd, buf);
		return;
	}
	r = syscall(SYS_finit_module, f, args, 0);
	snprintf(buf, sizeof(buf), "insmod: %s\r\n", r ? strerror(errno) : "ok");
	out(fd, buf);
	close(f);
}

/* ev [seconds]: print the key/touch events of all input devices (like evtest) */
static void do_ev(int fd, const char *arg)
{
	struct pollfd pfd[16];
	char names[16][64], buf[160], path[64];
	int n = 0, secs = atoi(arg), i;
	time_t end;

	if (secs <= 0)
		secs = 10;
	for (i = 0; i < 16; i++) {
		snprintf(path, sizeof(path), "/dev/input/event%d", i);
		pfd[n].fd = open(path, O_RDONLY | O_NONBLOCK);
		if (pfd[n].fd < 0)
			continue;
		pfd[n].events = POLLIN;
		strcpy(names[n], "?");
		ioctl(pfd[n].fd, EVIOCGNAME(sizeof(names[n])), names[n]);
		snprintf(buf, sizeof(buf), "event%d: %s\r\n", i, names[n]);
		out(fd, buf);
		n++;
	}
	snprintf(buf, sizeof(buf), "listening for %d s ...\r\n", secs);
	out(fd, buf);
	end = time(NULL) + secs;
	while (time(NULL) < end) {
		if (poll(pfd, n, 500) <= 0)
			continue;
		for (i = 0; i < n; i++) {
			struct input_event e;

			while (pfd[i].revents & POLLIN &&
			       read(pfd[i].fd, &e, sizeof(e)) == sizeof(e)) {
				if (e.type == EV_SYN)
					continue;
				snprintf(buf, sizeof(buf), "%s: type %d code %d value %d\r\n",
					 names[i], e.type, e.code, e.value);
				out(fd, buf);
			}
		}
	}
	for (i = 0; i < n; i++)
		close(pfd[i].fd);
	out(fd, "done\r\n");
}

static void do_echo(int fd, char *line)
{
	char *gt = strchr(line, '>');
	char *text = line, *file;

	if (!gt) {
		out(fd, text);
		out(fd, "\r\n");
		return;
	}
	*gt = 0;
	file = gt + 1;
	while (*file == ' ')
		file++;
	for (char *t = gt - 1; t >= text && *t == ' '; t--)
		*t = 0;
	if (wr(file, text) < 0)
		out(fd, "echo: write failed\r\n");
}

static void run(int fd, char *line)
{
	char *arg;

	while (*line == ' ')
		line++;
	arg = strchr(line, ' ');
	if (arg)
		*arg++ = 0;
	else
		arg = line + strlen(line);

	if (!*line) {
		return;
	} else if (!strcmp(line, "help")) {
		out(fd, "ls [dir] | cat file | echo text > file | dmesg | ev [secs] | insmod mod [args] | reboot | poweroff\r\n");
	} else if (!strcmp(line, "ls")) {
		do_ls(fd, arg);
	} else if (!strcmp(line, "cat")) {
		do_cat(fd, arg);
	} else if (!strcmp(line, "ev")) {
		do_ev(fd, arg);
	} else if (!strcmp(line, "dmesg")) {
		do_cat(fd, "/dev/kmsg");
	} else if (!strcmp(line, "echo")) {
		do_echo(fd, arg);
	} else if (!strcmp(line, "insmod")) {
		char *a = strchr(arg, ' ');
		if (a)
			*a++ = 0;
		do_insmod(fd, arg, a ? a : "");
	} else if (!strcmp(line, "reboot")) {
		sync();
		reboot(RB_AUTOBOOT);
	} else if (!strcmp(line, "poweroff")) {
		sync();
		reboot(RB_POWER_OFF);
	} else {
		out(fd, "unknown command (try: help)\r\n");
	}
}

static void shell(void)
{
	for (;;) {
		struct termios t;
		char line[512];
		int fd = open("/dev/ttyGS0", O_RDWR | O_NOCTTY), n = 0;

		if (fd < 0) {
			sleep(1);
			continue;
		}
		tcgetattr(fd, &t);
		cfmakeraw(&t);
		tcsetattr(fd, TCSANOW, &t);
		out(fd, "\r\nmainline debug init - type help\r\n# ");
		for (;;) {
			char c;
			int r = read(fd, &c, 1);

			if (r <= 0)
				break;
			if (c == '\r' || c == '\n') {
				out(fd, "\r\n");
				line[n] = 0;
				run(fd, line);
				n = 0;
				out(fd, "# ");
			} else if (c == 0x7f || c == 8) {
				if (n) {
					n--;
					out(fd, "\b \b");
				}
			} else if (n < (int)sizeof(line) - 1 && c >= ' ') {
				line[n++] = c;
				write(fd, &c, 1);
			}
		}
		close(fd);
		sleep(1);
	}
}

static void klog_file(const char *path, int max, const char *label)
{
	char line[400];
	FILE *f = fopen(path, "r");
	int n = 0;

	if (!f) {
		klog("%s: cannot open %s (%s)", label, path, strerror(errno));
		return;
	}
	while (n < max && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		klog("%s: %s", label, line);
		n++;
	}
	fclose(f);
	if (!n)
		klog("%s: (empty)", label);
}

static void klog_dir_files(const char *dir, const char *file, const char *label)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char p[512];

	if (!d) {
		klog("%s: no %s", label, dir);
		return;
	}
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.')
			continue;
		snprintf(p, sizeof(p), "%s/%s/%s", dir, e->d_name, file);
		if (access(p, R_OK))
			continue;
		char l2[100];
		snprintf(l2, sizeof(l2), "%s %s", label, e->d_name);
		klog_file(p, 4, l2);
	}
	closedir(d);
}

static void diag(void)
{
	DIR *d;
	struct dirent *e;
	char p[512], t[256];
	ssize_t n;

	klog("==== diagnostics ====");
	mount("none", "/sys/kernel/debug", "debugfs", 0, NULL);
	klog_dir_files("/sys/class/udc", "state", "udc");
	klog_file("/sys/kernel/config/usb_gadget/g1/UDC", 1, "gadget UDC");
	klog_dir_files("/sys/class/extcon", "name", "extcon");
	klog_dir_files("/sys/class/extcon", "state", "extcon state");
	d = opendir("/sys/bus/i2c/devices");
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.')
			continue;
		snprintf(p, sizeof(p), "/sys/bus/i2c/devices/%s/driver", e->d_name);
		n = readlink(p, t, sizeof(t) - 1);
		t[n > 0 ? n : 0] = 0;
		klog("i2c %s -> %s", e->d_name, n > 0 ? strrchr(t, '/') + 1 : "(no driver)");
	}
	if (d)
		closedir(d);
	klog_dir_files("/sys/class/drm", "status", "drm");
	klog_dir_files("/sys/class/drm", "modes", "drm modes");
	klog_dir_files("/sys/class/graphics", "name", "fb");
	klog_dir_files("/sys/class/backlight", "brightness", "backlight");
	klog_dir_files("/sys/class/input", "name", "input");
	klog_dir_files("/sys/bus/iio/devices", "name", "iio");
	klog_dir_files("/sys/class/leds", "brightness", "led");
	klog_dir_files("/sys/class/block", "size", "block");
	klog_file("/sys/kernel/debug/devices_deferred", 25, "deferred");
	klog_dir_files("/sys/class/power_supply", "online", "psy online");
	klog_dir_files("/sys/class/power_supply", "status", "psy status");
	klog("==== end of diagnostics ====");
}

static int cmdline_has(const char *key)
{
	char buf[4096] = "";
	int fd = open("/proc/cmdline", O_RDONLY), n;

	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = 0;
	return strstr(buf, key) != NULL;
}

static void hang(const char *why)
{
	klog("stopping here (%s); the gadget stays active. Reboot with the power key.", why);
	for (;;)
		sleep(3600);
}

int main(void)
{
	mk("/proc"); mk("/sys"); mk("/dev"); mk("/tmp");
	mount("none", "/proc", "proc", 0, NULL);
	mount("none", "/sys", "sysfs", 0, NULL);
	mount("none", "/dev", "devtmpfs", 0, NULL);
	kmsg = open("/dev/kmsg", O_WRONLY);
	klog("mainline debug init started, pid %d", getpid());
	/* make the early lines visible on the console */
	wr("/proc/sys/kernel/printk", "7 4 1 7");
	/* jdbg.stop=early|gadget|diag: stop at that point to read the screen (default: shell) */
	if (cmdline_has("jdbg.stop=early")) {
		diag();
		hang("jdbg.stop=early");
	}
	gadget();
	if (cmdline_has("jdbg.stop=gadget"))
		hang("jdbg.stop=gadget");
	sleep(2);
	diag();
	if (cmdline_has("jdbg.stop=diag"))
		hang("jdbg.stop=diag");
	klog("ready: connect USB, open the ACM serial port for a shell");
	shell();
	return 0;
}
