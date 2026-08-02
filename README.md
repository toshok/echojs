EchoJS — an ahead-of-time compiler and runtime for EcmaScript
=============================================================

[![CI](https://github.com/toshok/echojs/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/toshok/echojs/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/toshok/echojs)](https://github.com/toshok/echojs/releases/latest)

EchoJS compiles JavaScript directly to native executables — no
bundled interpreter, no JIT — via its own SSA middle-end (EIR) and
LLVM.  Programs get a generational moving GC with precise compiler-
emitted roots, hidden-class ("shape") tracking with guarded fast
paths, allocation sinking, and a compiler that is itself an EchoJS
program: the release binaries are the compiler compiled by itself,
proven by a byte-identity bootstrap on every CI run (macOS arm64,
Linux arm64/x86_64).

Install
-------

Prerequisites, everywhere: **LLVM 22** (`opt`/`llc` — the compiler
discovers a matching installation and refuses to run with a different
major; macOS: `brew install llvm`, Linux: [apt.llvm.org](https://apt.llvm.org))
and a C++ linker driver (`clang++` on PATH, or set `CXX`).  On Linux,
compiled programs also link against libuv and libunwind
(`apt install libuv1-dev libunwind-dev`).

**Via npm** (needs node ≥ 18; downloads the platform toolchain on
install):

```sh
$ npm install -g @pirouette/echojs
```

**Via release tarball** (no node needed):

```sh
$ curl -LO https://github.com/toshok/echojs/releases/download/v0.2.0/echojs-0.2.0-arm64-macos.tar.gz
$ tar xzf echojs-0.2.0-arm64-macos.tar.gz
$ cd echojs-0.2.0-arm64-macos
$ sudo ./install.sh                 # into /usr/local
$ ./install.sh --prefix ~/.local    # or anywhere writable
```

(Substitute `arm64-linux` or `x86_64-linux`; `--uninstall` reverses
it.  The unpacked directory also works in place — `bin/`, `include/`
and `lib/` just have to stay together.)

A Homebrew formula (`echojs.rb`) ships with each release; a tap is
coming.

Quickstart
----------

```sh
$ cat > hello.js <<'EOF'
class Greeter {
    constructor(who) { this.who = who; }
    greet() { return `hello, ${this.who}!`; }
}
console.log(new Greeter("world").greet());
EOF
$ ejs -o hello hello.js
$ ./hello
hello, world!
```

Multi-module programs compile from their entry point — `ejs` follows
relative imports (note: extensionless specifiers) and compiles the
whole program:

```sh
$ cat > greet.js <<'EOF'
export function greet(who) {
    return [...who].map((c) => c.toUpperCase()).join("");
}
EOF
$ cat > main.js <<'EOF'
import { greet } from "./greet";
import * as path from "@node-compat/path";
console.log(greet(path.basename("/tmp/echojs")));
EOF
$ ejs -o shout main.js
$ ./shout
ECHOJS
```

`@node-compat/*` modules (path, fs, process, ...) are a small
node-flavored standard library that ships with the toolchain.  On
macOS the final link prints `ld: warning: ... built for newer 'macOS'
version` — harmless, being fixed.

Language support, honestly
--------------------------

The ES2015 core is solid: classes, generators, iterators,
destructuring, template strings, arrow functions, modules,
spread/rest, Map/Set/WeakMap, symbols, typed arrays, Proxy/Reflect,
Promises.  It is exercised by a 400+-program suite whose expected
output comes from node, and by the compiler compiling itself (~50k
lines of tsc-generated JS).

JavaScript did not stand still, and the post-2015 catch-up is planned
but not landed ([docs/language-plan.md](docs/language-plan.md) is the
tracker).  Notably **missing** today: `async`/`await`, optional
chaining (`?.`), nullish coalescing (`??`), exponentiation (`**`),
class fields, object spread/rest, BigInt, and newer stdlib
(`padStart`, `flat`, `Object.entries`, `globalThis`).  A few known
divergences from node are deliberate pins (Annex B block-function
hoisting, `toLocaleString` ICU rounding, `Date.prototype` being a
Date instance).  test262 adoption is on the roadmap.

Flags and knobs
---------------

The surface most users touch (`ejs --help` has the rest):

- `-o FILE` — output executable name.
- `--script` — script-goal semantics: sloppy toplevel (strict only
  under a `"use strict"` directive) and toplevel `this` bound to
  `globalThis`.  The default is the ECMAScript Module goal: every
  file's toplevel is strict and `this` is `undefined`.
- `-O0`..`-O3` — optimizer suites, clang-style: `-O0` straight
  lowering, `-O1` the cheap always-sound tier, `-O2` (default) the
  full EIR pipeline, `-O3` = `-O2` with LLVM O3.  Individual passes
  toggle with `-f[no-]<pass>`; `--print-passes` shows the effective
  configuration.
- `--moduledir DIR` — additional native-module search path.
- `-g` / `--leave-temp` / `--dump-after eir-opt` — debugging the
  compile itself.
- `--record-types` / `--types` — the experimental type-feedback path
  (MAAM analysis-guided specialization).  The MAAM abstract interpreter
  is compiled into the compiler itself, so this works the same in the
  self-hosted binary as under node; `--types-dump` prints the inferred
  per-binding types.

Environment:

- `LLVM_BINDIR` — point the compiler at a specific LLVM `bin/`
  directory (it still verifies the major version; empty string means
  "just use PATH").
- `CXX` — the linker driver (defaults to `clang++` from PATH).
- `EJS_GC_*` — GC knobs for the *compiled program*, e.g.
  `EJS_GC_PROFILE=1` (collection stats on exit),
  `EJS_GC_NURSERY_BUDGET=bytes` (minor-collection trigger, default
  1MB), `EJS_GC_NURSERY=off` / `EJS_GC_COMPACT=off` (A/B the
  generational nursery / major compaction), `EJS_GC_GROWTH=percent`
  (full-collection trigger as % of footprint).  `GC.heapSize()` is
  callable from JS.

Building from source
--------------------

The build uses [buck2](https://buck2.build) and bootstraps on macOS
arm64 and Linux arm64/x86_64 (CI builds all three on every push):

```sh
$ brew install node llvm        # linux: apt llvm-22 + build-essential + libuv1-dev libunwind-dev
$ npm install
$ git submodule update --init
$ ./node-llvm/build-addon.sh    # the node addon the stage0 compiler drives llvm through
$ buck2 build //:ejs.exe        # stage1: node-hosted stage0 compiles the compiler
$ buck2 build //:test-stage3    # full bootstrap + suite: stage1 -> stage2 -> stage3 byte-identical
```

Useful targets: `//:ejs.exe.stage{1,2,3}` (bootstrap stages),
`//:test-stage{0,1,2,3}` (suite against each stage), `//:test-eir`
(compiler unit tests), `//:dist` (the release tarball),
`//:test-dist` (installed-layout smoke test).  If your LLVM lives
somewhere unusual, set `[llvm] prefix` in `.buckconfig` (macOS) or
pass `--config llvm.prefix=/usr/lib/llvm-22` (Linux).

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

4. Environments where a JIT isn't an option (iOS being the original
motivation) leave interpreters as the only JS competition.  Ahead-of-
time native code with type-feedback specialization should beat the
interpreters comfortably and chase the JITs.

The name?
---------

[Echo and Narcissus](https://en.wikipedia.org/wiki/Echo_and_Narcissus)

Big thanks
----------

Echo wouldn't be as far along as it is now (and certainly wouldn't be
as fun to work on) if not for the following:

1. Esprima:   [ariya/esprima](https://github.com/ariya/esprima)
2. Escodegen: [Constellation/escodegen](https://github.com/Constellation/escodegen)
3. LLVM:      http://llvm.org/git/llvm.git
4. Narcissus: [mozilla/narcissus](https://github.com/mozilla/narcissus)
