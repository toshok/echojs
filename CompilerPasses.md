
Compiler passes
===============

Initial Setup
-------------

1. insert_toplevel_func

   Wraps the entire script in a toplevel function.  this simplifies
   much of the closure conversion code because nothing is outside of
   a function.

Closure Conversion
------------------

2. HoiseFuncDecls

   Hoists nested function declarations to the top of the containing function.

3. FuncDeclsToVars

   Converts function declarations to variable assignment.  i.e. from this:

```javascript
	function foo () { }
```
   to this:
```javascript
	var foo = function foo () { };
```

4. HoistVars

   Hoists vars to the top of the containing functions, leaving initializers where they were.  That is, from:

```javascript
	{
	....
	var x = 5;
	....
	}
```
   to:
```javascript
	{
	   let x;
	   ....
	   x = 5;
	   ....
	}
```
   note the switch from `var` to `let` above.  After this phase there should be no `var` declarations.


5. ComputeFree

   Compute free and declared variables for each node and stores them on the nodes.

6. LocateEnv

   Computes where in the tree we need to add environments, and also
   computes which variables need to be moved into which environment.
   This stage computes closures in the top down manner as described on
   [Matt Might's awesome site](http://matt.might.net/articles/closure-conversion/).

7. SubstituteVariables

   Replaces variable references with environment property references.  After this
   pass the only free variables are references to globals.  This phase also replaces
   function expressions, `new` expressions, and call expressions with `%makeClosure`,
   `%invokeClosure`, and `%invokeClosure` intrinsics, respectively.

   so from:
```javascript
	var f = function f () { }
	g = f()
	h = new f();
```
   to
```javascript
	var f;
	f = %makeClosure(%current_env, "f", function f () { })
	g = %invokeClosure(f, undefined, 0, [])
	h = new %invokeClosure(f, undefined, 0, [])
```

   This phase might be nicer if the `new` expression could be further decomposed into 2 intrinsics, `%createObject` and `%invokeClosure`.  Then
   the LLVM IR generator wouldn't need to support new expressions at all.

   This phase also keeps track of the maximum number of call args required for any invocations.

8. LambdaLift

   After SubstituteVariables, it's safe to move all function expressions out to the toplevel, since they have no free variables except global references.
   After this phase there are no nested functions at all.

   This phase also prepends `%createArgScratchArea` intrinsic calls to the beginning of any function that needs it.


Code Generation
---------------

9. AddFunctions

   Adds the toplevel functions to the LLVM IR module so that when we
   walk the tree generating code, references are resolved to the proper
   functions.


10. LLVMIR

    Walks the tree one last time emitting the actual IR.
