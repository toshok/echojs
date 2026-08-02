// hand-written surface declaration for acorn-es6.js, covering only
// what the echo-js compiler uses (lib/parser.ts, the parser seam).
// acorn produces standard ESTree; the seam adapts it to the compiler's
// dialect (lib/estree.ts), so nodes are typed structurally here.

export interface AcornOptions {
    ecmaVersion: number | "latest";
    sourceType?: "script" | "module";
    locations?: boolean;
}

export interface AcornNode {
    type: string;
    start: number;
    end: number;
    [key: string]: unknown;
}

export function parse(source: string, options: AcornOptions): AcornNode;
