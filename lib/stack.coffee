
exports.Stack = class Stack
        constructor: (initial)->
                @stack = []
                @stack.unshift initial if initial?
                        

        push: (o) ->
                @stack.unshift o

        pop: ->
                throw new Error "stack is empty" if @stack.length is 0
                @stack.shift()

# add a 'top' property to make things a little clearer/nicer to read in compiler.coffee

Object.defineProperty Stack::, "top", {
        get: ->
                throw new Error "stack is empty" if @stack.length is 0
                @stack[0]
}

# and a 'depth' property
Object.defineProperty Stack::, "depth", get: -> @stack.length
