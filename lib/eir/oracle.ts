/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The MAAM type oracle.  With --types on, compile()
// hands us the desugared toplevel BEFORE collectEIRToplevel consumes it;
// we wrap its body as a Program (preserving node identity — the oracle
// is keyed on the exact node objects), run the echojs-maam abstract
// interpreter over it, log its stats, and return a TypeOracle
// over the result.  Lowering consumes it only under --types;
// --types-dump prints per-binding types for hand-checking.
//
// maam arrives through the static `$maam` import, so it is part of the
// compiler in BOTH hosts: the self-hosted compiler compiles maam's ESM
// build in (gather-imports follows the import), the node-hosted stage0
// require()s the CJS build (buck-gen-js.sh rewrites the specifier).
// Two rules, both load-bearing:
//   - the ANALYSIS runs only under --types — flag-off compiles never
//     invoke maam (module load is the only cost they pay);
//   - every analysis failure degrades to a compiler warning.  --types
//     must never turn a compiling program into a failing one.

import * as maam_namespace from "$maam";
import type * as e from "../estree";
import { reportWarning } from "../errors";
import * as commonIds from "../common-ids";

// The slice of maam's AnalysisResult the probe consumes, typed
// structurally so we never import (or resolve types from) the
// submodule itself.
interface MaamMetrics {
    reachedStates: number;
    configs: number;
    iterations: number;
    shapesInterned: number;
    unknownCalls: number;
    // added alongside this integration; absent in older builds
    degradedBindings?: number;
    stateCapHits?: number;
    stateCapFuncs?: number;
    shapeCapHits?: number;
}

// the slice of maam's Shape the shape queries consume (structural)
interface MaamShape {
    id: number;
    fields: ReadonlyArray<{ name: string; type: string }>;
    megamorphic?: boolean;
}

interface MaamResult {
    metrics: MaamMetrics;
    describe(): string;
    warnings(): Array<{ kind: string }>;
    // node-identity oracle: joined TypeSig ("num", "num|str", "⊤", …)
    // for the exact node object, undefined for unreached/unmapped nodes.
    typeOfNode(n: unknown): string | undefined;
    // node-identity shape queries; absent in older maam
    // builds (the oracle degrades to "no shape facts", never errors)
    receiverShapesOfNode?(n: unknown): MaamShape[] | undefined;
    fieldOrderOfShape?(s: MaamShape): readonly string[] | undefined;
}

interface MaamModule {
    analyze(program: unknown, spec: unknown): MaamResult;
    kCFA(...args: unknown[]): unknown;
}

// the loose "$maam" ambient surface (lib/maam.d.ts), narrowed to the
// structural slice above
const maam: MaamModule = maam_namespace as unknown as MaamModule;

function errorMessage(err: unknown): string {
    if (err instanceof Error) return `${err.name}: ${err.message}`;
    return String(err);
}

function warningSummary(warnings: Array<{ kind: string }>): string {
    if (warnings.length === 0) return "none";
    const counts = new Map<string, number>();
    for (const w of warnings) counts.set(w.kind, (counts.get(w.kind) || 0) + 1);
    return [...counts.entries()].map(([kind, n]) => `${kind}:${n}`).join(",");
}

// --- the TypeOracle contract --

export type TypeTag = "number" | "string" | "boolean" | "undefined" | "null" | "object" | "closure";

export interface EirType {
    // "top" = no information
    tags: ReadonlySet<TypeTag> | "top";
}

// one field of a receiver's shape, in insertion order.
// repr mirrors the runtime's EJSShapeRepr: "f64" iff the field's TypeSig is
// exactly "num" (the runtime classifies stored values the same way), else
// "boxed" — and a sig whose union straddles the num/non-num line has no
// determined repr, so the whole query declines (guard identity needs every
// field's repr, not just the accessed one).
export interface OracleShapeField {
    name: string;
    repr: "boxed" | "f64";
}

export type ShapeDeclineReason =
    | "unmapped" // node unknown to the analysis (or maam predates the query)
    | "polymorphic" // more terminal shapes than the guard budget (>2)
    | "megamorphic" // the ⊤ shape
    | "capped" // shapeCapHits > 0: some shape set was widened this module
    | "union-repr" // a field's TypeSig straddles num/non-num
    | "no-order" // no ordered witness for the shape
    | "empty"; // the empty shape (nothing to access)

