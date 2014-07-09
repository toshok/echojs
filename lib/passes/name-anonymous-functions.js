
import { TransformPass } from '../node-visitor';
import { FunctionExpression } from '../ast-builder';

module escodegen from '../../escodegen/escodegen-es6';

export class NameAnonymousFunctions extends TransformPass {
        visitAssignmentExpression (n) {
            n = super(n);
            let lhs = n.left;
            let rhs = n.right;

            // if we have the form
            //   <identifier> = function () { }
            // convert to:
            //   <identifier> = function <identifier> () { }
            // if lhs.type is Identifier and rhs.type is FunctionExpression and not rhs.id?.name
            //        rhs.display = <something pretty about the lhs>
            //
	    let rhs_name = null;
	    if (rhs.id)
		rhs_name = rhs.id.name;
            if (rhs.type === FunctionExpression && !rhs_name)
		rhs.displayName = escodegen.generate(lhs);
            return n;
	}
}
