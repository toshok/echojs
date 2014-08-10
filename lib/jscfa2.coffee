# based on jscfa.js from doctor.js here:
# 
# https://github.com/mozilla/doctor.js
#
# its license is as below:
# 
# /* ***** BEGIN LICENSE BLOCK *****
# * Version: MPL 1.1/GPL 2.0/LGPL 2.1
# *
# * The contents of this file are subject to the Mozilla Public License Version
# * 1.1 (the "License"); you may not use this file except in compliance with
# * the License. You may obtain a copy of the License at
# * http://www.mozilla.org/MPL/
# *
# * Software distributed under the License is distributed on an 'AS IS' basis,
# * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# * for the specific language governing rights and limitations under the
# * License.
# *
# * The Original Code is Bespin.
# *
# * The Initial Developer of the Original Code is
# * Dimitris Vardoulakis <dimvar@gmail.com>
# * Portions created by the Initial Developer are Copyright (C) 2010
# * the Initial Developer. All Rights Reserved.
# *
# * Contributor(s):
# *   Dimitris Vardoulakis <dimvar@gmail.com>
# *   Patrick Walton <pcwalton@mozilla.com>
# *
# * Alternatively, the contents of this file may be used under the terms of
# * either the GNU General Public License Version 2 or later (the "GPL"), or
# * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# * in which case the provisions of the GPL or the LGPL are applicable instead
# * of those above. If you wish to allow use of your version of this file only
# * under the terms of either the GPL or the LGPL, and not to allow others to
# * use your version of this file under the terms of the MPL, indicate your
# * decision by deleting the provisions above and replace them with the notice
# * and other provisions required by the GPL or the LGPL. If you do not delete
# * the provisions above, a recipient may use your version of this file under
# * the terms of any one of the MPL, the GPL or the LGPL.
# *
# * ***** END LICENSE BLOCK ***** */

esprima = require 'esprima'
escodegen = require 'escodegen'

ast = require 'ast-builder'

{ is_number_literal } = require "echo-util"

{ TreeVisitor } = require "nodevisitor"

{ ArrayExpression,
  ArrayPattern,
  ArrowFunctionExpression,
  AssignmentExpression,
  BinaryExpression,
  BlockStatement,
  BreakStatement,
  CallExpression,
  CatchClause,
  ClassBody,
  ClassDeclaration,
  ClassExpression,
  ClassHeritage,
  ComprehensionBlock,
  ComprehensionExpression,
  ConditionalExpression,
  ContinueStatement,
  DebuggerStatement,
  DoWhileStatement,
  EmptyStatement,
  ExportDeclaration,
  ExportBatchSpecifier,
  ExportSpecifier,
  ExpressionStatement,
  ForInStatement,
  ForOfStatement,
  ForStatement,
  FunctionDeclaration,
  FunctionExpression,
  Identifier,
  IfStatement,
  ImportDeclaration,
  ImportSpecifier,
  LabeledStatement,
  Literal,
  LogicalExpression,
  MemberExpression,
  MethodDefinition,
  ModuleDeclaration,
  NewExpression,
  ObjectExpression,
  ObjectPattern,
  Program,
  Property,
  ReturnStatement,
  SequenceExpression,
  SpreadElement,
  SwitchCase,
  SwitchStatement,
  TaggedTemplateExpression,
  TemplateElement,
  TemplateLiteral,
  ThisExpression,
  ThrowStatement,
  TryStatement,
  UnaryExpression,
  UpdateExpression,
  VariableDeclaration,
  VariableDeclarator,
  WhileStatement,
  WithStatement,
  YieldExpression } = esprima.Syntax

RESTARGS = -1;
INVALID_TIMESTAMP = -1
UNHANDLED_CONSTRUCT = 0
CFA_ERROR = 1
# number, string -> Error
# returns an Error w/ a "code" property, so that DrJS can classify errors
errorWithCode = (code, msg) ->
        e = new Error(msg)
        e.code = code
        e


