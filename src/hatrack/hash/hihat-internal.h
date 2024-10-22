#include "hatrack/hihat.h" // IWYU pragma: export

// The aforementioned flags, along with a bitmask that allows us to
// extract the epoch in the info field, ignoring any migration flags.
enum64(hihat_flag_t,
       HIHAT_F_MOVING   = 0x8000000000000000,
       HIHAT_F_MOVED    = 0x4000000000000000,
       HIHAT_F_INITED   = 0x2000000000000000,
       HIHAT_EPOCH_MASK = 0x1fffffffffffffff);
