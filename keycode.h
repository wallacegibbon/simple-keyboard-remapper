#ifndef __KEYCODE_H
#define __KEYCODE_H

/* Keycode macros (like KEY_ESC) used by remapper.c should be defined here. */

#if __linux__
/* Linux is the default platform, other platforms adapt to Linux */
#include <linux/uinput.h>
#endif

#endif
