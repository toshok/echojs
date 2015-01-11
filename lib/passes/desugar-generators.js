/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import * as b, { identifier } from '../ast-builder';
import { value_id, done_id, next_id, createIterResult_id } from '../common-ids';
import { generate as escodegenerate } from '../../escodegen/escodegen-es6';
import { intrinsic } from '../echo-util';
import { TransformPass } from '../node-visitor';

let stmts_that_need_own_state = [b.ThrowStatement, b.ReturnStatement];
function needs_own_state (n) {
    if (n.type === b.ExpressionStatement && n.expression.type === b.YieldExpression) return true;
    if (n.type === b.IfStatement) return true;
    return stmts_that_need_own_state.indexOf(n.type) !== -1;
}

let state_id    = identifier("state");
let excStack_id = identifier("excStack");
let _done_id    = identifier("_done");
let throw_id    = identifier("throw");
let getNext_id  = identifier("getNext");
let sent_id     = identifier('%sent');

// XXX these two need to be per-function or we'll get crazy high state
// numbers, and only 1 function will begin at state 0.
let state_num = 0;
function new_state (stmts=[]) {
    return { id: state_num++, stmts };
}

export class DesugarGenerators extends TransformPass {
        visitFunction (n) {
            let n = super(n);
            if (!n.generator)
                return n;
            
            let state_info = this.flattenBody(n, 0);
            return this.createGeneratorStateMachine(n, state_info);
        }

                
        // flattens a given statement into a state.  might create other states
        // (think an if statement with then/else branches/blocks)
        flattenStmt (n, state) { }
        
        flattenBody (n) {
            // this method flattens control flow within the body of
            // the generator function into an array of states. new
            // states are generated for every change in control
            // flow, including branches, yields, throws, and returns
            let states = [];
            let current_state = new_state();
            for (let stmt of n.body.body) {
                if (!needs_own_state(stmt))
                    current_state.stmts.push(stmt);
                else {
                    // if we have anything in the current state, push it
                    states.push(current_state);

                    // create (and push) a new state for just this statement
                    let nstate = new_state([stmt]);
                    states.push(nstate);

                    // and create a new state going forward
                    current_state = new_state();
                }
            }
            if (current_state.stmts.length !== 0)
                states.push(current_state);
            return { states };
        }
                
        createGeneratorStateMachine (n, state_info) {
            let new_body =                 this.generateStateVariables(n, state_info);
            let new_body = new_body.concat(this.generateGetNext       (n, state_info));
            let new_body = new_body.concat(this.generateReturn        (n, state_info));
                
            let new_func = {
                type: n.type,
                id: n.id,
                params: n.params,
                defaults: n.defaults,
                rest: n.rest,
                generator: false,
                expression: n.expression,
                body: {
                    type: b.BlockStatement,
                    body: new_body
                }
            };
            console.log(escodegenerate(new_func));
            return new_func;
        }

        generateGetNext (n, state_info) {
            console.log(`${state_info.states.length} states`);
                
            let create_case_from_state = (state) => {
                let switch_case = {
                    type: b.SwitchCase,
                    test: { type: b.Literal, value: state.id },
                    consequent: state.stmts
                };
                console.log(escodegenerate(switch_case));
                return switch_case;
            };
                        
            let switch_stmt = {
                type: b.SwitchStatement,
                discriminant: state_id,
                cases: state_info.states.map((state) => create_case_from_state(state))
            };
            console.log(1);
            console.log(JSON.stringify(switch_stmt));
            console.log(escodegenerate(switch_stmt));
            console.log(2);
            let get_next = {
                type: b.FunctionDeclaration,
                id: getNext_id,
                params: [sent_id],
                defaults: [],
                body: {
                    type: b.BlockStatement,
                    body: [switch_stmt]
                }
            };
            return [get_next];
        }
                
        generateStateVariables (n) {
            // let state = 0, // start state
            //     excStack = [],
            //     _done = false;
            let decls = {
                type: b.VariableDeclaration,
                kind: "let",
                declarations: [
                    { type: b.VariableDeclarator, id: state_id,    init: { type: b.Literal, value: 0 } },
                    { type: b.VariableDeclarator, id: _done_id,    init: { type: b.Literal, value: false } },
                    { type: b.VariableDeclarator, id: excStack_id, init: { type: b.ArrayExpression, elements: [] } }
                ]
            };

            return [decls];
        }

        generateReturn (n) {
            //return {
            //        next: function() { return { value: getNext(), done: _done }; },
            //        throw: function()  { ... }
            //};

            let getNext_call = {
                type: b.CallExpression,
                callee: getNext_id,
                arguments: [sent_id]
            };
                
            let next_init = {
                type: b.FunctionExpression,
                id: null,
                params: [sent_id],
                defaults: [],
                body: {
                    type: b.BlockStatement,
                    body: [{
                        type: b.ReturnStatement,
                        argument: intrinsic(createIterResult_id, [getNext_call, _done_id])
                    }]
                }
            };
            let ret_stmt = {
                type: b.ReturnStatement,
                argument: {
                    type: b.ObjectExpression,
                    properties: [
                        { type: b.Property, kind: "init", key: next_id, value: next_init },
                        { type: b.Property, kind: "init", key: throw_id, value: { type: b.Literal, value: 5 } }
                    ]
                }
            };
            return [ret_stmt];
        }
}                
