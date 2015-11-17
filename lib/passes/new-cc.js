/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import * as escodegen from '../../external-deps/escodegen/escodegen-es6';

import * as b from '../ast-builder';

import * as debug from '../debug';

import { reportError, reportWarning } from '../errors';

import { createGlobalsInterface } from '../runtime';

let runtime_globals = createGlobalsInterface(null);

import { Stack } from '../stack-es6';
import { TransformPass, TreeVisitor } from '../node-visitor';

import { genGlobalFunctionName,
         genAnonymousFunctionName,
         shallow_copy_object,
         map,
         foldl,
         reject,
         is_intrinsic,
         is_string_literal,
         intrinsic,
         startGenerator } from '../echo-util';

let hasOwnProperty = Object.prototype.hasOwnProperty;

import { arrayFromSpread_id,
         constructSuper_id,
         constructSuperApply_id,
         getGlobal_id,
         getLocal_id,
         invokeClosure_id,
         constructClosure_id,
         makeAnonClosure_id,
         makeClosureEnv_id,
         makeClosure_id,
         makeClosureNoEnv_id,
         setGlobal_id,
         setLocal_id,
         moduleGetSlot_id,
         moduleSetSlot_id,
         moduleGetExotic_id,
         setSlot_id,
         slot_id } from '../common-ids';

function assignStmt(l, op, r) {
    return b.expressionStatement(b.assignmentExpression(l, op, r));
}

function slotIntrinsic(name, slot) {
    return intrinsic(slot_id, [b.identifier(name), b.literal(slot)]);
}

function setSlotIntrinsic(name, slot, value) {
    try {
        return intrinsic(setSlot_id, [b.identifier(name), b.literal(slot), value]);
    }
    catch(e) {
        console.log("invalid setSlot intrinsic:");
        console.log(`name = ${name}`);
        console.log(`slot = ${slot}`);
        console.log(`value = ${JSON.stringify(value)}`);
        return null;
    }
}
        

function is_getset_intrinsic(n) {
    if (!is_intrinsic(n)) return false;
    if (n.callee.name === slot_id.name    || n.callee.name === getLocal_id.name || n.callee.name === getGlobal_id.name) return true;
    if (n.callee.name === setSlot_id.name || n.callee.name === setLocal_id.name || n.callee.name === setGlobal_id.name) return true;
    return false;
}

class Location {
        constructor(block, func) {
            this.block = block;
            this.func = func;
        }
}

// figure out a better way to make a private static
let scope_id = 0;

class Scope {
    constructor (location) {
        this.location = location;
        this.bindings = new Map();    // the identifiers declared in this scope, mapping from string(name) -> Binding
        this.referents = new Map();   // the references (rooted in other scopes) to identifiers declared in this scope, mapping from string(name) -> [Reference]
        this.references = new Map();  // the references rooted in this scope, mapping from string(name) -> [Reference]
        this.scope_id = scope_id;
        this.parentScope = null;
        this.children = [];
        scope_id += 1;
    }
                
    addBinding(binding) {
        this.bindings.set(binding.name, binding);
        binding.declaringScope = this;
    }
    getBinding(name) { return this.bindings.get(name); }
    hasBinding(name) { return this.bindings.has(name); }

    addReferent(ref) {
        let reflist = this.referents.get(ref.binding.name);
        if (!reflist) {
            reflist = [];
            this.referents.set(ref.binding.name, reflist);
        }
        reflist.push(ref);
    }
    getReferents(name) { return this.referents.get(name); }
    hasReferents(name) { return this.referents.has(name); }
                
    addReference(ref) {
        this.references.set(ref.binding.name, ref);
        ref.referencingScope = this;
        if (ref.binding.type === 'local' || ref.binding.type === 'arg')
            ref.binding.declaringScope.addReferent(ref);
    }
    getReference(name) { return this.references.get(name); }
    hasReference(name) { return this.references.has(name); }

    isFunctionBodyScope() { return this.location.block === this.location.func.body; }
    isAncestorOf(s) {
        let _s = this.parentScope;
        while (_s) {
            if (_s === s) return true;
            _s = _s.parentScope;
        }
        return false;
    }
                
    differentFunction(otherscope) { return this.location.func !== otherscope.location.func; }

