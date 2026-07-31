/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// this pass does two things
//
// 1. rewrites all sources to be relative to the toplevel path, recording
//    the resolved path on each import/export node as `source_path`;
//
// 2. builds up the module graph: a ModuleInfo per JS module (exports,
//    slots, import list) plus the native-module registry parsed from
//    .ejs manifests.

import { reportError } from "../errors";
import * as path from "@node-compat/path";
import * as fs from "@node-compat/fs";
import { TreeVisitor, VisitResult } from "../node-visitor";
import { is_string_literal, underline } from "../echo-util";
import { JSModuleInfo, NativeModuleInfo, ModuleInfo } from "../module-info";
import * as b from "../ast-builder";
import * as parser from "../parser";
import type * as e from "../estree";
import type { CompilerOptions, ImportVariable } from "../options";
import { passes } from "../pass-config";
import type { Triple } from "../triple";

function isNativeModule(source: string): boolean {
    return source[0] === "@";
}

const allModules = new Map<string, ModuleInfo>();
const nativeModules = new Map<string, NativeModuleInfo>();

type SourcedNode = e.ImportDeclaration | e.ExportNamedDeclaration | e.ExportAllDeclaration;

class GatherImports extends TreeVisitor {
    filename: string;
    path: string;
    toplevel_path: string;
    import_vars: ImportVariable[];
    importList: string[] = [];
    moduleInfo: JSModuleInfo;

    constructor(filename: string, p: string, toplevel_path: string, import_vars: ImportVariable[]) {
        super();
        this.filename = filename;
        this.path = p;
        this.toplevel_path = toplevel_path;
        this.import_vars = import_vars;

        // remove our .js suffix since all imports are suffix-free
        if (path.extname(this.filename) === ".js") {
            this.filename = this.filename.substring(0, this.filename.length - 3);
        }

        this.moduleInfo = new JSModuleInfo(this.filename);
        allModules.set(this.filename, this.moduleInfo);
    }

    private addSource<T extends SourcedNode>(n: T): T {
        if (!n.source) return n;

        if (!is_string_literal(n.source)) throw new Error("import sources must be strings");

        let source_path = String(n.source.value);

        for (const v of this.import_vars) {
            source_path = source_path.replace(`$${v.variable}`, v.value);
        }

        if (!isNativeModule(source_path)) {
            if (source_path[0] !== "/")
                source_path = path.resolve(this.toplevel_path, this.path, source_path);

            if (source_path.indexOf(process.cwd()) === 0)
                source_path = path.relative(process.cwd(), source_path);
        }

        if (this.importList.indexOf(source_path) === -1) this.importList.push(source_path);
        this.moduleInfo.addImportSource(source_path);

        n.source_path = b.literal(source_path) as e.Literal & { value: string };
        return n;
    }

    private addExportIdentifier(id: string, constval?: e.Literal): void {
        if (id === "default") this.moduleInfo.setHasDefaultExport();
        this.moduleInfo.addExport(id, constval);
    }

    override visitImportDeclaration(n: e.ImportDeclaration): VisitResult {
        return this.addSource(n);
    }

    override visitExportDefaultDeclaration(n: e.ExportDefaultDeclaration): VisitResult {
        this.moduleInfo.addExport("default");
        this.moduleInfo.setHasDefaultExport();
        return n;
    }

    override visitExportNamedDeclaration(n: e.ExportNamedDeclaration): VisitResult {
        if (n.declaration && (n.specifiers.length > 0 || n.source)) {
            reportError(
                Error,
                "invalid state in ExportNamedDeclaration",
                this.filename,
                n.loc ?? undefined
            );
        }

        this.addSource(n);

        if (n.specifiers.length > 0) {
            for (const spec of n.specifiers) {
                this.addExportIdentifier(spec.exported.name);
            }
            return n;
        }

        const declaration = n.declaration;
        if (!declaration) throw new Error("unhandled case in visitExportNamedDeclaration");

        if (declaration.type === "FunctionDeclaration" || declaration.type === "ClassDeclaration") {
            this.addExportIdentifier(declaration.id.name);
        } else if (declaration.type === "VariableDeclaration") {
            for (const decl of declaration.declarations) {
                if (decl.id.type !== "Identifier") continue;
                this.addExportIdentifier(
                    decl.id.name,
                    declaration.kind === "const" && decl.init && decl.init.type === "Literal"
                        ? decl.init
                        : undefined
                );
            }
        } else {
            throw new Error("unhandled case in visitExportNamedDeclaration");
        }
        return n;
    }

    override visitExportAllDeclaration(n: e.ExportAllDeclaration): VisitResult {
        throw new Error("GatherImports#visitExportAllDeclaration unimplemented");
    }
}

export function getAllModules(): Map<string, ModuleInfo> {
    return allModules;
}

function dumpModule(m: ModuleInfo): void {
    console.log(`'${m.path}'`);
    console.log(`   has default: ${m.hasDefaultExport()}`);
    if (m.exports.size > 0) {
        console.log("   slots:");
        m.exports.forEach((v, k) => {
            console.log(`      ${k}: ${v.slot_num}`);
        });
    }
}

