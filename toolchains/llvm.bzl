
# @unsorted-dict-items
_llvm_toolchain_attrs = {
    # Report unused dependencies
    # "report_unused_deps": False,
    # Rustc target triple to use
    # https://doc.rust-lang.org/rustc/platform-support.html
    # "rustc_target_triple": None,
}

LLVMToolchainInfo = provider(fields = _llvm_toolchain_attrs.keys())

def _system_llvm_toolchain_impl(ctx):
    return [
        DefaultInfo(),
        LLVMToolchainInfo(
        )
    ]

system_llvm_toolchain = rule(
    impl = _system_llvm_toolchain_impl,
    attrs = {
#        "allow_lints": attrs.list(attrs.string(), default = []),
#        "clippy_toml": attrs.option(attrs.dep(providers = [DefaultInfo]), default = None),
#        "default_edition": attrs.option(attrs.string(), default = None),
#        "deny_lints": attrs.list(attrs.string(), default = []),
#        "doctests": attrs.bool(default = False),
#        "extern_html_root_url_prefix": attrs.option(attrs.string(), default = None),
#        "pipelined": attrs.bool(default = False),
#        "report_unused_deps": attrs.bool(default = False),
#        "rustc_binary_flags": attrs.list(attrs.string(), default = []),
#        "rustc_check_flags": attrs.list(attrs.string(), default = []),
#        "rustc_flags": attrs.list(attrs.string(), default = []),
#        "rustc_target_triple": attrs.string(default = _DEFAULT_TRIPLE),
#        "rustc_test_flags": attrs.list(attrs.string(), default = []),
#        "rustdoc_flags": attrs.list(attrs.string(), default = []),
#        "warn_lints": attrs.list(attrs.string(), default = []),
    },
    is_toolchain_rule = True,
)