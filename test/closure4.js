// generator: none

let VariableDeclaration = 2;
let FunctionDeclaration = 2;

function something() {

    // this var and function are only here to guarantee that something() is allocated an environment
    var foo = 2;
    function fweep () {
      return foo;
    }

    // collect_decls is passed the environment from something(), although I don't understand why.  it could be passed env_0 (the toplevel).
    function collect_decls (body) {
        let rv = [];
        body.forEach ( function bodyForeach(statement){
            if (statement.type === VariableDeclaration || statement.type === FunctionDeclaration)
                rv.push(statement);
        });
        return rv;
    }
}
