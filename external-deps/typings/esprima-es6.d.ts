// CANONICAL COPY — external-deps/esprima and /escodegen are git
// submodules, so declaration files placed next to their .js are not
// tracked by this repo.  The build (lib/buck-gen-tsjs.sh) stages this
// file next to the vendored .js; for editor/manual-tsc use, keep the
// untracked sibling copy in sync (cp external-deps/typings/*.d.ts into
// the matching submodule directory).
// hand-written surface declaration for the vendored esprima fork; only
// what the compiler uses.
import type { Program } from "../../lib/estree";

export interface ParseOptions {
    loc?: boolean;
    raw?: boolean;
    tolerant?: boolean;
    sourceType?: "script" | "module";
}

export function parse(source: string, options?: ParseOptions): Program;
