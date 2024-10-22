# The Hatrack hash table test suite

This builds automatically when you run
```
meson compile
```

That will add an executable names `hash` under your build directory,
which, by default, runs a bunch of timing tests, and can also run some
very basic functionality tests (by passing the`--functional-tests`
flag).

If you'd like to see counters for most of the lock-free
implementations, to see how often compare-and-swap applications fail,
then compile with `-DHATRACK_COUNTERS` on. This will slow down the
algorithms that use it, considerably.

I've run these tests in the following environments:

1) An M1 Mac laptop, using clang.

2) An x86 without a 128-bit compare-and-swap, using `clang`.

3) The same x86, using `gcc`.

I've tested with a few different memory managers as well.

In my testing, the lock-free algorithms usually performs about as
well-as their locking counterparts, or even a tiny bit better. That
can be true even in my environments where there's no 128-bit
compare-and-swap operation.

I expect the lock-free algorithms to NOT be any sort of bottleneck
to scaling. The `malloc` implementation is more likely. In fact, I've
switched out `malloc`s using `LD_PRELOAD`, and on the Mac, the system
allocator definitely seems to be a bottleneck, as with `hoard`,
performance scales much more linearly.

You can run custom tests from the command line; `hash --help` should
get you started.