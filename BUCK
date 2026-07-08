load("//:defs.bzl", "EJS_OS", "EJS_SHORT_TRIPLE", "EJS_TRIPLE", "llvm_bindir")

platform(
    name = "linux-x86_64",
    constraint_values = [
        "@config//os/constraints:linux",
        "@config//cpu/constraints:x86_64",
    ],
)

platform(
    name = "macos-arm64",
    constraint_values = [
        "@config//os/constraints:macos",
        "@config//cpu/constraints:arm64",
    ],
)

export_file(
    name = "ejs-es6.js",
    visibility = ["PUBLIC"],
)

# A directory laid out the way `ejs --srcdir` expects a source checkout to
# look, containing everything needed to self-compile the compiler.
genrule(
    name = "srcdir-tree",
    srcs = ["buck-srcdir-tree.sh"],
    out = "root",
    cmd = "bash $SRCDIR/buck-srcdir-tree.sh" +
          ' "' + EJS_TRIPLE + '"' +
          ' "' + EJS_SHORT_TRIPLE + '"' +
          ' "' + EJS_OS + '"' +
          ' "$(location //runtime:headers)"' +
          ' "$(location //runtime:echo[static])"' +
          ' "$(location //runtime:platform-icc-o)"' +
          ' "$(location //external-deps:pcre-build[lib])"' +
          ' "$(location //external-deps:double-conversion-build)"' +
          ' "$(location //external-deps:compiler-js)"' +
          ' "$(location //lib:es6-srcs)"' +
          ' "$(location //lib:host-config.js)"' +
          ' "$(location :ejs-es6.js)"' +
          ' "$(location //node-compat:node-compat.ejs)"' +
          ' "$(location //node-compat:node-compat[static])"' +
          ' "$(location //ejs-llvm:ejs-llvm.ejs)"' +
          ' "$(location //ejs-llvm:ejs-llvm[static])"' +
          ' "$(location //runtime:echo-dtoa[static])"' +
          select({
              "DEFAULT": " -",
              "config//os:macos": ' "$(location //runtime:echo-objc[static])"',
          }),
)

# stage1: the babel'd compiler running under node (with the node-llvm
# addon) compiles ejs-es6.js to a native executable.
genrule(
    name = "ejs.exe.stage1",
    srcs = ["buck-stage.sh"],
    out = "ejs.exe.stage1",
    cmd = 'bash $SRCDIR/buck-stage.sh "$(location :srcdir-tree)" node ' +
          '"$(location //lib:generated)" "$(location //node-llvm:llvm.node)" ' +
          llvm_bindir(),
)

# stage2: stage1 compiles the compiler.
genrule(
    name = "ejs.exe.stage2",
    srcs = ["buck-stage.sh"],
    out = "ejs.exe.stage2",
    cmd = 'bash $SRCDIR/buck-stage.sh "$(location :srcdir-tree)" exe ' +
          '"$(location :ejs.exe.stage1)" - ' + llvm_bindir(),
)

# stage3: stage2 compiles the compiler; stage2 and stage3 should be
# functionally identical if the bootstrap is healthy.
genrule(
    name = "ejs.exe.stage3",
    srcs = ["buck-stage.sh"],
    out = "ejs.exe.stage3",
    cmd = 'bash $SRCDIR/buck-stage.sh "$(location :srcdir-tree)" exe ' +
          '"$(location :ejs.exe.stage2)" - ' + llvm_bindir(),
)

# `make` (all) builds stage1 and installs it as ejs.exe; mirror that.
alias(
    name = "ejs.exe",
    actual = ":ejs.exe.stage1",
)

# EIR unit tests (run under node against the babel'd tree):
# buck2 build //:test-eir
genrule(
    name = "test-eir",
    out = "test-eir.log",
    cmd = '(node "$(location //lib:generated)/lib/eir/tests.js" > $OUT 2>&1) || ' +
          "{ cat $OUT >&2; exit 1; }; tail -1 $OUT",
)

# run the test suite against a stage: buck2 build //:test-stage3
# the output artifact is the full test log; the build fails if any test
# fails.
genrule(
    name = "test-stage0",
    srcs = ["buck-test-stage.sh"],
    out = "test-stage0.log",
    cmd = 'bash $SRCDIR/buck-test-stage.sh "$(location :srcdir-tree)" ' +
          '"$(location //lib:generated)" - 0 "$(location //test:files)" ' + llvm_bindir(),
)

genrule(
    name = "test-stage0-ir",
    srcs = ["buck-test-stage.sh"],
    out = "test-stage0-ir.log",
    cmd = 'bash $SRCDIR/buck-test-stage.sh "$(location :srcdir-tree)" ' +
          '"$(location //lib:generated)" - 0 "$(location //test:files)" ' + llvm_bindir() +
          " --ir",
)

[
    genrule(
        name = "test-stage" + stage,
        srcs = ["buck-test-stage.sh"],
        out = "test-stage" + stage + ".log",
        cmd = 'bash $SRCDIR/buck-test-stage.sh "$(location :srcdir-tree)" ' +
              '"$(location //lib:generated)" "$(location :ejs.exe.stage' + stage + ')" ' +
              stage + ' "$(location //test:files)" ' + llvm_bindir(),
    )
    for stage in ["1", "2", "3"]
]
