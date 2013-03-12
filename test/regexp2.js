
var re = new RegExp("hello");

console.log ("hello, world".replace (re, "goodbye"));
console.log ("hello, world, hello".replace (re, "goodbye"));

re = /hello/;

console.log ("hello, world".replace (re, "goodbye"));
console.log ("hello, world, hello".replace (re, "goodbye"));
