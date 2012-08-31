{Set} = require 'set'
assert = require 'echo-assert'


# creation
#
assert.eq "create-empty", "#{new Set}", "{ }"
assert.eq "create-123", "#{new Set [1, 2, 3]}", "{ 1 2 3 }"

# union
assert.eq "union1", "#{(new Set [1, 2, 3]).union(new Set [4])}", "{ 1 2 3 4 }"
assert.eq "union2", "#{(new Set [1, 2, 3]).union(new Set [3])}", "{ 1 2 3 }"

# subtract
s3 = new Set [1, 2, 3, 4, 5, 6, 7, 8]
s4 = new Set [5, 6, 7, 8]
assert.eq "subtract1", "#{s3.subtract s4}", "{ 1 2 3 4 }"

# intersect
s5 = new Set [1, 2, 3, 4]; 
s6 = new Set [3, 4, 5];
assert.eq "intersect1", "#{(s5.intersect s6)}", "{ 3 4 }"

assert.dumpStats()
