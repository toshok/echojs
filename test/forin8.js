var a = { hello: 1, world: 2 };
var k;
for (k in a) {
    console.log(k);
    if (k == "hello") break;
}
