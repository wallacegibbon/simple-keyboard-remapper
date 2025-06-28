#ifndef __TIME_UTIL_H
#define __TIME_UTIL_H

#include <time.h>

int timespec_cmp(struct timespec *t1, struct timespec *t2);

int timespec_cmp_now_t(struct timespec *t);

void timespec_add(struct timespec *t1, struct timespec *t2,
		struct timespec *t);

void timespec_sub(struct timespec *t1, struct timespec *t2,
		struct timespec *t);

static inline long timespec_to_ms(struct timespec *t)
{
	return t->tv_sec * 1000 + t->tv_nsec / 1000000;
}

static inline void ms_to_timespec(long ms, struct timespec *t)
{
	t->tv_sec = ms / 1000;
	t->tv_nsec = (ms % 1000) * 1000000;
}

#endif