// a query answer carries ONE OR TWO exact shapes.  Two
// shapes is the measured 2-way polymorphic extension — every shape in the
// answer independently passes the full exactness screen (non-megamorphic,
// non-empty, ordered witness, single-tag reprs); a set where ANY member
// falls short declines the whole site (criterion 2 — no near-misses),
// and >2 declines "polymorphic" as before.
export type ShapeQuery =
    | { shapes: OracleShapeField[][]; declined?: undefined }
    | { declined: ShapeDeclineReason; shapes?: undefined };

export interface TypeOracle {
    // type of the value an expression node evaluates to (join over all
    // reached contexts); "top" when unknown/unanalyzed
    typeOfNode(n: e.Node): EirType;
    // the receiver-shape facts for a property
    // access's object node — exact facts only (non-megamorphic, uncapped,
    // all reprs single-tag, ordered witness present), at most two shapes
    // (the polymorphic-chain budget), everything else a counted decline.
    // Optional so stub oracles predating shapes keep working; absent =
    // no shape facts.
    receiverShapeOfNode?(n: e.Node): ShapeQuery;
    // required before any UNguarded consumption (guarded fast paths don't
    // need it)
    closedWorld(): boolean;
    describe(): string; // stats line for --types logging
}

// what the probe actually returns: the oracle plus query telemetry.  The
// `unknown` counter is the node-identity canary — a query for a node maam
// never saw (dead code, unmapped glue, or a node MINTED AFTER the probe,
// e.g. by normalizeDefaultExports' splicing) reads as "top"; if identity
// ever silently breaks at scale, this number says so.
export interface ProbeOracleStats {
    queries: number;
    unknown: number;
}

export interface ProbeOracle extends TypeOracle {
    readonly stats: ProbeOracleStats;
}

const TAG_BY_SIG: Record<string, TypeTag> = {
    num: "number",
    str: "string",
    bool: "boolean",
    undefined: "undefined",
    null: "null",
    obj: "object",
    fn: "closure",
};

// Map a maam TypeSig ("num", "num|str", "⊤", "never", …) to an EirType.
// Anything we do not positively recognize — including a missing sig and any
// unrecognized constituent a future maam might add — is "top": the oracle
// never guesses.  Exported for the unit tests in lib/eir/tests.ts.
export function typeSigToEirType(sig: string | undefined): EirType {
    if (sig === undefined || sig === "⊤" || sig === "never") return { tags: "top" };
    const tags = new Set<TypeTag>();
    for (const part of sig.split("|")) {
        const tag = TAG_BY_SIG[part];
        if (tag === undefined) return { tags: "top" };
        tags.add(tag);
    }
    return { tags };
}

// Map a maam field TypeSig to a runtime shape repr, or null when the sig
// straddles the num/non-num line (no single runtime repr exists — the
// object flips shapes at runtime and no one guard can be monomorphic).
// The runtime's classify_repr is EJSVAL_IS_NUMBER ? F64 : BOXED, so any
// union of non-num tags is uniformly BOXED.  Exported for unit tests.
export function typeSigToShapeRepr(sig: string): "boxed" | "f64" | null {
    if (sig === "num") return "f64";
    const parts = sig.split("|");
    for (const part of parts) {
        if (part === "num" || TAG_BY_SIG[part] === undefined) return null;
    }
    return "boxed";
}

// The common-ids singleton identifier nodes (ONE object each, spliced into
// many sites by the desugar passes).  Node-identity oracle queries on them
// would be ambiguous; the dump skips them outright (maam's ambiguity poison
// backstops any that slip through elsewhere).
let cached_singletons: Set<unknown> | null = null;
function singletonIdentifiers(): Set<unknown> {
    if (!cached_singletons) {
        cached_singletons = new Set(
            Object.values(commonIds).filter(
                (v) => typeof v === "object" && v !== null && (v as { type?: string }).type === "Identifier"
            )
        );
    }
    return cached_singletons;
}

