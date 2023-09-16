// generator: babel-node

function foo(...args) {
    console.log(args.length);
    console.log(args.join(" "));
}

foo("hello", "world");

function foo2(x, ...args) {
    console.log(args.length);
    console.log(x);
    console.log(args.join(" "));
}

foo2(5);
foo2(2, "hello", "world");
