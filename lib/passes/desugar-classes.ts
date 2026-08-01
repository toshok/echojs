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
    defineField_id,
    makePrivateMap_id,
    privFieldGet_id,
    privFieldSet_id,
    privFieldInit_id,
    privBrandCheck_id,
    privHas_id,
    privWriteError_id,
} from "../common-ids";
import { Stack } from "../stack-es6";
import { reportError } from "../errors";
import { TransformPass, VisitResult } from "../node-visitor";
import { intrinsic, is_intrinsic, startGenerator } from "../echo-util";
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

// one #name in a class's private scope.  fields store per
// object in a compiler-created weakmap; methods/accessors are shared
// closures guarded by the class's brand weakmap.
interface PrivInfo {
    kind: "field" | "method" | "accessor";
    name: string; // without the '#'
    mapId?: e.Identifier; // field: its weakmap
    brandId?: e.Identifier; // method/accessor: the class brand map
    fnId?: e.Identifier;
    getFnId?: e.Identifier;
    setFnId?: e.Identifier;
}

const privgen = startGenerator();
function freshPriv(tag: string): e.Identifier {
    return b.identifier(`%priv_${tag}_${privgen()}`);
}

// `#x` renders as "#x" in runtime TypeError messages
function privNameLit(info: PrivInfo): e.Literal {
    return b.literal(`#${info.name}`);
}

function refOf(id: e.Identifier): e.Identifier {
    return b.identifier(id.name);
}

// is this (post-visit) expression statement a desugared super() call?
function isSuperCallStatement(s: e.Statement): boolean {
    if (s.type !== "ExpressionStatement") return false;
    const ex = s.expression;
    return is_intrinsic(ex, "%constructSuper") || is_intrinsic(ex, "%constructSuperApply");
}

export class DesugarClasses extends TransformPass {
    private method_stack = new Stack<e.MethodDefinition>();
    // lexically-nested private scopes; resolution walks from the innermost
    private private_scopes: Map<string, PrivInfo>[] = [];

    // ---- private member rewriting ----------------------------------------

    private resolvePrivate(name: string, loc: e.SourceLocation | null | undefined): PrivInfo {
        for (let i = this.private_scopes.length - 1; i >= 0; i--) {
            const info = this.private_scopes[i]!.get(name);
            if (info) return info;
        }
        // acorn validates private names against enclosing classes, so this
        // is unreachable from real parses
        reportError(SyntaxError, `Private name #${name} is not defined`, this.filename, loc ?? undefined);
    }

    // NOT a type predicate: the false branch must keep plain
    // MemberExpressions in the union
    private isPrivateMember(n: e.Node): boolean {
        return (
            n.type === "MemberExpression" &&
            !n.computed &&
            (n.property as e.Node).type === "PrivateIdentifier"
        );
    }

    // obj.#x as a value
    private privRead(obj: e.Expression, info: PrivInfo): e.Expression {
        if (info.kind === "field")
            return intrinsic(privFieldGet_id, [refOf(info.mapId!), obj, privNameLit(info)]);
        const checked = intrinsic(privBrandCheck_id, [refOf(info.brandId!), obj, privNameLit(info)]);
        if (info.kind === "method")
            // brand-check obj, then the shared closure is the value
            return b.sequenceExpression([checked, refOf(info.fnId!)]);
        // accessor: call the getter with the checked receiver
        if (!info.getFnId)
            // write-only accessor read: runtime TypeError
            return b.sequenceExpression([
                checked,
                intrinsic(privWriteError_id, [privNameLit(info)]),
            ]);
        return b.callExpression(b.memberExpression(refOf(info.getFnId), call_id), [checked]);
    }

