var a = { hello: 1, world: 2 };
var k;
for (k in a) {
    if (k == "world") continue;
    console.log(k);
}