class JSCore
        toStr  = (args) -> new Answer AbstractVal.ASTR
        toNum  = (args) -> new Answer AbstractVal.ANUM
        toBool = (args) -> new Answer AbstractVal.ABOOL
        toVoid = (args) -> new Answer AbstractVal.AUNDEF
        toThis = (args) -> new Answer args[0]

        constructor: (heap) ->
                # Global object
                go = new AbstractObj({ addr: newCount() })
                goav = makeObjAbstractVal count

                JSCore.global_object_av      = heap[newCount()] = goav
                JSCore.global_object_av_addr = count

                # global identifiers and methods
                go.addProp "Infinity-",  aval:AbstractVal.ANUM,   writeable:false, enumerable:false, configurable:false
                go.addProp "NaN-",       aval:AbstractVal.ANUM,   writeable:false, enumerable:false, configurable:false
                go.addProp "undefined-", aval:AbstractVal.AUNDEF, writeable:false, enumerable:false, configurable:false

                # Object.prototype
                op = new AbstractObj({ addr: newCount() })
                opav = makeObjAbstractVal count
                JSCore.object_prototype_av = opav

                # Object.__proto__ (same as Function.prototype)
                o_p = new AbstractObj({ addr: newCount(), proto: opav })
                o_pav = makeObjAbstractVal count
                JSCore.function_prototype_av = o_pav

                # Function.prototype.prototype
                fpp = new AbstractObj({ addr: newCount(), proto: opav })
                o_p.addProp "prototype-",   aval: makeObjAbstractVal(count), enumerable:false, configurable:false
                fpp.addProp "constructor-", aval: o_pav,                     enumerable:false


                # Object
                _Object = do ->
                        # This object is used when Object is called w/out new.
                        # In reality the behavior doesn't change. In CFA, when there is no new, 
                        # it's better to bind THIS to nonewav instead of the global object.
                        new AbstractObj({ addr: newCount(), proto: opav })
                        nonewav = makeObjAbstractVal count

                        (args, withNew) ->
                                retval = if withNew then args[0] else nonewav
                                arg = args[1]
                                if not arg?
                                        retval.forEachObj (o) -> o.updateProto opav
                                        new Answer(retval)
                                else
                                        # throw errorWithCode(CFA_ERROR, "call a suitable constructor, " +
                                        #                     "hasn't been defined yet. FIXME")
                                        retval.forEachObj (o) -> o.updateProto opav
                                        new Answer(retval)
        
                # Object is a heap var that will contain an Aval that points to o
                o = go.attachMethod("Object", 0, _Object)
                oav = makeObjAbstractVal(count)
                o.addProp("prototype-",   { aval:opav, writeable:false, enumerable:false, configurable:false })
                op.addProp("constructor-", { aval:oav,                   enumerable: false })

                # Function
                f = new AbstractObj({ addr: newCount(), proto: o_pav })
                fav = makeObjAbstractVal(count)
                
                go.addProp  "Function-",    aval:fav,                    enumerable:false
                f.addProp   "prototype-",   aval:o_pav, writeable:false, enumerable:false, configurable:false
                o_p.addProp "constructor-", aval:fav,                    enumerable:false

                # Methods are attached here because o_pav must be defined already.
                go.attachMethod("isFinite",       1, toBool)
                go.attachMethod("isNaN",          1, toBool)
                go.attachMethod("parseInt",       1, toNum)
                go.attachMethod("hasOwnProperty", 1, toBool)
                go.attachMethod("toString",       0, toStr)
                go.attachMethod("valueOf",        0, toThis)
                o_p.attachMethod("toString",       0, toStr)
                o_p.attachMethod("call",           0, (args, withNew, cn) ->
                        f = args.shift()
                        args.unshift(JSCore.global_object_av) if not args[0]?
                        f.callFun(args, cn))

                o_p.attachMethod("apply", 2, (args, withNew, cn) ->
                        recv = args[1] or JSCore.global_object_av
                        a2 = args[2]
                        rands = undefined
                        av = undefined
                        maxArity = 0
                        retval = AbstractVal.BOTTOM
                        # We can't compute the arguments once for all functions that
                        # may be applied. The functions may have different arity which
                        # impacts what goes to the restargs for each function.
                        args[0].forEachObj (o) ->
                                clos = o.getFun()
                                return if !clos
                                if clos.builtin
                                        pslen = clos.arity
                                else
                                        pslen = clos.params.length
                                # compute arguments
                                restargs = AbstractVal.BOTTOM
                                rands = buildArray pslen, AbstractVal.BOTTOM
                                if a2 # a2 is the array passed at the call to apply.
                                        a2.forEachObj (o) ->
                                                if o.numPropsMerged
                                                        av = o.getNumProps()
                                                        restargs = AbstractVal.join restargs, av
                                                        for i in [0...pslen]
                                                                rands[i] = AbstractVal.join rands[i], av
                                                else
                                                        for i in [0...pslen]
                                                                av = o.getOwnExactProp("#{i}-") or AbstractVal.AUNDEF
                                                                rands[i] = AbstractVal.join rands[i], av
                                                        while true # search for extra arguments
                                                                av = o.getOwnExactProp i++ + "-"
                                                                # to find the end of the array, we must see that
                                                                # an elm *definitely* doesn't exist, different
                                                                # from AbstractVal.AUNDEF
                                                                break if !av
                                                                restargs = AbstractVal.join restargs, av
                                else
                                        rands = buildArray pslen, AbstractVal.BOTTOM
                                # do function call
                                rands.unshift recv
                                rands.push restargs
                                ans = evalFun clos, rands, false, cn
                                retval = AbstractVal.join retval, ans.v
                                errval = AbstractVal.maybejoin errval, ans.err
                        return new Answer(retval, undefined, errval))


                # Array.prototype
                ap = new AbstractObj({ addr: newCount(), proto: opav })
                apav = makeObjAbstractVal(count)

                putelms = (args) ->
                        args[0].forEachObj (o) -> o.updateNumProps args[i] for i in [0...args.length]
                        new Answer(AbstractVal.ANUM)

                getelms = (args) ->
                        av = AbstractVal.BOTTOM
                        args[0].forEachObj (o) -> av = AbstractVal.join(av, o.getNumProps())
                        new Answer(av)

                ap.attachMethod "concat", 0,
                        # lose precision by not creating a new array
                        (args) ->
                                thisarr = args[0]
                                av = AbstractVal.BOTTOM
                                # if arg is base, join it, if it's array join its elms
                                for i in [1...args.length]
                                        avarg = args[i]
                                        av = AbstractVal.join(av, avarg.getBase())
                                        avarg.forEachObj (o) ->
                                                av = AbstractVal.join(av, o.getNumProps()) if o.isArray()
                                        thisarr.forEachObj (o) -> o.updateNumProps(av)
                                new Answer(thisarr)

                ap.attachMethod("join",     1, toStr)
                ap.attachMethod("pop",      0, getelms)
                ap.attachMethod("push",     0, putelms)
                ap.attachMethod("slice",    2, toThis)
                ap.attachMethod("sort",     1, toThis)
                ap.attachMethod("splice",   0, toThis)
                ap.attachMethod("shift",    0, getelms)
                ap.attachMethod("toString", 0, toStr)
                ap.attachMethod("unshift",  0, putelms)

                # Array
                _Array = do ->
                        # This object is used when Array is called w/out new 
                        new AbstractObj({ addr: newCount(), proto: apav })
                        nonewav = makeObjAbstractVal(count)

                        return (args, withNew) ->
                                retval = if withNew then args[0] else nonewav
                                arglen = args.length
                                retval.forEachObj (o) ->
                                        o.updateProto(apav)
                                        if o.getOwnExactProp("length-")
                                                o.updateProp("length-", AbstractVal.ANUM)
                                        else
                                                o.addProp("length-", {aval:AbstractVal.ANUM, enumerable:false})
                                if arglen <= 2 # new Array(), new Array(size)
                                        # nothing
                                else # new Array(elm1, ... , elmN)
                                        retval.forEachObj (o) ->
                                                for i in [1...arglen]
                                                        o.updateProp("#{(i - 1)}-", args[i])
                                new Answer(retval)

                JSCore.array_constructor = _Array
                a = go.attachMethod("Array", 0, _Array)
                aav = makeObjAbstractVal(count)
                a.addProp("prototype-",    {aval:apav, writable:false, enumerable:false, configurable:false})
                ap.addProp("constructor-", {aval:aav,                  enumerable:false})

                do ->
                        # Number.prototype
                        np = new AbstractObj({ addr: newCount(), proto: opav })
                        npav = makeObjAbstractVal(count)
                        np.attachMethod("toString", 0, toStr)
                        np.attachMethod("valueOf",  0, toNum)
                        # create generic number object
                        new AbstractObj({ addr: newCount(), proto: npav})
                        JSCore.generic_number_av = makeObjAbstractVal(count)

                        # Number
                        _Number = (args, withNew) ->
                                if withNew
                                        args[0].forEachObj (o) ->o.updateProto(npav)
                                        return new Answer(args[0])
                                Answer(AbstractVal.ANUM)

                        n = go.attachMethod("Number", 0, _Number)
                        nav = makeObjAbstractVal(count)
                        n.addProp("prototype-",    {aval:npav, writable:false, enumerable:false, configurable:false})
                        np.addProp("constructor-", {aval:nav,                  enumerable:false})

                do ->
                        # String.prototype
                        sp = new AbstractObj({ addr: newCount(), proto: opav })
                        spav = makeObjAbstractVal(count)
                        sp.attachMethod("charAt",      1, toStr)
                        sp.attachMethod("charCodeAt",  1, toNum)
                        sp.attachMethod("indexOf",     2, toNum)
                        sp.attachMethod("lastIndexOf", 2, toNum)
                        # all Arrays returned by calls to match are merged in one
                        omatch = new AbstractObj({ addr: newCount() })
                        omatchav = AbstractVal.join(AbstractVal.ANULL, makeObjAbstractVal(count))
                        JSCore.array_constructor([omatchav], true)
                        omatch.updateNumProps(AbstractVal.ASTR)
                        omatch.addProp("index-", {aval:AbstractVal.ANUM})
                        omatch.addProp("input-", {aval:AbstractVal.ASTR})
                        sp.attachMethod("match",       1, (args) -> new Answer(omatchav))
                        sp.attachMethod("replace",     2, toStr)
                        sp.attachMethod("slice",       2, toStr)
                        sp.attachMethod("substr",      2, toStr)
                        sp.attachMethod("substring",   2, toStr)
                        sp.attachMethod("toLowerCase", 0, toStr)
                        sp.attachMethod("toString",    0, toStr)
                        sp.attachMethod("toUpperCase", 0, toStr)
                        # all Arrays returned by calls to split are merged in one
                        osplit = new AbstractObj({ addr: newCount() })
                        osplitav = makeObjAbstractVal(count)
                        JSCore.array_constructor([osplitav], true)
                        osplit.updateNumProps(AbstractVal.ASTR)
                        sp.attachMethod("split",   2, (args) -> new Answer(osplitav))
                        sp.attachMethod("valueOf", 0, toStr)
                        # create generic string object
                        new AbstractObj({ addr: newCount(), proto: spav })
                        JSCore.generic_string_av = makeObjAbstractVal(count)

                        # String
                        _String = (args, withNew) ->
                                if withNew
                                        args[0].forEachObj (o) -> o.updateProto(spav)
                                        return new Answer(args[0])
                                new Answer(AbstractVal.ASTR)

                        s = go.attachMethod("String", 1, _String)
                        sav = makeObjAbstractVal(count)
                        s.addProp("prototype-",    {aval:spav, writable:false, enumerable:false, configurable:false})
                        sp.addProp("constructor-", {aval:sav,                  enumerable:false})
                        s.attachMethod("fromCharCode", 0, toStr)

                do ->
                        # Error.prototype
                        ep = new AbstractObj({ addr: newCount(), proto: opav })
                        epav = makeObjAbstractVal(count)
                        ep.attachMethod("toString", 0, toStr)

                        # Error
                        _Error = (args) ->
                                args[0].forEachObj (o) ->
                                        o.updateProto(epav)
                                        o.updateProp("message-", args[1] or AbstractVal.ASTR)

                                new Answer(args[0])

                        e = go.attachMethod("Error", 1, _Error)
                        eav = makeObjAbstractVal(count)
                        e.addProp("prototype-",    {aval:epav, writable:false, enumerable:false, configurable:false})
                        ep.addProp("constructor-", {aval:eav,                  enumerable:false})
                        ep.addProp("name-",        {aval:AbstractVal.ASTR,     enumerable:false})

                        # SyntaxError.prototype
                        sep = new AbstractObj({ addr: newCount(), proto: epav })
                        sepav = makeObjAbstractVal(count)

                        # SyntaxError
                        _SyntaxError = (args) ->
                                args[0].forEachObj (o) ->
                                        o.updateProto(sepav)
                                        o.addProp("message-", {aval:AbstractVal.ASTR})

                                new Answer(args[0])

                        se = go.attachMethod("SyntaxError", 1, _SyntaxError)
                        seav = makeObjAbstractVal(count)
                        se.addProp("prototype-",    {aval:sepav, writable:false, enumerable:false, configurable:false})
                        sep.addProp("constructor-", {aval:seav,                  enumerable:false})
                        sep.addProp("name-",        {aval:AbstractVal.ASTR})

                        # toshok - more errors here...

                do ->
                        # RegExp.prototype
                        rp = new AbstractObj({ addr: newCount(), proto: opav })
                        rpav = makeObjAbstractVal(count)
                        # all Arrays returned by calls to exec are merged in one
                        oexec = new AbstractObj({ addr:newCount() })
                        oexecav = AbstractVal.join(AbstractVal.ANULL, makeObjAbstractVal(count))
                        JSCore.array_constructor([oexecav], true)
                        oexec.updateNumProps(AbstractVal.ASTR)
                        oexec.addProp("index-", {aval:AbstractVal.ANUM})
                        oexec.addProp("input-", {aval:AbstractVal.ASTR})
                        rp.attachMethod("exec", 1, (args) -> new Answer(oexecav))
                        rp.attachMethod("test", 1, toBool)

                        # RegExp
                        _RegExp = (args) ->
                                args[0].forEachObj (o) ->
                                        o.updateProto(rpav)
                                        o.addProp("global-",     {aval:AbstractVal.ABOOL, writable:false, enumerable:false, configurable:false})
                                        o.addProp("ignoreCase-", {aval:AbstractVal.ABOOL, writable:false, enumerable:false, configurable:false})
                                        o.addProp("lastIndex-",  {aval:AbstractVal.ANUM,                  enumerable:false, configurable:false})
                                        o.addProp("multiline-",  {aval:AbstractVal.ABOOL, writable:false, enumerable:false, configurable:false})
                                        o.addProp("source-",     {aval:AbstractVal.ASTR,  writable:false, enumerable:false, configurable:false})

                                new Answer(args[0])

                        JSCore.regexp_constructor = _RegExp
                        r = go.attachMethod("RegExp", 2, _RegExp)
                        rav = makeObjAbstractVal(count)
                        r.addProp("prototype-",    {aval:rpav, writable:false, enumerable:false, configurable:false})
                        rp.addProp("constructor-", {aval:rav,                  enumerable:false})

                do ->
                        # Date.prototype
                        dp = new AbstractObj({ addr:newCount(), proto:opav })
                        dpav = makeObjAbstractVal(count)
                        dp.attachMethod("getDate",           0, toNum)
                        dp.attachMethod("getDay",            0, toNum)
                        dp.attachMethod("getFullYear",       0, toNum)
                        dp.attachMethod("getHours",          0, toNum)
                        dp.attachMethod("getMilliseconds",   0, toNum)
                        dp.attachMethod("getMinutes",        0, toNum)
                        dp.attachMethod("getMonth",          0, toNum)
                        dp.attachMethod("getSeconds",        0, toNum)
                        dp.attachMethod("getTime",           0, toNum)
                        dp.attachMethod("getTimezoneOffset", 0, toNum)
                        dp.attachMethod("getYear",           0, toNum)
                        dp.attachMethod("setTime",           1, toNum)
                        dp.attachMethod("toString",          0, toStr)
                        dp.attachMethod("valueOf",           0, toNum)

                        # Date
                        _Date = (args, withNew) ->
                                if withNew
                                        args[0].forEachObj (o) -> o.updateProto(dpav)
                                        return new Answer(args[0])
                                new Answer(AbstractVal.ASTR)

                        d = go.attachMethod("Date", 0, _Date)
                        dav = makeObjAbstractVal(count)
                        d.addProp("prototype-",    {aval:dpav, writable:false, enumerable:false, configurable:false})
                        dp.addProp("constructor-", {aval:dav,                  enumerable:false})

                do ->
                        # Math
                        m = new AbstractObj({ addr: newCount(), proto: opav })
                        mav = makeObjAbstractVal(count)
                        go.addProp("Math-",       {aval:mav,                              enumerable:false})
                        m.addProp("constructor-", {aval:oav,                              enumerable:false})
                        m.addProp("E-",           {aval:AbstractVal.ANUM, writable:false, enumerable:false, configurable:false})
                        m.addProp("LN10-",        {aval:AbstractVal.ANUM, writable:false, enumerable:false, configurable:false})
                        m.addProp("LN2-",         {aval:AbstractVal.ANUM, writable:false, enumerable:false, configurable:false})
                        m.addProp("LOG10E-",      {aval:AbstractVal.ANUM, writable:false, enumerable:false, configurable:false})
                        m.addProp("LOG2E-",       {aval:AbstractVal.ANUM, writable:false, enumerable:false, configurable:false})
                        m.addProp("PI-",          {aval:AbstractVal.ANUM, writable:false, enumerable:false, configurable:false})
                        m.addProp("SQRT1_2-",     {aval:AbstractVal.ANUM, writable:false, enumerable:false, configurable:false})
                        m.addProp("SQRT2-",       {aval:AbstractVal.ANUM, writable:false, enumerable:false, configurable:false})
                        m.attachMethod("abs",    1, toNum)
                        m.attachMethod("acos",   1, toNum)
                        m.attachMethod("asin",   1, toNum)
                        m.attachMethod("atan",   1, toNum)
                        m.attachMethod("atan2",  1, toNum)
                        m.attachMethod("ceil",   1, toNum)
                        m.attachMethod("cos",    1, toNum)
                        m.attachMethod("exp",    1, toNum)
                        m.attachMethod("floor",  1, toNum)
                        m.attachMethod("log",    1, toNum)
                        m.attachMethod("max",    0, toNum)
                        m.attachMethod("min",    0, toNum)
                        m.attachMethod("pow",    2, toNum)
                        m.attachMethod("random", 0, toNum)
                        m.attachMethod("round",  1, toNum)
                        m.attachMethod("sin",    1, toNum)
                        m.attachMethod("sqrt",   1, toNum)
                        m.attachMethod("tan",    1, toNum)

                do ->
                        # Boolean.prototype
                        bp = new AbstractObj({ addr: newCount(), proto: opav })
                        bpav = makeObjAbstractVal(count)
                        bp.attachMethod("toString", 0, toStr)
                        bp.attachMethod("valueOf",  0, toBool)
                        # create generic boolean object
                        new AbstractObj({ addr: newCount(), proto: bpav })
                        JSCore.generic_boolean_av = makeObjAbstractVal(count)

                        # Boolean
                        _Boolean = (args, withNew) ->
                                if withNew
                                        args[0].forEachObj (o) -> o.updateProto(bpav)
                                        return new Answer(args[0])
                                new Answer(AbstractVal.ABOOL)
                        b = go.attachMethod("Boolean", 1, _Boolean)
                        bav = makeObjAbstractVal(count)
                        b.addProp("prototype-",    {aval:bpav, writable:false, enumerable:false, configurable:false})
                        bp.addProp("constructor-", {aval:bav,                  enumerable:false})

                do ->
                        _console = new AbstractObj({ addr: newCount(), proto: opav })
                        consoleav = makeObjAbstractVal(count)
                        go.addProp("console-",       {aval:consoleav,                               enumerable:false})
                        _console.attachMethod("log",    0, toVoid)
                        _console.attachMethod("warn",   0, toVoid)
                        

        globalObjectAbstractVal: () -> JSCore.global_object_av
        globalObjectAbstractValAddr: () -> JSCore.global_object_av_addr

##############################################
##############   UTILITIES  ##################
##############################################

Array::memq = (sth) ->
        for el in @
                return el if sth is el
        false

# starting at index, remove all elms that satisfy the pred in linear time.
Array::rmElmAfterIndex = (pred, index) ->
        return if index >= @length

        i = index
        j = index
        len = @length

        while i < len
                if not pred @[i]
                        @[j] = @[i]
                        j += 1
                        i += 1
        @length = j

# remove all duplicates from array (keep first occurence of each elm)
# pred determines the equality of elms
Array::rmDups = (pred) ->
        i = 0
        while i < (@length - 1)
                p = (elm) => pred elm, @[i]
                @rmElmAfterIndex p, i+1
                i += 1

# compare two arrays for structural equality
arrayeq = (eq, a1, a2) ->
        len = a1.length
        return false if len isnt a2.length
        for i in [0...a1.length]
                return false if not eq a1[i], a2[i]
        true

buildArray = (size, elm) ->
        a = new Array size
        for i in [0...size]
                a[i] = elm
        a

buildString = (size, elm) ->
        buildArray(size, elm).join("")

# merge two sorted arrays of numbers into a sorted new array, remove dups!
arraymerge = (a1, a2) ->
        i = 0
        j = 0
        len1 = a1.length
        len2 = a2.length
        a = new Array

        while true
                if i is len1
                        while j < len2
                                a.push a2[j]
                                j += 1
                        return a
                if j is len2
                        while i < len1
                                a.push a1[i]
                                i += 1
                        return a
                diff = a1[i] - a2[j]
                if diff < 0
                        a.push a1[i++]
                else if diff > 0
                        a.push a2[j++]
                else
                        i++

astSize = 0

# count is used to generate a unique ID for each node in the AST.
count = 0

newCount = () ->
        count += 1
        return count

# number -> AbstractVal
# When creating an abs. value, it can contain at most one object
makeObjAbstractVal = (objaddr) ->
        v = new AbstractVal
        v.base = 0
        v.objs = [objaddr]
        v

# string -> AbstractVal
makeStrLitAbstractVal = (s) ->
        v = new AbstractVal
        v.base = 2
        v.objs = []
        v.str = s
        v