    // obj.#x = value (operator "=" only; compound ops go through privReadModifyWrite)
    private privWrite(obj: e.Expression, info: PrivInfo, value: e.Expression, _loc: e.SourceLocation | null | undefined): e.Expression {
        if (info.kind === "field")
            return intrinsic(privFieldSet_id, [refOf(info.mapId!), obj, privNameLit(info), value]);
        if (info.kind === "method" || !info.setFnId) {
            // writing a private method / read-only accessor is a RUNTIME
            // TypeError (logical assignment can short-circuit past it);
            // obj and value still evaluate first, in order
            return b.sequenceExpression([
                intrinsic(privBrandCheck_id, [refOf(info.brandId!), obj, privNameLit(info)]),
                value,
                intrinsic(privWriteError_id, [privNameLit(info)]),
            ]);
        }
        // ((%o, %v) => { %set.call(%privBrandCheck(brand, %o, name), %v); return %v; })(obj, value)
        const o = freshPriv("o");
        const v = freshPriv("v");
        const setCall = b.callExpression(b.memberExpression(refOf(info.setFnId), call_id), [
            intrinsic(privBrandCheck_id, [refOf(info.brandId!), refOf(o), privNameLit(info)]),
            refOf(v),
        ]);
        return b.callExpression(
            b.arrowFunctionExpression(
                [o, v],
                b.blockStatement([b.expressionStatement(setCall), b.returnStatement(refOf(v))])
            ),
            [obj, value]
        );
    }

    // obj.#x op= rhs / obj.#x++ — single-evaluation read-modify-write.
    // mkResult picks the expression value: the new value (compound/prefix)
    // or the ToNumber'd old one (postfix).
    private privReadModifyWrite(
        obj: e.Expression,
        info: PrivInfo,
        mkNewValue: (oldValue: e.Expression) => e.Expression,
        postfix: boolean,
        loc: e.SourceLocation | null | undefined
    ): e.Expression {
        const o = freshPriv("o");
        const old = freshPriv("old");
        const read = this.privRead(refOf(o), info);
        // postfix returns ToNumeric(old): unary + on the read
        const oldInit = postfix ? b.unaryExpression("+", read) : read;
        const write = this.privWrite(refOf(o), info, mkNewValue(refOf(old)), loc);
        const body: e.Statement[] = [
            b.letDeclaration(old, oldInit),
            postfix
                ? b.expressionStatement(write)
                : b.returnStatement(write),
        ];
        if (postfix) body.push(b.returnStatement(refOf(old)));
        return b.callExpression(b.arrowFunctionExpression([o], b.blockStatement(body)), [obj]);
    }

    override visitMemberExpression(n: e.MemberExpression): VisitResult {
        if (this.isPrivateMember(n)) {
            const info = this.resolvePrivate((n.property as e.PrivateIdentifier).name, n.loc);
            return this.privRead(this.visitAs(n.object as e.Expression), info);
        }
        return super.visitMemberExpression(n);
    }

    override visitAssignmentExpression(n: e.AssignmentExpression): VisitResult {
        if (this.isPrivateMember(n.left)) {
            const target = n.left as e.MemberExpression;
            const info = this.resolvePrivate((target.property as e.PrivateIdentifier).name, target.loc);
            const obj = this.visitAs(target.object as e.Expression);
            const right = this.visitAs(n.right);
            if (n.operator === "=") return this.privWrite(obj, info, right, n.loc);
            // compound: strip the trailing '=' to get the binary operator
            const binop = n.operator.slice(0, -1) as e.BinaryOperator;
            return this.privReadModifyWrite(
                obj,
                info,
                (old) => b.binaryExpression(old, binop, right),
                false,
                n.loc
            );
        }
        return super.visitAssignmentExpression(n);
    }

    override visitUpdateExpression(n: e.UpdateExpression): VisitResult {
        if (this.isPrivateMember(n.argument)) {
            const target = n.argument as e.MemberExpression;
            const info = this.resolvePrivate((target.property as e.PrivateIdentifier).name, target.loc);
            const obj = this.visitAs(target.object as e.Expression);
            const one = b.literal(1);
            return this.privReadModifyWrite(
                obj,
                info,
                (old) => b.binaryExpression(old, n.operator === "++" ? "+" : "-", one),
                !n.prefix,
                n.loc
            );
        }
        return super.visitUpdateExpression(n);
    }

    override visitBinaryExpression(n: e.BinaryExpression): VisitResult {
        // ergonomic brand check: #x in obj
        if (n.operator === "in" && (n.left as e.Node).type === "PrivateIdentifier") {
            const info = this.resolvePrivate((n.left as e.PrivateIdentifier).name, n.loc);
            const mapId = info.kind === "field" ? info.mapId! : info.brandId!;
            return intrinsic(privHas_id, [refOf(mapId), this.visitAs(n.right)]);
        }
        return super.visitBinaryExpression(n);
    }

