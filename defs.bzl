# Shared definitions for the EchoJS buck2 build.

def llvm_prefix():
    return read_config("llvm", "prefix", "/opt/homebrew/opt/llvm@16")

def llvm_suffix():
    return read_config("llvm", "suffix", "")

def llvm_bindir():
    return llvm_prefix() + "/bin"

def llvm_bin(tool):
    return "{}/{}{}".format(llvm_bindir(), tool, llvm_suffix())

# Triple as lib/triple.js Triple.toString() renders the host triple
# (arch-vendor-os).  Used for the runtime/out/<triple> directory the
# compiler looks in when running with --srcdir.
EJS_TRIPLE = select({
    "config//os:linux": select({
        "config//cpu:arm64": "arm64-unknown-linux",
        "config//cpu:x86_64": "x86_64-unknown-linux",
    }),
    "config//os:macos": select({
        "config//cpu:arm64": "arm64-apple-macos",
        "config//cpu:x86_64": "x86_64-apple-macos",
    }),
})

# Triple.toShortString() (arch-os), which is what the Makefiles call
# SHORT_TRIPLE and what node-compat.ejs keys its module_file map on.
EJS_SHORT_TRIPLE = select({
    "config//os:linux": select({
        "config//cpu:arm64": "arm64-linux",
        "config//cpu:x86_64": "x86_64-linux",
    }),
    "config//os:macos": select({
        "config//cpu:arm64": "arm64-macos",
        "config//cpu:x86_64": "x86_64-macos",
    }),
})

# Short os name used for the external-deps build directory names
# (pcre-macos, double-conversion-linux, ...).
EJS_OS = select({
    "config//os:linux": "linux",
    "config//os:macos": "macos",
})

# GNU-style triple passed to autoconf --build (old config.guess scripts
# in the pcre submodule don't recognize arm64 macs).
GNU_TRIPLE = select({
    "config//os:linux": select({
        "config//cpu:arm64": "aarch64-unknown-linux-gnu",
        "config//cpu:x86_64": "x86_64-unknown-linux-gnu",
    }),
    "config//os:macos": select({
        "config//cpu:arm64": "aarch64-apple-darwin",
        "config//cpu:x86_64": "x86_64-apple-darwin",
    }),
})

# -mtriple for llc when compiling the .ll runtime sources.
LLC_MTRIPLE = select({
    "config//os:linux": select({
        "config//cpu:arm64": "aarch64-unknown-linux-gnu",
        "config//cpu:x86_64": "x86_64-unknown-linux-gnu",
    }),
    "config//os:macos": select({
        "config//cpu:arm64": "arm64-apple-macosx11.0.0",
        "config//cpu:x86_64": "x86_64-apple-macosx11.0.0",
    }),
})

# The runloop implementation baked into lib/host-config.js.
EJS_RUNLOOP_IMPL = select({
    "config//os:linux": "libuv",
    "config//os:macos": "darwin",
})

# Mirrors CFLAGS + per-target defines from mk/config.mk — except the
# optimization level: the runtime moved -O0 -> -O2 at gc-plan P0 (the
# plan's "single cheapest runtime speedup"; scanner assumptions
# re-verified there — MARK_REGISTERS spills callee-saved registers, the
# ABI pins live-across-call values to stack/callee-saved, and interior
# pointers canonicalize in both the page and (since P0) LOS lookups).
EJS_COMPILER_FLAGS = [
    "-g",
    "-O2",
    "-Wall",
    "-Wno-unused-function",
    "-Wno-unused-variable",
] + select({
    "config//os:linux": [
        "-DTARGET_LINUX=1",
        "-D_GNU_SOURCE",
    ],
    "config//os:macos": [
        "-DOSX=1",
        "-DTARGET_MACOS=1",
        "-D_XOPEN_SOURCE",
        "-Wno-deprecated-declarations",
    ],
}) + select({
    "config//cpu:arm64": [
        # both spellings are in use (ejs-gc.c vs ejs-node-compat.c)
        "-DTARGET_CPU_ARM64=1",
        "-DTARGET_CPU_AARCH64=1",
        "-DEJS_BITS_PER_WORD=64",
        "-DIS_LITTLE_ENDIAN=1",
    ],
    "config//cpu:x86_64": [
        "-DTARGET_CPU_AMD64=1",
        "-DEJS_BITS_PER_WORD=64",
        "-DIS_LITTLE_ENDIAN=1",
    ],
})
