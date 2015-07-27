/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
// 1) allocates the environment at the start of the n
// 2) adds mappings for all .closed variables
import { TransformPass } from '../node-visitor';
import { Stack } from '../stack-es6';

import { intrinsic, is_intrinsic, reject, shallow_copy_object } from '../echo-util';

import * as b from '../ast-builder';

import { invokeClosure_id, makeClosure_id, makeAnonClosure_id, makeClosureEnv_id, setSlot_id, slot_id } from '../common-ids';

import * as escodegen from '../../external-deps/escodegen/escodegen-es6';

let hasOwnProperty = Object.prototype.hasOwnProperty;

export class SubstituteVariables extends TransformPass {
    constructor (options) {
        super(options);
        this.function_stack = new Stack;
        this.mappings = new Stack;
    }

    currentMapping () {
        return this.mappings.depth > 0 ? this.mappings.top : Object.create(null);
    }

    visitIdentifier (n) {
        if (hasOwnProperty.call(this.currentMapping(), n.name))
            return this.currentMapping()[n.name];
        return n;
    }

    visitFor (n) {
        // for loops complicate things.
        // if any of the variables declared in n.init are closed over
        // we promote all of them outside of n.init.

        this.skipExpressionStatement = true;
        let init = this.visit(n.init);
        this.skipExpressionStatement = false;
        n.test = this.visit(n.test);
        n.update = this.visit(n.update);
        n.body = this.visit(n.body);
        if (Array.isArray(init)) {
            n.init = null;
            return b.blockStatement(init.concat([n]));
        }
        n.init = init;
        return n;
    }

    visitForIn (n) {
        // for-in loops complicate things.

        let left = this.visit(n.left);
        n.right = this.visit(n.right);
        n.body = this.visit(n.body);
        if (Array.isArray(left)) {
            console.log("whu?");
            n.left = b.identifier(left[0].declarations[0].id.name);
            return b.blockStatement(left.concat([n]));
        }

        n.left = left;
        return n;
    }

    visitVariableDeclaration (n) {
        // here we do some magic depending on whether or not
        // variables are closed over (i.e. pushed into the
        // environment).
        // 
        // 1. for variables that are closed over that aren't
        //    initialized (that is, they're implicitly
        //    'undefined'), we just remove their declaration
        //    entirely.  it's already been converted to a slot
        //    everywhere else, and env slots are explicitly
        //    initialized to undefined by the runtime.
        //
        // 2. for variables that are closed over that *are*
        //    initialized, we splice them into the list and
        //    split the VariableDeclaration node into two, so
        //    if 'y' is closed over in the following input:
        //
        //    let x = 2, y = x * 2, z = 10;
        //
        //    we'll end up with this in the output:
        //
        //    let x = 2;
        //    %slot(%env, 1, 'y') = x * 2;
        //    let z = 10;
        //
        let decls = n.declarations;

        let rv = [];
        
        let new_declarations = [];

        // we loop until we find a variable that's closed over *and* has an initializer.
        for (let decl of decls) {
            decl.init = this.visit(decl.init);

            let closed_over = hasOwnProperty.call(this.currentMapping(), decl.id.name);
            if (closed_over) {
                // for variables that are closed over but undefined, we skip them (thereby removing them from the list of decls)

                if (decl.init) { // FIXME: we should also check for an explicit 'undefined' here

                    // push the current set of new_declarations if there are any
                    if (new_declarations.length > 0)
                        rv.push(b.variableDeclaration(n.kind, new_declarations));

                    // splice in this assignment
                    rv.push(b.expressionStatement(b.assignmentExpression(this.currentMapping()[decl.id.name], "=", decl.init)));

                    // then re-init the new_declarations array
                    new_declarations = [];
                }
            }
            else {
                // for variables that aren't closed over, we just add them to the currect decl list.
                new_declarations.push(decl);
            }
        }
        

        // push the last set of new_declarations if there were any
        if (new_declarations.length > 0) {
            rv.push(b.variableDeclaration(n.kind, new_declarations));
        }

        if (rv.length === 0) {
            rv = b.emptyStatement();
        }
        return rv;
    }

    visitProperty (n) {
        if (n.computed)
            n.key = this.visit(n.key);
        n.value = this.visit(n.value);
        return n;
    }

