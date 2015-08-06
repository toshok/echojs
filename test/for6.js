// generator: babel-node

var a = Array(5);
for (let i = 0; i < 5; i ++) {
  a[i] = function () {
    console.log ("hello world " + i);
   };
}
for (let i = 0; i < 5; i ++) {
  a[i]();
}