#////////////////////////////////////////////////////////////////////////////////
#//////////////////////////   EVALUATION PREAMBLE   /////////////////////////////
#////////////////////////////////////////////////////////////////////////////////

# frame, identifier node, AbstractVal -> void
frameSet = (fr, param, val) ->
        fr[param.addr] = [val, timestamp] # record when param was bound to val
        return

# frame, identifier node -> AbstractVal
frameGet = (fr, param) ->
        pa = param.addr
        binding = fr[pa]
        if binding[1] < modified[pa]
                # if binding changed in heap, change it in frame to be sound
                binding[0] = AbstractVal.join(binding[0], heap[pa])
                binding[1] = timestamp
        return binding[0]

# fun. node, array of AbstractVal, timestamp  -> [AbstractVal, AbstractVal] or false
searchSummary = (n, args, ts) ->
        #console.log "searching summary for `#{n.id.name}'!"
        n_summaries = summaries[n.addr]
        if n_summaries.ts < ts
                #console.log "returning false"
                return false
        
        insouts = n_summaries.insouts
        # Start from the end to find the elm that was pushed last
        for i in [insouts.length-1..0]
                summary = insouts[i]
                # If no widening, turn AbstractVal.lt to AbstractVal.eq in the next line.
                if arrayeq(AbstractVal.lt, args, summary[0])
                        return summary.slice(-2)
        #console.log "default, returning false"
        false


# function node -> boolean
# check if any summary exists for this function node
existsSummary = (n) ->
        #console.log "checking summary for `#{n.id.name}'!"
        summaries[n.addr].ts isnt INVALID_TIMESTAMP

# fun. node, array of AbstractVal, AbstractVal, AbstractVal or undefined, timestamp  -> void
addSummary = (n, args, retval, errval, ts) ->
        #console.log "adding summary for `#{n.id.name}'!"
        addr = n.addr
        summary = summaries[addr]
        if summary.ts is ts
                summary.insouts.push([args, retval, errval])
        else if summary.ts < ts # discard summaries for old timestamps.
                summary.ts = ts
                summary.insouts = [[args, retval, errval]]
        # join new summary w/ earlier ones.
        insjoin = summary.type[0]
        for i in [0...insjoin.length]
                insjoin[i] = AbstractVal.join(insjoin[i], args[i] or AbstractVal.AUNDEF) #arg mismatch
        summary.type[1] = AbstractVal.join(summary.type[1], retval)
        summary.type[2] = AbstractVal.maybejoin(summary.type[2], errval)


showSummaries = () ->
        for own addr of summaries
                f = heap[addr].getFun()
                console.log("#{f.id.name}: #{funToType(f)}")

# function node, array of Aval -> number or undefined
# How evalFun interprets the return value of searchPending
# Zero: new frame on the stack
# Positive number: throw to clear the stack b/c of timestamp increase
# Negative number: throw to clear the stack during recursion
# undefined: return w/out throwing during recursion
searchPending = (n, args) ->
        bucket = pending[n.addr]
        len = bucket.length
        # We use the number of pending calls to n to clear the stack.
        return 0   if len is 0
        return len if bucket[0].ts < timestamp
        # Invariant: no two sets of args are related in \sqsubseteq.
        for i in [0...len]
                # No need to keep going, a more general frame is pending.
                return if arrayeq(AbstractVal.lt, args, bucket[i].args)
                        
                # The deeper frame can be widened, throw to it.
                if arrayeq(AbstractVal.lt, bucket[i].args, args)
                        return i - len
        0

# function node, {args, timestamp} -> void
addPending = (n, elm) -> pending[n.addr].push(elm)

# function node -> {args, timestamp}
rmPending = (n) ->
	# Uncomment when debugging
        # var elm = pending[n.addr].pop();
        # if (!elm) throw errorWithCode(CFA_ERROR, "Remove from empty pending.");
        # return elm;
        pending[n.addr].pop()


# evalExp & friends use Answer to return tuples
class Answer
        constructor: (@v, @fr, @err) ->
                # v:   EvalExp puts abstracts values here, evalStm puts statements
                # fr:  frame
                # err: AbstractVal for exceptions thrown
        toString: (indent) ->
                @v.toString(indent)

class AbstractProp
        # Constructor for abstract properties
        # Takes an object w/ the property's attributes
        constructor: (attribs) ->
                @aval = attribs.aval
                # writable, enumerable and configurable default to true
                @writable =     if "writable"     in attribs then attribs.writable     else true
                @enumerable =   if "enumerable"   in attribs then attribs.enumerable   else true
                @configurable = if "configurable" in attribs then attribs.configurable else true

        # optional number -> string
        toString: (indent) ->
                @aval.toString(indent)

class AbstractVal

        # number -> AbstractVal
        makeBaseAbstractVal = (base) ->
                v = new AbstractVal
                v.base = base
                v.objs = []
                v
                
        # used by parts of the code that don't know the representation of AbstractVal
        getBase: () -> makeBaseAbstractVal @base

        # void -> string or undefined
        getStrLit: () -> @str

        # Merge ATRU and AFALS to one generic boolean in the base b
        mergeBoolsInBase: (b) ->
                if b & 8
                        b |= 4
                b = ((b & 48) >> 1) | (b & 7)

        # Takes an optional array for cycle detection.
        toType: (seenObjs) ->
                i = 1
                base = @mergeBoolsInBase @base
                types = []

                basetypes = {1:"number", 2:"string", 4:"boolean", 8:"undefined", 16:"null"}
                while i <= 16
                        if (base & i) is i
                                types.push basetypes[i]
                        i *= 2

                # If uncommented, tags show string constants where possible.
                # if ((base & 2) && (this.str !== undefined)) {
                #   types.rmElmAfterIndex(function(s) {return s === "string";}, 0)
                #   types.push("\"" + this.str + "\"")
                # }
                seenObjs or (seenObjs = [])
                slen = seenObjs.length
                @forEachObj (o) ->
                        types.push o.toType seenObjs
                        seenObjs.splice slen, seenObjs.length

                return "any" if types.length is 0
                
                normalizeUnionType types
                return types[0] if types.length is 1
                
                "<#{types.join(' | ')}>"

        # void -> AbstractVal
        # Used when scalars need to be converted to objects
        baseToObj: () ->
                return @ if (@base & 15) is 0
                av = makeBaseAbstractVal(0)
                av = AbstractVal.join(av, JSCore.generic_string_av)  if @base & 2 isnt 0
                av = AbstractVal.join(av, JSCore.generic_number_av)  if @base & 1 isnt 0
                av = AbstractVal.join(av, JSCore.generic_boolean_av) if @base & 12 isnt 0
                av.objs = @objs
                av

        # fun takes an AbstractObj
        forEachObj: (fun) ->
                objaddrs = @objs
                objaddrs.forEach (addr) -> fun heap[addr]

        # Like forEachObj but fun returns a boolean; if it's true, we stop.
        forEachObjWhile: (fun) ->
                objaddrs = @objs
                len = objaddrs.length
                if len is 1 # make common case faster
                        fun heap[objaddrs[0]]
                else
                        i = 0
                        cont = false
                        while not cont and i < len
                                cont = fun heap[objaddrs[i]]
                                i += 1

        # string -> AbstractVal
        getProp: (pname) ->
                av = AbstractVal.BOTTOM
                @forEachObj (o) ->
                        av = AbstractVal.join av, o.getProp(pname) or AbstractVal.AUNDEF

                av

        # string, AbstractVal -> void
        updateProp: (pname, av) ->
                @forEachObj (o) ->
                        o.updateProp pname, av

        # array of AbstractVal, node -> Answer
        # Call each function with args. args[0] is what THIS is bound to.
        # FIXME: record error if rator contains base vals and non-functions
        callFun: (args, callNode) ->
                retval = AbstractVal.BOTTOM
                debugCalls = 0
                errval = undefined
                ans = undefined

                @baseToObj().forEachObj (o) ->
                        clos = o.getFun()
                        return if not clos
                        debugCalls += 1
                        ans    = evalFun(clos, args, false, callNode)
                        retval = AbstractVal.join(retval, ans.v)
                        errval = AbstractVal.maybejoin(errval, ans.err)

                # var ratorNode = callNode && callNode.children[0]
                # if (!debugCalls) {
                #   var funName, ln = ratorNode.lineno
                #   switch (ratorNode.type) {
                #   case IDENTIFIER:
                #     funName = ratorNode.name
                #     break
                #   case FUNCTION:
                #     funName = ratorNode.id.name
                #     break
                #   case DOT:
                #     if (ratorNode.children[1].type === STRING) {
                #       funName = ratorNode.children[1].value.slice(0, -1)
                #       break
                #     }
                #     # fall thru
                #   default:
                #     funName = "??"
                #     break
                #   }
                #   if (args[0] === JSCore.global_object_av)
                #     print("unknown function: " + funName + ", line " + (ln || "??"))
                #   else
                #     print("unknown method: " + funName + ", line " + (ln || "??"))
                # }
                new Answer(retval, undefined, errval)

        hasNum:  () -> @base & 1
        hasStr:  () -> @base & 2
        hasObjs: () -> @objs.length > 0

        # returns true if it can guarantee that the AbstractVal is falsy.
        isFalsy: () -> return @objs.length is 0 and @base isnt 0 and (@base % 8 is 0 or (@base is 2 and @str is "-"))

        # returns true if it can guarantee that the AbstractVal is truthy.
        isTruthy: () -> return (@objs.length is 0 and ((@base is 2 and @str isnt "-") or @base is 4))

        # optional number -> string
        toString: (indent) ->
                i1 = buildString(indent or 0, " ")
                i = 1
                base = @mergeBoolsInBase @base
                types = []
                basetypes = {1 : "number", 2 : "string", 4 : "boolean", 8 : "undefined", 16 : "null"}
                while i <= 16
                        if (base & i) is i
                                types.push basetypes[i]
                        i *= 2
                "#{i1}Base: #{types.join(', ')}\n#{i1}Objects: #{@objs.join(', ')}\n"

        # AbstractVal, AbstractVal -> AbstractVal
        @join: (v1, v2) ->
                os1  = v1.objs
                os1l = os1.length
                
                os2  = v2.objs
                os2l = os2.length

                b1 = v1.base
                b2 = v2.base
                av = makeBaseAbstractVal b1 | b2

                if av.base & 2
                        if b1 & 2
                                if (not (b2 & 2)) or v1.str is v2.str
                                        av.str = v1.str
                        else # (b2 & 2) is truthy
                                av.str = v2.str

                if os1l is 0
                        av.objs = os2 # need a copy of os2 here? I think not.
                else if os2l is 0
                        av.objs = os1 # need a copy of os1 here? I think not.
                else if os1 is os2
                        av.objs = os1
                else if os1l is os2l
                        i = 0
                        i += 1 while os1[i] is os2[i] and i < os1l
                        if i is os1l
                                av.objs = v2.objs = os1
                                return av

                        av.objs = arraymerge os1, os2
                else # merge the two arrays
                        av.objs = arraymerge os1, os2
                av


        # AbstractVal or undefined, AbstractVal or undefined -> AbstractVal or undefined
        @maybejoin: (v1, v2) ->
                return v1 if not v1
                return v1 if not v2
                @join v1, v2

        # AbstractVal, AbstractVal -> boolean
        # compares abstract values for equality
        @eq: (v1, v2) ->
                return false if v1.base isnt v2.base
                return false if v1.str isnt v2.str

                os1 = v1.objs
                os2 = v2.objs
                len = os1.length
                
                return false if len isnt os2.length
                return true if os1 is os2

                for i in [0...len]
                        return false if os1[i] isnt os2[i]
                true

        # AbstractVal, AbstractVal -> boolean
        # returns true if v1 is less than v2
        @lt: (v1, v2) ->
                b1 = v1.base
                b2 = v2.base
                
                return false if b1 > (b1 & b2)
                return false if (b1 & 2) and ("str" in v2) and v2.str isnt v1.str

                os1 = v1.objs
                os1l = os1.length
                os2 = v2.objs
                os2l = os2.length

                return true if os1l is 0 or os1 is os2
                return false if os1l > os2l

                j = 0
                for i in [0...os1l]
                        j += 1 while os2[j] < os1[i]
                        if j is os2l or os1[i] isnt os2[j]
                                return false # there's an elm is os1 that's not in os2
                true

        # abstract plus
        @plus: (v1, v2) ->
                if v1.objs.length isnt 0 or v2.objs.length isnt 0
                        return makeBaseAbstractVal 3
                base = ((v1.base | v2.base) & 2) # base is 0 or 2
                if (v1.base & 61) isnt 0 and (v2.base & 61) isnt 0
                        base |= 1
                makeBaseAbstractVal base


        # function node -> AbstractVal
        # If the program doesn't set a function's prototype property, create default.
        @makeDefaultProto: (n) ->
                paddr = n.defaultProtoAddr
                o = new AbstractObj { addr: paddr, proto: @object_prototype_av }
                o["constructor-"] = new AbstractProp {aval: makeObjAbstractVal(n.addr), enumerable: false}
                makeObjAbstractVal paddr

        # heap address, AbstractVal -> void
        @updateHeapAv: (addr, newv) ->
                oldv = heap[addr] # oldv shouldn't be undefined
                if not @lt newv, oldv
                        heap[addr] = @join oldv, newv
                        #print("++ts: 7")
                        modified[addr] = ++timestamp

        @BOTTOM: makeBaseAbstractVal(0)
        @ANUM:   makeBaseAbstractVal(1)
        @ASTR:   makeBaseAbstractVal(2)
        @ATRU:   makeBaseAbstractVal(4)
        @AFALS:  makeBaseAbstractVal(8)
        @ABOOL:  makeBaseAbstractVal(12)
        @AUNDEF: makeBaseAbstractVal(16)
        @ANULL:  makeBaseAbstractVal(32)

