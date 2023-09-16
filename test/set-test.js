var Set = require("../lib/set-es6").Set;
var assert = require("assert");

describe("Set", function () {
    describe("creation", function () {
        it("should return an empty set when no arguments are given to ctor", function () {
            var s = new Set();
            assert(s.empty());
        });
        it("should should populate the set with the array passed to ctor", function () {
            var s = new Set([1, 2, 3]);
            assert.equal(false, s.empty());
            assert(s.has(1));
            assert(s.has(2));
            assert(s.has(3));
        });
    });

    describe("union", function () {
        it("should return left set if right is null/undefined", function () {
            var l = new Set(1, 2, 3);
            assert.equal(l, l.union(null));
            assert.equal(l, l.union(undefined));
        });

        it("should return left set if right is empty", function () {
            var l = new Set([1, 2, 3]);
            var r = new Set();
            var s = l.union(r);
            assert(s.has(1), "1");
            assert(s.has(2), "2");
            assert(s.has(3), "3");
        });

        it("should return right set if left is empty", function () {
            var l = new Set();
            var r = new Set([1, 2, 3]);
            var s = l.union(r);
            assert(s.has(1), "1");
            assert(s.has(2), "2");
            assert(s.has(3), "3");
        });
    });

    describe("add", function () {
        it("should not increase key-count if duplicate key is added", function () {
            var s = new Set();
            s.add(1);
            s.add(1);
            assert.equal(1, s.keys().length);
        });
    });
});