    visitBlock (n) {
        if (!n.ejs_env)
            return super.visitBlock(n);
                
        let this_env_id = b.identifier(`%env_${n.ejs_env.id}`);
        let parent_env_name;
        if (n.ejs_env.parent)
            parent_env_name = `%env_${n.ejs_env.parent.id}`;
                        
        let env_prepends = [];
        let new_mapping = shallow_copy_object(this.currentMapping());
                
        if (n.ejs_env.closed.empty() && !n.ejs_env.nested_requires_env) {
            env_prepends.push(b.letDeclaration(this_env_id, b.nullLit()));
        }
        else {
            // insert environment creation (at the start of the block)
            env_prepends.push(b.letDeclaration(this_env_id, intrinsic(makeClosureEnv_id, [b.literal(n.ejs_env.closed.size() + (n.ejs_env.parent ? 1 : 0))])));
                
            n.ejs_env.slot_mapping = Object.create(null);
            var i = 0;
            if (n.ejs_env.parent) {
                n.ejs_env.slot_mapping[parent_env_name] = i;
                i += 1;
            }
            n.ejs_env.closed.map((el) => {
                n.ejs_env.slot_mapping[el] = i;
                i += 1;
            });
                                
                        
            if (n.ejs_env.parent) {
                let parent_env_slot = n.ejs_env.slot_mapping[parent_env_name];
                env_prepends.push(b.expressionStatement(intrinsic(setSlot_id, [this_env_id, b.literal(parent_env_slot), b.literal(parent_env_name), b.identifier(parent_env_name) ])));
                        }
            // XXX here's where function handling pushes closed over parameters.  i'm guessing we need special logic for incoming environment slots for loop variables?

            new_mapping["%slot_mapping"] = n.ejs_env.slot_mapping;
            
            var flatten_memberexp = (exp, mapping) => {
                if (exp.type !== CallExpression) {
                    return [b.literal(mapping[exp.name])];
                }
                else {
                    return flatten_memberexp(exp.arguments[0], mapping).concat([exp.arguments[1]]);
                }
            };
            
            let prepend_environment = (exps) => {
                let obj = this_env_id;
                for (let prop of exps)
                    obj = intrinsic(slot_id, [obj, prop]);
                return obj;
            };

            // if there are existing mappings prepend "%env." (a MemberExpression) to them
            for (let mapped in new_mapping) {
                let val = new_mapping[mapped];
                if (mapped !== "%slot_mapping")
                    new_mapping[mapped] = prepend_environment(flatten_memberexp(val, n.ejs_env.slot_mapping));
            }

            // and add mappings for all variables in .closed from "x" to "%env.x"
            new_mapping["%env"] = this_env_id;
            n.ejs_env.closed.keys().forEach ((sym) => {
                new_mapping[sym] = intrinsic(slot_id, [this_env_id, b.literal(n.ejs_env.slot_mapping[sym]), b.literal(sym)]);
            });
        }

        // remove all mappings for variables declared in this function
        if (n.ejs_decls) {
            new_mapping = reject(new_mapping, (sym) => (n.ejs_decls.has(sym) && !n.ejs_env.closed.has(sym)));
        }

        this.mappings.push(new_mapping);
        super.visitBlock(n);
        if (env_prepends.length > 0)
            n.body = env_prepends.concat(n.body);
        this.mappings.pop();
        return n;
    }

    visitFunctionBody (n) {
        n.scratch_size = 0;
        
        // we use this instead of calling super in visitFunction because we don't want to visit parameters
        // during this pass, or they'll be substituted with an %env.

        this.function_stack.push(n);
        n.body = this.visit(n.body);
        this.function_stack.pop();
        
        return n;
    }

