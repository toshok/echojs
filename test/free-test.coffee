esprima = require 'esprima'
{free} = require 'closure-conversion'
assert = require 'echo-assert'

syntax = esprima.Syntax

assert.eq "free null returns an empty set", (free null).empty(), true

assert.eq "empty statement", "#{(free {type:syntax.Identifier, name: 'test'})}", '{ test }'

assert.eq "identifier", "#{(free {type:syntax.Identifier, name: 'test'})}", '{ test }'


assert.dumpStats()
