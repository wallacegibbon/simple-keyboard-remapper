#include "time_util.h"

int timespec_cmp(struct timespec *t1, struct timespec *t2)
{
	if (t1->tv_sec > t2->tv_sec)
		return 1;
	if (t1->tv_sec < t2->tv_sec)
		return -1;

	/* `tv_sec` equals, now we compare `tv_nsec` */

	if (t1->tv_nsec > t2->tv_nsec)
		return 1;
	if (t1->tv_nsec < t2->tv_nsec)
		return -1;

	/* All equals */
	return 0;
}

int timespec_cmp_now_t(struct timespec *t)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return timespec_cmp(&now, t);
}

void timespec_add(struct timespec *t1, struct timespec *t2,
		 struct timespec *t)
{
	t->tv_sec = t1->tv_sec + t2->tv_sec;
	t->tv_nsec = t1->tv_nsec + t2->tv_nsec;
	if (t->tv_nsec >= 1000000000) {
		t->tv_nsec -= 1000000000;
		++t->tv_sec;
	}
}

/* Assumes t1 >= t2 here. */
void timespec_sub(struct timespec *t1, struct timespec *t2,
		 struct timespec *t)
{
	t->tv_sec = t1->tv_sec - t2->tv_sec;
	t->tv_nsec = t1->tv_nsec - t2->tv_nsec;
	if (t->tv_nsec < 0) {
		t->tv_nsec += 1000000000;
		--t->tv_sec;
	}
}