// Collect the DECLARATION-site binding identifiers of the wrapped program:
// variable-declarator ids (incl. pattern leaves), function-declaration names,
// and parameters (identifiers, pattern leaves, rest — both the RestElement
// and old-dialect `rest`-field forms).  These are minted per-site by the
// parser/desugars, so they are safe node-identity keys — except the
// singletons and %-named internals, which are skipped, and any node object
// encountered twice, which is a splice and skipped too.
function collectDeclarationIds(program: { body: e.Statement[] }): e.Identifier[] {
    const out: e.Identifier[] = [];
    const patternLeaves = (p: unknown): void => {
        if (!p || typeof p !== "object") return;
        const node = p as { type?: string };
        switch (node.type) {
            case "Identifier":
                out.push(node as e.Identifier);
                return;
            case "ObjectPattern":
                for (const prop of (node as unknown as { properties: unknown[] }).properties) {
                    const pr = prop as { type?: string; value?: unknown; argument?: unknown };
                    if (pr.type === "Property") patternLeaves(pr.value);
                    else patternLeaves(pr.argument);
                }
                return;
            case "ArrayPattern":
                for (const el of (node as unknown as { elements: unknown[] }).elements) patternLeaves(el);
                return;
            case "AssignmentPattern":
                return patternLeaves((node as unknown as { left: unknown }).left);
            case "RestElement":
            case "SpreadElement":
                return patternLeaves((node as unknown as { argument: unknown }).argument);
            default:
                return;
        }
    };
    const walk = (n: unknown): void => {
        if (!n || typeof n !== "object") return;
        if (Array.isArray(n)) {
            for (const x of n) walk(x);
            return;
        }
        const node = n as Record<string, unknown> & { type?: string };
        if (typeof node.type === "string") {
            if (node.type === "VariableDeclarator") patternLeaves(node.id);
            else if (node.type === "FunctionDeclaration" && node.id) patternLeaves(node.id);
            if (
                node.type === "FunctionDeclaration" ||
                node.type === "FunctionExpression" ||
                node.type === "ArrowFunctionExpression"
            ) {
                for (const p of (node.params as unknown[]) ?? []) patternLeaves(p);
                if (node.rest) patternLeaves(node.rest); // old-dialect rest field
            }
        }
        for (const key of Object.keys(node)) {
            if (key === "loc" || key === "range") continue;
            walk(node[key]);
        }
    };
    walk(program);

    const singletons = singletonIdentifiers();
    const seen = new Set<e.Identifier>();
    const dupes = new Set<e.Identifier>();
    for (const ident of out) {
        if (seen.has(ident)) dupes.add(ident); // spliced node: ambiguous key
        seen.add(ident);
    }
    return out.filter(
        (ident) => !dupes.has(ident) && !singletons.has(ident) && !ident.name.startsWith("%")
    );
}

function locOf(ident: e.Identifier): { line: number; col: number } | null {
    const loc = (ident as { loc?: { start?: { line: number; column: number } } }).loc;
    if (loc && loc.start) return { line: loc.start.line, col: loc.start.column + 1 };
    return null;
}

// Print one line per declaration-site binding: name, source position when the
// node has one (synthetic desugar nodes may not), and the raw maam TypeSig
// ("(unmapped→⊤)" for nodes the analysis never saw).  Sorted by position,
// synthetic nodes last, ties broken by name then collection order.
function dumpBindingTypes(
    result: MaamResult,
    program: { body: e.Statement[] },
    source_filename: string
): void {
    const ids = collectDeclarationIds(program);
    const rows = ids.map((ident, index) => ({ ident, index, loc: locOf(ident) }));
    // code-point name ordering, NOT localeCompare: the dump must be
    // byte-identical across hosts (node's ICU vs the self-hosted
    // runtime's collation)
    const byName = (a: string, b: string): number => (a < b ? -1 : a > b ? 1 : 0);
    rows.sort((a, b) => {
        if (a.loc && b.loc)
            return a.loc.line - b.loc.line || a.loc.col - b.loc.col ||
                byName(a.ident.name, b.ident.name) || a.index - b.index;
        if (a.loc) return -1;
        if (b.loc) return 1;
        return byName(a.ident.name, b.ident.name) || a.index - b.index;
    });
    for (const row of rows) {
        const sig = result.typeOfNode(row.ident);
        const where = row.loc ? `${row.loc.line}:${row.loc.col}` : "synthetic";
        console.warn(
            `--types-dump: ${source_filename}: ${row.ident.name} @${where} : ${sig ?? "(unmapped→⊤)"}`
        );
    }
}

