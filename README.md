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

Why "Echo?"
-----------

Originally (check git history, before [ed3020dde](https://github.com/toshok/echo-js/commit/ed3020dde7d33018720b26484e98390ab6c69718)) echo was based directly on narcissus.  See http://en.wikipedia.org/wiki/Echo_and_Narcissus

What works?
-----------

Not much at present.  There are no constructors, no prototypes, no
"this", no builtin objects at all.

What does work is very simple code.  closures do work ([test/sieve.js](https://github.com/toshok/echo-js/blob/master/test/sieve.js)
uses them.  a whole lot of them.), so you can do pretty interesting
things, even with the limitted feature set at present.

At the moment I'm more concerned with getting everything working in
this terribly slow universe of "everything-is-boxed,
closure-environments-are-js-objects, field-access-is-a-linear-lookup"
before worrying too much about performance, or even really about
semantic parity with ECMAScript.  Also I'm not sure I'll ever be
interested in supporting non-strict code, so ECMA parity is kinda out
the window already.


What's next?
------------

I'm currently trying to build up enough functionality in the compiler
to bootstrap itself.  There are a few areas that need to be fleshed
out:

1. Object and Array builtin types and some of the methods on them

2. Constructors and prototypes (and this, of course)

3. An ffi interface to call C code

4. A replacement set of llvm bindings for Echo.  I don't think I'll be
keeping the v8 interfaces.

5. enough of the node.js IO/process interfaces to build the ejs driver


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