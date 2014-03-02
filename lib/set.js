(function() {
var echo_util = require('echo-util');
var foldl = echo_util.foldl;

var hasOwn = Object.prototype.hasOwnProperty;

function Set(arr) {
    if (typeof arr === 'undefined')
        arr = [];

    this.set = Object.create(null);
    for (var i = 0, e = arr.length; i < e; i ++) {
        var a = arr[i];
        this.set[a] = a;
    }
}

Set.prototype.has = function (el) {
    return hasOwn.call(this.set, el);
};

Set.prototype.add = function (el) {
    this.set[el] = el;
};

Set.prototype.remove = function (el) {
    delete this.set[el];
};

Set.prototype.map = function (f) {
    var result = [];
    for (var p in this.set)
        result.push (f(p));
    return result;
};

Set.prototype.keys = function () {
    return this.values();
};
Set.prototype.values = function() {
    return Object.getOwnPropertyNames(this.set);
};

Set.prototype.union = function(other_set) {
    if (!other_set) return this;
    var result = new Set();
    var p;
    for (p in this.set)
        result.add (p);
    for (p in other_set.set)
        result.add (p);
    return result;
};

Set.prototype.subtract = function(other_set) {
    if (!other_set) return this;
    var result = new Set();
    for (var p in this.set) {
        if (!other_set.has(p)) {
            result.add (p);
        }
    }
    return result;
};

Set.prototype.intersect = function(other_set) {
    if (!other_set) return this;
    var result = new Set();
    for (var p in this.set) {
        if (other_set.has(p)) {
            result.add (p);
        }
    }
    return result;
};

Set.prototype.toString = function() {
    var str = "{ ";
    for (var p in this.set) {
        str += p;
        str += " ";
    }
    str += "}";
    return str;
};

Set.prototype.size = function() {
    return this.values().length;
};

Set.prototype.empty = function() {
    for (var p in this.set) {
        return false;
    }
    return true;
};

Set.union = function() {
    return foldl (function (a,b) { return a.union(b); }, new Set(), Array.prototype.slice.call (arguments, 0));
};

exports.Set = Set;
})();

/*
# Set tests

s1 = new Set [1, 2, 3, 4]
s2 = new Set [5, 6, 7, 8]

console.log "should be { 1 2 3 4 5 6 7 8 }:    #{(s1.union s2).toString()}"


s3 = new Set [1, 2, 3, 4, 5, 6, 7, 8]
s4 = new Set [5, 6, 7, 8]

console.log "should be { 1 2 3 4 }:    #{(s3.subtract s4).toString()}"

s5 = new Set [1, 2, 3, 4];
s6 = new Set [3, 4, 5];

console.log "should be { 3 4 }:    #{(s5.intersect s6).toString()}"

*/