    debugString() {
        let str = `scope `;
        if (this.location.block.loc)
            str = `#{str} at line ${this.location.block.loc.start.line}`;
        if (this.isFunctionBodyScope())
            str = `${str} : for function ${this.location.func.id.name}`;
        if (this.env)
            str = `${str} : environment = ${this.env.name}`;
        return str;
    }
}        

class Binding {
    constructor(name, type, is_const) {
        this.name = name;
        this.type = type;
        this.is_const = is_const;
    }
}

class LocalBinding extends Binding {
    constructor(name, is_const) {
        super(name, 'local', is_const);
    }
}

class GlobalBinding extends Binding {
    constructor(name, is_const) {
        super(name, 'global', is_const);
    }
}

class ModuleSlotBinding extends Binding {
    constructor(moduleString, moduleExport, name, is_const) {
        super(name, 'module', is_const);
        this.moduleString = moduleString;
        this.moduleExport = moduleExport;
    }
                
    getLoadIntrinsic() {
        return intrinsic(moduleGetSlot_id, [this.moduleString, this.moduleExport]);
    }
    getStoreIntrinsic(val) {
        return intrinsic(moduleSetSlot_id, [this.moduleString, this.moduleExport, val]);
    }

    toString() {
        return `moduleSlot(${this.moduleString.value} - ${this.moduleExport.value})`;
    }
}

class ModuleExoticBinding extends Binding {
    constructor(moduleString, name, is_const) {
        super(name, 'module-exotic', is_const);
        this.moduleString = moduleString;
    }
                
    getLoadIntrinsic() {
        return intrinsic(moduleGetExotic_id, [this.moduleString]);
    }
    // no store intrinsic
}


class Reference {
    constructor(binding) {
        this.binding = binding;
    }
}

class Environment {
    constructor(id, level) {
        this.id = id;
        this.level = level;
        this.name = `%env_${this.id}`;
        this.slot_map = new Map();
        this.parentEnv = null;
    }

    hasSlots() { return this.slot_map.size > 0; }
    hasSlot(name) { return this.slot_map.has(name); }
    getSlot(name) {
        let rv = this.slot_map.get(name);
        if (rv === undefined)
            throw new Error(`environment ${this.name} does not contain slot for ${name}`);
        return rv;
    }

    addSlot(name) {
        if (this.slot_map.has(name)) return ;
        this.slot_map.set(name, this.slot_map.size);
    }

    slotCount() { return this.slot_map.size; }

    addChild(env) {
        if (!env) throw new Error("invalid null child");
        if (env.parentEnv && env.parentEnv !== this)
            throw new Error(`attempting to set parent of ${env.name} to ${this.name}, but it already has a a parent, ${env.parentEnv.name}`);
        env.addSlot(this.name);
        env.parentEnv = this;
    }

    toString() { return this.name; }
}

let allFunctions = [];

let global_bindings = new Map();

class SubstituteVariables extends TransformPass {
    constructor(options, filename, allModules) {
        super(options, filename);
        this.allModules = allModules;
        this.filename = filename;
        this.options = options;
        this.current_scope = null;
    }
                
    visitBlock(n) {
        this.current_scope = n.scope;
        super.visitBlock(n);
        this.current_scope = this.current_scope.parentScope;
        return n;
    }

    env_name()     { return this.current_scope.env.name; }
    env_slot(name) { return this.current_scope.env.getSlot(name); }

    visitVariableDeclaration(n) {
        if (n.declarations.length > 1)
            throw new Error("VariableDeclarations should only have 1 declarator at this point");
        let decl = n.declarations[0];

        decl.init = this.visit(decl.init);
        // don't visit the id

        let referents = this.current_scope.getReferents(decl.id.name);
        if (referents) {
            for (let referent of referents) {
                if (referent.referencingScope.differentFunction(this.current_scope)) {
                    // it's closed over, so we need to set it in our allocated environment
                    return b.expressionStatement(setSlotIntrinsic(this.env_name(), this.env_slot(decl.id.name), decl.init || b.undefinedLit()));
                }
            }
        }
        return n;
    }

