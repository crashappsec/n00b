# N00b Source directory

Currently, this directory is all C code. N00b code lives in `/sys` or
`/test`.

Directories under this one:

- `compiler` Is the N00b compiler.
- `core` Is the core runtime, including the the type system,
   the garbage collector and the VM.
- `adts` Contains the base data types used throughout; most of them
  are exported to the language.
- `io` is the home of terminal and network code.
- `util` contains misc utilities used by the implementation.
- `hatrack` is a library of lock-free data structures, primarily hash
   tables. This is core to N00b's dictionary and set implementations,
   and eventually more of the data types there will get integrated.
- `quark` is a tiny shim for providing core atomic ops in environments
   where the linked standard library won't have them (this is really
   just MUSL right now).
- `tests` is where all tests go, for both N00b and Hatrack.
