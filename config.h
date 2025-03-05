#include "./simple-keyboard-remapper.h"

mod_key mod_map[] =
  {
    /// The most ergonomic idea for a qwerty keyboard.
    {KEY_SPACE, 0, KEY_LEFTCTRL},

    /// CAPSLOCK into ESC
    {KEY_CAPSLOCK, KEY_ESC},
  };
