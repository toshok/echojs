function tryInvoke(f) {
    try {
        f();
    } catch (e) {
        console.log("f = null");
    }
}

function foo() {
    tryInvoke(f);

    function f() {
        console.log("hello world1");
    }

    tryInvoke(f);

    f = null;

    tryInvoke(f);

    function f() {
        console.log("hello world2");
    }

    tryInvoke(f);
}

foo();
