/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Class birth-shape evidence, desugar-classes -> lower.ts, keyed by the
// method's function node.  A side table rather than expando properties
// on the AST nodes: marker stores would transition (or dict-convert)
// every marked node's shape, which is hostile to the shape machinery
// when the compiler compiles itself.  Evidence flows within a single
// module's compile (written during desugar, read during lowering), so
// pre_eir_convert clears the table at each module boundary and nothing
// outlives its module.

import type * as e from "./estree";

export interface ClassShapeEvidence {
    // base-class constructor whose leading `this.x = v` run defines the
    // shape (lower.ts classBirthShape re-derives under the ctor-fill
    // batching rules)
    ctorFn?: e.Function;
    // field-declaring classes: the declared field list in order...
    fieldNames?: string[];
    // ...and the parallel initializer expressions (null = no
    // initializer -> defines undefined), which set each field's birth
    // repr (numeric initializer = f64)
    fieldInits?: (e.Expression | null)[];
}

const evidence = new Map<object, ClassShapeEvidence>();

export function setClassShapeEvidence(fnNode: object, ev: ClassShapeEvidence): void {
    evidence.set(fnNode, ev);
}

export function getClassShapeEvidence(fnNode: object): ClassShapeEvidence | undefined {
    return evidence.get(fnNode);
}

// desugar rebuilds method fn nodes (create_proto_method); the evidence
// travels to the rebuilt node
export function copyClassShapeEvidence(fromNode: object, toNode: object): void {
    const ev = evidence.get(fromNode);
    if (ev !== undefined) evidence.set(toNode, ev);
}

export function clearClassShapeEvidence(): void {
    evidence.clear();
}
