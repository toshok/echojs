Echo - a batch compiler for JS, written in CoffeeScript
=======================================================

But... Why?
-----------

1. I was a PL geek in college, which is pretty much a lifetime ago.

2. I'd never written a compiler myself, nor played with LLVM, both of
which I now can quite confidently say are a blast.

3. I want to play around with what is essentially profile guided
optimization, but with runtime type information.  So you get a
partially specialized (at least as much as the static compilation can
give you) implementation, which then records type information at
runtime.  You feed this back into the compiler and get a more heavily
specialized version.

4. Right now there's no way to run a JIT on IOS devices.  So at the
moment the only JS competition for Echo in the use cases I'm
envisioning are the JS engines in interpreter mode.  When I did my
initial testing, spidermonkey was faster than JavaScriptCore, so I've
been using SM as the performance goal.  It should be possible to beat
the interpreter pretty easily, and the PGO/type inference gains should
get us up near (but likely not reaching in the general case) the JITs.

Why "Echo?"
-----------

Originally (check git history, before
[ed3020dde](https://github.com/toshok/echo-js/commit/ed3020dde7d33018720b26484e98390ab6c69718))
Echo was based directly on mozilla's Narcissus.  See
http://en.wikipedia.org/wiki/Echo_and_Narcissus

What works?
-----------

Quite a bit, although nearly everything has that
first-pass-is-a-throwaway-implementation feel to it.

Object, Function, String, Number, Array, Date, Regexp, Math builtins
are there and mostly implemented against ECMA262.  Prototypes are
there, although property lookups are still a linear search.

The gc is a very simple mark and sweep, but seems to keep everything
under control.

Echo now uses mozilla's NaN-boxing jsval (called ejsval here) so
primitive ints and doubles are stored unboxed.

Closure environments are now using a much more compact (and more
importantly much faster) representation.  Access to closed-over
identifiers is now nearly as fast as to non-closed identifiers
(a constant number of dereferences dictated by the number of
enclosing environments.)

What's next?
------------

Right now it's mostly tracking down compiler/runtime bugs that are
keeping the self-hosted compiler from running.

Typed arrays are another area that I'd like to see fleshed out sooner
rather than later.  They're pretty simple from an implementation
perspective.  Work on this has begun, check out runtime/ejs-typedarrays.c.

I'm starting to bring over the objective-c binding code from coffeekit,
originally closed-source but on its way into the open.

Pass the crackpipe?
-------------------

Echo will never (for obvious reasons) support eval or new Function to
generate new code, but there's no reason a JS interpreter (say,
narcissus) couldn't be embedded to fill that role.

I'm fighting hard the urge to rebase the C runtime on top of the awesome
[LLJS](http://mbebenita.github.com/LLJS/) syntax and compile that directly to efficient native code.
What has two thumbs and wants to write a GC in JS?


Big thanks
----------

Echo wouldn't be as far along as it is now (and certainly wouldn't be
as fun to work on) if not for the following:

1. Esprima:   [ariya/esprima](https://github.com/ariya/esprima)
2. Escodegen: [Constellation/escodegen](https://github.com/Constellation/escodegen)
3. LLVM:      http://llvm.org/git/llvm.git
4. Narcissus: [mozilla/narcissus](https://github.com/mozilla/narcissus)
