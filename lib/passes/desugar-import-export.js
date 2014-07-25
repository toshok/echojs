/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { startGenerator, intrinsic } from '../echo-util';
import * as b, { ExportBatchSpecifier, FunctionDeclaration, ClassDeclaration, VariableDeclaration, VariableDeclarator } from '../ast-builder';
import { reportError, reportWarning } from '../errors';
import { moduleGet_id } from '../common-ids';
import { TransformPass } from '../node-visitor';

let exports_id = b.identifier("exports"); // XXX as with compiler.coffee, this 'exports' should be '%exports' if the module has ES6 module declarations
let get_id = b.identifier("get");
let Object_id = b.identifier("Object");
let defineProperty_id = b.identifier("defineProperty");
let Object_defineProperty = b.memberExpression(Object_id, defineProperty_id);

let importGen = startGenerator();
function freshId (prefix) {
    return b.identifier(`%${prefix}_${importGen()}`);
}

function define_export_property (exported_id, local_id = exported_id) {
    // return esprima.parse("Object.defineProperty(exports, '#{exported_id.name}', { get: function() { return #{local_id.name}; } });");
    
    let getter = b.functionExpression(undefined, [], b.blockStatement([b.returnStatement(local_id)]));
    
    let property_literal = b.objectExpression([b.property(get_id, getter)]);

    return b.expressionStatement(b.callExpression(Object_defineProperty, [exports_id, b.literal(exported_id.name), property_literal]));
}


export class DesugarImportExport extends TransformPass {
    constructor (options, filename, exportLists) {
        this.exportLists = exportLists;
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
            return intrinsic(moduleGet_id, [n.source_path]);
        }

        // otherwise create a fresh declaration for the module object
        // 
        // let %import_decl = %moduleGet("moduleName")
        // 
        let import_tmp = freshId("import");
        let import_decls =  b.letDeclaration(import_tmp, intrinsic(moduleGet_id, [n.source_path]));

        let list = this.exportLists[n.source_path.value];
        if (n.kind === "default") {
            //
            // let ${n.specifiers[0].id} = %import_decl.default
            //
            if (!list || !list.has_default)
                reportError(ReferenceError, `module '${n.source_path.value}' doesn't have default export`, this.filename, n.loc);

            if (n.specifiers.length !== 1)
                reportError(ReferenceError, "default imports should have only one ImportSpecifier", this.filename, n.loc);
            import_decls.declarations.push(b.variableDeclarator(n.specifiers[0].id, b.memberExpression(import_tmp, b.identifier("default"))));
        }
        else {
            for (let spec of n.specifiers) {
                //
                // let ${spec.id} = %import_decl.#{spec.name || spec.id }
                //
                if (!list || !list.ids.has(spec.id.name))
                    reportError(ReferenceError, `module '${n.source_path.value}' doesn't export '${spec.id.name}'`, this.filename, spec.id.loc);
                import_decls.declarations.push(b.variableDeclarator(spec.name || spec.id, b.memberExpression(import_tmp, spec.id)));
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
            let export_decl = b.letDeclaration(import_tmp, intrinsic(moduleGet_id, [n.source_path]));

            let export_stuff = [];
            if (n.default) {
                // export * from "foo"
                if (n.specifiers.length !== 1 || n.specifiers[0].type !== ExportBatchSpecifier)
                    reportError(SyntaxError, "invalid export", this.filename, n.loc);
                this.batch_exports.push({ source: import_tmp, specifiers: [] });
            }
            else {
                // export { ... } from "foo"
                let list = this.exportLists[n.source_path.value];
                for (let spec of n.specifiers) {
                    if (!list || !list.ids.has(spec.id.name))
                        reportError(ReferenceError, `module '${n.source_path.value}' doesn't export '${spec.id.name}'`, this.filename, spec.id.loc);
                    
                    let spectmp = freshId("spec");
                    export_decl.declarations.push(b.variableDeclarator(spectmp, b.memberExpression(import_tmp, spec.id)));

                    this.exports.push({ name: spec.name, id: spectmp });
                    export_stuff.push(define_export_property(spec.name || spec.id, spectmp));
                }
            }
            export_stuff.unshift(export_decl);
            return export_stuff;
        }

        let export_id = b.identifier("exports");;
        
        // export function foo () { ... }
        if (n.declaration.type === FunctionDeclaration) {
            this.exports.push({ id: n.declaration.id });
            
            return [n.declaration, define_export_property(n.declaration.id)];
        }

        // export class Foo () { ... }
        if (n.declaration.type === ClassDeclaration) {
            this.exports.push({ id: n.declaration.id });
            
            return [n.declaration, define_export_property(n.declaration.id)];
        }

        // export let foo = bar;
        if (n.declaration.type === VariableDeclaration) {
            let export_defines = [];
            for (let decl of n.declaration.declarations) {
                this.exports.push({ id: decl.id });
                export_defines.push(define_export_property(decl.id));
            }
            export_defines.unshift(n.declaration);
            return export_defines;
        }

        // export foo = bar;
        if (n.declaration.type === VariableDeclarator) {
            this.exports.push({ id: n.declaration.id });
            return [n.declaration, define_export_property(n.declaration.id)];
        }

        // export default = ...;
        // 
        if (n.default) {
            let local_default_id = b.identifier("%default");
            let default_id = b.identifier("default");
            this.exports.push({ id: default_id });
            
            let local_decl = b.varDeclaration(local_default_id, n.declaration);

            let export_define = define_export_property(default_id, local_default_id);
            return [local_decl, export_define];
        }

        reportError(SyntaxError, `Unsupported type of export declaration ${n.declaration.type}`, this.filename, n.loc);
    }

    visitModuleDeclaration (n) {
        // this isn't quite right.  I believe this form creates
        // a new instance and puts new properties on it that
        // map to the module, instead of just returning the
        // module object.
        let init = intrinsic(moduleGet_id, [n.source_path]);
        return b.letDeclaration(n.id, init);
    }
}
