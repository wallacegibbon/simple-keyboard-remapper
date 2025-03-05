#include "libevdev/libevdev-uinput.h"
#include <sys/types.h>

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

static inline long
mod_key_primary_function (mod_key *self)
{
  return self->primary_function > 0 ? self->primary_function : self->key;
}