    visitCallExpression (n) {
        // if it's one of our get/set Slot/Local/Global intrinsics, bail
        if (is_getset_intrinsic(n)) return n;

        // otherwise we need to visit the args
        if (is_intrinsic(n)) {
            n.arguments = this.visit(n.arguments);
            if (is_intrinsic(n, constructSuperApply_id.name))
                this.current_scope.location.func.scratch_size = Math.max(this.current_scope.location.func.scratch_size, n.arguments.length + 1);
            else if (is_intrinsic(n, constructSuper_id.name))
                this.current_scope.location.func.scratch_size = Math.max(this.current_scope.location.func.scratch_size, n.arguments.length + 1);
            else if (is_intrinsic(n, arrayFromSpread_id.name))
                this.current_scope.location.func.scratch_size = Math.max(this.current_scope.location.func.scratch_size, n.arguments.length + 1);
            return n;
        }

        n = super.visitCallExpression(n);
        this.current_scope.location.func.scratch_size = Math.max(this.current_scope.location.func.scratch_size, n.arguments.length + 1);
        let rv = intrinsic(invokeClosure_id, [n.callee].concat(n.arguments));
        rv.loc = n.loc;
        return rv;
    }

    visitNewExpression(n) {
        n = super.visitNewExpression(n);

        this.current_scope.location.func.scratch_size = Math.max(this.current_scope.location.func.scratch_size, n.arguments.length + 1);

        let rv = intrinsic(constructClosure_id, [n.callee].concat(n.arguments));
        rv.loc = n.loc;
        return rv;
    }

    visitFunction(n) {
        n.scratch_size = 0;
        n.body = this.visit(n.body);

        if (n.toplevel) return n;

        if (n.type === b.FunctionDeclaration)
            throw new Error("there should be no FunctionDeclarations at this point");

        let intrinsic_args = [];
        let intrinsic_id;

        if (n.id) {
            intrinsic_id = makeClosure_id;
            if (n.params[0].name === "%env_unused")
                intrinsic_id = makeClosureNoEnv_id;
            else
                intrinsic_args.push(b.identifier(n.params[0].name, n.loc));

            if (n.id.type === b.Identifier)
                intrinsic_args.push(b.literal(n.id.name));
            else
                intrinsic_args.push(b.literal(escodegen.generate(n.id)));
        }
        else {
            intrinsic_id = makeAnonClosure_id;
            if (n.params[0].name === "%env_unused")
                intrinsic_args.push(b.undefinedLit());
            else
                intrinsic_args.push(b.identifier(n.params[0].name, n.loc));
        }

        intrinsic_args.push(n);
                        
        return intrinsic(intrinsic_id, intrinsic_args);
    }
                                

    visitAssignmentExpression(n) {
        if (n.left.type !== b.Identifier) return super.visitAssignmentExpression(n);

        let rhs = this.visit(n.right);
        let leftname = n.left.name;
                
        let referents = this.current_scope.getReferents(leftname);
        if (referents) {
            for (let referent of referents) {
                if (referent.referencingScope.differentFunction(this.current_scope)) {
                    // it's closed over, so we need to set it in our allocated environment
                    let rv = setSlotIntrinsic(this.env_name(), this.env_slot(leftname), rhs);
                    rv.loc = n.loc;
                    return rv;
                }
            }
        }
        else if (this.current_scope.hasReference(leftname)) {
            let ref = this.current_scope.getReference(leftname);
            if (ref.binding.type === 'local' || ref.binding.type === 'arg') {
                let declaringScope = ref.binding.declaringScope;
                let declaringEnv = declaringScope.env;
                if (declaringEnv && declaringEnv.hasSlot(leftname)) {
                    let rv = setSlotIntrinsic(declaringEnv.name, declaringEnv.getSlot(leftname), rhs);
                    rv.loc = n.loc;
                    return rv;
                }
                else {
                    let rv = intrinsic(setLocal_id, [n.left, rhs]);
                    rv.loc = n.loc;
                    return rv;
                }
            }
            else if (ref.binding.type === 'global') {
                if (leftname === "undefined")
                    reportError(SyntaxError, "reassigning 'undefined' not permitted.", this.filename, n.loc);
                let rv = intrinsic(setGlobal_id, [n.left, rhs]);
                rv.loc = n.loc;
                return rv;
            }
            else if (ref.binding.type === 'module') {
                let rv = ref.binding.getStoreIntrinsic(this.visit(rhs));
                rv.loc = n.loc;
                return rv;
            }
            else {
                throw new Error(`unhandled binding type ${ref.binding.type}`);
            }
        }

        let rv = intrinsic(setLocal_id, [n.left, rhs]);
        rv.loc = n.loc;
        return rv;
    }
                                                
