// "sending" from kangax
var sent;
function * generator(){
  sent = [yield 5, yield 6];
};
var iterator = generator();
iterator.next();
iterator.next('foo');
iterator.next('bar');
console.log (sent);
console.log (sent[0] === 'foo' && sent[1] === 'bar');
