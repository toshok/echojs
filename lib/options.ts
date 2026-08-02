/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The driver's options object (defaults live in ejs-es6.ts).  The passes
// and the compiler consume slices of this; it is threaded everywhere.

export interface ImportVariable {
    variable: string;
    value: string;
}

export interface OutputWriter {
    write(msg: string, want_newline?: boolean): void;
}

export interface CompilerOptions {
    opt_level: number;
    debug: boolean;
    debug_level: number;
    debug_passes: Set<string>;
    warn_on_undeclared: boolean;
    frozen_global: boolean;
    record_types: boolean;
    // MAAM type-analysis probe: run the analysis and
    // log stats; codegen consumes nothing yet.  Distinct from record_types
    // (the runtime type-recording instrumentation).
    types: boolean;
    // --types plus a per-binding type dump (implies types).
    types_dump: boolean;
    output_filename: string | null;
    show_help: boolean;
    leave_temp_files: boolean;
    native_module_dirs: string[];
    extra_clang_args: string;
    ios_sdk: string;
    ios_min: string;
    osx_min: string;
    import_variables: ImportVariable[];
    srcdir: boolean;
    stdout_writer: OutputWriter;
    quiet?: boolean;
    // "acorn" (default) or "esprima" (the retained fork, kept for
    // bisection during the parser transition)
    parser: string;
    // script-goal semantics: sloppy toplevel (strict only under a
    // "use strict" directive) and toplevel `this` = globalThis.  The
    // default is the Module goal: every toplevel is strict and `this`
    // is undefined.  Parsing uses the module grammar either way.
    script: boolean;
}
