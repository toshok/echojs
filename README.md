EchoJS - an ahead of time compiler and runtime for EcmaScript
=============================================================

Building EchoJS
---------------

You need a couple of external dependencies to get things running:

1. node.js
2. llvm 3.4
3. coffeescript

The following commands should get you from 0 to echo-js built and tests run.

```sh
$ brew install node
$ brew install llvm34
$ npm install -g coffeescript
$ cd echo-js
$ make
$ make check
```

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


Big thanks
----------

Echo wouldn't be as far along as it is now (and certainly wouldn't be
as fun to work on) if not for the following:

1. Esprima:   [ariya/esprima](https://github.com/ariya/esprima)
2. Escodegen: [Constellation/escodegen](https://github.com/Constellation/escodegen)
3. LLVM:      http://llvm.org/git/llvm.git
4. Narcissus: [mozilla/narcissus](https://github.com/mozilla/narcissus)
