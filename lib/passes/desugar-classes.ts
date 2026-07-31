/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */
//
// converts:
//
// class Subclass extends Baseclass
//   constructor (/* ctor args */) { /* ctor body */ }
//
//   method (/* method args */) { /* method body */ }
//
// to:
//
// let Subclass = (function(%super) {
//   function Subclass (/* ctor args */) { /* ctor body */ }
//   Subclass.prototype.method = function(/* method args */) { /* method body */ };
//   return Subclass;
// })(Baseclass)
//
//
import * as b from "../ast-builder";
import {
    setConstructorKindDerived_id,
    setConstructorKindBase_id,
    superid,
    constructSuper_id,
    constructSuperApply_id,
    setPrototypeOf_id,
    objectCreate_id,
    call_id,
    prototype_id,
    constructor_id,
    get_id,
    set_id,
    Object_id,
    proto_id,
    defineProperty_id,
    enumerable_id,
    value_id,
    defineProperties_id,
} from "../common-ids";
import { Stack } from "../stack-es6";
import { reportError } from "../errors";
import { TransformPass, VisitResult } from "../node-visitor";
import { intrinsic, startGenerator } from "../echo-util";
import type * as e from "../estree";

// identifiers that appear in VALUE position must be fresh AST nodes per
// use: the EIR scope analysis resolves references in a map keyed by node,
// so a shared singleton node (like common-ids' superid) walked in two
// class iifes would resolve every occurrence to the LAST iife's binding.
// property-position identifiers (`.prototype`, object keys) are never
// resolved and may stay shared.
function freshSuper(): e.Identifier {
    return b.identifier(superid.name);
}
function freshProto(): e.Identifier {
    return b.identifier(proto_id.name);
}

function createSuperReference(is_static: boolean, id?: e.Expression): e.Expression {
    if (id && id.type === "Identifier" && id.name === "constructor") return freshSuper();

    const obj = is_static ? freshSuper() : b.memberExpression(freshSuper(), prototype_id);

    if (!id) return obj;

    return b.memberExpression(obj, id);
}

// a class node whose (possibly synthesized) id is known present
type NamedClass = e.ClassBase & { id: e.Identifier };

const classgen = startGenerator();
function freshClassId(): e.Identifier {
    return b.identifier(`%anonClass_${classgen()}`);
}

// one prototype/static property's accessors (a get/set pair for the same
// non-computed name shares an entry — keying by the key AST node lost the
// getter, latent bug #14/#26)
interface AccessorEntry {
    get?: e.MethodDefinition;
    set?: e.MethodDefinition;
    computed: boolean;
}

export class DesugarClasses extends TransformPass {
    private method_stack = new Stack<e.MethodDefinition>();

    override visitCallExpression(n: e.CallExpression): VisitResult {
        if (n.callee.type === "Super") {
            const method = this.method_stack.top;
            if (this.nameOfKey(method.key) !== "constructor") {
                reportError(
                    SyntaxError,
                    "calls to super() are only allowable in constructors.",
                    this.filename,
                    n.callee.loc ?? undefined
                );
            }

            const super_ref = createSuperReference(method.static === true, method.key);
            n.callee = constructSuper_id;
            n.arguments.unshift(super_ref);
        } else if (n.callee.type === "MemberExpression" && n.callee.object.type === "Super") {
            const method = this.method_stack.top;
            const super_ref = createSuperReference(method.static === true, method.key);
            n.callee = b.memberExpression(super_ref, call_id);
            n.arguments.unshift(b.thisExpression());
        } else {
            n.callee = this.visitAs(n.callee);
        }
        n.arguments = this.visitArray(n.arguments);
        return n;
    }

    override visitNewExpression(n: e.NewExpression): VisitResult {
        n.callee = this.visitAs(n.callee);
        n.arguments = this.visitArray(n.arguments);
        return n;
    }

