/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Pass configuration: the clang-style -O/-f surface.
//
// One registry table maps each canonical pass name to its PassConfig
// field, its default at each -O level, and its help text; the driver's
// --help and --print-passes listings are generated from it so they
// can't drift.  Passes read the resolved snapshot via passes() — never
// process.env, which under the self-hosted runtime is a
// rebuild-the-whole-environment getter (the SinkFlags lesson in
// optimize.ts, now the rule for every pass).
//
// Resolution order (gcc semantics): the -O suite's defaults, then
// -f<pass>/-fno-<pass> overrides in command-line order, last-wins.
// EJS_FLAGS in the environment is tokenized and applied after the real
// argv by the driver — the single debugging escape for harnesses that
// don't thread driver flags.
//
// Suites: -O0 is straight lowering (no EIR optimizer; the lowering and
// emission behaviors that were never opt_level-gated stay on — see the
// per-pass levels below); -O1 is the cheap always-sound intra-function
// tier; -O2 (the default) adds the module-level tier and is exactly the
// pre-P5 default pipeline; -O3 is -O2 on the EIR side (the LLVM
// pipeline still runs default<O3> — -fllvm-opt=<n> decouples it).

export interface PassConfig {
    // the EIR optimizer (integrate.ts drives, optimize.ts runs)
    eirOpt: boolean;
    eirCleanup: boolean;
    slotCse: boolean;
    shapedSink: boolean;
    argsSink: boolean;
    flowSink: boolean;
    // the module-level tier
    devirt: boolean;
    eirSpec: boolean;
    exportWrapper: boolean;
    ctorSink: boolean;
    shapeFusion: boolean;
    // lowering-time behaviors (oracle-gated where applicable)
    shapeGuards: boolean;
    polyShapeGuards: boolean;
    bornShaped: boolean;
    promote: boolean;
    // -fno-promote=<substr,...>: decline promotion only for module paths
    // containing one of the substrings (the old EJS_NO_PROMOTE list)
    promoteExclude: string[];
    // emission controls (emit.ts)
    gcFrames: boolean;
    inlineAlloc: boolean;
    inlineEnvSlots: boolean;
    // opt-in probes
    lowtier: boolean;
    // LLVM pipeline level escape hatch: null = follow the -O level
    llvmOpt: number | null;
}

// which boolean field a pass name controls (promoteExclude and llvmOpt
// are the two valued knobs, handled specially in applyFlag)
type BoolField = {
    [K in keyof PassConfig]: PassConfig[K] extends boolean ? K : never;
}[keyof PassConfig];

export interface PassDesc {
    name: string; // canonical -f/-fno- spelling
    field: BoolField;
    // lowest -O level the pass defaults on at (0 = always, including
    // -O0; OPT_IN = never — only an explicit -f enables it)
    minLevel: number;
    help: string;
}

const OPT_IN = 99;

