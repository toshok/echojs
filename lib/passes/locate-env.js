import { ComputedPropertyKey } from '../ast-builder';
import { TransformPass } from '../node-visitor';
import { Stack } from '../stack-es6';
import { Set } from '../set-es6';

//
// each element in env is an object of the form:
//   { exp: ...     // the AST node corresponding to this environment.  will be a function or a BlockStatement. (right now just functions)
//     id:  ...     // the number/id of this environment
//     decls: ..    // a set of the variable names that are declared in this environment
//     closed: ...  // a set of the variable names that are used from this environment (i.e. the ones that need to move to the environment)
//     parent: ...  // the parent environment object
//   }
// 
export class LocateEnv extends TransformPass {
        constructor() {
                super();
                this.envs = new Stack;
                this.env_id = 0;
	}

        visitFunction (n) {
	    let current_env = (this.envs.depth > 0) ? this.envs.top : null;

            let new_env = { id: this.env_id, decls: n.ejs_decls ? n.ejs_decls : new Set(), closed: new Set(), parent: current_env };
            this.env_id ++;
            n.ejs_env = new_env;
            this.envs.push(new_env);
            // don't visit the parameters for the same reason we don't visit the id's of variable declarators below:
            // we don't want a function to look like:  function foo (%env_1.x) { }
            // instead of                              function foo (x) { }
            n.body = this.visit(n.body);
            this.envs.pop();
            return n;
	}

        visitVariableDeclarator (n) {
            // don't visit the id, as that will cause us to mark it ejs_substitute = true, and we'll end up with things like:
            //   var %env_1.x = blah
            // instead of what we want:
            //   var x = blah
            n.init = this.visit(n.init);
            return n;
	}

        visitProperty (n) {
            if (n.key.type === ComputedPropertyKey)
		n.key = this.visit(n.key);
            n.value = this.visit(n.value);
	    return n;
	}

        visitIdentifier (n) {
            // find the environment in the env-stack that includes this variable's decl.  add it to the env's .closed set.
            let current_env = this.envs.top;
            let env = current_env;

            // if the current environment declares that identifier, nothing to do.
            if (env.decls.has(n.name)) return n;

            let closed_over = false;
            // look up our environment stack for the decl for it.
            env = env.parent;
            while (env) {
                if (env.decls && env.decls.has(n.name)) {
                    closed_over = true;
		    break;
		}
                else {
                    env = env.parent;
		}
	    }

            // if we found it higher on the function stack, we need to walk back up the stack forcing environments along the way, and make
            // sure the frame that declares it knows that something down the stack closes over it.
            if (closed_over) {
                env = current_env.parent;
                while (env) {
                    if (env.decls && env.decls.has(n.name)) {
                        env.closed.add(n.name);
			break;
		    }
                    else {
                        env.nested_requires_env = true;
                        env = env.parent;
		    }
		}
	    }

	    // XXX shouldn't this next line be inside the above if?
            n.ejs_substitute = true;
	    return n;
	}
}
