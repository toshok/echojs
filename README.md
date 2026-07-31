EchoJS - an ahead of time compiler and runtime for EcmaScript
=============================================================

[![Build Status](https://dl.circleci.com/status-badge/img/gh/toshok/echojs/tree/main.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/gh/toshok/echojs/tree/main)

Building EchoJS
---------------

Things only build reliably on OSX.  I have easy access to other platforms, I just haven't had the time/motivation to do it given OSX is what I have in front of me every hour of every day, and ios development is my biggest goal.  Patches welcome!

On OSX

The build uses [buck2](https://buck2.build).  You'll need:

1. node.js
2. llvm (homebrew's current keg; the path lives in `.buckconfig` under `[llvm] prefix`)
3. buck2

The following commands should get you from 0 (well, Homebrew and Xcode) to echo-js built and tested:

```sh
$ brew install node llvm
$ npm install
$ git submodule init
$ git submodule update
$ ./node-llvm/build-addon.sh      # builds the node addon the stage0 compiler uses
$ buck2 build //:ejs.exe          # stage1 compiler (node-hosted stage0 compiles ejs-es6.js)
$ buck2 build //:ejs.exe.stage3   # full bootstrap: stage1 -> stage2 -> stage3
$ buck2 build //:test-stage3      # run the test suite against stage3
```

Useful targets:

- `//:ejs.exe.stage{1,2,3}` — the bootstrap stages (`//:ejs.exe` is an alias for stage1)
- `//:test-stage{1,2,3}` — build a stage and run the test suite (`test/tester.ts`) against it; the build fails if any test fails, and the output artifact is the test log
- `//:srcdir-tree` — the assembled `--srcdir` layout the compiler runs against

If your llvm lives somewhere other than `/opt/homebrew/opt/llvm`, change `[llvm] prefix` in `.buckconfig`.

On Linux

The BUCK files carry `config//os:linux` selects for the runtime and deps, but the linux build hasn't been exercised recently.  Patches welcome!



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
