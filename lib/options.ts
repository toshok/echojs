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
    // script-goal semantics: sloppy toplevel (strict only under a
    // "use strict" directive) and toplevel `this` = globalThis.  The
    // default is the Module goal: every toplevel is strict and `this`
    // is undefined.  Parsing uses the module grammar either way.
    script: boolean;
    // -W<name> warnings the driver enables (today: "unused-exports")
    warnings: Set<string>;
    // the -Wunused-exports collector, driver-owned: every export slot
    // defined and every use — residual slot loads AND compile-time
    // constant folds (a folded const is used even though no load
    // survives; the warning must not lie about it)
    export_census?: {
        have: Map<string, { module: string; name: string; promoted: boolean }>;
        used: Set<string>;
    } | null;
    // --ic-profile <file>: a training run's ICPROF/ICPROFP dump (see
    // -fic-profile-dump).  The driver parses it once into
    // ic_profile_map: site-id -> unique {key, slot} (proto-tier records
    // carry the immediate proto's key too), sites with conflicting
    // records dropped; lowering inlines the guarded fast path at listed
    // sites (checked tier — a stale profile is a guard miss, never a
    // wrong answer).
    ic_profile: string | null;
    ic_profile_map?: Map<
        string,
        { key: string; slot: number; evals: number; protoKey?: string }
    > | null;
}