    override visitObjectExpression(n: e.ObjectExpression): VisitResult {
        for (const property of n.properties) {
            if (property.computed) property.key = this.visitAs(property.key);
            property.value = this.visitAs(property.value);
        }
        return n;
    }

    override visitSuper(): VisitResult {
        return createSuperReference(this.method_stack.top.static === true);
    }

    override visitClassDeclaration(n: e.ClassDeclaration): VisitResult {
        if (!n.id) n.id = freshClassId();
        n.superClass = this.visitNullable(n.superClass);
        const iife = this.generateClassIIFE(n);
        return b.letDeclaration(n.id, b.callExpression(iife, n.superClass ? [n.superClass] : []));
    }

    override visitClassExpression(n: e.ClassExpression): VisitResult {
        if (!n.id) n.id = freshClassId();
        n.superClass = this.visitNullable(n.superClass);
        const iife = this.generateClassIIFE(n as NamedClass);
        return b.callExpression(iife, n.superClass ? [n.superClass] : []);
    }

    private generateClassIIFE(n: NamedClass): e.FunctionExpression {
        // visit all the functions defined in the class so that 'super' is
        // replaced with '%super'
        for (const class_element of n.body.body) {
            this.method_stack.push(class_element);
            class_element.value = this.visitAs(class_element.value);
            this.method_stack.pop();
        }

        let class_init_iife_body: e.Statement[] = [];

        const { properties, methods, sproperties, smethods } = this.gather_members(n);

        // a fresh node per value-position use of the class name: n.id
        // itself becomes the OUTER let declarator (visitClassDeclaration),
        // and node-keyed reference resolution must not alias the two scopes
        const cname = () => b.identifier(n.id.name);

        class_init_iife_body.push(
            b.letDeclaration(b.identifier("proto"), b.memberExpression(cname(), prototype_id))
        );

        let ctor: e.MethodDefinition | null = null;
        methods.forEach((m, mkey) => {
            // the method named 'constructor' becomes the special ctor function
            if (mkey === "constructor") {
                ctor = m;
            } else {
                class_init_iife_body.push(this.create_proto_method(m, n));
            }
        });
        smethods.forEach((sm) => class_init_iife_body.push(this.create_static_method(sm, n)));

        const proto_props = this.create_properties(properties, n, false);
        if (proto_props) class_init_iife_body.push(proto_props);

        const static_props = this.create_properties(sproperties, n, true);
        if (static_props) class_init_iife_body.push(static_props);

        // generate and prepend a default ctor if there isn't one declared.
        // It looks like this in code:
        //   function Subclass (...args) { %super.call(this, args...); }
        if (!ctor) {
            ctor = this.create_default_constructor(n);

            // we didn't visit it above, so do it now
            this.method_stack.push(ctor);
            ctor.value = this.visitAs(ctor.value);
            this.method_stack.pop();
        }

        const ctor_func = this.create_constructor(ctor, n);
        if (n.superClass) {
            class_init_iife_body.unshift(
                b.expressionStatement(
                    b.assignmentExpression(
                        b.memberExpression(
                            b.memberExpression(cname(), prototype_id),
                            constructor_id
                        ),
                        "=",
                        cname()
                    )
                )
            );

            // also set ctor.prototype = Object.create(superClass.prototype)
            class_init_iife_body.unshift(
                b.expressionStatement(
                    b.callExpression(setPrototypeOf_id, [
                        b.memberExpression(cname(), prototype_id),
                        b.callExpression(objectCreate_id, [
                            b.memberExpression(freshSuper(), prototype_id),
                        ]),
                    ])
                )
            );

            // 14.5.17 step 9, make sure the constructor's __proto__ is set to superClass
            class_init_iife_body.unshift(
                b.expressionStatement(b.callExpression(setPrototypeOf_id, [cname(), freshSuper()]))
            );

            class_init_iife_body.unshift(
                b.expressionStatement(intrinsic(setConstructorKindDerived_id, [cname()]))
            );
        } else {
            class_init_iife_body.unshift(
                b.expressionStatement(intrinsic(setConstructorKindBase_id, [cname()]))
            );
        }

        class_init_iife_body.unshift(ctor_func);

        // make sure we return the function from our iife
        class_init_iife_body.push(b.returnStatement(cname()));

        // (function (%super?) { ... })
        const iife_body = b.blockStatement(class_init_iife_body, n.loc ?? null);
        return b.functionExpression(
            b.identifier(`${n.id.name || "anonclass"}_iife`),
            n.superClass ? [freshSuper()] : [],
            iife_body
        );
    }

