#include "remapper.h"
#include "keycode.h"

#define COUNTOF(x) (sizeof(x) / sizeof(*(x)))

struct modkey {
	long key, map1, map2;			/* Original and mapped keys */
	long value, last_value;			/* Key values */
	tsms_t last_down_time;
};

static struct modkey mod_map[] = {
	{ .key = KEY_SPACE,	.map1 = KEY_SPACE,	.map2 = KEY_LEFTCTRL },
	{ .key = KEY_CAPSLOCK,	.map1 = KEY_ESC },
	{ .key = KEY_ESC,	.map1 = KEY_GRAVE },
};

static struct modkey *mod_map_find(long key)
{
	struct modkey *k = mod_map;
	struct modkey *end = mod_map + COUNTOF(mod_map);

	for (; k < end; ++k) {
		if (k->key == key)
			return k;
	}

	return NULL;
};

static int try_send_map2(struct modkey *k, int value)
{
	if (k->last_value == value)
		return 0;
	if (send_key(k->map2, value) < 0)
		return -1;

	k->last_value = value;
	return 1;
}

static int send_active_map2_once()
{
	struct modkey *k = mod_map;
	struct modkey *end = mod_map + COUNTOF(mod_map);
	int n = 0, t = 0;

	for (; k < end; ++k, n += t) {
		if (k->value == 1 && k->map2 > 0) {
			if ((t = try_send_map2(k, 1)) < 0)
				return -1;
		}
	}

	return n;
}

static inline int modkey_timeout(struct modkey *k)
{
	return k->last_down_time + MAX_DELAY_MS < current_time();
}

static int send_map1_down_up(struct modkey *k)
{
	int n = 0;
	if ((n = send_active_map2_once()) < 0)
		return -1;
	if (send_key(k->map1, 1) < 0)
		return -2;
	if (send_key(k->map1, 0) < 0)
		return -3;

	return n + 2;
}

static int handle_complex_down(struct modkey *k)
{
	k->value = 1;
	k->last_down_time = current_time();
	return 0;
}

static int handle_complex_up(struct modkey *k)
{
	int n = 0;
	debug("Duration: %ld\n", current_time() - k->last_down_time);

	k->value = 0;
	if ((n = try_send_map2(k, 0)) < 0)
		return -1;
	if (n > 0)
		return n;

	/* The map2 key was not sent, send map1 key unless timeout. */
	if (modkey_timeout(k))
		return 0;
	if ((n = send_map1_down_up(k)) < 0)
		return -1;

	return n;
}

static int handle_complex_repeat(struct modkey *k)
{
	/* The repeating trigger time could be lower than delay timeout */
	if (!modkey_timeout(k))
		return 0;

	if (try_send_map2(k, 1) < 0)
		return -1;
	else
		return 1;
}

static int handle_complex(struct modkey *k, int value)
{
	switch (value) {
	case 0:		return handle_complex_up(k);
	case 1:		return handle_complex_down(k);
	case 2:		return handle_complex_repeat(k);
	default:	return 0;
	}
}

static int handle_normal(int keycode, int keyvalue)
{
	int n = 0;
	/* For simple keys, we send modkeys on press, not release. */
	if (keyvalue == 1) {
		if ((n = send_active_map2_once()) < 0)
			return -1;
	}
	if (send_key(keycode, keyvalue) < 0)
		return -2;

	return n + 1;
}

static int handle_ev(long keycode, int keyvalue)
{
	struct modkey *k = mod_map_find(keycode);
	if (k == NULL)
		return handle_normal(keycode, keyvalue);
	if (k->map1 == 0) {
		log_error("map1 of key \"%ld\" should not be 0\n", k->key);
		return -1;
	}
	if (k->map2 != 0)
		return handle_complex(k, keyvalue);
	else
		return handle_normal(k->map1, keyvalue);
}

int main(int argc, const char **argv)
{
	long keycode, keyvalue;

	if (argc < 2) {
		log_error("Argument Error: Argument missing.\n");
		return 1;
	}

	if (init(argv[1]) != 0) {
		log_error("Failed initializing the system.\n");
		return 1;
	}

	while (!event_loop(&keycode, &keyvalue))
		handle_ev(keycode, keyvalue);

	return 0;
}
