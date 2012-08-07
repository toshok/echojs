{ deep_copy_object, map, foldl } = require 'echo-util'

hasOwn = Object.hasOwnProperty

exports.Set = class Set
        constructor: (arr = []) ->
                @set = {}
                @set[a] = a for a in arr

        member: (el) -> hasOwn.apply @set, [el]

        add: (el) -> @set[el] = el
        remove: (el) -> delete @set[el]
                
        map: (f) ->  f el for el of @set when hasOwn.apply @set, [el]
        
        union: (other_set) ->
                result = new Set
                @map (el) -> result.add el
                other_set.map (el) -> result.add el
                result

        subtract: (other_set) ->
                result = new Set 
                @map (el) -> result.add el if not other_set.member el
                result

        intersect: (other_set) ->
                result = new Set
                @map (el) -> result.add el if other_set.member el
                result

        toString: ->
                str = "{"
                str += " #{el}" for el of @set
                str += " }"
                str

        empty: ->
                els = (el for el of @set when hasOwn.apply @set, [el])
                els.length is 0
                
        @union: (sets...) ->
                (foldl ((a,b) -> a.union b), (new Set), [sets...])

# Set tests

# s1 = new Set [1, 2, 3, 4]
# s2 = new Set [5, 6, 7, 8]

# console.log "should be { 1 2 3 4 5 6 7 8 }:    #{(s1.union s2).toString()}"


# s3 = new Set [1, 2, 3, 4, 5, 6, 7, 8]
# s4 = new Set [5, 6, 7, 8]

# console.log "should be { 1 2 3 4 }:    #{(s3.subtract s4).toString()}"

# s5 = new Set [1, 2, 3, 4];
# s6 = new Set [3, 4, 5];

# console.log "should be { 3 4 }:    #{(s5.intersect s6).toString()}"

