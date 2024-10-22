N00b is a new programming language targeting embedability within
arbitrary programs (like Lua). Generally, your typical user should
feel like they're editing a typical config file, but your power users
can have a lot of flexibility (and that flexibility is under your
control).

Over time, we plan to make N00b a more general-purpose programming
language, aimed at being easy and flexible in the vein of Python, yet
strongly type safe (achieved by static type checking, but full type
inferencing).

As such, there are still a lot of features planned on that path, so
1.0 is not close.

Nonetheless, N00b is useful at scale; we have widely deployed it in
our own projects running across millions of containers.

Feature overview:

- Designed to be embedded in other languages as a config file
  language, with full support for users writing callbacks your program
  can call, even after their config is fully loaded.

- Strongly typed; almost everything is fully type checked at compile
  time. Full type information is available and usable at run-time.

- Fully typed inferenced. Currently, there are no cases where a type
  must be specified.  Soon, a pretty-printer will support
  automatically adding the explicit types infered back into code, so
  that people can write w/o types, but get the benefit of the checking
  and documentation.

- Builtin 'Attributes'; n00b can fully check your user's config, then
  make it available to you via a simple interface. You can specify the
  allowed attributes and sections, use builtin validators, or write
  custom ones.

- Lock-free dictionaries and sets built in.

- Can magically auto-marshal any data.

- Supports save and restore for execution state, allowing for
  incremental execution. We use this in Chalk to load a massive amount
  of base configuration, in advance that runs once when we compile the
  EXE. The only incremental work needed is

- Builtin rich text type, with a system for defining text styles.

- A builtin text layout (grid) type, also with styling, and primitives
  for printing trees, etc.

- Builtin BNF parsing capabilities.

- No-code getopts capability that is more flexible than any other
  getopt library.

- Garbage collected.

- Easy FFI for wrapping C calls.

- Stack traces include both the n00b stack and the stack of any C
  code whenever possible.


Some of the major planned items:

- A REPL with save / restore, and many debugging tools.

- An option to generate fully compiled programs (later on).

- Full IO teeing.
