/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { startGenerator, intrinsic } from '../echo-util';
import * as b, { ExportBatchSpecifier, FunctionDeclaration, FunctionExpression, ClassDeclaration, ClassExpression, VariableDeclaration, VariableDeclarator } from '../ast-builder';
import { reportError, reportWarning } from '../errors';
import { moduleGetSlot_id, moduleSetSlot_id, moduleGetExotic_id } from '../common-ids';
import { TransformPass } from '../node-visitor';

let importGen = startGenerator();
function freshId (prefix) {
    return b.identifier(`%${prefix}_${importGen()}`);
}

export class DesugarImportExport extends TransformPass {
    constructor (options, filename, allModules) {
       this.allModules = allModules;
        this.filename = filename;
        super(options);
    }
    
    visitFunction (n) {
        if (!n.toplevel)
            return n;
        
        this.exports = [];
        this.batch_exports = [];

        return super(n);
    }
    
    visitImportDeclaration (n) {
        if (n.specifiers.length === 0) {
            // no specifiers, it's of the form:  import from "foo"
            // don't waste a decl for this type
            return b.expressionStatement(intrinsic(moduleGetExotic_id, [n.source_path]));
        }

        let import_decls = b.letDeclaration();
        let module = this.allModules.get(n.source_path.value);

        for (let spec of n.specifiers) {
            if (spec.kind === "default") {
                //
                // let ${n.specifiers[0].id} = %import_decl.default
                //
                if (!module.hasDefaultExport())
                    reportError(ReferenceError, `module '${n.source_path.value}' doesn't have default export`, this.filename, n.loc);

                import_decls.declarations.push(b.variableDeclarator(n.specifiers[0].id, intrinsic(moduleGetSlot_id, [n.source_path, b.literal('default')])));
            }
            else if (spec.kind === "named") {
                //
                // let ${spec.id} = %import_decl.#{spec.name || spec.id }
                //
                if (!module.exports.has(spec.id.name))
                    reportError(ReferenceError, `module '${n.source_path.value}' doesn't export '${spec.id.name}'`, this.filename, spec.id.loc);
                import_decls.declarations.push(b.variableDeclarator(spec.name || spec.id, intrinsic(moduleGetSlot_id, [n.source_path, b.literal(spec.id.name)])));
            }
            else /* if spec.kind is "batch" */ {
                // let #{spec.name} = %import_decl
                import_decls.declarations.push(b.variableDeclarator(spec.name, intrinsic(moduleGetExotic_id, [n.source_path])));
            }
        }
        return import_decls;
    }
    
    visitExportDeclaration (n) {
        // handle the following case in two parts:
        //    export * from "foo"

        if (n.source) {
            // we either have:
            //   export * from "foo"
            // or
            //   export { ... } from "foo"

            // import the module regardless
            let import_tmp = freshId("import");
            let export_stuff = [b.letDeclaration(import_tmp, intrinsic(moduleGetExotic_id, [n.source_path]))]
            if (n.default) {
                // export * from "foo"
                if (n.specifiers.length !== 1 || n.specifiers[0].type !== ExportBatchSpecifier)
                    reportError(SyntaxError, "invalid export", this.filename, n.loc);
                this.batch_exports.push({ source: import_tmp, specifiers: [] });
            }
            else {
                // export { ... } from "foo"
                for (let spec of n.specifiers) {
                    if (!this.allModules.get(n.source_path.value).exports.has(spec.id.name))
                        reportError(ReferenceError, `module '${n.source_path.value}' doesn't export '${spec.id.name}'`, this.filename, spec.id.loc)
                    
                    let export_name = spec.name || spec.id;

                    export_stuff.push(b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(this.filename), b.literal(export_name.name), b.memberExpression(import_tmp, spec.id)])));
                }
            }
            return export_stuff;
        }

        // export function foo () { ... }
        if (n.declaration.type === FunctionDeclaration) {
            this.exports.push({ id: n.declaration.id });

            // we're going to pass it to the moduleSetSlot intrinsic, so it needs to be an expression (or else escodegen freaks out)
            n.declaration.type = FunctionExpression;
            return b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(this.filename), b.literal(n.declaration.id.name), this.visit(n.declaration)]));
        }

        // export class Foo () { ... }
        if (n.declaration.type === ClassDeclaration) {
            this.exports.push({ id: n.declaration.id });

            n.declaration.type = ClassExpression;
            return b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(this.filename), b.literal(n.declaration.id.name), this.visit(n.declaration)]));
        }

        // export let foo = bar;
        if (n.declaration.type === VariableDeclaration) {
            let export_defines = [];
            for (let decl of n.declaration.declarations) {
                this.exports.push({ id: decl.id });
                export_defines.push(b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(this.filename), b.literal(decl.id.name), this.visit(decl.init)])));
            }
            return export_defines;
        }

        // export foo = bar;
        if (n.declaration.type === VariableDeclarator) {
            this.exports.push({ id: n.declaration.id });
            return b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(this.filename), b.literal(n.declaration.id.name), this.visit(n.declaration)]));
        }

        // export default = ...;
        // 
        if (n.default) {
            return b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(this.filename), b.literal("default"), this.visit(n.declaration)]));
        }

        reportError(SyntaxError, `Unsupported type of export declaration ${n.declaration.type}`, this.filename, n.loc);
    }

    visitModuleDeclaration (n) {
        // this isn't quite right.  I believe this form creates
        // a new instance and puts new properties on it that
        // map to the module, instead of just returning the
        // module object.
        let init = intrinsic(moduleGetExotic_id, [n.source_path]);
        return b.letDeclaration(n.id, init);
    }
}
