#pragma once

#include "vendor/md4c.h"
#include "vendor/map.h"

// XXHash we use for dictionaries, giving us enough of a collision
// margin to be confident in using the hash value as an identity proxy
// for any use.
#define XXH_INLINE_ALL
#include "vendor/xxhash.h"

// Komi we use for type hashing, which uses 62-bit values. Komi never
// uses dynamic allocation on us.
#include "vendor/komihash.h"

#include <backtrace.h>
#include <curl/curl.h>
#include <linebreak.h>
#include <utf8proc.h>
