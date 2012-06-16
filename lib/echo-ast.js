/*
 * Decl objects have the following properties
 *
 * name        - the current name of the variable
 * initializer - optional (in non-Const subclasses) initializer expression
 */

var Decl = (function() {
  function Decl(name, initializer)
  {
    this.name = name;
    this.initializer = initializer;
  }
  Decl.prototype = new Expression();

  return new Decl();
})();

var Const = (function() {
  function Const(name, initializer)
  {
  }
  Const.prototype = new Decl();
})();
/*
 * Function objects have the following properties
 *
 * name        - the current name of the function (can be different than definedName if lifted and/or conflicts)
 * definedName - the name given to the function in the source file
 * parameters  - an array of Parameter objects
 * body        - the AST subtree of the function's body
 */