# node, optional script node -> boolean
# returns true iff n contains RETURN directly (not RETURNs of inner functions).
# toshok - this is pretty terrible.  it destructively updates the ast, changing node types, splicing them out, etc

class FixAST extends TreeVisitor
        visitProgram: (n, scope) ->
                n.varDecls = [] if not n.varDecls?
                fixAST(stm, n) for stm in n.body
                false
                
        visitFunctionDeclaration: (n, scope) ->
                throw new Error("there shouldn't be FunctionDeclarations by this point") if not n.toplevel
                n.hasReturn = fixAST n.body
                false
                
        visitFunctionExpression: (n, scope) ->
                n.hasReturn = fixAST n.body
                false

        visitArrowFunctionExpression: (n, scope) ->
                n.hasReturn = fixAST n.body
                false

        visitBlock: (n, scope) ->
                n.varDecls = [] if not n.varDecls?
                ans = false
                for stm in n.body
                        ans = fixAST(stm, n) || ans
                ans

        visitLabeledStatement: (n, scope) -> fixAST n.body, scope

        visitEmptyStatement: (n, scope) -> false

        visitExpressionStatement: (n, scope) ->
                fixAST n.expression, scope
                false
                
        visitSwitch: (n, scope) ->
                fixAST n.discriminant, scope
                for c in n.cases
                        return true if fixAST(c, scope)
                false
                
        visitCase: (n, scope) ->
                fixAST n.test, scope
                fixAST n.consequent, scope
                
        visitFor: (n, scope) ->
                fixAST(n.init, scope)   if n.init?
                fixAST(n.test, scope)   if n.test?
                fixAST(n.update, scope) if n.update?
                fixAST(n.body, scope)
                
        visitWhile: (n, scope) ->
                fixAST(n.test, scope)
                fixAST(n.body, scope)
                
        visitIf: (n, scope) ->
                fixAST(n.test, scope)
                ans = fixAST(n.consequent, scope)
                ans = (fixAST(n.alternate, scope) or ans) if n.alternate?
                ans
                
        visitForIn: (n, scope) ->
                fixAST(n.left, scope)
                fixAST(n.right, scope)
                fixAST(n.body, scope)
                
        visitForOf: (n, scope) ->
                fixAST(n.left, scope)
                fixAST(n.right, scope)
                fixAST(n.body, scope)
                
        visitDo: (n, scope) ->
                fixAST(n.test, scope)
                fixAST(n.body, scope)
                
        visitIdentifier:     (n, scope) -> false
        visitLiteral:        (n, scope) -> false
        visitThisExpression: (n, scope) -> false
        visitBreak:          (n, scope) -> false
        visitContinue:       (n, scope) -> false
                
        visitTry: (n, scope) ->
                ans = fixAST(n.block, scope)
                for c in n.handlers
                        ans = (fixAST(c.block, scope) or ans)
                ans = (fixAST(n.finalizer, scope) or ans) if n.finalizer?
                ans

        visitCatchClause: (n, scope) ->
                fixAST(n.param, scope)
                fixAST(n.guard, scope)
                fixAST(n.body, scope)
                
        visitThrow: (n, scope) ->
                fixAST(n.argument, scope)
                false
                
        visitReturn: (n, scope) ->
                fixAST(n.argument, scope)
                true
                
        visitVariableDeclaration: (n, scope) ->
                fixAST(decl, scope) for decl in n.declarations
                false

        visitVariableDeclarator: (n, scope) ->
                console.log "pushing vardecl #{n.id.name}"
                n.id.addr = newCount();
                scope.varDecls.push(n.id)
                fixAST(n.init, scope)
                false
                                
        visitAssignmentExpression: (n, scope) ->
                fixAST(n.left, scope)
                fixAST(n.right, scope)
                false
                
        visitConditionalExpression: (n, scope) ->
                fixAST(n.test, scope)
                fixAST(n.consequent, scope)
                fixAST(n.alternate, scope)
                false
                
        visitLogicalExpression: (n, scope) ->
                fixAST(n.left, scope)
                fixAST(n.right, scope)
                false
                
        visitBinaryExpression: (n, scope) ->
                fixAST(n.left, scope)
                fixAST(n.right, scope)
                false

        visitUnaryExpression: (n, scope) ->
                ###
                if n.operator is "-" and is_number_literal(n.argument)
                        n.type = Literal
                        n.value = -n.argument.value
                        n.raw = "#{n.value}"
                        delete n.argument
                else
                        n.argument = fixAST(n.argument)
                ###
                fixAST(n.argument, scope)
                false

        visitUpdateExpression: (n, scope) ->
                fixAST(n.argument, scope)
                false

        visitMemberExpression: (n, scope) ->
                fixAST(n.object, scope)
                fixAST(n.property, scope)
                # toshok - i don't like this:
                if n.computed and is_number_literal(n.property)
                        n.property.raw = "'#{n.property.value}'"
                        n.property.value = "#{n.property.value}-"
                false
                
        visitSequenceExpression: (n, scope) ->
                fixAST(exp, scope) for exp in n.expressions
                false
                
        visitNewExpression: (n, scope) ->
                fixAST(n.callee, scope)
                fixAST(arg, scope) for arg in n.arguments
                false

        visitObjectExpression: (n, scope) ->
                # toshok - FIXME need to support get/set elements
                fixAST(prop, scope) for prop in n.properties
                false

        visitArrayExpression: (n, scope) ->
                fixAST(elm, scope) for elm in n.elements
                false

        visitProperty: (n, scope) ->
                fixAST(n.key, scope)
                fixAST(n.value, scope)
                false
                                
        visitCallExpression: (n, scope) ->
                fixAST(n.callee, scope)
                fixAST(arg, scope) for arg in n.arguments
                false

        ###
        visitModuleDeclaration: (n, scope) ->
                n.id = fixAST n.id, scope
                n.body = fixAST n.body, scope
                n

        visitExportDeclaration: (n, scope) ->
                n.declaration = fixAST n.declaration, scope
                n
                
        visitImportDeclaration: (n, scope) ->
                fixAST(spec, scope) For spec in n.specifiers
                n

        visitImportSpecifier: (n, scope) ->
                n.id = fixAST n.id, scope
                n

        visitArrayPattern: (n, scope) ->
                fixAST(elm, scope) for prop in n.elements
                n

        visitObjectPattern: (n, scope) ->
                fixAST(prop, scope) for prop in n.properties
                n
        ###

        visit: (n, scope) ->
                astSize += 1
                super

fixAST = do ->
        o = new FixAST
        o.visit.bind(o)

# Invariants of the AST after fixStm:
# - no VAR and CONST nodes, they've become semicolon comma nodes.
#   Unfortunately, this isn't independent of exceptions.
#   If a finally-block breaks or continues, the exception isn't propagated.
#   I will falsely propagate it (still sound, just more approximate).
# - function nodes only in blocks, not in scripts
# - no empty SEMICOLON nodes
# - no switches w/out branches
# - each catch clause has a property exvar which is an IDENTIFIER node
# - all returns have .value (the ones that didn't, got an undefined)
# - the property names in DOT and OBJECT_INIT end with a dash.
# - value of a NUMBER can be negative (UNARY_MINUS of constant became NUMBER).
# - each function node has a property hasReturn to show if it uses RETURN.
# - Array literals don't have holes (null children) in them.

class LabelAST extends TreeVisitor
        visitArrayExpression: (n) ->
                n = super
                n.addr = newCount()
                n

        visitNewExpression: (n) ->
                n = super
                n.addr = newCount()
                n

        visitLiteral: (n) ->
                n = super
                if typeof n.raw is "string" and n.raw[0] is '/'
                        # it's a regexp
                        n.addr = newCount()
                n
                
        visitObjectExpression: (n) ->
                n = super
                n.addr = newCount()
                n

        visitCatchClause: (n) ->
                n = super
                n.param.addr = newCount()
                n

        visitFunction: (n) ->
                n.addr = newCount()
                n.defaultProtoAddr = newCount()
                n.id.addr = newCount() if n.id?
                n.params.forEach (p) ->
                        p.addr = newCount()
                labelAST(n.body)
                n

labelAST = do ->
        o = new LabelAST
        o.visit.bind(o)
                
STACK = 0
HEAP = 1

        
# node, array of id nodes, array of id nodes -> void
# Classify variable references as either stack or heap references.
class TagVarRefs extends TreeVisitor

        visitIdentifier: (n, innerscope, otherscopes, extra) ->
                varname = n.name
                console.log "looking up #{varname}"
                # search var in innermost scope
                if innerscope.length > 0
                        for i in [innerscope.length-1..0]
                                boundvar = innerscope[i]
                                if boundvar.name is varname
                                        console.log "stack ref: #{varname}"
                                        n.kind = STACK
                                        # if boundvar is a heap var and some of its heap refs get mutated,
                                        # we may need to update bindings in frames during the cfa.
                                        n.addr = boundvar.addr
                                        return n
                                
                # search var in other scopes
                if otherscopes.length > 0
                        for i in [otherscopes.length-1..0]
                                boundvar = otherscopes[i]
                                if boundvar.name is varname
                                        console.log "heap ref: #{varname}"
                                        n.kind = HEAP
                                        boundvar.kind = HEAP
                                        n.addr = boundvar.addr
                                        flags[boundvar.addr] = true
                                        return n

                # var has no binder in the program
                if commonJSmode and varname is "exports"
                        n.kind = HEAP
                        n.addr = exports_object_av_addr
                        p = extra # exported property name passed as extra arg
                        if p? and p.type is Identifier
                                exports_object.lines[p.name.slice(0, -1)] = p.lineno
                        return n
                        
                #print("global: " + varname + " :: " + n.lineno)
                n.type = MemberExpression
                nthis = ast.identifier("internalVar: global object")
                nthis.kind = HEAP
                nthis.addr = JSCore.global_object_av_addr
                n.object = nthis
                n.property = ast.identifier("#{n.name}-")
                n

        visitMemberExpression: (n, innerscope, otherscopes) ->
                if n.computed
                        ###
                        shadowed = false
                        ch = n.children, n0 = ch[0], shadowed = false
                        # use new non-terminal only  if "arguments" refers to the arguments array
                        if (n0.type === IDENTIFIER && n0.name === "arguments") {
                          for (var i = innerscope.length - 1; i >= 0; i--) 
                            if (innerscope[i].name === "arguments") {
                              shadowed = true
                              break
                            }
                          if not shadowed
                            n.type = ARGUMENTS
                            n.arguments = innerscope; #hack: innerscope is params (maybe extended)
                            ch[0] = ch[1]
                            ch.splice(1, 1)
                            # undo the changes made for INDEX nodes only in fixExp
                            if (ch[0].type === STRING && propIsNumeric(ch[0].value)) {
                              ch[0].type = NUMBER
                              ch[0].value = ch[0].value.slice(0, -1) - 0
                            }
                          }
                        }
                        ###
                        tagVarRefs(n.object, innerscope, otherscopes)
                        tagVarRefs(n.property, innerscope, otherscopes)
                else
                        # don't classify property names
                        if commonJSmode and n.object.type is Identifier and n.object.name is "exports"
                                tagVarRefs(n.object, innerscope, otherscopes, n.property) # toshok - special extra arg, ugh
                        else 
                                tagVarRefs(n.object, innerscope, otherscopes)
                        n


        visitFunction: (n, innerscope, otherscopes) ->
                fn = n.id
                params = n.params
                len = otherscopes.length
                # extend otherscopes
                Array::push.apply(otherscopes, innerscope)
                # fun name is visible in the body & not a heap ref, add it to scope
                params.push(fn)
                tagVarRefs(n.body, params, otherscopes)
                params.pop()
                fn.kind = STACK if fn.kind isnt HEAP
                params.forEach (p) ->
                        if p.kind isnt HEAP
                                p.kind = STACK
                # trim otherscopes
                otherscopes.splice(len, innerscope.length)
                n

        _blocklike = (n, innerscope, otherscopes, extra) ->
                vdecls = n.varDecls
                # extend inner scope
                j = innerscope.length
                if vdecls?
                        if vdecls.length > 0
                                console.log escodegen.generate n
                        Array::push.apply(innerscope, vdecls)
                # tag the var refs in the body
                n.body.forEach (stm) -> tagVarRefs(stm, innerscope, otherscopes)
                # tag formals
                if vdecls?
                        vdecls.forEach (vd) ->
                                # for toplevel vars, assigning flags
                                # causes the Aval`s to be recorded in
                                # the heap. After the analysis, we use
                                # that for ctags.
                                flags[vd.addr] = true if extra is "toplevel"
                                vd.kind = STACK if vd.kind isnt HEAP
                # trim inner scope
                if vdecls?
                        innerscope.splice(j, vdecls.length)
                        console.log "after #{innerscope.length}"
                n

        visitBlock: _blocklike
        visitProgram: _blocklike

        visitTry: (n, innerscope, otherscopes) ->
                tagVarRefs(n.tryBlock, innerscope, otherscopes)
                n.handlers.forEach (clause) ->
                        xv = clause.param
                        innerscope.push(param)
                        tagVarRefs(clause.guard, innerscope, otherscopes) if clause.guard?
                        tagVarRefs(clause.body, innerscope, otherscopes)
                        innerscope.pop()
                        xv.kind = STACK if xv.kind isnt HEAP

                tagVarRefs(n.finalizer, innerscope, otherscopes) if n.finalizer?
                n

