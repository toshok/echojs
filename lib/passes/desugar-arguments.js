import { reportError } from '../errors';
import { argPresent_id, getArg_id, getArgumentsObject_id } from '../common-ids';
import { intrinsic } from '../echo-util';
import { TransformPass } from '../node-visitor';
import * as b from '../ast-builder';

export class DesugarArguments extends TransformPass {
    constructor (options, filename) {
        super(options);
        this.filename = filename;
    }

    visitIdentifier(n) {
        if (n.name === "arguments") return intrinsic(getArgumentsObject_id);
        return super(n);
    }

    visitVariableDeclarator(n) {
        if (n.id.name === "arguments")
            reportError(SyntaxError, "Cannot declare variable named 'arguments'", this.filename, n.id.loc);
        return super(n);
    }

    visitAssignmentExpression(n) {
        if (n.left.type === b.Identifier && n.left.name === "arguments")
            reportError(SyntaxError, "Cannot set 'arguments'", this.filename, n.left.loc);
        return super(n);
    }

    visitProperty(n) {
        if (n.key.type === b.ComputedPropertyKey)
            n.key = this.visit(n.key);
        n.value = this.visit(n.value);
        return n;
    }
}