export function dumpModules(): void {
    console.log(underline("modules"));
    allModules.forEach((m) => dumpModule(m));
}

// promote non-exported module-level vars to hidden module slots; the EIR
// pipeline routes references through module_slot_load/store, so functions
// referencing mutable module state see one shared storage.
//
// only DIRECT toplevel declarations promote.  a `var` re-declaration of
// the same name nested inside a toplevel statement (`if (x) { var state
// = ... }`) shares the binding but wouldn't be rewritten, so any name
// with such a nested declaration is excluded entirely.  `const name =
// <literal>` stays a plain local: it constant-folds instead.
function promoteModuleVars(moduleInfo: ModuleInfo, tree: e.Program): void {
    // debugging: -fno-promote disables promotion outright;
    // -fno-promote=substr1,substr2 only for matching module paths
    // (bisecting promotion-related miscompiles)
    const pcfg = passes();
    if (!pcfg.promote) return;
    for (const pat of pcfg.promoteExclude) {
        if (moduleInfo.path.indexOf(pat) !== -1) return;
    }
    // names declared by `var` nested below a direct toplevel statement
    // (but outside any function -- function bodies are their own scope)
    const nestedVarNames = new Set<string>();
    const walkNested = (n: unknown): void => {
        if (!n || typeof n !== "object") return;
        if (Array.isArray(n)) {
            for (const el of n) walkNested(el);
            return;
        }
        const node = n as e.Node;
        if (
            node.type === "FunctionDeclaration" ||
            node.type === "FunctionExpression" ||
            node.type === "ArrowFunctionExpression"
        )
            return;
        if (node.type === "VariableDeclaration" && node.kind === "var") {
            for (const d of node.declarations) {
                if (d.id.type === "Identifier") nestedVarNames.add(d.id.name);
            }
        }
        for (const k of Object.keys(node)) {
            if (k === "loc") continue;
            walkNested((node as unknown as Record<string, unknown>)[k]);
        }
    };
    for (const stmt of tree.body) {
        if (stmt.type === "VariableDeclaration") continue; // direct: handled below
        walkNested(stmt);
    }

    for (const stmt of tree.body) {
        if (stmt.type === "VariableDeclaration") {
            for (const d of stmt.declarations) {
                if (d.id.type !== "Identifier") continue;
                if (moduleInfo.exports.has(d.id.name)) continue; // already slotted
                if (nestedVarNames.has(d.id.name)) continue;
                if (stmt.kind === "const" && d.init && d.init.type === "Literal") continue;
                moduleInfo.addPromotedSlot(d.id.name);
            }
        } else if (stmt.type === "ClassDeclaration" && stmt.id) {
            // classes don't hoist, so the setSlot rewrite at the source
            // position is exactly their declaration semantics
            if (moduleInfo.exports.has(stmt.id.name)) continue;
            if (nestedVarNames.has(stmt.id.name)) continue;
            moduleInfo.addPromotedSlot(stmt.id.name);
        } else if (stmt.type === "FunctionDeclaration" && stmt.id) {
            // function declarations promote too: their slot holds the one
            // closure, so references (calls, value uses, `new Foo()`)
            // resolve identically.  the declaration becomes a slot store
            // at its source position, so -- exactly like exported
            // functions -- hoisting across toplevel *initialization* code
            // is lost.
            if (moduleInfo.exports.has(stmt.id.name)) continue;
            if (nestedVarNames.has(stmt.id.name)) continue;
            moduleInfo.addPromotedSlot(stmt.id.name);
        }
    }
}

function gatherImports(
    filename: string,
    p: string,
    top_path: string,
    tree: e.Program,
    import_vars: ImportVariable[]
): string[] {
    const visitor = new GatherImports(filename, p, top_path, import_vars);
    visitor.visit(tree);
    promoteModuleVars(visitor.moduleInfo, tree);
    return visitor.importList;
}

function parseFile(filename: string, content: string, options: CompilerOptions): e.Program {
    try {
        if (!options.quiet) {
            // loop over import variables, replacing their values with
            // their names for output
            let output_name = filename;
            for (const ivar of options.import_variables) {
                output_name = output_name.replace(ivar.value, `$${ivar.variable}`);
            }
            options.stdout_writer.write(`PARSE ${output_name}`);
        }
        // NOT tolerant: true — tolerant mode collects parse errors into
        // ast.errors and returns a partial AST, which we would then
        // silently miscompile (e.g. `async m() {}` object methods
        // compiled to nonsense).  a program that doesn't parse must fail
        // loudly here.  sourceType "module" is what makes import/export
        // parse at all (tolerant mode used to recover past the spurious
        // script-mode error on every import) and, per spec, makes the
        // parse strict.
        return parser.parse(content, { loc: true, raw: true, sourceType: "module" });
    } catch (err) {
        console.warn(`${filename}: ${String(err)}:`);
        return process.exit(-1);
    }
}

