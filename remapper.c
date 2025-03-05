#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <poll.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define log_error(...) do { fprintf (stderr, __VA_ARGS__); fflush (stderr); } while (0)
//#define debug(...) do { printf (__VA_ARGS__); fflush (stdout); } while (0)
#define debug(...) (void)0

#define COUNTOF(x) (sizeof(x) / sizeof(*(x)))

typedef struct
{
  /// `key', `primary_function' and `secondary_function' are all key constants or `0'.
  long key;
  long primary_function;
  long secondary_function;

  long value;
  long last_secondary_function_value;
}
  mod_key;

mod_key mod_map[] =
  {
    {KEY_SPACE, 0, KEY_LEFTCTRL},
    {KEY_CAPSLOCK, KEY_ESC},
  };

static inline int
mod_key_pri_or_key (mod_key *self)
{
  return self->primary_function ? self->primary_function : self->key;
}

static int
mod_map_find (long key)
{
  for (size_t i = 0; i < COUNTOF (mod_map); i++)
    {
      if (mod_map[i].key == key)
	return i;
    }
  return -1;
};

static void
send_key_ev_and_sync (struct libevdev_uinput *uidev, long code, int value)
{
  int err;
  err = libevdev_uinput_write_event (uidev, EV_KEY, code, value);
  if (err != 0)
    {
      perror ("Error in writing EV_KEY event\n");
      exit (err);
    }

  err = libevdev_uinput_write_event (uidev, EV_SYN, SYN_REPORT, 0);
  if (err != 0)
    {
      perror ("Error in writing EV_SYN, SYN_REPORT, 0.\n");
      exit (err);
    }

  debug ("Sending %ld %d\n", code, value);
}

static int
send_2nd_fun_once (struct libevdev_uinput *uidev, mod_key *k, int value)
{
  if (k->last_secondary_function_value != value)
    {
      send_key_ev_and_sync (uidev, k->secondary_function, value);
      k->last_secondary_function_value = value;
      return 1;
    }
  else
    return 0;
}

static void
send_2nd_fun_all_active (struct libevdev_uinput *uidev)
{
  for (size_t i = 0; i < COUNTOF (mod_map); i++)
    {
      mod_key *k = &mod_map[i];
      if (k->value == 1 && k->secondary_function > 0)
	send_2nd_fun_once (uidev, k, 1);
    }
}

static inline void
send_pri_fun_mod (struct libevdev_uinput *uidev, mod_key *k, int value)
{
  send_key_ev_and_sync (uidev, mod_key_pri_or_key (k), value);
}

static void
send_pri_fun_code (struct libevdev_uinput *uidev, long code, int value)
{
  int i = mod_map_find (code);
  if (i >= 0)
    send_pri_fun_mod (uidev, &mod_map[i], value);
  else
    send_key_ev_and_sync (uidev, code, value);
}

static void
handle_ev_key_with_2nd_fun (struct libevdev_uinput *uidev, long code,
			    int value, mod_key *k)
{
  if (value == 0)
    {
      k->value = 0;
      if (send_2nd_fun_once (uidev, k, 0))
	{
	  /// Worked as secondary function and had been released.  Done.
	}
      else
	{
	  send_2nd_fun_all_active (uidev);
	  send_pri_fun_mod (uidev, k, 1);
	  send_pri_fun_mod (uidev, k, 0);
	}
    }
  else if (value == 1)
    {
      k->value = 1;
    }
  else
    {
      /// Ignore
    }
}

static void
handle_ev_key_no_2nd_fun (struct libevdev_uinput *uidev, long code, int value)
{
  if (value == 0)
    {
      send_pri_fun_code (uidev, code, 0);
    }
  else if (value == 1)
    {
      send_2nd_fun_all_active (uidev);
      send_pri_fun_code (uidev, code, 1);
    }
  else
    {
      /// Ignore
    }
}

