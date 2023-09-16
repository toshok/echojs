var __slice = [].slice;

var foo = function () {
    var args, n;
    (n = arguments[0]), (args = 2 <= arguments.length ? __slice.call(arguments, 1) : []);

    console.log(n);
    console.log(args.length);
};

foo(1);
foo(1, 2);
foo(1, 2, 3, 4);
