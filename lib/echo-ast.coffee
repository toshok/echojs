class Script
  constructor: (@children) ->

class Expression
  constructor: ->
  setParent: (parent) ->
    @parent = parent
  getParent: -> @parent
  getScript: ->
    if @parent is Script
      return @parent
    return @parent.getScript

class Identifier extends Expression
  constructor: (@name) ->
    super()

class Number extends Expression
  constructor: (@value) ->
    super()

class Decl extends Expression
  constructor: (@name, @initializer) ->
    super()

class Const extends Decl
  constructor: (@name, @initializer) ->
    if (!@initializer)
      throw "Const declaration must have an initializer"
    super

class Let extends Decl
  constructor: (@name, @initializer) ->
    super

class Var extends Decl
  constructor: (@name, @initializer) ->
    super

class Function extends Expression
  constructor: (@name, @parameters, @body) ->
    super()

class Call extends Expression
  constructor: (@callee, @args) ->
    super()

