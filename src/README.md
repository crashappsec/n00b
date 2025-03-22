# N00b Source directory

Currently, this directory is all C code. N00b code lives in `/sys` or
`/test`.

Directories under this one:

- `cmd` is for end-executables, such as the compiler.
- `compiler` Is the core N00b compiler implementation.
- `core` Is the core runtime, including the the type system,
   the garbage collector and the VM.
- `text` Contains the string implementation, formatting, tables, etc.   
- `adts` Contains the rest of base data types used throughout; most of them
   are exported to the language. The directory name is a bit of a misnomer,
   as it's not just abstract types.
- `io` is the home of the full IO system, including io streams, network code
  and terminal-related code.
- `util` contains misc utilities used by the implementation.
- `hatrack` is my library of lock-free data structures, primarily hash
   tables that predates n00b, and currently does live here. This is core
   to N00b's dictionary and set implementations, and some of the other
   types are slowly being wrapped as needed.
- `tests` is where all tests go, for both N00b and Hatrack.
- `harness` is the old test harness; this will go away soon.
- `crypto` is for wrapped cryptography (currently only SHA-256).
- `quark` is a tiny shim for providing core atomic ops in environments
   where the linked standard library won't have them (this is really
   just MUSL right now).