tagVarRefs = do ->
        o = new TagVarRefs
        o.visit.bind(o)

# Initialize the heap for each fun decl, var decl and heap var.
# Because of this function, we never get undefined by reading from heap.
# Must be run after initGlobals and after initCoreObjs.
# Most AbstractObj`s that aren't core are created here.

class InitDeclsInHeap extends TreeVisitor

        visitLiteral: (n) ->
                if typeof n.value is "string" and n.raw[0] is '/'      
                        new AbstractObj({ addr: n.addr })
                        regexp_constructor([makeObjAbstractVal(n.addr)])
                n
        # FIXME?: when array elms have the same type, they can be prematurely merged
        # to help the speed of the algo e.g. in 3d-cube
        visitArrayExpression: (n) ->
                new AbstractObj({ addr: n.addr })
                JSCore.array_constructor([makeObjAbstractVal(n.addr)], true)
                n.elements.forEach(initDeclsInHeap)
                n
                
        visitObjectExpression: (n) ->
                new AbstractObj({ addr: n.addr, proto: JSCore.object_prototype_av })
                n.properties.forEach (prop) ->
                        initDeclsInHeap(prop.key)
                        initDeclsInHeap(prop.value)
                n
                
        visitNewExpression: (n) ->
                new AbstractObj({ addr: n.addr })
                initDeclsInHeap(n.callee)
                n.arguments.forEach(initDeclsInHeap)
                n
                
        visitTry: (n) ->
                initDeclsInHeap(n.body)
                n.handlers.forEach (c) ->
                        heap[c.param.addr] = AbstractVal.BOTTOM if c.param.kind is HEAP
                        initDeclsInHeap(c.guard) if c.guard?
                        initDeclsInHeap(c.body)
                initDeclsInHeap(n.finalizer) if n.finalizer?
                n
                
        visitFunction: (n) ->
                console.log "addr for function #{n.id.name} is #{n.addr}"
                objaddr = n.addr
                fn = n.id 
                obj = new AbstractObj({ addr: objaddr, fun: n, proto: JSCore.function_prototype_av })
                obj.addProp("prototype-", {aval:AbstractVal.BOTTOM, enumerable:false})
                heap[fn.addr] = makeObjAbstractVal(objaddr) if fn.kind is HEAP
                n.params.forEach (p) ->
                        heap[p.addr] = AbstractVal.BOTTOM if p.kind is HEAP
                flags[objaddr] = n
                # initialize summaries and pending
                summaries[objaddr] = {
                        ts: INVALID_TIMESTAMP,
                        insouts: [],
                        type: [buildArray(n.params.length + 1, AbstractVal.BOTTOM), AbstractVal.BOTTOM] # arg 0 is for THIS
                }
                pending[objaddr] = []
                initDeclsInHeap(n.body)
                n

        visitBlock: (n) ->
                n.body.forEach(initDeclsInHeap)
                n

        #visitProgram: (n) ->
        #        n.varDecls.forEach (vd) ->
        #                console.log "#{vd.name} = BOTTOM"
        #                heap[vd.addr] = AbstractVal.BOTTOM if flags[vd.addr]
        #        n.body.forEach(initDeclsInHeap)
        #        n

initDeclsInHeap = do ->
        o = new InitDeclsInHeap
        o.visit.bind(o)

# void -> AbstractVal
# Used to analyze functions that aren't called
makeGenericObj = () ->
        new AbstractObj({ addr: newCount(), proto: JSCore.object_prototype_av })
        makeObjAbstractVal(count)

#////////////////////////////////////////////////////////////////////////////////
#//////////////////////////   EVALUATION FUNCTIONS   ////////////////////////////
#////////////////////////////////////////////////////////////////////////////////

# function for evaluating lvalues
# node, Answer, optional AbstractVal -> Answer
# use n to get an lvalue, do the assignment and return the rvalue
evalLval = (n, operator, ans, oldlval) ->
        console.log "yo!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?!?"
        _stackref = (n, operator, ans, oldlval) ->
                console.log "****_stackref"
                if operator isnt "="
                        if operator[0] is '+'
                                ans.v = AbstractVal.plus(ans.v, oldlval)
                        else
                                ans.v = AbstractVal.ANUM

                newav = AbstractVal.join(frameGet(ans.fr, n), ans.v)
                frameSet(ans.fr, n, newav)
                # if n is a heap var, update heap so its heap refs get the correct AbstractVal.
                AbstractVal.updateHeapAv(n.addr, newav) if flags[n.addr]
                ans
        
        _heapref = (n, operator, ans, oldlval) ->
                console.log "****_heapref"
                if operator isnt "="
                        if operator[0] is '+'
                                ans.v = AbstractVal.plus(ans.v, oldlval)
                        else
                                ans.v = AbstractVal.ANUM
                AbstractVal.updateHeapAv(n.addr, ans.v)
                ans

        console.log "n.type = #{escodegen.generate n}"
        switch n.type
                when Identifier
                        if n.kind is STACK
                                return _stackref(n, operator, ans, oldlval)
                        else
                                return _heapref(n, operator, ans, oldlval)

                when MemberExpression
                        rval = ans.v
                        fr = ans.fr
                        if operator is '+'
                                rval = AbstractVal.plus(rval, oldlval)
                        # XXX other operators?
                        ansobj = evalExp(n.object, fr)
                        avobj = ansobj.v
                        fr = ansobj.fr
                        errval = ansobj.err
                        property = n.property
                        
                        if n.computed
                                # Unsound: ignore everything the index can eval to except numbers & strings
                                if property.type is Literal
                                        avobj.updateProp(property.value, rval)
                                        return # XXX
                                else
                                        ansprop = evalExp(prop, fr)
                                        avprop = ansprop.v
                                        fr = ansprop.fr
                                        errval = AbstractVal.maybejoin(errval, ansprop.err)
                                        if avprop.hasNum() 
                                                avobj.forEachObj (o) -> o.updateNumProps(rval)
                                        if avprop.hasStr()
                                                slit = avprop.getStrLit()
                                                if (slit)
                                                        avobj.updateProp(slit, rval)
                                                else
                                                        avobj.forEachObj (o) -> o.updateStrProps(rval)

                        else
                                ansobj.v.updateProp(property.name, rval)

                        return new Answer(rval, fr, AbstractVal.maybejoin(errval, ans.err))

                #override(ARGUMENTS, function(n, ans, oldlval) {
                #        # FIXME: handle assignment to the arguments array
                #        return ans
                #})

                # in extremely rare cases, you can see a CALL as the lhs of an assignment.
                # toshok - wait, you can?
                when CallExpression
                        return ans
                
                else
                        throw new Error("unhandled lval type #{n.type}")

