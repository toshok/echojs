var a = 2, b = function(){};
Object.defineProperty(b, Symbol.create, { value: function() { a = 4; return {};} });
new b();
console.log( a === 4 );