    visitIdentifier(n) {
        let referents = this.current_scope.getReferents(n.name);
        if (referents) {
            for (let referent of referents) {
                if (referent.referencingScope.differentFunction(this.current_scope)) {
                    // it's closed over, so we need to set it in our allocated environment
                    return slotIntrinsic(this.env_name(), this.env_slot(n.name));
                }
            }
        }
        else if (this.current_scope.hasReference(n.name)) {
            let ref = this.current_scope.getReference(n.name);
            let binding = ref.binding;
            if (binding.type === 'local' || binding.type === 'arg') {
                let declaringScope = binding.declaringScope;
                let declaringEnv = declaringScope.env;

                if (declaringEnv && declaringEnv.hasSlot(n.name)) {
                    let rv = slotIntrinsic(declaringEnv.name, declaringEnv.getSlot(n.name));
                    rv.loc = n.loc;
                    return rv;
                }
                else {
                    let rv = intrinsic(getLocal_id, [n]);
                    rv.loc = n.loc;
                    return rv;
                }
            }
            else if (binding.type === 'global') {
                let rv = intrinsic(getGlobal_id, [n]);
                rv.loc = n.loc;
                return rv;
            }
            else if (binding.type === 'module') {
                // check if the export is const+literal.  if it is, just propagate it here
                let module_info = this.allModules.get(binding.moduleString.value);
                let export_info = module_info.exports.get(binding.moduleExport.value);
                if (export_info.constval)
                    return export_info.constval;
                return binding.getLoadIntrinsic();
            }
            else if (binding.type === 'module-exotic') {
                let rv = binding.getLoadIntrinsic();
                rv.loc = n.loc;
                return rv;
            }
            else {
                throw new Error(`unhandled binding type ${binding.type}`);
            }
        }
        let rv = intrinsic(getLocal_id, [n]);
        rv.loc = n.loc;
        return rv;
    }

    visitMemberExpression (n) {
        n = super.visitMemberExpression(n);
                
        if (!is_intrinsic(n.object, "%moduleGetExotic"))
            return n;
        if (n.property.type !== b.Identifier && !is_string_literal(n.property))
            return n;

        let moduleString = n.object.arguments[0];
        let moduleExport = n.property.type === b.Identifier ? n.property.name : n.property.raw;
                
        if (moduleString.value[0] === '@') return n;
        if (!this.allModules.has(moduleString.value)) return n;
                
        // we have a member expression where the object is a module
        // exotic and the property is either an identifier or a string
        // literal, both of which we can resolve at compile time.
        //
        // rewrite it to use moduleGetSlot.


        let module_info = this.allModules.get(moduleString.value);
        if (!module_info.exports.has(moduleExport))
            throw new Error(`${moduleString.value} doesn't export ${moduleExport}`); // XXX
        let export_info = module_info.exports.get(moduleExport);

        let rv = intrinsic(moduleGetSlot_id, [moduleString, b.literal(moduleExport)]);
        rv.loc = n.loc;
        return rv;
    }

    visitObjectExpression(n) {
        for (let property of n.properties) {
            if (property.computed)
                property.key = this.visit(property.key);
            property.value = this.visit(property.value);
        }
        return n;
    }

    visitCatchClause(n) {
        // don't visit the parameter here or else we'll try to rewrite it as %get*(param-name)
        n.body = this.visitBlock(n.body);
        return n;
    }
                
    visitLabeledStatement(n) {
        // we need to override this method so we can skip the identifier being used as the label
        n.body  = this.visit(n.body);
        return n;
    }
}

class FlattenDeclarations extends TransformPass {
    constructor(options, filename, allModules) {
        super(options, filename);
        this.filename = filename;
        this.allModules = allModules;
    }

    visitBlock(n) {
        let decl_map = new Map();
        n = super.visitBlock(n, decl_map);
        let new_body = [];
        for (let s of n.body) {
            if (decl_map.has(s))
                new_body = new_body.concat(decl_map.get(s));
            else
                new_body.push(s);
        }
        n.body = new_body;
        return n;
    }
                
    visitVariableDeclaration(n, decl_map) {
        if (n.declarations.length == 1)
            return super.visitVariableDeclaration(n,decl_map);

        let decl_replacement = [];
        for (let decl of n.declarations)
            decl_replacement.push(b.variableDeclaration(n.kind, decl.id, decl.init ? this.visit(decl.init) : b.undefinedLit()));
                
        decl_map.set(n, decl_replacement);
        return n;
    }
}
        
