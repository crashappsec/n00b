# ADTs-- Abstract Data Types

This directory contains C implementations for data types provided by
N00b. A few things primarily live in Hatrack-- the set implementation
and the dictionary implementation. What's in this directory is minor
n00b-specific stuff, like integration with the garbage collector.

Current status of data types:

1. Strings. N00b strings are meant to be non-mutable. The API
supports both UTF-8 and UTF-32 encodings (I'm considering having
strings hang on to both representations if the string is used in a way
that requires a conversion). Additionally, strings carry style
information out-of-band, that is applied when rendering (generally via
the ascii module in the io directory). These are available through
N00b itself.

The core of the implementation is in `strings.c`, but a bunch of
related code lives in the `util` subdirectory, like code to support
'rich' text literals (available using a variety of lit modifiers).

2. Grids. Grids build on top of strings to make it easy to do things
like tables, flows, etc. These are meant to be first class citizens in
N00b, and currently have some functionaility exposed.

3. A buffer type, available from N00b.

4. A stream type, not yet exposed (but will be).

5. A `callback` type, which probably doesn't work yet, but should be
exposed.

6. A tuple type, exposed to the language.

7. A tree type, not yet exposed.

8. Standard numeric types. Currently, we lack 16 bit and 128 bit
types. `char` in N00b is a 32 bit int.

9. A bitfield, not yet exposed.

10. An internal `box` type. Eventually, the `mixed` type will be
exposed, but it's not close to done.

11. Some light hatrack wrappers.  `set` and `dict` are well exposed,
but things like `rings` are not.
