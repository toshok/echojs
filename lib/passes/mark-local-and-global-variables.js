
import { createGlobalsInterface } from '../runtime';
import { reportError, reportWarning } from '../errors';
import { Stack } from '../stack-es6';
import { TransformPass } from '../node-visitor';
import { is_intrinsic, intrinsic } from '../echo-util';

import { getLocal_id, getGlobal_id, setLocal_id, setGlobal_id, setSlot_id } from '../common-ids';
import { Identifier } from '../ast-builder';

let runtime_globals = createGlobalsInterface(null);
let hasOwnProperty = Object.prototype.hasOwnProperty;

export class MarkLocalAndGlobalVariables extends TransformPass {
    constructor (options, filename) {
        super(options);
	this.filename = filename;
        // initialize the scope stack with the global (empty) scope
        this.scope_stack = new Stack(new Map());
    }

    findIdentifierInScope (ident) {
        for (let scope of this.scope_stack.stack)
	    if (scope.has(ident.name))
                return true;
        // at this point it's going to be a global reference.
        // make sure the identifier is one of our globals
        if (hasOwnProperty.call(runtime_globals, ident.name))
            return false;

        if (this.options.warn_on_undeclared)
            reportWarning(`undeclared identifier '${ident.name}'`, this.filename, ident.loc);
        else
            reportError(ReferenceError, `undeclared identifier '${ident.name}'`, this.filename, ident.loc);
        return false;
    }

    intrinsicForIdentifier (id) {
        let is_local = this.findIdentifierInScope(id);
        return is_local ? getLocal_id : getGlobal_id;
    }
    
    visitWithScope (scope, children) {
        this.scope_stack.push(scope);
	let rv;

        if (Array.isArray(children)) {
	    rv = [];
	    for (let child of children)
		rv.push(this.visit(child));
	}
        else {
            rv = this.visit(children);
	}
        this.scope_stack.pop();
	return rv;
    }

    visitVariableDeclarator (n) {
        this.scope_stack.top.set(n.id.name, true);
        return n;
    }

    visitImportSpecifier (n) {
        this.scope_stack.top.set(n.id.name, true);
        return n;
    }

    visitObjectExpression (n) {
        for (let property of n.properties)
	    property.value = this.visit(property.value);
        return n;
    }

    visitAssignmentExpression (n) {
        let lhs = n.left;
        let visited_rhs = this.visit(n.right);
        
        if (is_intrinsic(lhs, "%slot")) {
            return intrinsic(setSlot_id, [lhs.arguments[0], lhs.arguments[1], visited_rhs], lhs.loc);
	}
        else if (lhs.type === Identifier) {
            if (this.findIdentifierInScope(lhs))
                return intrinsic(setLocal_id, [lhs, visited_rhs], lhs.loc);
            else
                return intrinsic(setGlobal_id, [lhs, visited_rhs], lhs.loc);
	}
        else {
            n.left = this.visit(n.left);
            n.right = visited_rhs;
            return n;
	}
    }
    
    visitBlock (n) {
        n.body = this.visitWithScope(new Map(), n.body);
        return n;
    }

    visitCallExpression (n) {
        // at this point there shouldn't be call expressions
        // for anything other than intrinsics, so we leave
        // callee alone and just iterate over the args.  we
        // skip the first one since that is guaranteed to be an
        // identifier that we don't want rewrapped, or an
        // intrinsic already wrapping the identifier.

	let new_args;

        if (n.arguments[0].type === Identifier) {
            this.findIdentifierInScope(n.arguments[0]);
            new_args = this.visit(n.arguments.slice(1));
            new_args.unshift(n.arguments[0]);
	}
        else {
            new_args = this.visit(n.arguments);
	}
        n.arguments = new_args;
        return n;
    }

    visitNewExpression (n) {
        // at this point there shouldn't be call expressions
        // for anything other than intrinsics, so we leave
        // callee alone and just iterate over the args.  we
        // skip the first one since that is guaranteed to be an
        // identifier that we don't want rewrapped, or an
        // intrinsic already wrapping the identifier.

        if (n.arguments[0].type === Identifier) {
            this.findIdentifierInScope(n.arguments[0]);
	}
        let new_args = this.visit(n.arguments.slice(1));
        new_args.unshift(n.arguments[0]);
        n.arguments = new_args;
        return n;
    }
    
    visitFunction (n) {
        let new_scope = new Map;
	for (let p of n.params)
	    new_scope.set(p.name, true);
        new_scope.set("arguments", true);
        let new_body = this.visitWithScope(new_scope, n.body);
        n.body = new_body;
        return n;
    }

    visitLabeledStatement (n) {
        n.body = this.visit(n.body);
        return n;
    }

    visitCatchClause (n) {
        let new_scope = new Map;
        new_scope.set(n.param.name, true);
        n.guard = this.visit(n.guard);
        n.body = this.visitWithScope(new_scope, n.body);
        return n;
    }

    visitIdentifier (n) {
        return intrinsic(this.intrinsicForIdentifier(n), [n]);
    }
}
