// generator: babel-node
// xfail: generator support isn't 100%

// "yield *, iterator closing" from kangax

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

var closed = '';
var iter = __createIterableObject(1, 2, 3);
iter['return'] = function(){
  closed += 'a';
  return {done: true};
}
var gen = (function* generator(){
  try {
    yield *iter;
  } finally {
    closed += 'b';
  }
})();
gen.next();
gen['return']();
console.log(closed === 'ab');