// Run the probe over the module's desugared tree.  `tree` is the
// post-pre_eir_convert Program whose body[0] is the synthetic toplevel
// FunctionDeclaration (insert_toplevel_func) holding the module's
// statements.  Returns a TypeOracle over the analysis (so compile() can
// thread it onward), or null when anything degraded; callers
// must treat null as "no type information", never as an error.
export function runTypeAnalysisProbe(
    tree: e.Program,
    source_filename: string,
    dump = false
): ProbeOracle | null {
    const toplevel = tree.body[0];
    if (!toplevel || toplevel.type !== "FunctionDeclaration") {
        reportWarning(
            "--types: expected the synthetic toplevel FunctionDeclaration; type analysis skipped.",
            source_filename
        );
        return null;
    }

    // Same body array, same node objects — no cloning.
    const program = { type: "Program", sourceType: "script", body: toplevel.body.body };

    const started = Date.now();
    try {
        const result = maam.analyze(
            program,
            maam.kCFA(1, "flow-sensitive", "call-site", /*shapeCap*/ 64, false, false, false, /*stateCap*/ 512)
        );
        const wall = Date.now() - started;
        const m = result.metrics;
        const statsLine =
            `--types: ${source_filename}: wall=${wall}ms reachedStates=${m.reachedStates} ` +
            `configs=${m.configs} iterations=${m.iterations} shapesInterned=${m.shapesInterned} ` +
            `unknownCalls=${m.unknownCalls} degradedBindings=${m.degradedBindings ?? 0} ` +
            `stateCapHits=${m.stateCapHits ?? 0} stateCapFuncs=${m.stateCapFuncs ?? 0} ` +
            `shapeCapHits=${m.shapeCapHits ?? 0} warnings=${warningSummary(result.warnings())}`;
        console.warn(statsLine);
        console.warn(result.describe());
        if (dump) dumpBindingTypes(result, program as { body: e.Statement[] }, source_filename);

        const stats: ProbeOracleStats = { queries: 0, unknown: 0 };
        return {
            stats,
            typeOfNode: (n) => {
                stats.queries++;
                const sig = result.typeOfNode(n);
                if (sig === undefined) stats.unknown++;
                return typeSigToEirType(sig);
            },
            // exact receiver-shape facts, every
            // near-miss a counted decline (promotion criterion 2 — no
            // near-misses).  Up to TWO shapes survive (the poly
            // budget); each must pass the full screen independently.
            receiverShapeOfNode: (n): ShapeQuery => {
                if (!result.receiverShapesOfNode || !result.fieldOrderOfShape)
                    return { declined: "unmapped" }; // older maam build
                if ((m.shapeCapHits ?? 0) > 0) return { declined: "capped" };
                const shapes = result.receiverShapesOfNode(n);
                if (shapes === undefined || shapes.length === 0)
                    return { declined: "unmapped" };
                if (shapes.length > 2) return { declined: "polymorphic" };
                const out: OracleShapeField[][] = [];
                for (const s of shapes) {
                    if (s.megamorphic) return { declined: "megamorphic" };
                    if (s.fields.length === 0) return { declined: "empty" };
                    const order = result.fieldOrderOfShape(s);
                    if (!order || order.length !== s.fields.length)
                        return { declined: "no-order" };
                    const typeByName = new Map(s.fields.map((f) => [f.name, f.type]));
                    const fields: OracleShapeField[] = [];
                    for (const name of order) {
                        const sig = typeByName.get(name);
                        if (sig === undefined) return { declined: "no-order" };
                        const repr = typeSigToShapeRepr(sig);
                        if (repr === null) return { declined: "union-repr" };
                        fields.push({ name, repr });
                    }
                    out.push(fields);
                }
                return { shapes: out };
            },
            // The plan text gates closedWorld() on unknownCalls alone because it
            // predates the degradedBindings counter (unmodeled imports, rest
            // params — Chunks A/D).  Both must be zero: either one means some
            // value in the store is a stand-in, not a fact.
            closedWorld: () => m.unknownCalls === 0 && (m.degradedBindings ?? 0) === 0,
            describe: () => statsLine,
        };
    } catch (err) {
        const wall = Date.now() - started;
        reportWarning(
            `--types: analysis failed after ${wall}ms (${errorMessage(err)}); continuing without type information.`,
            source_filename
        );
        return null;
    }
}
