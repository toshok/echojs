/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { startGenerator, intrinsic } from "../echo-util";
import * as b from "../ast-builder";
import { reportError, reportWarning } from "../errors";
import { moduleGetSlot_id, moduleSetSlot_id, moduleGetExotic_id } from "../common-ids";
import { TransformPass } from "../node-visitor";

let importGen = startGenerator();
function freshId(prefix) {
    return b.identifier(`%${prefix}_${importGen()}`);
}

export class DesugarImportExport extends TransformPass {
    constructor(options, filename, allModules) {
        super(options);
        this.allModules = allModules;
        this.filename = filename;
    }

    visitFunction(n) {
        if (!n.toplevel) return n;

        this.exports = [];
        this.batch_exports = [];

        return super.visitFunction(n);
    }

    visitImportDeclaration(n) {
        if (n.specifiers.length === 0) {
            // no specifiers, it's of the form:  import from "foo"
            // don't waste a decl for this type
            return b.expressionStatement(intrinsic(moduleGetExotic_id, [n.source_path]));
        }

        let import_decls = b.letDeclaration();
        let module = this.allModules.get(n.source_path.value);

        for (let spec of n.specifiers) {
            if (spec.type === b.ImportDefaultSpecifier) {
                //
                // let ${spec.local} = %import_decl.default
                //
                if (!module.hasDefaultExport())
                    reportError(
                        ReferenceError,
                        `module '${n.source_path.value}' doesn't have default export`,
                        this.filename,
                        n.loc
                    );

                import_decls.declarations.push(
                    b.variableDeclarator(
                        spec.local,
                        intrinsic(moduleGetSlot_id, [n.source_path, b.literal("default")])
                    )
                );
            } else if (spec.type === b.ImportSpecifier) {
                //
                // let ${spec.local} = %import_decl.#{spec.imported}
                //
                if (!module.exports.has(spec.imported.name))
                    reportError(
                        ReferenceError,
                        `module '${n.source_path.value}' doesn't export '${spec.imported.name}'`,
                        this.filename,
                        spec.imported.loc
                    );
                import_decls.declarations.push(
                    b.variableDeclarator(
                        spec.local,
                        intrinsic(moduleGetSlot_id, [n.source_path, b.literal(spec.imported.name)])
                    )
                );
            } else if (spec.type === b.ImportNamespaceSpecifier) {
                // let #{spec.name} = %import_decl
                import_decls.declarations.push(
                    b.variableDeclarator(spec.local, intrinsic(moduleGetExotic_id, [n.source_path]))
                );
            } else {
                reportError(
                    Error,
                    `unknown import specifier type ${spec.type}`,
                    this.filename,
                    n.loc
                );
            }
        }
        return import_decls;
    }

    visitExportDefaultDeclaration(n) {
        // export default = ...;
        //
        return b.expressionStatement(
            intrinsic(moduleSetSlot_id, [
                b.literal(this.filename),
                b.literal("default"),
                this.visit(n.declaration),
            ])
        );
    }

    visitExportAllDeclaration(n) {
        let import_tmp = freshId("import");
        let export_stuff = [
            b.letDeclaration(import_tmp, intrinsic(moduleGetExotic_id, [n.source_path])),
        ];

        this.batch_exports.push({ source: import_tmp, specifiers: [] });
    }

    visitExportNamedDeclaration(n) {
        if (n.source) {
            //   export { ... } from "foo"

            // import the module regardless
            let import_tmp = freshId("import");
            let export_stuff = [
                b.letDeclaration(import_tmp, intrinsic(moduleGetExotic_id, [n.source_path])),
            ];

            for (let spec of n.specifiers) {
                if (!this.allModules.get(n.source_path.value).exports.has(spec.local.name))
                    reportError(
                        ReferenceError,
                        `module '${n.source_path.value}' doesn't export '${spec.exported.name}'`,
                        this.filename,
                        spec.local.loc
                    );

                export_stuff.push(
                    b.expressionStatement(
                        intrinsic(moduleSetSlot_id, [
                            b.literal(this.filename),
                            b.literal(spec.exported.name),
                            b.memberExpression(import_tmp, spec.local),
                        ])
                    )
                );
            }
            return export_stuff;
        }

        if (!n.declaration) {
            //   export { ... }
            let export_stuff = [];
            for (let spec of n.specifiers) {
                export_stuff.push(
                    b.expressionStatement(
                        intrinsic(moduleSetSlot_id, [
                            b.literal(this.filename),
                            b.literal(spec.exported.name),
                            spec.local,
                        ])
                    )
                );
            }
            return export_stuff;
        }

        // export function foo () { ... }
        if (n.declaration.type === b.FunctionDeclaration) {
            this.exports.push({ id: n.declaration.id });

            // we're going to pass it to the moduleSetSlot intrinsic, so it needs to be an expression (or else escodegen freaks out)
            n.declaration.type = b.FunctionExpression;
            return b.expressionStatement(
                intrinsic(moduleSetSlot_id, [
                    b.literal(this.filename),
                    b.literal(n.declaration.id.name),
                    this.visit(n.declaration),
                ])
            );
        }

        // export class Foo () { ... }
        if (n.declaration.type === b.ClassDeclaration) {
            this.exports.push({ id: n.declaration.id });

            n.declaration.type = b.ClassExpression;
            return b.expressionStatement(
                intrinsic(moduleSetSlot_id, [
                    b.literal(this.filename),
                    b.literal(n.declaration.id.name),
                    this.visit(n.declaration),
                ])
            );
        }

        // export let foo = bar;
        if (n.declaration.type === b.VariableDeclaration) {
            let export_defines = [];
            for (let decl of n.declaration.declarations) {
                this.exports.push({ id: decl.id });
                export_defines.push(
                    b.expressionStatement(
                        intrinsic(moduleSetSlot_id, [
                            b.literal(this.filename),
                            b.literal(decl.id.name),
                            this.visit(decl.init),
                        ])
                    )
                );
            }
            return export_defines;
        }

        // export foo = bar;
        if (n.declaration.type === b.VariableDeclarator) {
            this.exports.push({ id: n.declaration.id });
            return b.expressionStatement(
                intrinsic(moduleSetSlot_id, [
                    b.literal(this.filename),
                    b.literal(n.declaration.id.name),
                    this.visit(n.declaration),
                ])
            );
        }

        reportError(
            SyntaxError,
            `Unsupported type of export declaration ${n.declaration.type}`,
            this.filename,
            n.loc
        );
    }

    visitModuleDeclaration(n) {
        // this isn't quite right.  I believe this form creates
        // a new instance and puts new properties on it that
        // map to the module, instead of just returning the
        // module object.
        let init = intrinsic(moduleGetExotic_id, [n.source_path]);
        return b.letDeclaration(n.id, init);
    }
}
