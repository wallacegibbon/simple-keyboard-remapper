#include "remapper.h"
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

/* This variable will hold fd to uinput device after initialization */
static int physical_fd, uinput_fd;

int send_key(int key, int value)
{
	struct input_event e = { .type = EV_KEY, .code = key, .value = value };
	struct input_event s = { .type = EV_SYN, .code = SYN_REPORT };

	if (write(uinput_fd, &e, sizeof(e)) < 0)
		return -1;
	if (write(uinput_fd, &s, sizeof(s)) < 0)
		return -2;

	debug("Key: %d (value: %d)\n", key, value);
	return 1;
}

tsms_t current_time()
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

int event_loop(long *keycode, long *keyvalue)
{
	struct input_event ev = { 0 };
	for (;;) {
		if (read(physical_fd, &ev, sizeof(ev)) <= 0)
			return 1;
		if (ev.type == EV_KEY)
			break;
	}
	*keycode = ev.code;
	*keyvalue = ev.value;
	return 0;
}

int init(const char *initarg)
{
	struct uinput_user_dev uidev = { 0 };
	int i;

#define LONGBITS (8 * sizeof(unsigned long))
	unsigned long kbits[KEY_MAX / LONGBITS + 1] = { 0 };

	/* !! Sleep 100ms on startup, or the program behave weird !! */
	usleep(100000);

	physical_fd = open(initarg, O_RDONLY);
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