    override visitCallExpression(n: e.CallExpression): VisitResult {
        // obj.#m(args): brand-check/read, then call with obj as receiver
        if (this.isPrivateMember(n.callee)) {
            const callee = n.callee as e.MemberExpression;
            const info = this.resolvePrivate((callee.property as e.PrivateIdentifier).name, callee.loc);
            const obj = this.visitAs(callee.object as e.Expression);
            const args = this.visitArray(n.arguments);
            if (info.kind === "method") {
                // %fn.call(%privBrandCheck(brand, obj, name), args) — obj
                // evaluates exactly once inside the check
                return b.callExpression(b.memberExpression(refOf(info.fnId!), call_id), [
                    intrinsic(privBrandCheck_id, [refOf(info.brandId!), obj, privNameLit(info)]),
                    ...args,
                ]);
            }
            // field / accessor: the receiver is needed twice (read + call)
            const o = freshPriv("o");
            const fnValue = this.privRead(refOf(o), info);
            const callExpr = b.callExpression(b.memberExpression(fnValue, call_id), [
                refOf(o),
                ...args,
            ]);
            return b.callExpression(
                b.arrowFunctionExpression([o], b.blockStatement([b.returnStatement(callExpr)])),
                [obj]
            );
        }
        if (n.callee.type === "Super") {
            const method = this.method_stack.top;
            if (this.nameOfKey(method.key as e.Expression) !== "constructor") {
                reportError(
                    SyntaxError,
                    "calls to super() are only allowable in constructors.",
                    this.filename,
                    n.callee.loc ?? undefined
                );
            }

            const super_ref = createSuperReference(method.static === true, method.key as e.Expression);
            n.callee = constructSuper_id;
            n.arguments.unshift(super_ref);
        } else if (n.callee.type === "MemberExpression" && n.callee.object.type === "Super") {
            // super.foo(...) / super[k](...): the target is looked up on
            // %super(.prototype) under the CALLEE's key (not the enclosing
            // method's) and runs with this's this
            const method = this.method_stack.top;
            const callee = n.callee;
            if (callee.computed) callee.property = this.visitAs(callee.property as e.Expression);
            const super_base = createSuperReference(method.static === true);
            const super_ref = b.memberExpression(super_base, callee.property as e.Expression);
            super_ref.computed = callee.computed;
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
            if (property.type === "SpreadElement") {
                // still a SpreadElement here: DesugarSpread runs later
                property.argument = this.visitAs(property.argument);
                continue;
            }
            if (property.computed) property.key = this.visitAs(property.key);
            property.value = this.visitAs(property.value);
        }
        return n;
    }

    override visitSuper(): VisitResult {
        return createSuperReference(this.method_stack.top.static === true);
    }

    override visitClassDeclaration(n: e.ClassDeclaration): VisitResult {
        if (!n.id) {
            // only `export default class {}` lacks an id — NamedEvaluation
            // gives it the name "default"
            (n as unknown as Record<string, unknown>)["ejs_ctor_display_name"] = "default";
            n.id = freshClassId();
        }
        n.superClass = this.visitNullable(n.superClass);
        const iife = this.generateClassIIFE(n);
        return b.letDeclaration(n.id, b.callExpression(iife, n.superClass ? [n.superClass] : []));
    }

    override visitClassExpression(n: e.ClassExpression): VisitResult {
        if (!n.id) {
            // NamedEvaluation: the parser stamps ejs_display_name when the
            // class sits in a naming position; otherwise .name is ""
            const rec = n as unknown as Record<string, unknown>;
            rec["ejs_ctor_display_name"] = rec["ejs_display_name"] ?? "";
            n.id = freshClassId();
        }
        n.superClass = this.visitNullable(n.superClass);
        const iife = this.generateClassIIFE(n as NamedClass);
        return b.callExpression(iife, n.superClass ? [n.superClass] : []);
    }

