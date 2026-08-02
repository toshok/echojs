// the "$maam" import variable resolves to the echojs-maam build the host
// carries: dist/src (the ESM build) compiled into the self-hosted
// compiler, dist/cjs require()d by the node-hosted stage0 (buck-gen-js.sh
// rewrites the specifier).  The surface is declared loosely here;
// eir/oracle.ts narrows it structurally to the slice it consumes.
declare module "$maam" {
    export function analyze(program: unknown, spec: unknown): unknown;
    export function kCFA(...args: unknown[]): unknown;
}
