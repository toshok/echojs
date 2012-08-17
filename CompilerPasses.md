
Compiler passes
===============

Initial Setup
-------------

1. insert_toplevel_func

   Wraps the entire script in a toplevel _ejs_script function.  this
   simplifies much of the traversal/closure conversion code because
   nothing is outside of a function.

Desugar
-------

2. VarToLet

   Replace all var's with let's (with the let placed at the top of the
   function and any initializer left in the spot where the var was.  After
   this pass all scoping is block scoped and explicit.

3. RemoveIIFE

   An optimization made possible by VarToLet.  now that all variables
   are block scoped we can remove the mechanism JS programmers have
   adopted to simulate block scope -- IIFE's -- and replace them with
   their body inlined where the CallExpression was.  At present only
   IIFE's with zero parameters are converted.

   This has a number of benefits.  There is less closure conversion
   work to be done, we end up with fewer toplevel functions, more code
   is made available to the llvm optimizer, and there's no
   makeClosure/invokeClosure machinery involved.

4. FlattenExpressionStatements

   Given the nodevisitor architecture, we need another pass to remove
   ExpressionStatement nodes that parented the IIFE's CallExpression.

Closure Conversion
------------------

5. free

   Compute free and declared variables for each node and stores them on the nodes.

6. LocateEnv

   Computes where in the tree we need to add environments, and also
   computes which variables need to be moved into which environment.

7. SubstiteVariables

   Replaces variable references with environment property references.  After this
   pass there are no free variables.

8. LambdaLift

   Lift all the functions out to the toplevel of the program.  after this point there
   are no nested functions.


Code Generation
---------------

9. AddFunctions

   Adds the toplevel functions to the llvm IR module so that when we
   walk the tree generating code, calls are resolved to the proper
   functions.


10. LLVMIR

    Walks the tree one last time emitting the actual IR.