    private gather_members(ast_class: NamedClass): {
        properties: Map<string | e.Expression, AccessorEntry>;
        methods: Map<string, e.MethodDefinition>;
        sproperties: Map<string | e.Expression, AccessorEntry>;
        smethods: Map<string, e.MethodDefinition>;
    } {
        const methods = new Map<string, e.MethodDefinition>();
        const smethods = new Map<string, e.MethodDefinition>();
        const properties = new Map<string | e.Expression, AccessorEntry>();
        const sproperties = new Map<string | e.Expression, AccessorEntry>();

        for (const class_element of ast_class.body.body) {
            const class_element_name = this.nameOfKey(class_element.key);
            if (class_element.static && class_element_name === "prototype")
                reportError(
                    SyntaxError,
                    'Illegal method name "prototype" on static class member.',
                    this.filename,
                    class_element.loc ?? undefined
                );

            if (class_element.kind === "method" || class_element.kind === "constructor") {
                // a method
                const method_map = class_element.static ? smethods : methods;
                if (method_map.has(class_element_name))
                    reportError(
                        SyntaxError,
                        `method '${class_element_name}' has already been defined.`,
                        this.filename,
                        class_element.loc ?? undefined
                    );
                method_map.set(class_element_name, class_element);
            } else if (class_element.kind === "get" || class_element.kind === "set") {
                // an accessor property
                const property_map = class_element.static ? sproperties : properties;

                // key non-computed accessors by NAME so a get/set pair for
                // the same property shares one entry: keying by the key
                // AST node put them in separate entries, and the emitted
                // `{ n: {get}, n: {set} }` object literal lost the getter
                const prop_key = class_element.computed ? class_element.key : class_element_name;

                let entry = property_map.get(prop_key);
                if (!entry) {
                    entry = { computed: class_element.computed === true };
                    property_map.set(prop_key, entry);
                }

                if (entry[class_element.kind])
                    reportError(
                        SyntaxError,
                        `a '${class_element.kind}' method for '${this.nameOfKey(
                            class_element.key
                        )}' has already been defined.`,
                        this.filename,
                        class_element.loc ?? undefined
                    );

                if (class_element.kind === "set") {
                    const params = class_element.value.params;
                    const last_param = params[params.length - 1];
                    if (last_param && last_param.type === "RestElement")
                        reportError(
                            SyntaxError,
                            "Setters are not allowed to have a rest",
                            this.filename,
                            last_param.loc ?? undefined
                        );
                }

                // XXX this doesn't work for properties where one accessor
                // is computed and the other isn't...
                if (entry.computed !== (class_element.computed === true))
                    reportError(
                        Error,
                        "unsupported mismatch computed state for property accessors",
                        this.filename,
                        class_element.loc ?? undefined
                    );

                entry[class_element.kind] = class_element;
            } else {
                reportError(
                    Error,
                    `unhandled class element kind '${class_element.kind}'`,
                    this.filename,
                    class_element.loc ?? undefined
                );
            }
        }

        return { properties, methods, sproperties, smethods };
    }

    private create_constructor(
        ast_method: e.MethodDefinition,
        ast_class: NamedClass
    ): e.FunctionDeclaration {
        // fresh id: ast_class.id is the outer let declarator's node
        return b.functionDeclaration(
            b.identifier(ast_class.id.name),
            ast_method.value.params,
            ast_method.value.body,
            ast_method.value.defaults
        );
    }