evalExp = (n, fr) ->
        _stackref = (n, fr) -> new Answer(frameGet(fr, n), fr)
        _heapref  = (n, fr) ->
                new Answer(heap[n.addr], fr)

        _binary2bool = (n, fr) ->
                ans1 = evalExp(n.left, fr)
                ans2 = evalExp(n.right, ans1.fr)
                new Answer(AbstractVal.ABOOL, ans2.fr, AbstractVal.maybejoin(ans1.err, ans2.err))

        _binary2num = (n, fr) -> 
                ans1 = evalExp(n.left, fr)
                ans2 = evalExp(n.right, ans1.fr)
                new Answer(AbstractVal.ANUM, ans2.fr, AbstractVal.maybejoin(ans1.err, ans2.err))
       
        _andor = (n, fr, pred1, pred2) ->
                ans1 = evalExp(n.left, fr)
                return ans1 if pred1.call(ans1.v)
                
                ans2 = evalExp(n.right, ans1.fr)

                new Answer((if pred2.call(ans1.v) then ans2.v else AbstractVal.join(ans1.v, ans2.v)), ans2.fr, AbstractVal.maybejoin(ans1.err, ans2.err))

        _handleBinaryExpression = (n, fr) ->
                op = n.operator
                if op is "in"
                        ans1 = evalExp(n.left, fr)
                        ans2 = evalExp(n.right, ans1.fr)
                        pname = ans1.v.getStrLit()
                        ans2v = ans2.v
                        if not ans2v.hasObjs()
                                return new Answer(AbstractVal.AFALS, ans2.fr, AbstractVal.maybejoin(ans1.err, ans2.err))
                        else if not pname
                                return new Answer(AbstractVal.ABOOL, ans2.fr, AbstractVal.maybejoin(ans1.err, ans2.err))
                        else
                                av = AbstractVal.BOTTOM
                                ans2v.forEachObj (o) ->
                                        av = AbstractVal.join(av, if not o.getProp(pname) then AbstractVal.AFALS else AbstractVal.ATRU)
                                        
                                return new Answer(av, ans2.fr, AbstractVal.maybejoin(ans1.err, ans2.err))

                if ["!=", "==", "!==", "===", "<", "<=", ">", ">=", "instanceof"].memq(op)
                        return _binary2bool(n, fr)

                if op is "+"
                        ans1 = evalExp(n.left, fr)
                        ans2 = evalExp(n.right, ans1.fr)
                        return new Answer(AbstractVal.plus(ans1.v, ans2.v), ans2.fr, AbstractVal.maybejoin(ans1.err, ans2.err))

                if ["-", "*", "/", "%", "|", "^", "&", "<<", ">>", "<<<", ">>>"].memq(op)
                        return _binary2num(n, fr)

                return _andor(n, fr, AbstractVal::isFalsy, AbstractVal::isTruthy) if op is "&&"
                return _andor(n, fr, AbstractVal::isTruthy, AbstractVal::isFalsy) if op is "||"

                throw new Error("unhandled binary expression type #{n.type}")

        
        switch n.type
                when Identifier
                        return _stackref(n, fr) if n.kind is STACK
                        return _heapref(n, fr)

                when Literal
                        return new Answer(AbstractVal.ANULL, fr)  if n.value is null
                        return new Answer(AbstractVal.AUNDEF, fr) if n.value is undefined
                        return new Answer(AbstractVal.ANUM, fr)   if typeof n.value is "number"
                        return new Answer(makeStrLitAbstractVal(n.value), fr) if typeof n.value is "string" and (n.raw[0] is '"' or n.raw[0] is "'")
                        return new Answer(makeObjAbstractVal(n.addr), fr) if typeof n.value is "string" and n.raw[0] is '/'
                        return new Answer((if n.value then AbstractVal.ATRU else AbstractVal.AFALS), fr) if typeof n.value is "boolean"
                        throw new Error "Internal error: unrecognized literal of type #{typeof n.value}"

                when ThisExpression then return new Answer(fr.thisav, fr)

                when UnaryExpression                
                        return new Answer(AbstractVal.AUNDEF, fr) if n.operator is "void" and n.argument.type is Literal and n.argument.value is 0

                        if n.operator is "!"
                                ans = evalExp(n.argument, fr)
                                av = ans.v
                                if av.isTruthy()
                                        ans.v = AbstractVal.AFALS
                                else if av.isFalsy()
                                        ans.v = AbstractVal.ATRU
                                else
                                        ans.v = AbstractVal.ABOOL
                                return ans

                        if n.operator is "typeof"
                                ans = evalExp(n.argument, fr)
                                ans.v = AbstractVal.ASTR
                                return ans

                        # unsound: I'm not deleting anything
                        if n.operator is "delete"
                                ans = evalExp(n.argument, fr)
                                ans.v = ABOOL
                                return ans
                                                
                        ans = evalExp(n.argument, fr)
                        ans.v = AbstractVal.ANUM
                        ans

                when BinaryExpression  then return _handleBinaryExpression(n, fr)
                when LogicalExpression then return _handleBinaryExpression(n, fr)
                
                when AssignmentExpression
                        # handle a = b
                        return evalLval(n.left, n.operator, evalExp(n.right, fr))

                when FunctionExpression      then return new Answer(makeObjAbstractVal(n.addr), fr)
                when ArrowFunctionExpression then return new Answer(makeObjAbstractVal(n.addr), fr)

                when SequenceExpression
                        ans = undefined
                        av = undefined
                        errval = undefined
                        n.expressions.forEach (exp) =>
                                ans = evalExp(exp, fr)
                                av = ans.v # keep last one
                                fr = ans.fr
                                errval = AbstractVal.maybejoin(errval, ans.err)
                        ans.v = av
                        ans.err = errval
                        return ans

                when ObjectExpression
                        ans = undefined
                        errval = undefined
                        objaddr = n.addr
                        newobj = heap[objaddr]
                        n.properties.forEach (pinit) =>
                                ans = evalExp(pinit.value, fr)
                                fr = ans.fr
                                newobj.updateProp(pinit.key.name, ans.v)        # toshok - won't work for get/set properties, will it?
                                errval = AbstractVal.maybejoin(errval, ans.err)

                        return new Answer(makeObjAbstractVal(objaddr), fr, errval)

                when ArrayExpression
                        ans = undefined
                        errval = undefined
                        arrayaddr = n.addr
                        newarray = heap[arrayaddr]
                        n.elements.forEach (elm, i) =>
                                ans = evalExp(elm, fr)
                                fr = ans.fr
                                newarray.updateProp("#{i}-", ans.v)
                                errval = AbstractVal.maybejoin(errval, ans.err)
                        return new Answer(makeObjAbstractVal(arrayaddr), fr, errval)

                when MemberExpression
                        if (not n.computed and n.property.name is "prototype") or (n.computed and n.property.type is Literal and n.property.raw is "prototype")
                                ans = evalExp(n.object, fr)
                                ans2 = undefined
                                av = AbstractVal.BOTTOM
                                av2 = undefined
                                errval = ans.err
                                # FIXME: record error if ans.v contains base values
                                ans.v.forEachObj (o) ->
                                        clos = o.getFun()
                                        if not clos # if o isn't a function, this is just a property access
                                                av2 = o.getProp("prototype-")
                                                av = AbstractVal.join(av, av2 or AbstractVal.AUNDEF)
                                        else
                                                proto = o.getProp("prototype-")
                                                if AbstractVal.eq(AbstractVal.BOTTOM, proto)
                                                        # create default prototype and return it    
                                                        proto = makeDefaultProto(clos)
                                                        o.updateProp("prototype-", proto)
                                                        av = AbstractVal.join(av, proto)
                                                else
                                                        av = AbstractVal.join(av, proto)

                                ans2 = new Answer(av, ans.fr, errval)
                                ans2.thisav = ans.v # used by method calls
                                return ans2

                        if n.computed
                                ansobj = evalExp(n.object, fr)
                                avobj = ansobj.v.baseToObj()
                                prop = n.property
                                errval = ansobj.err
                                av = undefined
                                ans = undefined
                                fr = ansobj.fr
                                # If [] notation is used with a constant, try to be precise.
                                # Unsound: ignore everything the index can eval to except numbers & strings
                                if prop.type is Literal and typeof prop.value is "string" and (prop.raw[0] is '"' or prop.raw[1] is "'")
                                        av = avobj.getProp(prop.value)
                                else
                                        ansprop = evalExp(prop, fr)
                                        avprop = ansprop.v
                                        fr = ansprop.fr
                                        errval = AbstractVal.maybejoin(errval, ansprop.err)
                                        av = AbstractVal.BOTTOM
                                        if avprop.hasNum()
                                                avobj.forEachObj (o) -> av = AbstractVal.join(av, o.getNumProps())
                                        if avprop.hasStr()
                                                slit = avprop.getStrLit()
                                                if slit
                                                        av = AbstractVal.join(av, avobj.getProp(slit))
                                                else
                                                        avobj.forEachObj (o) -> av = AbstractVal.join(av, o.getStrProps())
                                ans = new Answer(av, fr, errval)
                                ans.thisav = avobj
                                return ans

                        # for non-computed property access (foo.bar) things are simpler
                        ans        = evalExp(n.object, fr)
                        avobj      = ans.v.baseToObj()
                        ans.thisav = avobj        # used by method calls
                        ans.v      = avobj.getProp(n.property.name)
                        return ans

                when CallExpression
                        ans    = evalExp(n.callee, fr)
                        errval = undefined
                        rands  = []
                        rands.push(if ans.thisav then ans.thisav else JSCore.global_object_av)
                        fr     = ans.fr
                        errval = ans.err
                        # evaluate arguments
                        n.arguments.forEach (rand) ->
                                ans1   = evalExp(rand, fr)
                                fr     = ans1.fr
                                rands.push(ans1.v)
                                errval = AbstractVal.maybejoin(errval, ans1.err)
                                
                        # call each function that can flow to the operator position
                        ans     = ans.v.callFun(rands, n)
                        ans.fr  = fr
                        ans.err = AbstractVal.maybejoin(errval, ans.err)
                        return ans

                when NewExpression
                        rands = []
                        retval = AbstractVal.BOTTOM
                        ans = evalExp(n.callee, fr)
                        objaddr = n.addr
                        thisobj = heap[objaddr]
                        rands.push(makeObjAbstractVal(objaddr))
                        fr = ans.fr
                        errval = ans.err
                        # evaluate arguments
                        n.arguments.forEach (rand) ->
                                ans1 = evalExp(rand, fr)
                                rands.push(ans1.v)
                                fr = ans1.fr
                                errval = AbstractVal.maybejoin(errval, ans1.err)
                        
                        # FIXME: record error if rator contains base vals and non-functions
                        ans.v.baseToObj().forEachObj (o) ->
                                clos = o.getFun()
                                return if not clos
                        
                                proto = o.getProp("prototype-")
                                if AbstractVal.eq(AbstractVal.BOTTOM, proto)
                                        # create default prototype & use it
                                        proto = makeDefaultProto(clos)
                                        o.updateProp("prototype-", proto)
                                thisobj.updateProto(proto)
                                # if a fun is called both w/ and w/out new, assume it's a constructor
                                clos.withNew = true
                                ans = evalFun(clos, rands, true, n)
                                if clos.hasReturn # constructor uses return
                                        retval = AbstractVal.join(retval, ans.v)
                                else # constructor doesn't use return
                                        retval = AbstractVal.join(retval, rands[0])
                                errval = AbstractVal.maybejoin(errval, ans.err)
                        return new Answer(retval, fr, errval)

                else
                        throw new Error("unhandled expression type #{n.type}")
                # jscfa.js has this block as well:
                # 
                #  override(ARGUMENTS, function(n, fr) {
                #    var index = n.children[0], ps = n.arguments
                #    var restargs = fr[RESTARGS] || AbstractVal.BOTTOM, ans, av, errval
                #    if (index.type === NUMBER) {
                #      var iv = index.value
                #      if (iv < 0)
                #        av = AUNDEF
                #      else if (iv < ps.length)
                #        av = frameGet(fr, ps[iv])
                #      else
                #        av = restargs; # unsound: not checking if iv > #args
                #    }
                #    else {
                #      ans = evalExp(index, fr)
                #      fr = ans.fr
                #      errval = ans.err
                #      av = AbstractVal.BOTTOM
                #      # when we don't know the index, we return the join of all args
                #      ps.forEach(function(p) { av = AbstractVal.join(av, frameGet(fr, p)); })
                #      av = AbstractVal.join(av, restargs)
                #    }
                #    return new Answer(av, fr, errval)
                #  })

# function for evaluating statements
# node, frame -> Answer
# Evaluate the statement and find which statement should be executed next.
evalStm = (n, fr) ->
        #console.log "evalStm #{escodegen.generate n, escodegen.FORMAT_MINIFY}"
        switch n.type
                when ExpressionStatement
                        ans = evalExp(n.expression, fr)
                        ans.v = n.kreg
                        return ans

                when Program             then return new Answer(n.kreg, fr)
                        
                when FunctionDeclaration then return new Answer(n.kreg, fr)
                when VariableDeclaration
                        n.declarations.forEach (vd) -> frameSet(fr, vd.id, AbstractVal.BOTTOM)
                        return new Answer(n.kreg, fr)
                when BlockStatement      then return new Answer(n.kreg, fr)
                when SwitchCase          then return new Answer(n.kreg, fr)
                when DoWhileStatement    then return new Answer(n.kreg, fr)
                when WhileStatement      then return new Answer(n.kreg, fr)
                when TryStatement        then return new Answer(n.kreg, fr)
                when ForStatement        then return new Answer(n.kreg, fr)
                when IfStatement         then return new Answer(n.kreg, fr)
                when SwitchStatement     then return new Answer(n.kreg, fr)

                when ForInStatement
                        # For most kinds of iterators at FOR/IN we have to be conservative 
                        # (e.g. DOTs or INDEXes). Without flow sensitivity, we even have to be
                        # conservative for stack refs that have been initialized, we can't forget
                        # their current value. We can only be precise when the iterator is a stack
                        # reference and the variable is AbstractVal.BOTTOM in the frame.
                        ans = evalExp(n.right, fr)
                        it = n.left
                        b = n.body
                        if it.type is Identifier and it.kind is STACK && AbstractVal.eq(AbstractVal.BOTTOM, frameGet(fr, it))
                                av = ans.v
                                errval = ans.err
                                av.forEachObj (o) ->
                                        o.forEachEnumProp (p) ->
                                                # wipe the value of it from the previous iteration
                                                frameSet(fr, it, makeStrLitAbstractVal(p))
                                                AbstractVal.updateHeapAv(it.addr, makeStrLitAbstractVal(p)) if flags[it.addr]
                                                ans = evalStm(b, fr)
                                                errval = AbstractVal.maybejoin(errval, ans.err)
                                ans.v = b.kreg
                                ans.err = errval
                        else
                                av = AbstractVal.BOTTOM
                                ans.v.forEachObj (o) ->
                                        o.forEachEnumProp (p) ->
                                                if propIsNumeric(p)
                                                        av = AbstractVal.join(av, AbstractVal.ANUM)
                                                else
                                                        av = AbstractVal.join(av, AbstractVal.ASTR)
                                ans.v = av
                                ans = evalLval(n.iterator, "=", ans)
                                ans.v = b
                        return ans

                else
                        throw new Error("Unhandled node type #{n.type} in evalStm")


# node, node, node -> void
# For every node N in the AST, add refs from N to the node that is normally 
# exec'd after N and to the node that is exec'd if N throws an exception.
class MarkConts extends TreeVisitor

        visitFunctionDeclaration: (n, kreg, kexc) ->
                n.kreg = kreg
                n.kexc = kexc
                super(n, undefined, undefined)
                
        visitFunction: (n) ->
                n.body = markConts(n.body, undefined, undefined)
                n

        visitBlock: (n, kreg, kexc) ->
                ch = n.body
                len = ch.length
                n.kexc = kexc
                if len is 0
                        n.kreg = kreg
                        return

                len -= 1
                for i in [0...len]
                        ch[i] = markConts(ch[i], ch[i+1], kexc)
                        if ch[i]? and not n.kreg?
                                n.kreg = ch[i]
                                
                ch[len] = markConts(ch[len], kreg, kexc)
                if ch[len]? and not n.kreg?
                        n.kreg = ch[len]
                n

        visitProgram: (n, kreg, kexc) ->
                # visit our contents as if we were a block
                @visitBlock(n, kreg, kexc)

        visitExpressionStatement: (n, kreg, kexc) ->
                n.kreg = kreg
                n.kexc = kexc
                n.expression = markConts(n.expression)
                n
                
        # normally, return & throw don't use their kreg. But this analysis allows 
        # more permissive control flow, to be faster.
        visitThrow: (n, kreg, kexc) ->
                n.kreg = kreg
                n.kexc = kexc
                n.argument = markConts(n.argument)
                n

        visitReturn: (n, kreg, kexc) ->
                n.kreg = kreg
                n.kexc = kexc
                n.argument = markConts(n.argument)
                n

        visitIf: (n, kreg, kexc) ->
                k = kreg
                
                if n.alternate?
                        n.alternate = markConts(n.alternate, k, kexc)
                        k = n.alternate
                        
                n.consequent = markConts(n.consequent, k, kexc)
                k = n.consequent

                condStm = ast.expressionStatement(n.test)
                markConts(condStm, k, kexc)

                n.kreg = condStm # first run the test
                n.kexc = kexc
                n

        visitTry: (n, kreg, kexc) ->
                # process back-to-front to avoid if-madness
                if n.finalizer?
                        n.finalizer = markConts(n.finalizer, kreg, kexc)
                        knocatch = kexc = kreg = n.finalizer # TRY & CATCHes go to fin no matter what
                len = clauses.length
                for i in [len..0]
                        clauses[i] = markConts(clauses[i], knocatch, kreg, kexc)
                        knocatch = clauses[i]
                n.body = markConts(n.body, kreg, knocatch or kexc)
                n.kreg = n.block
                n

        visitCatchClause: (n, knocatch, kreg, kexc) ->
                if n.guard? # Mozilla catch
                        # The guard is of the form (var if expr).
                        # If expr is truthy, the catch body is run w/ var bound to the exception.
                        # If expr is falsy, we go to the next block (another catch or finally).
                        # If the guard or the body throw, the next catches (if any) can't handle
                        # the exception, we go to the finally block (if any) directly.      
                        n.guard = markConts(n.guard)
                        guardStm = ast.expressionStatement(n.guard)
                        n.kreg = guardStm
                        guardStm.kcatch = body       # this catch handles the exception
                        guardStm.knocatch = knocatch # this catch doesn't handle the exception
                        guardStm.kexc = kexc         # the guard throws a new exception

                n.body = markConts(n.body, kreg, kexc)
                n

        visitSwitch: (n, kreg, kexc) ->
                cases = n.cases
                discStm = ast.expressionStatement(n.discriminant)
                n.kreg = discStm # first run the discriminant, then all branches in order
                n.kexc = kexc
                markConts(discStm, cases[0], kexc)
                len = cases.length - 1
                markConts(cases[i],   cases[i+1], kexc) for i in [0...len] # no empty switch, len >= 0
                markConts(cases[len], kreg,       kexc)
                n
                
        visitCase: (n, kreg, kexc) ->
                test = n.test
                consequent = n.consequent
                
                n.kexc = kexc
                if test?
                        testStm = ast.expressionStatement(n.test)
                        n.kreg = testStm
                        markConts(testStm, stms, kexc)
                else
                        n.kreg = consequent
                markConts(consequent, kreg, kexc)
                n
                
        visitWhile: (n, kreg, kexc) ->
                body = n.body
                testStm = ast.expressionStatement(n.test)
                n.kreg = testStm
                n.kexc = kexc
                markConts(testStm, body, kexc)
                markConts(body, kreg, kexc)
                n
                
        visitDo: (n, kreg, kexc) ->
                body = n.body
                testStm = ast.expressionStatement(n.test)
                n.kreg = body
                n.kexc = kexc
                markConts(body, testStm, kexc)
                markConts(testStm, kreg, kexc)
                n

        visitFor: (n, kreg, kexc) ->
                body = n.body
                n.kexc = kexc
                # Do setup, condition, body & update once.
                init = n.init
                test = n.test
                update = n.update

                n.kexc = kexc
                if not init? and not test?    # for (;;<maybe-update>),     move immediately to the body
                        n.kreg = body
                else if init? and not test?   # for ($init;;<maybe-update>) move to init then body
                        initStm = ast.expressionStatement(init)
                        n.kreg = initStm
                        markConts(initStm, body, kexc)
                else # test exists
                        testStm = ast.expressionStatement(test)
                        markConts(testStm, body, kexc)
                        if init             # for ($init;$test;<maybe-update>) do init followed by test
                                initStm = ast.expressionStatement(init)
                                n.kreg = initStm
                                markConts(initStm, testStm, kexc)
                        else                # for (;$test;<maybe-update>) do test followed by body
                                n.kreg = initStm

                if update?
                        # the update is the body's continuation, insert it between body and kreg
                        updStm = ast.expressionStatement(update)
                        markConts(body, updStm, kexc)
                        markConts(updStm, kreg, kexc)
                else
                        # no update, body -> kreg
                        markConts(body, kreg, kexc)

                n

        visitVariableDeclaration: (n, kreg, kexc) ->
                n.kreg = kreg
                n.kexc = kexc
                n
                
        # XXX toshok -- this appears to be lacking - do we not want to
        # convert n.right into an ExpressionStatement and make it the
        # n.kreg, followed by the body?
        visitForIn: (n, kreg, kexc) ->
                body = n.body
                n.kreg = body
                n.kexc = kexc
                markConts(body, kreg, kexc)
                n