class CollectScopeNestingInfo extends TransformPass {
    constructor(options, filename, allModules) {
        super(options, filename);
        this.options = options;
        this.filename = filename;
        this.allModules = allModules;
        this.block_stack = new Stack();
        this.func_stack = new Stack();
        this.current_scope = null;
        this.root_scope = null;
    }
                
    visitVariableDeclarator(n) {
        // skip the id
        n.init = this.visit(n.init);
        return n;
    }

    doWithScope(scope, fn) {
        scope.parentScope = this.current_scope;
        if (this.current_scope)
            this.current_scope.children.push(scope);
        this.current_scope = scope;
        fn()
        this.current_scope = scope.parentScope;
    }

    doWithBlock(n, fn) {
        this.block_stack.push(n);
        fn();
        this.block_stack.pop();
    }

    doWithFunc(n, fn) {
        this.func_stack.push(n);
        fn();
        this.func_stack.pop();
    }

    createBindingsForScope(block, for_scope) {
        for (let s of block.body) {
            if (s.type === b.VariableDeclaration) {
                // we're guaranteed to have variable declarations with a single declarator by the FlattenDeclarations pass
                let d = s.declarations[0];
                if (d.init) {
                    if (is_intrinsic(d.init, "%moduleGetSlot")) {
                        for_scope.addBinding(new ModuleSlotBinding(d.init.arguments[0], d.init.arguments[1], d.id.name));
                        // we inline module slot loads at all their use points, so we no longer need this decl at all
                        s.type = b.EmptyStatement;
                    }
                    else if (is_intrinsic(d.init, "%moduleGetExotic")) {
                        for_scope.addBinding(new ModuleExoticBinding(d.init.arguments[0], d.id.name));
                    }
                    else {
                        for_scope.addBinding(new LocalBinding(d.id.name, s.kind === 'const'));
                    }
                }
                else {
                    for_scope.addBinding(new LocalBinding(d.id.name, s.kind === 'const'));
                }
            }
            else if (s.type === b.FunctionDeclaration && s.id) {
                for_scope.addBinding(new LocalBinding(s.id.name, false, for_scope));
            }
            else if (s.type === b.ExpressionStatement && is_intrinsic(s.expression, "%moduleSetSlot")) {
                let args = s.expression.arguments;
                for_scope.addBinding(new ModuleSlotBinding(args[0], args[1], args[1].value));
            }
        }
    }
                
    visitBlock(n, initial_bindings) {
        let this_scope = new Scope(new Location(n, this.func_stack.top));
        if (this.root_scope === null)
            this.root_scope = this_scope;

        if (initial_bindings)
            for (let binding of initial_bindings)
                this_scope.addBinding(binding);

        // we have to gather decls before visiting our children
        // so that if they refer to ids in this scope, we can
        // create the proper Reference objects
        this.createBindingsForScope(n, this_scope);

        this.doWithScope(this_scope, () => {
            this.doWithBlock(n, () => {
                super.visitBlock(n);
            });
        });

        Object.defineProperty(n, 'scope', { value: this_scope });
        return n;
    }

    visitFunction(n) {
        //if (n.id)
        //    debug.log(`function ${n.id?.name} has idx of ${allFunctions.length}`);
                
        allFunctions.push(n);
        //param_bindings = (new Binding(p.name, 'arg', false) for p in n.params)
        this.doWithFunc(n, () => {
            n.body = this.visitBlock(n.body); //, param_bindings
        });
        return n;
    }

    visitCatchClause(n) {
        // visit our body with a new local binding for the catch parameter
        n.body = this.visitBlock(n.body, [new LocalBinding(n.param.name, false)]);
        return n;
    }

    find_binding_in_scope(ident) {
        let name = ident.name;
        let s = this.current_scope;
        while (s) {
            if (s.hasBinding(name)) return new Reference(s.getBinding(name));
            s = s.parentScope;
        }

        if (hasOwnProperty.call(runtime_globals, name))
            return new Reference(new GlobalBinding(name));

        if (this.options.warn_on_undeclared) {
            reportWarning(`undeclared identifier: ${ident.name}`, this.filename, ident.loc);
            let binding = global_bindings.get(ident.name);
            if (!binding) {
                binding = new GlobalBinding(ident.name, false);
                global_bindings.set(ident.name, binding);
            }
            return new Reference(binding);
        }
        else {
            reportError(ReferenceError, `undeclared identifier '${ident.name}'`, this.filename, ident.loc);
        }
        return null;
    }