export const PASSES: readonly PassDesc[] = [
    {
        name: "eir-opt",
        field: "eirOpt",
        minLevel: 1,
        help: "the EIR optimizer as a whole; off = straight lowering to LLVM",
    },
    {
        name: "eir-cleanup",
        field: "eirCleanup",
        minLevel: 1,
        help: "constant folding, trivial-param pruning, typeof/boolean rewrites, lattice-typed f64 lowering",
    },
    {
        name: "slot-cse",
        field: "slotCse",
        minLevel: 1,
        help: "module-slot load CSE over stable %self slots",
    },
    {
        name: "shaped-sink",
        field: "shapedSink",
        minLevel: 1,
        help: "scalar replacement of non-escaping shaped literals",
    },
    {
        name: "args-sink",
        field: "argsSink",
        minLevel: 1,
        help: "rest/arguments objects used only for .length fold to arg_len",
    },
    {
        name: "flow-sink",
        field: "flowSink",
        minLevel: 1,
        help: "flow-sensitive sinking of written/partially-escaping literals",
    },
    {
        name: "devirt",
        field: "devirt",
        minLevel: 2,
        help: "direct-call devirtualization of module-local closures",
    },
    {
        name: "eir-spec",
        field: "eirSpec",
        minLevel: 2,
        help: "oracle-driven function specialization (needs --types)",
    },
    {
        name: "export-wrapper",
        field: "exportWrapper",
        minLevel: 2,
        help: "guarded entry wrappers so escaping functions keep specialized clones (needs --types)",
    },
    {
        name: "ctor-sink",
        field: "ctorSink",
        minLevel: 2,
        help: "epoch-guarded constructor-result sinking (needs --types)",
    },
    {
        name: "shape-fusion",
        field: "shapeFusion",
        minLevel: 2,
        help: "heterogeneous shape+numeric region merging and in-loop numeric folding",
    },
    {
        name: "shape-guards",
        field: "shapeGuards",
        minLevel: 0,
        help: "has_shape guard diamonds on oracle-known receivers (needs --types)",
    },
    {
        name: "poly-shape-guards",
        field: "polyShapeGuards",
        minLevel: 0,
        help: "2-way polymorphic shape-guard chains (needs --types)",
    },
    {
        name: "born-shaped",
        field: "bornShaped",
        minLevel: 0,
        help: "object literals and constructor prefixes allocate at their birth shape",
    },
    {
        name: "promote",
        field: "promote",
        minLevel: 0,
        help: "promote non-exported module-level vars to hidden module slots; -fno-promote=<substr,...> declines only matching module paths",
    },
    {
        name: "gc-frames",
        field: "gcFrames",
        minLevel: 0,
        help: "precise GC frames for values live across safepoints",
    },
    {
        name: "inline-alloc",
        field: "inlineAlloc",
        minLevel: 0,
        help: "inline bump allocation for environments",
    },
    {
        name: "inline-env-slots",
        field: "inlineEnvSlots",
        minLevel: 0,
        help: "inline env slot addressing instead of runtime accessor calls",
    },
    {
        name: "lowtier",
        field: "lowtier",
        minLevel: OPT_IN,
        help: "swap the lowtier_* probe function bodies for hand-built low-tier EIR (test hook)",
    },
];

const byName = new Map<string, PassDesc>(PASSES.map((p) => [p.name, p]));

export function defaultPassConfig(optLevel: number): PassConfig {
    const cfg = {
        promoteExclude: [],
        llvmOpt: null,
    } as unknown as PassConfig;
    for (const p of PASSES) cfg[p.field] = optLevel >= p.minLevel;
    return cfg;
}

// apply one -f/-fno- token.  returns an error message, or null on
// success.  `prov` (when given) records the token as each touched
// setting's provenance, for --print-passes.
export function applyPassFlag(
    cfg: PassConfig,
    token: string,
    prov?: Map<string, string>
): string | null {
    if (token.indexOf("-f") !== 0) return `not a pass flag: ${token}`;
    let body = token.substring(2);
    let enable = true;
    if (body.indexOf("no-") === 0) {
        enable = false;
        body = body.substring(3);
    }
    let value: string | null = null;
    const eq = body.indexOf("=");
    if (eq !== -1) {
        value = body.substring(eq + 1);
        body = body.substring(0, eq);
    }

    // the LLVM-side escape hatch is a valued knob, not a registry pass
    if (body === "llvm-opt") {
        if (!enable) {
            if (value !== null) return `-fno-llvm-opt does not take a value`;
            cfg.llvmOpt = 0;
        } else {
            const n = value === null ? NaN : parseInt(value, 10);
            if (!(n >= 0 && n <= 3)) return `-fllvm-opt wants =<0..3>, got '${token}'`;
            cfg.llvmOpt = n;
        }
        if (prov) prov.set("llvm-opt", token);
        return null;
    }

    const desc = byName.get(body);
    if (!desc) {
        return `unknown pass '${body}' in ${token} (see --print-passes for the list)`;
    }
    if (value !== null) {
        // -fno-promote=<substr,...> is the one valued spelling: decline
        // promotion only for matching module paths
        if (desc.name !== "promote" || enable)
            return `pass '${desc.name}' does not take a value: ${token}`;
        cfg.promote = true;
        cfg.promoteExclude = value.split(",").filter((s) => s.length > 0);
    } else {
        cfg[desc.field] = enable;
        if (desc.name === "promote") cfg.promoteExclude = [];
    }
    if (prov) prov.set(desc.name, token);
    return null;
}