// the .ejs manifest shape (JSON, one per native module)
interface NativeManifest {
    module_name?: string;
    init_function?: string;
    link_flags?: string | Record<string, string>;
    module_file?: string | Record<string, string>;
    exports?: string[];
    submodules?: NativeManifest[];
}

function getModuleFile(manifest: NativeManifest, triple: Triple): string {
    const module_file = manifest.module_file!;
    if (typeof module_file == "string") {
        return module_file;
    }
    const module_file_key = triple.toShortString();
    const file = module_file[module_file_key];
    if (!file) {
        throw new Error(
            `module ${manifest.module_name} doesn't have a module file for ${module_file_key}`
        );
    }
    return file;
}

function getModuleLinkFlags(manifest: NativeManifest, triple: Triple): string {
    const link_flags = manifest.link_flags!;
    if (typeof link_flags === "string") {
        return link_flags;
    }
    const module_file_key = triple.toShortString();
    const flags = link_flags[module_file_key];
    if (!flags) {
        throw new Error(
            `module ${manifest.module_name} doesn't have link flags for ${module_file_key}`
        );
    }
    return flags;
}

function registerNativeModuleInfo(
    ejs_dir: string,
    module_name: string,
    link_flags: string[],
    module_files: string[],
    manifest: NativeManifest,
    triple: Triple
): void {
    if (manifest.link_flags)
        link_flags = link_flags.concat(getModuleLinkFlags(manifest, triple));
    if (manifest.module_file) module_files = module_files.concat(getModuleFile(manifest, triple));

    if (manifest.init_function) {
        // this module can be imported
        const m = new NativeModuleInfo(
            module_name,
            manifest.init_function,
            link_flags,
            module_files,
            ejs_dir
        );
        if (manifest.exports) manifest.exports.forEach((v) => m.addExport(v));

        nativeModules.set(module_name, m);
    }
    if (manifest.submodules) {
        for (const sm of manifest.submodules) {
            if (!sm.module_name)
                throw new Error(`${module_name} submodule missing module_name property`);
            registerNativeModuleInfo(
                ejs_dir,
                `${module_name}/${sm.module_name}`,
                link_flags,
                module_files,
                sm,
                triple
            );
        }
    }
}

function gatherNativeModuleInfo(ejs_file: string, triple: Triple): void {
    const manifest = JSON.parse(fs.readFileSync(ejs_file, "utf-8")) as NativeManifest;
    const module_name = manifest.module_name || path.basename(ejs_file, ".ejs");

    registerNativeModuleInfo(path.dirname(ejs_file), module_name, [], [], manifest, triple);
}

function gatherAllNativeModules(module_dirs: string[], triple: Triple): void {
    // gather a list of all native modules, flattening their submodule lists
    for (const mdir of module_dirs) {
        try {
            const files = fs.readdirSync(mdir);
            files.forEach((f) => {
                if (path.extname(f) === ".ejs") {
                    try {
                        gatherNativeModuleInfo(path.resolve(mdir, f), triple);
                    } catch (err) {
                        console.warn(`parsing of module file ${f} failed: ${String(err)}`);
                    }
                }
            });
        } catch (err) {
            // a missing module dir is fine
        }
    }
}

export interface GatheredFile {
    file_name: string;
    file_ast: e.Program;
}

export function gatherAllModules(
    file_args: string[],
    options: CompilerOptions,
    triple: Triple
): GatheredFile[] {
    const work_list = file_args.slice();
    const files: GatheredFile[] = [];

    gatherAllNativeModules(options.native_module_dirs, triple);

    // starting at the main file, gather all files we'll need
    while (work_list.length !== 0) {
        const file = work_list.pop()!;

        let found = false;
        let jsfile = file;
        if (path.extname(jsfile) !== ".js") {
            jsfile = jsfile + ".js";
        }

        try {
            found = fs.statSync(jsfile).isFile();
        } catch (err) {
            found = false;
        }

        if (!found) {
            try {
                if (fs.statSync(file).isDirectory()) {
                    jsfile = path.join(file, "index.js");
                    found = fs.statSync(jsfile).isFile();
                }
            } catch (err) {
                found = false;
            }
        }

        if (found) {
            const file_contents = fs.readFileSync(jsfile, "utf-8");
            const file_ast = parseFile(jsfile, file_contents, options);

            const imports = gatherImports(
                file,
                path.dirname(jsfile),
                process.cwd(),
                file_ast,
                options.import_variables
            );

            files.push({ file_name: file, file_ast: file_ast });

            for (const i of imports) {
                if (work_list.indexOf(i) === -1 && !files.some((el) => el.file_name === i)) {
                    work_list.push(i);
                }
            }
        } else {
            // check if the file is a native module
            if (!allModules.has(file)) {
                if (file[0] != "@") {
                    throw new Error(`module ${file} not found`);
                }
                const native_path = file.slice(1);
                const native = nativeModules.get(native_path);
                if (!native) {
                    throw new Error(`native module ${file} not found`);
                }

                allModules.set(file, native);
            }
        }
    }

    return files;
}
