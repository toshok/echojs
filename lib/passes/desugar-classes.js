/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
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
import * as b from '../ast-builder';
import { setConstructorKindDerived_id, setConstructorKindBase_id, superid, constructSuper_id, constructSuperApply_id, setPrototypeOf_id, objectCreate_id, call_id, prototype_id, constructor_id, get_id, set_id, Object_id, proto_id, defineProperty_id, enumerable_id, value_id, defineProperties_id } from '../common-ids';
import { Stack } from '../stack-es6';
import { reportError } from '../errors';
import { TransformPass } from '../node-visitor';
import { intrinsic, startGenerator } from '../echo-util';

import * as escodegen from '../../external-deps/escodegen/escodegen-es6';

function createSuperReference (is_static, id) {
    if (id && id.name === 'constructor')
        return superid;

    let obj = is_static ? superid : b.memberExpression(superid, prototype_id);

    if (!id)
        return obj;
    
    return b.memberExpression(obj, id);
}

let classgen = startGenerator();
function freshClassId () {
    return b.identifier(`%anonClass_${classgen()}`);
}

export class DesugarClasses extends TransformPass {
    constructor (options) {
        super(options);
        this.class_stack = new Stack;
        this.method_stack = new Stack;
    }

    visitCallExpression (n) {
        if (n.callee.type === b.Super) {
            if (this.method_stack.top.key.name !== 'constructor') {
                reportError(SyntaxError, 'calls to super() are only allowable in constructors.', this.filename, n.callee.loc);
            }

            let super_ref = createSuperReference(this.method_stack.top.static, this.method_stack.top.key);
            n.callee = constructSuper_id;
            n.arguments.unshift(super_ref);
        }
        else if (n.callee.type === b.MemberExpression && n.callee.object.type === b.Super) {
            let super_ref = createSuperReference(this.method_stack.top.static, this.method_stack.top.key);
            n.callee = b.memberExpression(super_ref, call_id);
            n.arguments.unshift(b.thisExpression());
        }
        else {
            n.callee = this.visit(n.callee);
        }
        n.arguments = this.visitArray(n.arguments);
        return n;
    }

    visitNewExpression (n) {
        n.callee = this.visit(n.callee);
        n.arguments = this.visitArray(n.arguments);
        return n;
    }

    visitObjectExpression (n) {
        for (let property of n.properties) {
            if (property.computed)
                property.key = this.visit(property.key);
            property.value = this.visit(property.value);
        }
        return n;
    }

    visitSuper () {
        return createSuperReference(this.method_stack.top.static);
    }

    visitClassDeclaration (n) {
        if (!n.id) n.id = freshClassId();
        n.superClass = this.visit(n.superClass);
        let iife = this.generateClassIIFE(n);
        return b.letDeclaration(n.id, b.callExpression(iife, n.superClass ? [n.superClass] : []));
    }
    
    visitClassExpression (n) {
        if (!n.id) n.id = freshClassId();
        n.superClass = this.visit(n.superClass);
        let iife = this.generateClassIIFE(n);
        return b.callExpression(iife, n.superClass ? [n.superClass] : []);
    }

    generateClassIIFE (n) {
        // we visit all the functions defined in the class so that 'super' is replaced with '%super'
        this.class_stack.push(n);

        // XXX this push/pop should really be handled in this.visitMethodDefinition
        for (let class_element of n.body.body) {
            this.method_stack.push(class_element);
            class_element.value = this.visit(class_element.value);
            this.method_stack.pop();
        }
        
        this.class_stack.pop();
        
        let class_init_iife_body = [];

        let [properties, methods, sproperties, smethods] = this.gather_members(n);

        class_init_iife_body.push(b.letDeclaration(b.identifier('proto'), b.memberExpression(n.id, prototype_id)));

        let ctor = null;
        methods.forEach ((m, mkey) => {
            // if it's a method with name 'constructor' output the special ctor function
            if (mkey === 'constructor') {
                ctor = m;
            }
            else {
                class_init_iife_body.push(this.create_proto_method(m, n));
            }
        });
        smethods.forEach((sm) => class_init_iife_body.push(this.create_static_method(sm, n)));

        let proto_props = this.create_properties(properties, n, false);
        if (proto_props)
            class_init_iife_body = class_init_iife_body.concat(proto_props);
        
        let static_props = this.create_properties(sproperties, n, true);
        if (static_props)
            class_init_iife_body = class_init_iife_body.concat(static_props);

        // generate and prepend a default ctor if there isn't one declared.
        // It looks like this in code:
        //   function Subclass (...args) { %super.call(this, args...); }
        if (!ctor) {
            ctor = this.create_default_constructor(n);

            // we didn't visit it above, so do it now
            this.method_stack.push(ctor);
            ctor.value = this.visit(ctor.value);
            this.method_stack.pop();
        }

        let ctor_func = this.create_constructor(ctor, n);
        if (n.superClass) {
            class_init_iife_body.unshift(b.expressionStatement(b.assignmentExpression(b.memberExpression(b.memberExpression(n.id, prototype_id), constructor_id), '=', n.id)));

            // also set ctor.prototype = Object.create(superClass.prototype)
            class_init_iife_body.unshift(b.expressionStatement(b.callExpression(setPrototypeOf_id, [b.memberExpression(ctor_func.id, prototype_id), b.callExpression(objectCreate_id, [b.memberExpression(superid, prototype_id)])])));
            
            // 14.5.17 step 9, make sure the constructor's __proto__ is set to superClass
            class_init_iife_body.unshift(b.expressionStatement(b.callExpression(setPrototypeOf_id, [ctor_func.id, superid])));

            class_init_iife_body.unshift(b.expressionStatement(intrinsic(setConstructorKindDerived_id, [ctor_func.id])));
        }
        else {
            class_init_iife_body.unshift(b.expressionStatement(intrinsic(setConstructorKindBase_id, [ctor_func.id])));
        }

        class_init_iife_body.unshift(ctor_func);

        // make sure we return the function from our iife
        class_init_iife_body.push(b.returnStatement(n.id));

        // (function (%super?) { ... })
        let iife_body = b.blockStatement(class_init_iife_body, n.loc);
        return b.functionExpression(b.identifier(`${n.id.name || 'anonclass'}_iife`), (n.superClass ? [superid] : []), iife_body, [], null, n.loc);
    }