    private generateClassIIFE(n: NamedClass): e.FunctionExpression {
        // ---- partition the class body ------------------------------------
        // public methods go through the ES6 machinery below; fields,
        // private members, and static blocks desugar separately
        const priv_method_elements: e.MethodDefinition[] = [];
        const instance_fields: e.PropertyDefinition[] = [];
        // static fields and static blocks, in declaration order
        const static_elements: (e.PropertyDefinition | e.StaticBlock)[] = [];
        for (const el of n.body.body) {
            if (el.type === "MethodDefinition") {
                if ((el.key as e.Node).type === "PrivateIdentifier") priv_method_elements.push(el);
            } else if (el.type === "PropertyDefinition") {
                if (el.static) static_elements.push(el);
                else instance_fields.push(el);
            } else if (el.type === "StaticBlock") {
                static_elements.push(el);
            }
        }

        // ---- this class's private scope ----------------------------------
        const privScope = new Map<string, PrivInfo>();
        let brandId: e.Identifier | null = null;
        let instanceBrand = false;
        let staticBrand = false;
        for (const el of priv_method_elements) {
            const name = (el.key as e.PrivateIdentifier).name;
            if (!brandId) brandId = freshPriv("brand");
            if (el.static) staticBrand = true;
            else instanceBrand = true;
            let info = privScope.get(name);
            if (!info) {
                info = {
                    kind: el.kind === "get" || el.kind === "set" ? "accessor" : "method",
                    name,
                    brandId,
                };
                privScope.set(name, info);
            }
            if (el.kind === "get") info.getFnId = freshPriv(`get_${name}`);
            else if (el.kind === "set") info.setFnId = freshPriv(`set_${name}`);
            else info.fnId = freshPriv(`m_${name}`);
        }
        for (const el of [...instance_fields, ...static_elements]) {
            if (el.type === "PropertyDefinition" && (el.key as e.Node).type === "PrivateIdentifier") {
                const name = (el.key as e.PrivateIdentifier).name;
                privScope.set(name, { kind: "field", name, mapId: freshPriv(`f_${name}`) });
            }
        }

        // visit all the member functions/initializers so that 'super' is
        // replaced with '%super' and private references are rewritten; the
        // scope must be active for every nested expression
        this.private_scopes.push(privScope);

        const dummyContext = (is_static: boolean): e.MethodDefinition =>
            ({
                type: "MethodDefinition",
                key: b.identifier("%fieldinit"),
                value: null,
                kind: "method",
                computed: false,
                static: is_static,
            }) as unknown as e.MethodDefinition;

        for (const el of n.body.body) {
            if (el.type === "MethodDefinition") {
                this.method_stack.push(el);
                el.value = this.visitAs(el.value);
                this.method_stack.pop();
            } else if (el.type === "PropertyDefinition") {
                this.method_stack.push(dummyContext(el.static));
                if (el.computed) el.key = this.visitAs(el.key);
                if (el.value) el.value = this.visitAs(el.value);
                this.method_stack.pop();
            } else if (el.type === "StaticBlock") {
                this.method_stack.push(dummyContext(true));
                el.body = this.visitArray(el.body);
                this.method_stack.pop();
            }
        }

        let class_init_iife_body: e.Statement[] = [];

        const { properties, methods, sproperties, smethods } = this.gather_members(n);

        // ---- private declarations + field initializer functions ----------
        // computed field keys evaluate once, at class-definition time
        const privDecls: e.Statement[] = [];
        if (brandId) privDecls.push(b.letDeclaration(brandId, intrinsic(makePrivateMap_id, [])));
        privScope.forEach((info) => {
            if (info.kind === "field")
                privDecls.push(b.letDeclaration(info.mapId!, intrinsic(makePrivateMap_id, [])));
        });
        for (const el of priv_method_elements) {
            const privName = (el.key as e.PrivateIdentifier).name;
            const info = privScope.get(privName)!;
            const fnId = el.kind === "get" ? info.getFnId! : el.kind === "set" ? info.setFnId! : info.fnId!;
            // spec .name: "#m" (accessors: "get #m"/"set #m")
            const prefix = el.kind === "get" || el.kind === "set" ? `${el.kind} ` : "";
            (el.value as unknown as Record<string, unknown>)["ejs_display_name"] = `${prefix}#${privName}`;
            privDecls.push(b.letDeclaration(fnId, el.value));
        }

        // one statement per field, into the given initializer body; `this`
        // is the instance (or the class, for statics)
        const fieldStmt = (el: e.PropertyDefinition): e.Statement => {
            const value = el.value ?? b.undefinedLit();
            if ((el.key as e.Node).type === "PrivateIdentifier") {
                const info = privScope.get((el.key as e.PrivateIdentifier).name)!;
                return b.expressionStatement(
                    intrinsic(privFieldInit_id, [refOf(info.mapId!), b.thisExpression(), value])
                );
            }
            let keyExpr: e.Expression;
            if (el.computed) {
                const kt = freshPriv("fldkey");
                privDecls.push(b.letDeclaration(kt, el.key as e.Expression));
                keyExpr = refOf(kt);
            } else {
                keyExpr = b.literal(this.nameOfKey(el.key as e.Expression));
            }
            return b.expressionStatement(
                intrinsic(defineField_id, [b.thisExpression(), keyExpr, value])
            );
        };

        let initFieldsId: e.Identifier | null = null;
        if (instance_fields.length > 0 || instanceBrand) {
            initFieldsId = freshPriv("initFields");
            const stmts: e.Statement[] = [];
            if (instanceBrand)
                stmts.push(
                    b.expressionStatement(
                        intrinsic(privFieldInit_id, [refOf(brandId!), b.thisExpression(), b.literal(true)])
                    )
                );
            for (const el of instance_fields) stmts.push(fieldStmt(el));
            privDecls.push(
                b.letDeclaration(initFieldsId, b.functionExpression(null, [], b.blockStatement(stmts)))
            );
        }

        let initStaticsId: e.Identifier | null = null;
        if (static_elements.length > 0 || staticBrand) {
            initStaticsId = freshPriv("initStatics");
            const stmts: e.Statement[] = [];
            if (staticBrand)
                stmts.push(
                    b.expressionStatement(
                        intrinsic(privFieldInit_id, [refOf(brandId!), b.thisExpression(), b.literal(true)])
                    )
                );
            for (const el of static_elements) {
                if (el.type === "PropertyDefinition") stmts.push(fieldStmt(el));
                // a static block keeps its own let/const scope
                else stmts.push(b.blockStatement(el.body));
            }
            privDecls.push(
                b.letDeclaration(initStaticsId, b.functionExpression(null, [], b.blockStatement(stmts)))
            );
        }

        this.private_scopes.pop();

        // a fresh node per value-position use of the class name: n.id
        // itself becomes the OUTER let declarator (visitClassDeclaration),
        // and node-keyed reference resolution must not alias the two scopes
        const cname = () => b.identifier(n.id.name);

        class_init_iife_body.push(
            b.letDeclaration(b.identifier("proto"), b.memberExpression(cname(), prototype_id))
        );

        // private maps/brands, private method closures, field initializer
        // functions — all in the iife's scope, closed over by the methods
        class_init_iife_body.push(...privDecls);

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

        // fields initialize on construction: at the top of a base-class
        // ctor, or right after super() returns in a derived one.  the
        // injected AST is already in post-visit form.
        if (initFieldsId) {
            const mkInitCall = () =>
                b.expressionStatement(
                    b.callExpression(b.memberExpression(refOf(initFieldsId!), call_id), [
                        b.thisExpression(),
                    ])
                );
            if (n.superClass) this.insertAfterSuperCalls(ctor.value.body, mkInitCall);
            else ctor.value.body.body.unshift(mkInitCall());
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

        // static fields and static blocks run last, `this` = the class
        if (initStaticsId) {
            class_init_iife_body.push(
                b.expressionStatement(
                    b.callExpression(b.memberExpression(refOf(initStaticsId), call_id), [cname()])
                )
            );
        }

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

    // walk the (post-visit) ctor body and insert mkStmt() after every
    // desugared super() call statement.  covers super() in nested blocks,
    // ifs, loops, try and switch; a super() captured in an arrow is not
    // found (rare enough to accept — fields would not initialize there).
    private insertAfterSuperCalls(block: e.BlockStatement, mkStmt: () => e.Statement): void {
        const walkStmts = (stmts: e.Statement[]): void => {
            for (let i = 0; i < stmts.length; i++) {
                const s = stmts[i]!;
                if (isSuperCallStatement(s)) {
                    stmts.splice(i + 1, 0, mkStmt());
                    i++;
                    continue;
                }
                walkStmt(s);
            }
        };
        const intoBlock = (s: e.Statement): e.Statement => {
            // a bare super(); as an if/loop body needs a block to append into
            if (isSuperCallStatement(s)) return b.blockStatement([s, mkStmt()]);
            walkStmt(s);
            return s;
        };
        const walkStmt = (s: e.Statement): void => {
            switch (s.type) {
                case "BlockStatement":
                    walkStmts(s.body);
                    break;
                case "IfStatement":
                    s.consequent = intoBlock(s.consequent);
                    if (s.alternate) s.alternate = intoBlock(s.alternate);
                    break;
                case "WhileStatement":
                case "DoWhileStatement":
                case "ForStatement":
                case "ForInStatement":
                case "ForOfStatement":
                    s.body = intoBlock(s.body);
                    break;
                case "LabeledStatement":
                    s.body = intoBlock(s.body);
                    break;
                case "TryStatement":
                    walkStmts(s.block.body);
                    if (s.handlers) for (const h of s.handlers) walkStmts(h.body.body);
                    if (s.finalizer) walkStmts(s.finalizer.body);
                    break;
                case "SwitchStatement":
                    for (const c of s.cases) walkStmts(c.consequent);
                    break;
                default:
                    break;
            }
        };
        walkStmts(block.body);
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
            // fields, static blocks and private methods desugar separately
            // in generateClassIIFE; only public methods go through here
            if (class_element.type !== "MethodDefinition") continue;
            if ((class_element.key as e.Node).type === "PrivateIdentifier") continue;
            const class_element_name = this.nameOfKey(class_element.key as e.Expression);
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
                const prop_key = class_element.computed ? (class_element.key as e.Expression) : class_element_name;

                let entry = property_map.get(prop_key);
                if (!entry) {
                    entry = { computed: class_element.computed === true };
                    property_map.set(prop_key, entry);
                }

                if (entry[class_element.kind])
                    reportError(
                        SyntaxError,
                        `a '${class_element.kind}' method for '${this.nameOfKey(
                            class_element.key as e.Expression
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
        const fd = b.functionDeclaration(
            b.identifier(ast_class.id.name),
            ast_method.value.params,
            ast_method.value.body,
            ast_method.value.defaults
        );
        // an originally-anonymous class carries its NamedEvaluation name
        // (possibly "") — without this the synthesized %anonClass_N id
        // leaks into .name
        const dn = (ast_class as unknown as Record<string, unknown>)["ejs_ctor_display_name"];
        if (dn !== undefined) (fd as unknown as Record<string, unknown>)["ejs_display_name"] = dn;
        return fd;
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
        const method_name = this.nameOfKey(ast_method.key as e.Expression);
        const method_key = ast_method.computed ? (ast_method.key as e.Expression) : b.literal(method_name);
        const method = b.functionExpression(
            b.identifier(`${ast_class.id.name}:${method_name}`),
            ast_method.value.params,
            ast_method.value.body,
            ast_method.value.defaults
        );
        // b.functionExpression hardcodes generator: false — losing the
        // flag here left `*method() {}` yields undesugared
        method.generator = ast_method.value.generator;
        // the qualified id is the LLVM symbol; .name is the bare key
        if (!ast_method.computed)
            (method as unknown as Record<string, unknown>)["ejs_display_name"] = method_name;

        const Object_defineProperty = b.memberExpression(Object_id, defineProperty_id);
        // spec method attributes: writable, non-enumerable, configurable
        const defineProperty_args = b.objectExpression([
            b.property(value_id, method),
            b.property(b.identifier("writable"), b.literal(true)),
            b.property(enumerable_id, b.literal(false)),
            b.property(b.identifier("configurable"), b.literal(true)),
        ]);
        return b.expressionStatement(
            b.callExpression(Object_defineProperty, [freshProto(), method_key, defineProperty_args])
        );
    }

    private create_static_method(
        ast_method: e.MethodDefinition,
        ast_class: NamedClass
    ): e.Statement {
        const method_name = this.nameOfKey(ast_method.key as e.Expression);
        const method_key = ast_method.computed ? (ast_method.key as e.Expression) : b.literal(method_name);
        const method = b.functionExpression(
            ast_method.key.type === "Identifier" ? ast_method.key : null,
            ast_method.value.params,
            ast_method.value.body,
            ast_method.value.defaults
        );
        method.generator = ast_method.value.generator;

        const Object_defineProperty = b.memberExpression(Object_id, defineProperty_id);
        // spec method attributes: writable, non-enumerable, configurable
        const defineProperty_args = b.objectExpression([
            b.property(value_id, method),
            b.property(b.identifier("writable"), b.literal(true)),
            b.property(enumerable_id, b.literal(false)),
            b.property(b.identifier("configurable"), b.literal(true)),
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
                key = getter.key as e.Expression;
            }
            if (setter) {
                accessors.push(b.property(set_id, setter.value));
                key = setter.key as e.Expression;
            }
            // spec accessor attributes: non-enumerable (the default here),
            // configurable
            accessors.push(b.property(b.identifier("configurable"), b.literal(true)));

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
