#include "time_util.h"
#include <linux/uinput.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define log_error(...) fprintf(stderr, __VA_ARGS__)

#if (DEBUG == 1)
#define debug(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)
#else
#define debug(...) (void)0
#endif

#define COUNTOF(x) (sizeof(x) / sizeof(*(x)))

struct modkey {
	long key, primary_function, secondary_function;	/* key codes */
	long value, last_value;				/* key values */
	struct timespec last_time_down;
};

struct modkey mod_map[] = {
	{KEY_SPACE, 0, KEY_LEFTCTRL},
	{KEY_CAPSLOCK, KEY_ESC},
};

/*
 * If a key is held down for a time (milleseconds) greater than max_delay,
 * it will not send its primary function when released.
 */
long max_delay = 200;

/* Will be filled with `max_delay'. */
struct timespec delay_timespec;

static inline int modkey_primary_or_key(struct modkey *self)
{
	return self->primary_function
		? self->primary_function
		: self->key;
}

static struct modkey *mod_map_find(long key)
{
	struct modkey *m = mod_map;
	struct modkey *end = mod_map + COUNTOF(mod_map);

	for (; m < end; ++m) {
		if (m->key == key)
			return m;
	}

	return NULL;
};

/*
 * For functions that may send `input_event` data, return positive number of
 * data sent, or negagive number on error.  (SYN is not calculated)
 */

/* This function always return 1 on successful sent. */
static int send_key(int fd, int key, int value)
{
	struct input_event e = {.type = EV_KEY, .code = key, .value = value};
	struct input_event s = {.type = EV_SYN, .code = SYN_REPORT};

	if (write(fd, &e, sizeof(e)) < 0)
		return -1;
	if (write(fd, &s, sizeof(s)) < 0)
		return -2;

	debug("Key: %d (value: %d)\n", key, value);
	return 1;
}

static int try_send_2nd(int fd, struct modkey *k, int value)
{
	if (k->last_value == value)
		return 0;

	if (send_key(fd, k->secondary_function, value) < 0)
		return -1;

	k->last_value = value;
	return 1;
}

static int active_modkeys_send_1_once(int fd)
{
	struct modkey *m = mod_map;
	struct modkey *end = mod_map + COUNTOF(mod_map);
	int n = 0, t;

	for (; m < end; ++m) {
		if (m->value == 1 && m->secondary_function > 0) {
			if ((t = try_send_2nd(fd, m, 1)) < 0)
				return -1;
			n += t;
		}
	}

	return n;
}

static long duration_to_now(struct timespec *t)
{
	struct timespec now, tmp;
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespec_sub(&now, t, &tmp);
	return timespec_to_ms(&tmp);
}

static int send_primary_on_short_stroke(int fd, struct modkey *k)
{
	struct timespec t;
	int n = 0;

	timespec_add(&k->last_time_down, &delay_timespec, &t);

	/* Just ignore the stroke when it has been held for too long. */
	if (timespec_cmp_now(&t) > 0)
		return 0;

	if ((n = active_modkeys_send_1_once(fd)) < 0)
		return -1;

	if (send_key(fd, modkey_primary_or_key(k), 1) < 0)
		return -2;

	if (send_key(fd, modkey_primary_or_key(k), 0) < 0)
		return -3;

	return n + 2;
}

static int handle_complex(int fd, struct modkey *k, int value)
{
	int n = 0;

	if (value < 0 || value > 2)
		return 0;

	/* Key press.  Just record it and update key down time. */
	if (value == 1) {
		k->value = 1;
		clock_gettime(CLOCK_MONOTONIC, &k->last_time_down);
		return 0;
	}

	/* Key repeat.  which means key has been held for some time. */
	if (value == 2) {
		if ((n = try_send_2nd(fd, k, 1)) < 0)
			return -1;
		return 0;
	}

	/* Key release.  This is the complex situation. */

	debug("Duration: %ld\n", duration_to_now(&k->last_time_down));

	k->value = 0;
	if ((n = try_send_2nd(fd, k, 0)) < 0)
		return -1;

	/* 2nd fun is sent on this release, done. */
	if (n > 0)
		return n;

	/* The 2nd fun was not sent, send primary key unless timeout. */
	if ((n = send_primary_on_short_stroke(fd, k)) < 0)
		return -1;

	return n;
}

static int handle_normal(int fd, int code, int value)
{
	int n = 0;

	/* For simple keys, we send modkeys on press, not release. */
	if (value == 1) {
		if ((n = active_modkeys_send_1_once(fd)) < 0)
			return -1;
	}

	if (send_key(fd, code, value) < 0)
		return -2;

	return n + 1;
}

static int handle_ev(int fd, long code, int value)
{
	struct modkey *k = mod_map_find(code);
	if (k == NULL)
		return handle_normal(fd, code, value);

	if (k->secondary_function != 0)
		return handle_complex(fd, k, value);
	else
		return handle_normal(fd, modkey_primary_or_key(k), value);
}

int main(int argc, const char **argv)
{
	struct uinput_user_dev uidev = {0};
	struct input_event ev = {0};
	int physical_fd, uinput_fd, i;

#define LONGBITS (8 * sizeof(unsigned long))
	unsigned long kbits[KEY_MAX / LONGBITS + 1] = {0};

	if (argc < 2) {
		log_error("Argument Error: Argument missing.\n");
		return 1;
	}

	/* Sleep 100ms on startup, or the program behave weird. */
	usleep(100000);

	physical_fd = open(argv[1], O_RDONLY);
	if (physical_fd < 0) {
		perror("Failed to open the source keyboard\n");
		return 2;
	}

	if (ioctl(physical_fd, EVIOCGRAB, 1) < 0) {
		perror("EVIOCGRAB failed");
		goto err1;
	}

	if (ioctl(physical_fd, EVIOCGBIT(EV_KEY, sizeof(kbits)), kbits) < 0) {
		perror("EVIOCGBIT");
		goto err1;
	}

	uinput_fd = open("/dev/uinput", O_WRONLY);
	if (uinput_fd < 0) {
		perror("Failed to open /dev/uinput.\n");
		goto err2;
	}

	if (ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY) != 0) {
		log_error("Failed on UI_SET_EVBIT.\n");
		goto err3;
	}

	for (i = 0; i <= KEY_MAX; ++i) {
		if (kbits[i / LONGBITS] & (1UL << (i % LONGBITS))) {
			if (ioctl(uinput_fd, UI_SET_KEYBIT, i) != 0) {
				log_error("Failed on UI_SET_EVBIT.\n");
				goto err3;
			}
		}
	}

	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "Keyboard Remapper");
	uidev.id.bustype = BUS_USB;
	if (write(uinput_fd, &uidev, sizeof(uidev)) < 0) {
		log_error("Failed configure virtual device.\n");
		goto err3;
	}

	if (ioctl(uinput_fd, UI_DEV_CREATE) != 0) {
		log_error("Failed UI_DEV_CREATE.\n");
		goto err3;
	}

	/* Give time for device to register. */
	sleep(1);

	debug("Simple Keyboard Remapper is started.\n");

	ms_to_timespec(max_delay, &delay_timespec);

	while (read(physical_fd, &ev, sizeof(ev)) > 0) {
		if (ev.type == EV_KEY) {
			if (handle_ev(uinput_fd, ev.code, ev.value) < 0)
				goto err4;
		}
	}

	return 0;

err4:
	ioctl(uinput_fd, UI_DEV_DESTROY);
err3:
	close(uinput_fd);
err2:
	ioctl(physical_fd, EVIOCGRAB, 0);
err1:
	close(physical_fd);

	return -1;
}