static void
handle_ev_key (struct libevdev_uinput *uidev, long code, int value)
{
  int i = mod_map_find (code);
  if (i >= 0 && mod_map[i].secondary_function > 0)
    handle_ev_key_with_2nd_fun (uidev, code, value, &mod_map[i]);
  else
    handle_ev_key_no_2nd_fun (uidev, code, value);
}

/// The official documents of `libevdev' says:
///   You do not need libevdev_has_event_pending()
///   if you're using select(2) or poll(2).
///
/// But that's not the case for this program.
/// We need to call `libevdev_has_event_pending' before `poll'.
static void
evdev_block_for_events (struct libevdev *dev)
{
  struct pollfd poll_fd = {.fd = libevdev_get_fd (dev), .events = POLLIN};
  int has_pending_events = libevdev_has_event_pending (dev);
  if (has_pending_events == 1)
    {
      /// Nothing to do.
    }
  else if (has_pending_events == 0)
    {
      /// Block waiting for new events.
      if (poll (&poll_fd, 1, -1) <= 0)
	{
	  perror ("poll failed");
	  exit (1);
	}
    }
  else if (has_pending_events < 0)
    {
      perror ("libevdev check pending failed");
      exit (1);
    }
}

static int
evdev_read_and_skip_sync (struct libevdev *dev, struct input_event *event)
{
  int r =
    libevdev_next_event (dev,
			 LIBEVDEV_READ_FLAG_NORMAL |
			 LIBEVDEV_READ_FLAG_BLOCKING, event);

  if (r == LIBEVDEV_READ_STATUS_SYNC)
    {
      while (r == LIBEVDEV_READ_STATUS_SYNC)
	r = libevdev_next_event (dev, LIBEVDEV_READ_FLAG_SYNC, event);
    }
  return r;
}

int
main (int argc, char **argv)
{
  if (argc < 2)
    {
      log_error ("Argument Error: Necessary argument is not given.\n");
      exit (1);
    }

  debug ("Simple Keyboard Remapper is started.\n");

  /// let (KEY_ENTER), value 0 go through
  usleep (100000);

  int read_fd = open (argv[1], O_RDONLY);
  if (read_fd < 0)
    {
      perror ("Failed to open device\n");
      exit (1);
    }

  int ret;

  struct libevdev *dev = NULL;
  ret = libevdev_new_from_fd (read_fd, &dev);
  if (ret < 0)
    {
      log_error ("Failed to init libevdev (%s)\n", strerror (-ret));
      exit (1);
    }

  int write_fd = open ("/dev/uinput", O_RDWR);
  if (write_fd < 0)
    {
      log_error ("uifd < 0 (Do you have the right privileges?)\n");
      return -errno;
    }

  struct libevdev_uinput *uidev;

  /// IMPORTANT: Creating a new (e.g. /dev/input/event18) input device.
  ret = libevdev_uinput_create_from_device (dev, write_fd, &uidev);
  if (ret != 0)
    return ret;

  /// IMPORTANT: Blocking the events of the original keyboard device.
  ret = libevdev_grab (dev, LIBEVDEV_GRAB);
  if (ret < 0)
    {
      log_error ("grab < 0\n");
      return -errno;
    }

  do
    {
      evdev_block_for_events (dev);
      struct input_event event;
      ret = evdev_read_and_skip_sync (dev, &event);
      if (ret == LIBEVDEV_READ_STATUS_SUCCESS)
	{
	  if (event.type == EV_KEY)
	    handle_ev_key (uidev, event.code, event.value);
	}
    }
  while (ret == LIBEVDEV_READ_STATUS_SYNC
	 || ret == LIBEVDEV_READ_STATUS_SUCCESS || ret == -EAGAIN);

  /// If the program reach here, something is wrong.

  if (ret != LIBEVDEV_READ_STATUS_SUCCESS && ret != -EAGAIN)
    log_error ("Failed to handle events: %s\n", strerror (-ret));

  return 0;
}