    gather_members (ast_class) {
        let methods     = new Map;
        let smethods    = new Map;
        let properties  = new Map;
        let sproperties = new Map;

        for (let class_element of ast_class.body.body) {
            let class_element_name = this.nameOfKey(class_element.key);
            if (class_element.static && class_element_name === 'prototype')
                reportError(SyntaxError, 'Illegal method name "prototype" on static class member.', this.filename, class_element.loc);

            if (class_element.kind === 'method' || class_element.kind === 'constructor') {
                // a method
                let method_map = class_element.static ? smethods : methods;
                if (method_map.has(class_element_name))
                    reportError(SyntaxError, `method '${class_element_name}' has already been defined.`, this.filename, class_element.loc);
                method_map.set(class_element_name, class_element);
            }
            else {
                // a property
                let property_map = class_element.static ? sproperties : properties;

                if (!property_map.has(class_element.key))
                    property_map.set(class_element.key, new Map);

                if (property_map.get(class_element.key).has(class_element.kind))
                    reportError(SyntaxError, `a '${class_element.kind}' method for '${escodegen.generate(class_element.key)}' has already been defined.`, this.filename, class_element.loc);

                if (class_element.kind === 'set') {
                    if (class_element.value.params.length > 0) {
                        let last_param = class_element.value.params[class_element.value.params.length-1];
                        if (last_param.type == b.RestElement)
                            reportError(SyntaxError, 'Setters are not allowed to have a rest', this.filename, last_param.loc);
                    }
                }

                // XXX this doesn't work for properties where one accessor is computed and the other isn't...
                let computed = class_element.computed;

                if (property_map.get(class_element.key).has('computed')) {
                    if (computed != property_map.get(class_element.key).get('computed'))
                        reportError(Error, 'unsupported mismatch computed state for property accessors', this.filename, class_element.loc);
                }
                
                property_map.get(class_element.key).set(class_element.kind, class_element);

                property_map.get(class_element.key).set('computed', computed);
            }
        }

        return [properties, methods, sproperties, smethods];
    }
    
    
    create_constructor (ast_method, ast_class) {
        return b.functionDeclaration(ast_class.id, ast_method.value.params, ast_method.value.body, ast_method.value.defaults, ast_method.value.rest);
    }

    create_default_constructor (ast_class) {
        // splat args into the call to super's ctor if there's a superclass
        let args_id = b.identifier('args');
        let functionBody = b.blockStatement(ast_class.superClass ? [b.expressionStatement(intrinsic(constructSuperApply_id, [superid, args_id]))] : []);
        return b.methodDefinition(constructor_id, b.functionExpression(null, [b.restElement(args_id)], functionBody, []));
    }
    
    nameOfKey(key) {
        return key.type == b.Identifier ? key.name : key.value;
    }

    create_proto_method (ast_method, ast_class) {
        let method_name = this.nameOfKey(ast_method.key);
        let method_key = ast_method.computed ? ast_method.key : b.literal(method_name);
        let method = b.functionExpression(b.identifier(`${ast_class.id.name}:${method_name}`), ast_method.value.params, ast_method.value.body, ast_method.value.defaults, ast_method.value.rest);

        let Object_defineProperty = b.memberExpression(Object_id, defineProperty_id);
        let defineProperty_args = b.objectExpression([b.property(value_id, method), b.property(enumerable_id, b.literal(false))]);
        return b.expressionStatement(b.callExpression(Object_defineProperty, [proto_id, method_key, defineProperty_args]));
    }

    create_static_method (ast_method, ast_class) {
        let method_name = this.nameOfKey(ast_method.key);
        let method_key = ast_method.computed ? ast_method.key : b.literal(method_name);
        let method = b.functionExpression(ast_method.key, ast_method.value.params, ast_method.value.body, ast_method.value.defaults, ast_method.value.rest);

        let Object_defineProperty = b.memberExpression(Object_id, defineProperty_id);
        let defineProperty_args = b.objectExpression([b.property(value_id, method), b.property(enumerable_id, b.literal(false))]);
        return b.expressionStatement(b.callExpression(Object_defineProperty, [ast_class.id, method_key, defineProperty_args]));
    }

    create_properties (properties, ast_class, are_static) {
        let propdescs = [];

        properties.forEach((prop_map, prop) => {
            let accessors = [];
            let key = null;

            let getter = prop_map.get('get');
            let setter = prop_map.get('set');

            if (getter) {
                accessors.push(b.property(get_id, getter.value));
                key = prop;
            }
            if (setter) {
                accessors.push(b.property(set_id, setter.value));
                key = prop;
            }

            propdescs.push(b.property(key, b.objectExpression(accessors), 'init', prop_map.get('computed') == true));
        });

        if (propdescs.length === 0) return null;

        let propdescs_literal = b.objectExpression(propdescs);

        let target;
        if (are_static)
            target = ast_class.id;
        else
            target = b.identifier('proto');

        return b.expressionStatement(b.callExpression(b.memberExpression(Object_id, defineProperties_id), [target, propdescs_literal]));
    }
}