    visitIdentifier(n) {
        this.current_scope.addReference(this.find_binding_in_scope(n));
        return n;
    }

    visitObjectExpression(n) {
        for (let property of n.properties) {
            if (property.computed)
                property.key = this.visit(property.key);
            property.value = this.visit(property.value);
        }
        return n;
    }

    visitCallExpression(n) {
        // if it's one of our get/set Slot/Local/Global intrinsics, bail
        if (is_getset_intrinsic(n)) return n;
                
        // otherwise we need to visit the args
        if (is_intrinsic(n)) {
            n.arguments = this.visit(n.arguments);
            return n;
        }
        return super.visitCallExpression(n);
    }
                
    visitLabeledStatement(n) {
        // we need to override this method so we can skip the identifier being used as the label
        n.body  = this.visit(n.body);
        return n;
    }
}

function placeEnvironments(root_scope) {
    let env_id = 0;
        
    // A map : function -> [environment]
    let func_to_envs = new Map();

    function get_func_envs(f) {
        let func_idx = allFunctions.indexOf(f);
        let func_envs = func_to_envs.get(func_idx);
        if (!func_envs) {
            func_envs = [];
            func_to_envs.set(func_idx, func_envs);
        }
        return func_envs;
    }

    function add_func_env(func_envs, env) {
        if (func_envs[env.level]) {
            if (func_envs[env.level] !== env)
                throw new Error("multiple paths to an environment?  shouldn't be possible.");
        }
        else {
            func_envs[env.level] = env;
        }
    }
                
    function dump_func_envs() {
        func_to_envs.forEach((v,k) => {
            debug.log(`function ${allFunctions[k].id.name} (${k}) requires these environments:`);
            if (!v || v.length === 0) {
                debug.log("  none!");
            }
            else {
                for (var e of v) {
                    if (e) {
                        debug.log(`  env: ${e.name}`);
                    }
                }
            }
        });
    }


    //
    // given an array of arrays of the form:
    //
    // [    ,    , e2 ,    ]
    // [ e0 , e1 ,    ,    ]
    //
    // returns
    //
    // [ e0 , e1 , e2 ,    ]
    //
    // used for merging the func_env arrays returned from children
    // 
    function flatten_func_envs(fe_arr) {
        let rv = [];
        for (let fe of fe_arr) {
            if (fe) {
                for (let idx = 0, ei = fe.length; idx < ei; idx ++) {
                    let e = fe[idx];
                    if (e) rv[idx] = e; // should probably check if !rv[idx] or rv[idx] === e
                }
            }
        }
        return rv;
    }
                
    function dump_scopes(s, level) {
        //debug.log(s.debugString());
        s.children.forEach((c) => {
            //debug.indent();
            dump_scopes(c, level + 1);
            //debug.unindent();
        });
    }

    function walk_scope1(s, level) {
        //
        // Collect external references to bindings defined in this scope.
        //
        //          referent                 reference
        //     s <-------------> binding <---------------> referencingScope
        //
        //debug.log(`dealing with scope from line ${s.location.block.loc.start.line}, environment will be ${env_id}`);
        let env = new Environment(env_id, level);
        env_id ++;
                
        // walk over this scope's referents.  if any of this scope's
        // bindings are referred to from outside the function, we need
        // an environment
        s.referents.forEach((reflist) => {
            reflist.forEach((ref) => {
                let referencingScope = ref.referencingScope;
                //debug.log(`referent name is ${ref.binding.name}, s.func = ${s.location.func.id.name}/${s.location.func.loc.start.line}, referencing scope = ${referencingScope.location.func.id.name}/${referencingScope.location.func.loc.start.line}`);
                if (referencingScope.differentFunction(s)) {
                    // the scopes are in different functions
                    //debug.log(`reference to '${ref.binding.name}' from outside declaring function (in function ${referencingScope.location.func.id.name})!`);

                    // and add a slot for the referent
                    env.addSlot(ref.binding.name);

                    // also, mark the referencing scope's function as needing this environment
                    let func_envs = get_func_envs(referencingScope.location.func);
                    add_func_env(func_envs, env);
                }
            });
        });

        if (env.slotCount() > 0) {
            //debug.log(`creating environment '${env.name}' for function ${s.location.func.id.name}`);
            s.env = env;
        }

        // recurse into our child scopes.
        let child_func_envs = s.children.map((c) => {
            //debug.indent();
            let ce = walk_scope1(c, level + 1);
            //debug.unindent();
            return ce;
        });

        let child_env_reqs = flatten_func_envs(child_func_envs);
                        
        // we're back in the scope passed to this function,
        // having visited all parents and all children.  we
        // should now know exactly which parent scopes have
        // environments, and should be able to calculate the
        // path to any bindings we reference.

        //debug.log("before removing our environment, function ${s.location.func.id.name} has the following required (from children) environments: ${env for env in child_env_reqs}`);

        if (s.env) {
            let idx = child_env_reqs.indexOf(s.env);
            if (idx !== -1) {
                //debug.log(`removing env ${s.env.name} from list of required environments`);
                child_env_reqs.splice(idx, 1);
            }
        }
                        
        //debug.log(`function ${s.location.func.id.name} has the following required (from children) environments: ${env for env in child_env_reqs}`);

        s.nestedEnvironments = child_env_reqs;
                
        if (s.isFunctionBodyScope()) {
            let func = s.location.func;
            let func_idx = allFunctions.indexOf(func);
            //debug.log(" scope is function body scope for //{func_idx} //{func.id?.name}///{func.loc?.start.line}");

            let func_envs = get_func_envs(func);

            // add the nested environments required by our descendents (that have not been added somewhere in this function) as though they are required by us
            s.nestedEnvironments.forEach((nested_env) => {
                if (!nested_env) return;
                //debug.log "adding //{nested_env}"
                add_func_env(func_envs, nested_env);
            });

            //debug.log("after adding nested environments, func_envs for function is //{fenv for fenv in func_envs}")
                        
            return func_envs;
        };

        return s.nestedEnvironments;
    }
                

    // for every environment, calculate the parent they must have by all the paths we've computed in func_to_envs
    function collapse_paths() {
        let parent_envs = new Map();

        func_to_envs.forEach ((func_envs, func) => {
            if (func_envs.length === 0) return;

            func_envs = func_envs.filter((a_env) => a_env);
                                
            for (let idx = func_envs.length - 1; idx >= 1; idx --) {
                let current_e = func_envs[idx];
                let prospective_parent = func_envs[idx-1];
                if (!parent_envs.has(current_e) || parent_envs.get(current_e).level < prospective_parent.level) {
                    parent_envs.set(current_e, prospective_parent);
                }
            }
        });

        parent_envs.forEach((e, p_e) => e.addChild(p_e));

        // now insert dependencies for parent envs between environments that require them
        func_to_envs.forEach((func_envs, func) => {
            if (func_envs.length < 2) return;

            let collapsed_func_envs = func_envs.filter((a_env) => a_env);

            for (let idx = collapsed_func_envs.length - 1; idx >= 1; idx --) {
                let current_e = collapsed_func_envs[idx];
                let prior_e = collapsed_func_envs[idx-1];
                if (current_e.parentEnv !== prior_e) {
                    // we need to walk current_e's parent chain until we reach prior_e, adding the environments to func_envs
                    let e = current_e.parentEnv;
                    while (e !== prior_e) {
                        add_func_env(func_envs, e);
                        e = e.parentEnv;
                    }
                }
            }
        });
    }
                        

    function walk_scope2(s, level) {
        //debug.log(`walk_scope2 for scope ${s.debugString()}`);
                
        if (s.env) {
            if (s.env.parentEnv && s.env.parentEnv.slotCount() > 0) {
                //debug.log "outputting parent environment assignment.  parentEnv.name = //{s.env.parentEnv.name}, parentEnv.slotCount = //{s.env.parentEnv.slotCount()}"
                s.location.block.body.unshift(b.expressionStatement(setSlotIntrinsic(s.env.name, s.env.getSlot(s.env.parentEnv.name), b.identifier(s.env.parentEnv.name))));
            }
            s.location.block.body.unshift(b.letDeclaration(b.identifier(s.env.name), intrinsic(makeClosureEnv_id, [b.literal(s.env.slot_map.size)])));
        }
        else {
            //debug.log "scope from line //{s.location.block.loc?.start.line} doesn't have environment"
        }

        if (s.isFunctionBodyScope()) {
            let func = s.location.func;
            let func_idx = allFunctions.indexOf(func);
            //debug.log(" ******* scope is function body scope for //{func_idx} //{func.id?.name} //{func.loc?.start.line}");
            let func_envs = get_func_envs(s.location.func);

            let env_assignments = [];

            let env_name;

            if (func_envs.length === 0) {
                //debug.log "unused environment, func_envs.length = //{func_envs?.length}"
                env_name = "%env_unused";
            }
            else { // func_envs.length >= 1
                func_envs = func_envs.filter((a_env) => a_env);

                let last_idx = func_envs.length-1;
                                        
                env_name = func_envs[last_idx].name;
                                        
                last_idx -= 1;
                                        
                while (last_idx >= 0) {
                    //debug.log "adding const //{func_envs[last_idx].name} = slotIntrinsic(//{func_envs[last_idx+1].name}, //{func_envs[last_idx+1].name}.getSlot(//{func_envs[last_idx].name}));"
                    //debug.log "       const //{func_envs[last_idx].name} = slotIntrinsic(//{func_envs[last_idx+1].name}, //{func_envs[last_idx+1].getSlot(func_envs[last_idx].name)});"
                    let env_decl = b.constDeclaration(b.identifier(func_envs[last_idx].name), slotIntrinsic(func_envs[last_idx+1].name, func_envs[last_idx+1].getSlot(func_envs[last_idx].name)));
                    env_decl.loc = func.body.loc;
                    env_assignments.push(env_decl);
                    last_idx -= 1;
                }
            }

            // add the parameter we need
            func.params.unshift(b.identifier(env_name, func.loc));
                                
            // and add the assignments of all the environments this function needs
            if (env_assignments.length > 0) {
                func.body.body = env_assignments.concat(func.body.body);
            }
        }
        s.children.forEach ((c) => walk_scope2(c, level + 1) );
    }
        
    walk_scope1(root_scope, 0);
    collapse_paths();
    //dump_scopes(root_scope, 0);
    walk_scope2(root_scope, 0);
    //dump_func_envs();
}
        