    private create_default_constructor(ast_class: NamedClass): e.MethodDefinition {
        // splat args into the call to super's ctor if there's a superclass
        const args_id = b.identifier("args");
        const functionBody = b.blockStatement(
            ast_class.superClass
                ? [
                      b.expressionStatement(
                          intrinsic(constructSuperApply_id, [freshSuper(), args_id])
                      ),
                  ]
                : []
        );
        return b.methodDefinition(
            constructor_id,
            b.functionExpression(null, [b.restElement(args_id)], functionBody, [])
        );
    }

    private nameOfKey(key: e.Expression): string {
        return key.type === "Identifier" ? key.name : String((key as e.Literal).value);
    }

    private create_proto_method(
        ast_method: e.MethodDefinition,
        ast_class: NamedClass
    ): e.Statement {
        const method_name = this.nameOfKey(ast_method.key);
        const method_key = ast_method.computed ? ast_method.key : b.literal(method_name);
        const method = b.functionExpression(
            b.identifier(`${ast_class.id.name}:${method_name}`),
            ast_method.value.params,
            ast_method.value.body,
            ast_method.value.defaults
        );
        // b.functionExpression hardcodes generator: false — losing the
        // flag here left `*method() {}` yields undesugared
        method.generator = ast_method.value.generator;

        const Object_defineProperty = b.memberExpression(Object_id, defineProperty_id);
        const defineProperty_args = b.objectExpression([
            b.property(value_id, method),
            b.property(enumerable_id, b.literal(false)),
        ]);
        return b.expressionStatement(
            b.callExpression(Object_defineProperty, [freshProto(), method_key, defineProperty_args])
        );
    }

    private create_static_method(
        ast_method: e.MethodDefinition,
        ast_class: NamedClass
    ): e.Statement {
        const method_name = this.nameOfKey(ast_method.key);
        const method_key = ast_method.computed ? ast_method.key : b.literal(method_name);
        const method = b.functionExpression(
            ast_method.key.type === "Identifier" ? ast_method.key : null,
            ast_method.value.params,
            ast_method.value.body,
            ast_method.value.defaults
        );
        method.generator = ast_method.value.generator;

        const Object_defineProperty = b.memberExpression(Object_id, defineProperty_id);
        const defineProperty_args = b.objectExpression([
            b.property(value_id, method),
            b.property(enumerable_id, b.literal(false)),
        ]);
        return b.expressionStatement(
            b.callExpression(Object_defineProperty, [
                b.identifier(ast_class.id.name),
                method_key,
                defineProperty_args,
            ])
        );
    }

    private create_properties(
        properties: Map<string | e.Expression, AccessorEntry>,
        ast_class: NamedClass,
        are_static: boolean
    ): e.Statement | null {
        const propdescs: e.Property[] = [];

        properties.forEach((entry) => {
            const accessors: e.Property[] = [];
            let key: e.Expression | null = null;

            const getter = entry.get;
            const setter = entry.set;

            // the map key is a name for non-computed accessors (so a
            // get/set pair shares an entry); the emitted property key is
            // the accessor's own key node
            if (getter) {
                accessors.push(b.property(get_id, getter.value));
                key = getter.key;
            }
            if (setter) {
                accessors.push(b.property(set_id, setter.value));
                key = setter.key;
            }

            propdescs.push(
                b.property(key!, b.objectExpression(accessors), "init", entry.computed)
            );
        });

        if (propdescs.length === 0) return null;

        const propdescs_literal = b.objectExpression(propdescs);

        const target = are_static ? b.identifier(ast_class.id.name) : b.identifier("proto");

        return b.expressionStatement(
            b.callExpression(b.memberExpression(Object_id, defineProperties_id), [
                target,
                propdescs_literal,
            ])
        );
    }
}
