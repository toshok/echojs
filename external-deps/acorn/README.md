# acorn (vendored bundle)

`acorn-es6.js` is [acorn](https://github.com/acornjs/acorn) 8.18.0 —
the upstream flat ES-module bundle (`dist/acorn.mjs`), mechanically
transpiled to ES5-level syntax (see `regen.sh`).  It is the compiler's
parser as of language-P2 (`lib/parser.ts` is the seam; `-es6` in the
filename means "ES module", the repo convention).

Unlike the sibling external-deps this is not a forked submodule: the
file is a generated artifact, reproducible from pinned versions with
`./regen.sh`, and upstream is used as-is.

Why the transpile: stage1+ self-compiles this file, so it must stay
inside the subset the echojs backend supports.  The transpile target
can be raised as language-P3 features land.

License: MIT (see LICENSE, acorn's).
