(function() {
var echo_util = require('echo-util');
var foldl = echo_util.foldl;

var hasOwn = Object.prototype.hasOwnProperty;

function Map() {
    this.map = Object.create(null);
    this.map_size = 0;
}

Map.prototype.has = function (key) {
    return hasOwn.call(this.map, "%map" + key);
};

Map.prototype.set = function (key, val) {
    var entry_key = "%map" + key;
    var had_before = hasOwn.call(this.map, entry_key);
    this.map[entry_key] = { key: key, val: val };
    if (!had_before) this.map_size++;
};

Map.prototype.get = function (key, val) {
    var entry_key = "%map" + key;
    if (!hasOwn.call(this.map, entry_key)) return undefined;
    return this.map[entry_key].val;
};

Map.prototype.remove = function (key) {
    var entry_key = "%map" + key;
    var had_before = hasOwn.call(this.map, entry_key);

    delete this.map[entry_key];
    if (had_before) this.map_size--;
};

Map.prototype.clear = function () {
    this.map = Object.create(nul);
    this.size = 0;
};

Map.prototype.forEach = function (f) {
    for (var p in this.map) {
	if (!hasOwn.call(this.map, p))
	    continue;
	var entry = this.map[p];
	f (entry.val, entry.key, this);
    }
};

Map.prototype.keys = function () {
    var result = [];
    for (var p in this.map) {
	if (!this.has (p))
	    continue;
	var entry = this.map[p];
	result.push (entry.key);
    }
    return result;
};
Map.prototype.values = function() {
    var result = [];
    for (var p in this.map) {
	if (!this.has (p))
	    continue;
	var entry = this.map[p];
	result.push (entry.value);
    }
    return result;
};

Map.prototype.size = function() {
    return this.map_size;
};

exports.Map = Map;
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
