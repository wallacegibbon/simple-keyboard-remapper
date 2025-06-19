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
	long key;
	long primary_function;
	long secondary_function;

	long value;
	long last_value;

	struct timespec last_time_down;
};

struct modkey mod_map[] = {
	{KEY_SPACE, 0, KEY_LEFTCTRL},
	{KEY_CAPSLOCK, KEY_ESC},
};

/*
 * If a key is held down for a time greater than max_delay,
 * it will not send its primary function when released.
 */
long max_delay = 200;

/*
 * Max delay set by user stored as a timespec struct.
 * It will be filled with `max_delay'.
 */
struct timespec delay_timespec;

static inline int modkey_primary_or_key(struct modkey *self)
{
	return self->primary_function
		? self->primary_function
		: self->key;
}

static int mod_map_find(long key)
{
	for (size_t i = 0; i < COUNTOF(mod_map); i++) {
		if (mod_map[i].key == key)
			return i;
	}
	return -1;
};

static int send_key(int fd, int key, int value)
{
	struct input_event e = {.type = EV_KEY, .code = key, .value = value};
	gettimeofday(&e.time, NULL);
	if (write(fd, &e, sizeof(e)) < 0)
		return -1;
	return 0;
}

static int send_sync(int fd)
{
	struct input_event s = {.type = EV_SYN, .code = SYN_REPORT, .value = 0};
	gettimeofday(&s.time, NULL);
	if (write(fd, &s, sizeof(s)) < 0)
		return -1;
	return 0;
}

static int send_key_and_sync(int fd, int key, int value)
{
	if (send_key(fd, key, value) < 0)
		return -1;
	if (send_sync(fd) < 0)
		return -2;

	return 0;
}

/* Return the number of keys sent, or -1 on error */
static int send_2nd_fun_once(int fd, struct modkey *k, int value)
{
	if (k->last_value == value)
		return 0;

	if (send_key_and_sync(fd, k->secondary_function, value) < 0)
		return -1;

	k->last_value = value;
	return 1;
}

static int active_modkeys_send_1_once(int fd)
{
	for (size_t i = 0; i < COUNTOF(mod_map); i++) {
		struct modkey *k = &mod_map[i];
		if (k->value == 1 && k->secondary_function > 0) {
			if (send_2nd_fun_once(fd, k, 1) < 0)
				return -1;
		}
	}
	return 0;
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
	timespec_add(&k->last_time_down, &delay_timespec, &t);

	/* Just ignore the stroke when it has been held for too long */
	if (timespec_cmp_now(&t) > 0)
		return 0;

	if (active_modkeys_send_1_once(fd) < 0)
		return -1;
	if (send_key(fd, modkey_primary_or_key(k), 1) < 0)
		return -2;
	if (send_key(fd, modkey_primary_or_key(k), 0) < 0)
		return -3;
	if (send_sync(fd))
		return -4;
	return 0;
}

static int handle_ev_modkey_with_2nd_fun(int fd, struct modkey *k, int value)
{
	int tmp;
	if (value == 0) {
		debug("Duration: %ld\n", duration_to_now(&k->last_time_down));
		k->value = 0;
		tmp = send_2nd_fun_once(fd, k, 0);
		if (tmp < 0)
			return -1;
		if (tmp > 0)
			return 0;

		/* 2nd fun NOT sent, it may be a normal stroke */
		if (send_primary_on_short_stroke(fd, k) < 0)
			return -1;
	} else if (value == 1) {
		k->value = 1;
		clock_gettime(CLOCK_MONOTONIC, &k->last_time_down);
	} else {
		/* Ignore */
	}
	return 0;
}

static int handle_ev_modkey_no_2nd_fun(int fd, struct modkey *k, int value)
{
	if (value == 1) {
		if (active_modkeys_send_1_once(fd) < 0)
			return -1;
	}

	if (send_key_and_sync(fd, modkey_primary_or_key(k), value) < 0)
		return -2;

	return 0;

}

static int handle_ev_normal_key(int fd, int code, int value)
{
	if (value == 1) {
		if (active_modkeys_send_1_once(fd) < 0)
			return -1;
	}

	if (send_key_and_sync(fd, code, value) < 0)
		return -2;

	return 0;
}

static int handle_ev_key(int fd, long code, int value)
{
	struct modkey *k;
	int i;

	i = mod_map_find(code);
	if (i < 0)
		return handle_ev_normal_key(fd, code, value);

	k = &mod_map[i];
	if (k->secondary_function > 0)
		return handle_ev_modkey_with_2nd_fun(fd, k, value);
	else
		return handle_ev_modkey_no_2nd_fun(fd, k, value);
}

int main(int argc, const char **argv)
{
	struct uinput_user_dev uidev = {0};
	struct input_event ev = {0};
	int read_fd, uinput_fd, i;

	if (argc < 2) {
		log_error("Argument Error: Argument missing.\n");
		return 1;
	}

	/* Sleep 100ms on startup, or the program behave weird. */
	usleep(100000);

	read_fd = open(argv[1], O_RDONLY);
	if (read_fd < 0) {
		perror("Failed to open the source keyboard\n");
		return 2;
	}

	ioctl(read_fd, EVIOCGRAB, 0);
	if (ioctl(read_fd, EVIOCGRAB, 1) < 0) {
		perror("EVIOCGRAB failed");
		goto err1;
	}

	uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (uinput_fd < 0) {
		perror("Failed to open /dev/uinput.\n");
		goto err2;
	}

	if (ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY) != 0) {
		log_error("Failed on UI_SET_EVBIT.\n");
		goto err3;
	}

	for (i = 0; i <= KEY_MAX; ++i) {
		if (ioctl(uinput_fd, UI_SET_KEYBIT, i) != 0) {
			log_error("Failed on UI_SET_EVBIT.\n");
			goto err3;
		}
	}

	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "key-remapper");
	uidev.id.bustype = BUS_USB;
	if (write(uinput_fd, &uidev, sizeof(uidev)) < 0) {
		log_error("Failed configure virtual device.\n");
		goto err3;
	}

	if (ioctl(uinput_fd, UI_DEV_CREATE) != 0) {
		log_error("Failed UI_DEV_CREATE.\n");
		goto err3;
	}

	/* Give time for device to register */
	sleep(1);

	debug("Simple Keyboard Remapper is started.\n");

	ms_to_timespec(max_delay, &delay_timespec);

	while (read(read_fd, &ev, sizeof(ev)) > 0) {
		if (ev.type == EV_KEY) {
			if (handle_ev_key(uinput_fd, ev.code, ev.value) < 0)
				goto err4;
		}
	}

	return 0;

err4:
	ioctl(uinput_fd, UI_DEV_DESTROY);
err3:
	close(uinput_fd);
err2:
	ioctl(read_fd, EVIOCGRAB, 0);
err1:
	close(read_fd);

	return -1;
}
