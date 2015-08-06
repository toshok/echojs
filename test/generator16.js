// generator: babel-node
// xfail: generator support isn't 100%

// "yield *, iterator closing via throw()" from kangax

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

var closed = false;
var iter = __createIterableObject(1, 2, 3);
iter['throw'] = undefined;
iter['return'] = function(){
  closed = true;
  return {done: true};
}
var gen = (function*(){
  try {
    yield *iter;
  } catch(e){}
})();
gen.next();
gen['throw']();
console.log(closed);
