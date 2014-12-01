// we have a loop that looks like:
//
//  for (let x = ...; $test; $update) {
//      /* body */
//  }
//
// we desugar this to:
//
//  for (var %loop_x = ...; $test(with x replaced with %loop_x); $update(with x replaced with %loop_x) {
//    let x = %loop_x;
//    try {
//      /* body */
//    }
//    finally {
//      %loop_x = x;
//    }
//  }

import * as b from '../ast-builder';
import { Stack } from '../stack-es6';
import { shallow_copy_object, startGenerator } from '../echo-util';
import { TransformPass } from '../node-visitor';

let hasOwn = Object.prototype.hasOwnProperty;

let vargen = startGenerator();
function freshLoopVar (ident) {
    return `%loop_${ident}_${vargen()}`;
}

export class DesugarLetLoopVars extends TransformPass {
        
    constructor (options, filename) {
	super(options);
	this.filename = filename;
    }

    visitFor (n) {
	// if the loop looks like: `for (; ...)` there's nothing for us to do
	if (!n.init) return n;

	// if the loop looks like: `for (var i = 0; ...)` or `for (i = 0; ...)` there's nothing for us to do
	if (n.init.type !== b.VariableDeclaration || n.init.kind !== 'let') return n;

        n.init.kind = "var";
                
        let mappings = Object.create(null);

        for (let decl of n.init.declarations) {
            let loopvar = b.identifier(freshLoopVar(decl.id.name));
            mappings[decl.id.name] = loopvar;
            decl.id = loopvar;
	}
                        
        let assignments = [];
        let new_body = b.blockStatement();
                
        for (let loopvar in mappings) {
            // this gives us the "let x = %loop_x"
            // assignments, so get our fresh binding per
            // loop iteration
            new_body.body.push(b.variableDeclaration('let', b.identifier(loopvar), mappings[loopvar]));
                        
            // and this gives us the assignment we put in
            // the finally block to capture changes made to
            // the loop variable in the body
            assignments.push(b.expressionStatement(b.assignmentExpression(mappings[loopvar], '=', b.identifier(loopvar))));
	}

        new_body.body.push(b.tryStatement(n.body, [], b.blockStatement(assignments)));

        let remap = new RemapIdentifiers(this.options, this.filename, mappings);
        n.test = remap.visit(n.test);
        n.update = remap.visit(n.update);

        n.body = this.visit(new_body);

        return n;
    }
}

class RemapIdentifiers extends TransformPass {
    constructor (options, filename, initial_mapping) {
	super(options);
	this.filename = filename;
        this.mappings = new Stack(initial_mapping);
    }

    visitBlock (n) {
        // clone the mapping and push it onto the stack
        this.mappings.push(shallow_copy_object(this.currentMapping()));
        super(n);
        this.mappings.pop();
        return n;
    }

    visitVariableDeclarator (n) {
        // if the variable's name exists in the mapping clear it out
        this.currentMapping()[n.id.name] = null;
    }

    visitObjectPattern (n) {
	for (let prop of n.properties)
            this.currentMapping()[prop.key] = null;
        super(n);
    }

    visitCatchClause (n) {
        this.mappings.push(shallow_copy_object(this.currentMapping()));
        this.currentMapping()[n.param.name] = null;
        super(n);
        this.mappings.pop();
        return n;
    }

    visitFunction (n) {
	if (n.id)
            this.currentMapping()[n.id.name] = null;

        this.mappings.push(shallow_copy_object(this.currentMapping()));
	if (n.rest)
            this.currentMapping()[n.rest.name] = null;
        super(n);
        this.mappings.pop();
        return n;
    }
                
    visitIdentifier (n) {
        if (hasOwn.call(this.currentMapping(), n.name)) {
            let mapped = this.currentMapping()[n.name];
	    if (mapped) return mapped;
	}
        return n;
    }
                
    currentMapping() {
	return (this.mappings.depth > 0) ? this.mappings.top : Object.create(null);
    }
}
                