    visitFunction (n) {
        try {
            // XXX this should be a let, but ejs currently pukes if we close over it.
            var this_env_id = b.identifier(`%env_${n.ejs_env.id}`);
            let parent_env_name;

            if (n.ejs_env.parent)
                parent_env_name = `%env_${n.ejs_env.parent.id}`;

            let env_prepends = [];
            // XXX this should be a let, but ejs currently pukes if we close over it.
            var new_mapping = shallow_copy_object(this.currentMapping());
            if (n.ejs_env.closed.empty() && !n.ejs_env.nested_requires_env) {
                env_prepends.push(b.letDeclaration(this_env_id, b.nullLit()));
            }
            else {
                // insert environment creation (at the start of the function body)
                env_prepends.push(b.letDeclaration(this_env_id, intrinsic(makeClosureEnv_id, [b.literal(n.ejs_env.closed.size() + (n.ejs_env.parent ? 1 : 0))])));

                n.ejs_env.slot_mapping = Object.create(null);
                // XXX this should be a let, but ejs currently pukes if we close over it.
                var i = 0;
                if (n.ejs_env.parent) {
                    n.ejs_env.slot_mapping[parent_env_name] = i;
                    i ++;
                }
                n.ejs_env.closed.map ( (el) => {
                    n.ejs_env.slot_mapping[el] = i;
                    i ++;
                });
                
                if (n.ejs_env.parent) {
                    let parent_env_slot = n.ejs_env.slot_mapping[parent_env_name];
                    env_prepends.push(b.expressionStatement(intrinsic(setSlot_id, [this_env_id, b.literal(parent_env_slot), b.literal(parent_env_name), b.identifier(parent_env_name) ])));
                }
                
                // we need to push assignments of any closed over parameters into the environment at this point
                for (let param of n.params) {
                    if (n.ejs_env.closed.has(param.name))
                        env_prepends.push(b.expressionStatement(intrinsic(setSlot_id, [ this_env_id, b.literal(n.ejs_env.slot_mapping[param.name]), b.literal(param.name), b.identifier(param.name) ])));
                }

                new_mapping["%slot_mapping"] = n.ejs_env.slot_mapping;

                var flatten_memberexp = (exp, mapping) => {
                    if (exp.type !== CallExpression) {
                        return [b.literal(mapping[exp.name])];
                    }
                    else {
                        return flatten_memberexp(exp.arguments[0], mapping).concat([exp.arguments[1]]);
                    }
                };

                let prepend_environment = (exps) => {
                    let obj = this_env_id;
                    for (let prop of exps)
                        obj = intrinsic(slot_id, [obj, prop]);
                    return obj;
                };

                // if there are existing mappings prepend "%env." (a MemberExpression) to them
                for (let mapped in new_mapping) {
                    let val = new_mapping[mapped];
                    if (mapped !== "%slot_mapping")
                        new_mapping[mapped] = prepend_environment(flatten_memberexp(val, n.ejs_env.slot_mapping));
                }
                
                // and add mappings for all variables in .closed from "x" to "%env.x"
                new_mapping["%env"] = this_env_id;
                n.ejs_env.closed.keys().forEach ((sym) => {
                    new_mapping[sym] = intrinsic(slot_id, [this_env_id, b.literal(n.ejs_env.slot_mapping[sym]), b.literal(sym)]);
                });

            }
            // remove all mappings for variables declared in this function
            if (n.ejs_decls) {
                new_mapping = reject(new_mapping, (sym) => (n.ejs_decls.has(sym) && !n.ejs_env.closed.has(sym)));
            }

            this.mappings.push(new_mapping);
            this.visitFunctionBody(n);
            if (env_prepends.length > 0)
                n.body.body = env_prepends.concat(n.body.body);
            this.mappings.pop();

            // convert function expressions to an explicit closure creation, so:
            // 
            //    function X () { ...body... }
            //
            // replace inline with:
            //
            //    makeClosure(%current_env, "X", function X () { ...body... })

            if (!n.toplevel) {
                if (n.type === FunctionDeclaration) {
                    throw new Error("there should be no FunctionDeclarations at this point");
                }
                else { // n.type is FunctionExpression
                    let intrinsic_args = [];
                    intrinsic_args.push(n.ejs_env.parent ? b.identifier(parent_env_name) : b.nullLit());
                    
                    let intrinsic_id;
                    if (n.id) {
                        intrinsic_id = makeClosure_id;
                        if (n.id.type === Identifier) {
                            intrinsic_args.push(b.literal(n.id.name));
                        }
                        else {
                            intrinsic_args.push(b.literal(escodegen.generate(n.id)));
                        }
                    }
                    else {
                        intrinsic_id = makeAnonClosure_id;
                    }

                    intrinsic_args.push(n);
                    
                    return intrinsic(intrinsic_id, intrinsic_args);
                }
            }
            return n;
        }
        catch (e) {
            console.warn (`exception: ${e}`);
            //console.warn "compiling the following code:"
            //console.warn escodegen.generate n
            throw e;
        }
    }
    
    visitCallExpression (n) {
        n = super.visitCallExpression(n);

        // replace calls of the form:
        //   X (arg1, arg2, ...)
        // 
        // with
        //   invokeClosure(X, %this, %argCount, arg1, arg2, ...);

        if (is_intrinsic(n)) return n;
        this.function_stack.top.scratch_size = Math.max(this.function_stack.top.scratch_size, n.arguments.length);
        return intrinsic(invokeClosure_id, [n.callee].concat(n.arguments));
    }

    visitNewExpression (n) {
        n = super.visitNewExpression(n);

        // replace calls of the form:
        //   new X (arg1, arg2, ...)
        // 
        // with
        //   invokeClosure(X, %this, %argCount, arg1, arg2, ...);

        this.function_stack.top.scratch_size = Math.max(this.function_stack.top.scratch_size, n.arguments.length);

        let rv = intrinsic(invokeClosure_id, [n.callee].concat(n.arguments));
        rv.type = NewExpression;
        return rv;
    }
}
