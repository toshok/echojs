EchoJS - an ahead of time compiler and runtime for EcmaScript
=============================================================

[![Build Status](https://travis-ci.org/toshok/echojs.svg?branch=master)](https://travis-ci.org/toshok/echojs)
[![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/toshok/echojs?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Building EchoJS
---------------

Things only build reliably on OSX.  I have easy access to other platforms, I just haven't had the time/motivation to do it given OSX is what I have in front of me every hour of every day, and ios development is my biggest goal.  Patches welcome!

On OSX

You'll need a couple of external dependencies to get things running:

1. node.js
2. llvm 3.6
3. coffeescript

The following commands should get you from 0 to echo-js built:

```sh
$ brew tap homebrew/versions  # so we can get the specific version of llvm below
$ brew install node
$ brew install llvm
$ export PATH=/usr/local/opt/llvm/bin:$PATH
$ npm install
$ npm install -g node-gyp babel
$ export MIN_OSX_VERSION=10.8 # only if you're running 10.8, see below
$ cd echo-js
$ git submodule init
$ git submodule update
$ make
```

The environment variable `LLVM_SUFFIX` can be set and its value will be appended to the names of all llvm executables (e.g. `llvm-config-3.6` instead of `llvm-config`.)  use this if you have a different build of
llvm you want to use.  Homebrew installs llvm 3.6 executables without the suffix, so you can leave this blank.

As for `MIN_OSX_VERSION`: homebrew's formula for llvm (3.4, at least.  haven't verified with 3.6) doesn't specify a `-mmacosx-version-min=` flag, so it builds to whatever you have on your machine.  Node.js's gyp support in node-gyp, however, *does* put a `-mmacosx-version-min=10.5` flag.  A mismatch here causes the node-llvm binding to allocate llvm types using incorrect size calculations, and causes all manner of memory corruption.  If you're either running 10.5 or 10.9, you can leave the variable unset.  Otherwise, set it to the version of OSX you're running.  Hopefully some discussion with the homebrew folks will get this fixed upstream.

both of these variable assignments can be placed in `echo-js/build/config-local.mk`.


On Linux



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
