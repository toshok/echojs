// hand-written surface declaration for astring-es6.js, covering only
// what the echo-js compiler uses (lib/debug-codegen.ts: the base
// generator table it extends with the old-esprima dialect, and
// generate() itself).
import type { Node } from "../../lib/estree";

/** The writer state astring threads through generator functions. */
export interface AstringState {
    write(code: string, node?: unknown): void;
    generator: Record<string, (node: Node, state: AstringState) => void>;
}

export type AstringGenerator = Record<string, (node: Node, state: AstringState) => void>;

export const GENERATOR: AstringGenerator;

export function generate(node: Node, options?: { generator?: AstringGenerator; indent?: string }): string;
