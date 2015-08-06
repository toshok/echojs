// generator: babel-node
// xfail: generator support isn't 100%

// "yield *, generic iterables" from kangax

function __createIterableObject(a, b, c) {
    var arr = [a, b, c, ,];
    var iterable = {
        next: function() {
	    return { value: arr.shift(), done: arr.length <= 0 };
        }
    };
    iterable[Symbol.iterator] = function(){ return iterable; };
    return iterable;
}


var iterator = (function * generator() {
  yield * __createIterableObject(5, 6, 7);
}());
var item = iterator.next();
var passed = item.value === 5 && item.done === false;
item = iterator.next();
passed    &= item.value === 6 && item.done === false;
item = iterator.next();
passed    &= item.value === 7 && item.done === false;
item = iterator.next();
passed    &= item.value === undefined && item.done === true;
console.log(passed);