markConts = do ->
        o = new MarkConts
        o.visit.bind(o)

############################################
##############   CFA2  code   ##############
############################################

# abstract objects and abstract values are different!!!

heap = null
# modified[addr] is a timestamp that shows the last time heap[addr] was updated
modified = []
timestamp = 0
# If i is the addr of a var, flags[i] is true if the var is a heap var.
flags = []
exports_object = null
exports_object_av_addr = 0
commonJSmode = false
timedout = false
timeout = 120  # stop after 2 minutes if you're not done

# A summary contains a function node (fn), an array of abstract values (args),
# a timestamp (ts) and abstract values (res) and (err). It means: when we call
# fn w/ args and the heap's timestamp is ts, the result is res and if fn can
# throw an exception, the value thrown is err.

# summaries: a map from addresses of fun nodes to triples {ts, insouts, type},
# where ts is a timestamp, insouts is an array of args-result pairs,
# and type is the join of all args-result pairs.
summaries = {}

# pending contains info that exists in the runtime stack. For each pending call
# to evalFun, pending contains an object {args, ts} where args is the arguments
# of the call and ts is the timestamp at the time of the call.
pending = {}
# core JS functions also use pending but differently.

# when initGlobals is called, count has its final value (core objs are in heap)
# FIXME, I'm violating the invariant in "function cfa2". Change it?
initGlobals = () ->
        big_ts = 1000 # used only when debugging
        timestamp = 0
        heap = new Array count # reserve heap space, don't grow it gradually
        modified = buildArray count, timestamp
        summaries = {} # We use {} instead of an Array b/c it's sparse.
        pending = {}
        flags = {}
        exports_object = {lines : {}}

# string -> void
# works only in NodeJS 
dumpHeap = (filename) ->
        fs = require('fs')
        fd = fs.openSync filename, "w", 0o0777
        printf = (fd, s) -> fs.writeSync(fd, s, null, encoding='utf8')
        heap.forEach (h, i) ->
                printf fd, "[#{i}]\n#{if h? then h.toString(2) else ''}\n"
        fs.closeSync fd


# non-empty array of strings -> void
normalizeUnionType = (types) ->
        # any is a supertype of all types
        if types.memq "any"
                types[0] = "any"
                types.length = 1
        else
                types.rmDups (e1, e2) -> e1 is e2

# An abstract object o1 is represented as a JS object o2. 
# A property of o1 named p has the name p- in o2.
# A special property p of o1 has the name -p in o2.
# Special properties: -number, -string, -proto, -fun, -addr
# -addr: the address of o1 in the heap
# -proto: the address of o1's prototype in the heap
# -fun: may point to a function node
# -number: may contain an AbstractVal (join of properties w/ "numeric" names)
# -string: may contain an AbstractVal (join of all properties)

class AbstractObj
        constructor: (specialProps) ->
                @["-#{p}"] = specialProps[p] for own p of specialProps
                heap[specialProps.addr] = @ # put new object in heap

        # toshok - errr...
        # AbstractObj.prototype = new Array()

        # Takes an optional array for cycle detection.
        toType: (seenObjs) ->
                self = @
                if seenObjs?
                        if seenObjs.memq self
                                return "any"
                        else
                                seenObjs.push self
                else
                        seenObjs = [self]

                if @["-fun"]?
                        return funToType @["-fun"], seenObjs
                c = @getProp "constructor-"
                types = []
                return "Global Object" if c is undefined
                c.forEachObj (o) ->
                        types.push o["-fun"].id.name if o["-fun"]?
                if types.length is 0
                        throw errorWithCode CFA_ERROR, "Didn't find a name for constructor"
                normalizeUnionType types
                if types.length is 1
                        if types[0] is "Array"
                                return @toArrayType seenObjs
                        return types[0]
                "<#{types.join(' | ')}>"

        # void -> boolean
        isArray: () ->
                c = @getProp "constructor-"
                if c is undefined
                        return false
                if c.objs.length isnt 1
                        return false
                cobj = heap[c.objs[0]]
                return cobj["-fun"]?.id.name is "Array"

        # For homogeneous arrays, include the type of their elms.
        # Takes an optional array for cycle detection.
        toArrayType: (seenObjs) ->
                elmtype = AbstractVal.BOTTOM
                self = @

                this.forEachOwnProp (p) ->
                        if propIsNumeric p
                                elmtype = AbstractVal.join elmtype, @getOwnExactProp p
                                
                elmtype = elmtype.toType seenObjs
                if /\|/.test elmtype or elmtype is "any"
                        return "Array"
                else
                        return "Array[#{elmtype}]"

        # void -> function node
        getFun: () -> @["-fun"]

        # AbstractVal -> void
        updateProto: (av) ->
                if @["-proto"]
                	if !AbstractVal.lt av, @["-proto"]
                		@["-proto"] = AbstractVal.join @["-proto"], av
                		#print("++ts: 1")
                		timestamp += 1
                	return
                @["-proto"] = av
                #print("++ts: 2")
                timestamp += 1

        # string -> AbstractVal or undefined
        # doesn't look in prototype chain
        getOwnExactProp: (pname) ->
                @[pname]?.aval

        # string -> AbstractVal or undefined
        # doesn't look in prototype chain
        # pname is not "-number" or "-string"
        getOwnProp: (pname) ->
                if @hasOwnProperty pname
                        return @[pname].aval
                if @numPropsMerged and propIsNumeric pname
                        return @["-number"].aval
                if @strPropsMerged
                        return @["-string"].aval

        getProp: (pname, lax = true) ->
                levels = 0
                o = @
                
                while o isnt undefined and levels <= 2  # toshok - we only walk up two levels of prototype chain?  seems wrong
                        if o.hasOwnProperty(pname)
                                return o[pname].aval
                        if lax and o.numPropsMerged
                                return o["-number"].aval if propIsNumeric(pname)
                                
                                if o.strPropsMerged
                                        av = AbstractVal.maybejoin(av, o["-string"].aval)
                        levels += 1
                        o = o["-proto"] and heap[o["-proto"].objs[0]]
                av

        getNumProps: () ->
                @mergeNumProps()
                @["-number"].aval

        getStrProps: () ->
                levels = 0
                av = AbstractVal.BOTTOM
                seenProps = []
                o = @

                while o isnt undefined and levels <= 2
                        if o.strPropsMerged?
                                av = AbstractVal.join av, o["-string"].aval
                        else
                                o.forEachOwnProp (pname, pval) ->
                                        if pval.enumerable and not seenProps.memq pname
                                                av = AbstractVal.join av, o[pname].aval
                                                if pname isnt "-number"
                                                        seenProps.push pname
                        levels++
                        o = o["-proto"] and heap[o["-proto"].objs[0]]
                av

        # string, Object -> void
        # attribs.aval is the property's value
        # The property shouldn't be already present, it'll be overwritten
        addProp: (prop, attribs) ->
                @[prop] = new AbstractProp attribs

        # string, AbstractVal -> void
        updateExactProp: (pname, newv) ->
                if @hasOwnProperty pname
                        p = @[pname]
                        if not AbstractVal.lt newv, p.aval
                                p.aval = AbstractVal.join p.aval, newv
                                #print("++ts: 3")
                                timestamp += 1
                        return
                @[pname] = new AbstractProp aval : newv
                #print("++ts: 4")
                timestamp += 1

        # string, AbstractVal -> void
        # pname is not "-number" or "-string"
        updateProp: (pname, newv) ->
                upd = (pname, pval, newv) ->
                        if not AbstractVal.lt newv, pval.aval
                                pval.aval = AbstractVal.join pval.aval, newv
                                #print("++ts: 5")
                                timestamp += 1

                if @hasOwnProperty pname
                        # either it's not enumerable, or it doesn't interfere with "-number"
                        upd pname, @[pname], newv
                else # the new property is enumerable
                        if @strPropsMerged
                                upd "-string", @["-string"], newv
                        if @numPropsMerged? and propIsNumeric pname
                                upd "-number", @["-number"], newv
                        else if not @strPropsMerged?
                                @[pname] = new AbstractProp aval : newv
                                #print("++ts: 6 " + pname)
                                timestamp += 1

        # AbstractVal -> void
        updateNumProps: (newv) ->
                @mergeNumProps()
                @updateExactProp "-number", newv

        # AbstractVal -> void
        updateStrProps: (newv) ->
                @mergeStrProps()
                @updateExactProp "-number", newv
                @updateExactProp "-string", newv

        # merge all numeric properties of the object to one generic number property
        mergeNumProps: () ->
                return if @numPropsMerged
                av = AbstractVal.BOTTOM
                @forEachOwnProp (p) =>
                        if propIsNumeric p
                                av = AbstractVal.join av, @[p].aval
                                delete @[p] # don't keep a mix of merged and unmerged properties.
                @updateExactProp "-number", av # this bumps timestamp, don't bump again
                @numPropsMerged = true

        mergeStrProps: () ->
                return if @strPropsMerged
                @mergeNumProps() if not @numPropsMerged
                av = AbstractVal.BOTTOM
                this.forEachOwnProp (pname, pval) =>
                        if pval.enumerable
                                av = AbstractVal.join av, pval.aval
                                delete @[pname] if pname isnt "-number"

                @updateExactProp "-string", av # this bumps timestamp, don't bump again
                @strPropsMerged = true

        forEachOwnProp: (f) ->
                for p of @
                        if @[p] instanceof AbstractProp
                                f p, @[p] # f may ignore its second argument

        forEachEnumProp: (f) ->
                levels = 0
                av = AbstractVal.BOTTOM
                seenProps = []
                o = @

                while o isnt undefined and levels <= 2
                        for p of o
                                pval = o[p]
                                if pval instanceof AbstractProp and pval.enumerable and not seenProps.memq p
                                        f p, pval.aval # f may ignore its second argument
                                        seenProps.push p
                        levels += 1
                        o = o["-proto"] and heap[o["-proto"].objs[0]]
                av

        # string, function, number -> function node
        funToNode = (name, code, arity) ->
                n = ast.functionDeclaration(ast.identifier(name), [], ast.blockStatement())
                n.builtin = true
                n.addr = newCount()
                pending[count] = 0
                # built-in funs have no params property but they have an arity property
                # instead. It's only used by the apply method.
                n.arity = arity
                n.acode = code
                n

        # AbstractObj, string, function, number -> void
        attachMethod: (mname, arity, mcode) ->
                n = funToNode(mname, mcode, arity)
                addr = n.addr
                fobj = new AbstractObj({addr: addr, proto: JSCore.function_prototype_av, fun: n})
                @addProp("#{mname}-", {aval: makeObjAbstractVal(addr), enumerable : false})
                fobj

        # optional number -> string
        # Returns a multiline string, each line starts with indent (or more) spaces
        toString: (indent) ->
                indent = indent || 0
                i1 = buildString indent, " "
                i2 = i1 + "  "

                s = i1 + "<proto>:\n"
                s += (if @["-proto"]? then @["-proto"].toString(indent + 2) else (i2 + "??\n"))
  
                if @["-fun"]?
                        s += i1 + "<function>:\n" + i2 + @["-fun"].id.name +
                                (if @["-fun"].lineno? then "@" + @["-fun"].lineno else "") + "\n"

                this.forEachOwnProp (p) =>
                        pname = p
                        if p isnt "-number" and p isnt "-string"
                                pname = p.slice 0, -1
                        s += i1 + pname + ":\n" + @[p].toString indent + 2


                s


