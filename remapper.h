#ifndef __REMAPPER_H
#define __REMAPPER_H

typedef unsigned long tsms_t;	/* timestamp in milliseconds */

#define MAX_DELAY_MS	200	/* Boundary time for map1 function */

/* OS specific functions */
tsms_t current_time();
int event_loop(long *keycode, long *keyvalue);
int send_key(int key, int value);
int init(const char *initarg);

/* Log utilities */
#include <stdio.h>
#define log_error(...) fprintf(stderr, __VA_ARGS__)

#if DEBUG == 1
#define debug(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)
#else
#define debug(...) (void)0
#endif

#endif