function is_undefined_literal(e) {
    if (e.type === b.Literal && e.value === undefined) return true;
    return e.type === b.UnaryExpression && e.operator === "void" && e.argument.value === 0;
}

class ValidateEnvironments extends TreeVisitor {
    constructor(options, filename) {
        super();
        this.filename = filename;
        this.options = options;
    }

    visitCallExpression(n) {
        n = super.visitCallExpression(n);
        // make sure that the environment we assign to a
        // closure is the same as the first arg to the function
        if (is_intrinsic(n, makeClosure_id.name) || is_intrinsic(n, makeAnonClosure_id.name)) {
            //debug.log escodegen.generate n.arguments[0]
            //debug.log is_undefined_literal(n.arguments[0])
            if (!is_undefined_literal(n.arguments[0])) {
                let closure_env = n.arguments[0].name;
                let closure_func = n.arguments[is_intrinsic(n, makeClosure_id.name) ? 2 : 1];
                let func_env = closure_func.params[0].name;
                if (closure_env !== func_env) {
                    throw new Error(`closure created using environment ${closure_env}, while function takes ${func_env}`);
                }
            }
        }
        else if (is_intrinsic(n, setSlot_id.name)) {
            let env_name = n.arguments[0].name;
            let env_id = env_name.substring("%env_".length);
            let slot = n.arguments[1].value;
        }
        else if (is_intrinsic(n, slot_id.name)) {
            let env_name = n.arguments[0].name;
            let env_id = env_name.substring("%env_".length);
            let slot = n.arguments[1].value;
        }
        return n;
    }
}

export class NewClosureConvert {
    constructor(options, filename, allModules) {
        this.options = options;
        this.filename = filename;
        this.allModules = allModules;
    }

    visit(tree) {
        let flattenDecls = new FlattenDeclarations(this.options, this.filename, this.allModules);
        let collectScopes = new CollectScopeNestingInfo(this.options, this.filename, this.allModules);
        let substituteVariables = new SubstituteVariables(this.options, this.filename, this.allModules);

        tree = flattenDecls.visit(tree);

        tree = collectScopes.visit(tree);

        //debug.log escodegen.generate tree
        placeEnvironments(collectScopes.root_scope);

        tree = substituteVariables.visit(tree);
/*
        let validator = new ValidateEnvironments(this.options, this.filename);
        tree = validator.visit(tree);
*/      
        allFunctions = [];
        global_bindings = new Map();

        return tree;
    }
}