getProp = (o, pname, lax) ->
        levels = 0

        while o isnt undefined and levels <= 2
                if o.hasOwnProperty pname
                        return o[pname].aval
                if lax? and o.numPropsMerged?
                        if propIsNumeric pname
                                return o["-number"]
                        if o.strPropsMerged?
                                av = AbstractVal.maybejoin av, o["-string"].aval
                levels += 1
                o = o["-proto"] and heap[o["-proto"].objs[0]]
        av

# string -> boolean
propIsNumeric = (p) ->
	return p is "-number" or (/-$/.test(p) and p.slice 0, -1)

# act: AbstractObj, optional Array[string] -> {val : A, stop : boolean}
# combine: Array[A] -> A
foldProtoChain = (o, act, combine) ->
        recur = (o, seenObjs, seenProps) ->
                v = act o, seenProps
                val = v.val
                return val if v.stop # stop going up the chain
                return val if not o["-proto"]? # reached the end of the prototype chain
                if seenObjs.memq o
                        return val
                else
                        seenObjs.push o
                a = []
                solen = seenObjs.length
                splen = seenProps.length
                
                o["-proto"].forEachObj (proto) ->
                        a.push recur proto, seenObjs, seenProps
                        seenObjs.splice solen, seenObjs.length
                        seenProps.splice splen, seenProps.length
                a.push val # The current level's result is last, 'combine' pops to get it
                combine a

        recur o, [], []

# StackCleaner inherits from Error and is not used to signal errors, but to
# manage the size of the runtime stack.

# function node, number, optional array of AbstractVal
class StackCleaner extends Error
        constructor: (@fn, @howmany, @args) ->

# function node, array of AbstractVal, boolean, optional call node -> Answer w/out fr
# Arg 4 is the node that caused the function call (if there is one).
evalFun = (fn, args, withNew, cn) ->
        #console.log "evalFun #{fn.id.name}"
        script = fn.body

        retval = AbstractVal.BOTTOM
        errval = AbstractVal.BOTTOM

        # stm node (exception continuation), av (exception value) -> void
        stmThrows = (n, errav) ->
                if (n)
                        if n.type is CatchClause
                                exvar = n.param
                                if exvar.kind is HEAP
                                        heap[exvar.addr] = AbstractVal.join(errav, heap[exvar.addr])
                                if fr[exvar.addr] # revealing the representation of frame here.
                                        frameSet(fr, exvar, AbstractVal.join(errav, frameGet(fr, exvar)))
                                else
                                        frameSet(fr, exvar, errav)
                        w.push(n)
                else
                        errval = AbstractVal.join(errval, errav)

        throw new Error "timeout" if process.uptime() > timeout

        # treat built-in functions specially
        if fn.builtin
                addr = fn.addr
                if pending[addr] > 1
                        return new Answer(AbstractVal.BOTTOM, undefined, AbstractVal.BOTTOM)
                pending[addr] += 1
                try
                        ans = fn.body(args, withNew, cn)
                        pending[addr] -= 1
                        return ans
                catch e
                        pending[addr] -= 1 if e instanceof StackCleaner
                        throw e

                result = searchSummary(fn, args, timestamp)
                return new Answer(result[0], undefined, result[1] if result?)

        while true
                try
                        tsAtStart = timestamp
                        # pending & exceptions prevent the runtime stack from growing too much.
                        pelm2 = searchPending(fn, args)
                        if pelm2 is 0
                                pelm1 = { args: args, ts: timestamp }
                                addPending(fn, pelm1)
                        else if pelm2 is undefined
                                # If a call eventually leads to itself, stop analyzing & return AbstractVal.BOTTOM.
                                # Add a summary that describes the least solution.
                                addSummary(fn, args, AbstractVal.BOTTOM, AbstractVal.BOTTOM, tsAtStart)
                                return new Answer(AbstractVal.BOTTOM, undefined, AbstractVal.BOTTOM)
                        else if pelm2 > 0
                                # There are pending calls that are obsolete because their timestamp is
                                # old. Discard frames to not grow the stack too much.
                                throw new StackCleaner(fn, pelm2)
                        else # if pelm2 < 0
                                throw new StackCleaner(fn, -pelm2, args)
                        w = []
                        fr = {}
                        params = fn.params
                        frameSet(fr, fn.id, makeObjAbstractVal(fn.addr))
                        # args[0] is always the obj that THIS is bound to.
                        # THIS never has a heap ref, so its entry in the frame is special.
                        fr.thisav = args[0]
                        # Bind formals to actuals
                        for i in [0...params.length]
                                param = params[i]
                                arg = args[i+1] or AbstractVal.AUNDEF # maybe #args < #params
                                if param.kind is HEAP
                                        AbstractVal.updateHeapAv(param.addr, arg)
                                frameSet(fr, param, arg)
                        argslen = args.length
                        i += 1
                        if i < argslen # handling of extra arguments
                                restargs = AbstractVal.BOTTOM
                                while i < argslen
                                        restargs = AbstractVal.join(restargs, args[i])
                                        i += 1
                                fr[RESTARGS] = restargs # special entry in the frame.
                                
                        # bind a non-init`d var to bottom, not undefined.
                        #if not script.varDecls?
                        #        console.warn "node #{script.type} lacks varDecls"
                        #else
                        #        script.varDecls.forEach (vd) -> frameSet(fr, vd, AbstractVal.BOTTOM)
                                
                        w.push script.kreg
                        while w.length isnt 0
                                n = w.pop()
                                if n isnt undefined
                                        if n.type is ReturnStatement
                                                #if n.argument?
                                                #        console.log "return #{escodegen.generate n.argument}"
                                                #else
                                                #        console.log "return (void)0"
                                                ans = if n.argument? then evalExp(n.argument, fr) else new Answer(AbstractVal.AUNDEF, fr)
                                                # fr is passed to exprs/stms & mutated, no need to join(fr, ans.fr)
                                                fr = ans.fr
                                                retval = AbstractVal.join(retval, ans.v)
                                                w.push n.kreg
                                                stmThrows(n.kexc, ans.err) if ans.err?
                                        else
                                                ans = evalStm n, fr
                                                fr = ans.fr
                                                w.push ans.v
                                                stmThrows(n.kexc, ans.err) if ans.err?

                        rmPending fn
                        retval = AbstractVal.AUNDEF if not fn.hasReturn
                        # Find if a summary has been added during recursion.
                        result = searchSummary fn, args, tsAtStart
                        if not result or AbstractVal.lt(retval, result[0]) and AbstractVal.lt(errval, result[1])
                                # Either fn isn't recursive, or the fixpt computation has finished.
                                #console.log "case1"
                                addSummary(fn, args, retval, errval, tsAtStart if not result)
                                return new Answer retval, undefined, errval
                        else
                                #console.log "case2"
                                #console.log "result[0] is #{JSON.stringify result[0]}, retval is #{JSON.stringify retval}"
                                #console.log "result[1] is #{JSON.stringify result[1]}, errval is #{JSON.stringify errval}"
                                retval = AbstractVal.join result[0], retval
                                errval = AbstractVal.join result[1], errval
                                # The result changed the last summary; update summary and keep going.
                                addSummary(fn, args, retval, errval, tsAtStart)
                catch e
                        if not (e instanceof StackCleaner)
                                # analysis error, irrelevant to the stack-handling code
                                throw e
                        throw e if not pelm1
                        rmPending fn
                        throw e if e.fn isnt fn
                        if e.howmany isnt 1
                                e.howmany -= 1
                                throw e
                        if e.args?
                                args = e.args

# maybe merge with evalFun at some point
evalToplevel = (tl) ->
        w = []
        fr = {}
        
        initDeclsInHeap(tl)

        fr.thisav = JSCore.global_object_av

        # evaluate the stms of the toplevel in order
        w.push(tl.kreg)
        while w.length isnt 0
                n = w.pop()
                if n isnt undefined # end of toplevel reached
                        if n.type is ReturnStatement
                                # record error, return in toplevel
                                #console.log("toplevel return with with non-empty stack")
                        else
                                #console.time("evalStm");
                                ans = evalStm(n, fr)
                                #console.timeEnd("evalStm");
                                fr = ans.fr
                                w.push(ans.v)
                                # FIXME: handle toplevel uncaught exception

        # each function w/out a summary is called with unknown arguments
        for addr of summaries
                f = heap[addr].getFun()
                if not existsSummary(f)
                        any_args = buildArray(f.params.length, AbstractVal.BOTTOM)
                        any_args.unshift(makeGenericObj())
                        evalFun(f, any_args, false)
        showSummaries()
                        
# initGlobals and initCoreObjs are difficult to override. The next 2 vars help
# clients of the analysis add stuff to happen during initialization
initOtherGlobals = undefined
initOtherObjs = undefined

exports.CFA2 = class CFA2
        constructor: (@filename) ->

        visit: (ast) ->
                count = 0
                astSize = 0
                initGlobals()
                initOtherGlobals() if initOtherGlobals?
                console.time("fixAST")
                fixAST ast
                console.timeEnd("fixAST")
                new JSCore(heap)
                initOtherObjs() if initOtherObjs?
                if commonJSmode # create the exports object
                        e = new AbstractObj(addr: newCount())
                        eav = makeObjAbstractVal count
                        heap[newCount()] = eav
                        exports_object_av_addr = count
                        exports_object.obj = e
                        
                console.time("labelAST")
                labelAST(ast)
                console.timeEnd("labelAST")
                console.time("tagVarRefs")
                tagVarRefs(ast, [], [], "toplevel")
                console.timeEnd("tagVarRefs")
                console.time("markConts")
                markConts(ast, undefined, undefined)
                console.timeEnd("markConts")
                try
                        console.log "Done with preamble. Analysis starting on node #{ast.type}."
                        console.time("eval")
                        evalToplevel(ast)
                        console.timeEnd("eval")
                        console.log "after cfa2"
                        console.log "AST size: #{astSize}"
                        console.log "ts: #{timestamp}"
                        dumpHeap "heapdump.txt"

                catch e
                        console.timeEnd("eval")
                        if e.message isnt "timeout"
                                console.log e.message
                                e.code = CFA_ERROR if not ("code" in e)
                                throw e
                        else
                                console.log "timed out"
                                timedout = true

                ast

        
        
# function node -> string
funToType = (n, seenObjs) ->
        return "function" if n.builtin # FIXME: tag built-in nodes w/ their types
        if seenObjs?
                if seenObjs.memq(n)
                        return "any"
                else
                        seenObjs.push(n)
        else
                seenObjs = [n]
  
        addr = n.addr
        summary = summaries[addr]
        if summary.ts is INVALID_TIMESTAMP # the function was never called
                return "function"
        insjoin = summary.type[0]
        instypes = []
        slen = seenObjs.length
        for i in [1...insjoin.length]
                instypes[i - 1] = insjoin[i].toType(seenObjs)
                # each argument must see the same seenObjs, the initial one.
                seenObjs.splice(slen, seenObjs.length)

        if n.withNew and !n.hasReturn
                outtype = n.id.name
                # If a fun is called both w/ and w/out new, assume it's a constructor.
                # If a constructor is called w/out new, THIS is bound to the global obj.
                # In this case, the result type must contain void.
                thisObjType = insjoin[0].toType(seenObjs)
                if /Global Object/.test(thisObjType)
                        outtype = "<void | " + outtype + ">"
        else
                outtype = summary.type[1].toType(seenObjs)
  
        outtype = "void" if outtype is "undefined"
        "#{outtype} function(#{instypes.join(', ')})"

#new exports.CFA2("test").visit esprima.parse("function f(x) { return x * 2; }; f(2);")

new exports.CFA2("test").visit esprima.parse("
primes = function primes (n) {
  primes_internal = function primes_internal (cur, remaining, filter) {
    if (remaining === 0)
      return;
    else {
      if (!filter(cur)) {
	console.log (cur);
	primes_internal (cur+1, remaining-1, function prime_filter (test) {
			   return test%cur === 0 || filter (test);
			 });
      }
      else {
      	primes_internal (cur+1, remaining, filter);
      }
    }
  };

  base_filter = function base_filter (test) { return false; };
  primes_internal (2, n, base_filter);
};

primes (5);
")