export interface ResolvedPasses {
    config: PassConfig;
    // canonical name -> what decided it ("-O2 suite" or the flag token)
    provenance: Map<string, string>;
    errors: string[];
}

export function resolvePassConfig(optLevel: number, flagTokens: string[]): ResolvedPasses {
    const config = defaultPassConfig(optLevel);
    const provenance = new Map<string, string>();
    for (const p of PASSES) provenance.set(p.name, `-O${optLevel} suite`);
    provenance.set("llvm-opt", `-O${optLevel} suite`);
    const errors: string[] = [];
    for (const token of flagTokens) {
        const err = applyPassFlag(config, token, provenance);
        if (err) errors.push(err);
    }
    return { config, provenance, errors };
}

// --- the per-run snapshot ---------------------------------------------------

// the driver resolves once at startup and installs; library callers and
// the unit tests get today's default pipeline (-O2) unless they say
// otherwise.  Snapshot semantics: mutate only through set/with below.
let current: PassConfig = defaultPassConfig(2);

export function passes(): PassConfig {
    return current;
}

export function setPassConfig(cfg: PassConfig): void {
    current = cfg;
}

// tests: run f with named settings overridden, restoring on the way out
export function withPassConfig<T>(overrides: Partial<PassConfig>, f: () => T): T {
    const prev = current;
    current = { ...prev, ...overrides };
    try {
        return f();
    } finally {
        current = prev;
    }
}

// --- generated listings -----------------------------------------------------

// the --help section: one line per pass, from the registry
export function formatPassHelp(): string {
    const lines: string[] = [];
    lines.push("Pass flags (-f<pass> enables, -fno-<pass> disables; applied after the -O suite,");
    lines.push("last one wins).  Defaults: [0] on at every level incl. -O0, [1] on at -O1+,");
    lines.push("[2] on at -O2+, [-] off unless enabled explicitly:");
    for (const p of PASSES) {
        const lvl = p.minLevel === OPT_IN ? "-" : String(p.minLevel);
        lines.push(`   -f[no-]${p.name}  [${lvl}]  ${p.help}`);
    }
    lines.push(
        "   -fllvm-opt=<0..3>  [=]  run the LLVM pipeline at this level instead of the -O level"
    );
    return lines.join("\n");
}

// the --print-passes listing: the effective configuration and where
// each setting came from
export function formatEffectiveConfig(r: ResolvedPasses, optLevel: number): string {
    const lines: string[] = [];
    lines.push(`effective pass configuration at -O${optLevel}:`);
    for (const p of PASSES) {
        const on = r.config[p.field];
        let state = on ? "on " : "off";
        if (p.name === "promote" && on && r.config.promoteExclude.length > 0)
            state = `on (except ${r.config.promoteExclude.join(",")})`;
        lines.push(
            `   ${p.name.padEnd(18)} ${state.padEnd(6)} (${r.provenance.get(p.name) || "?"})`
        );
    }
    const llvm = r.config.llvmOpt === null ? `O${optLevel}` : `O${r.config.llvmOpt}`;
    lines.push(
        `   ${"llvm-opt".padEnd(18)} ${llvm.padEnd(6)} (${r.provenance.get("llvm-opt") || "?"})`
    );
    return lines.join("\n");
}

