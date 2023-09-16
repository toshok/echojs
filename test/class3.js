// generator: babel-node

class Supest {
    constructor(foo) {
        console.log("Supest " + foo);
    }
}

class Super extends Supest {
    constructor(foo) {
        super();
        console.log("Super " + foo);
        this.foo = foo;
    }
}

class Sub extends Super {
    constructor(foo) {
        super(foo);
        console.log(this.foo);
    }
}

var s = new Sub(50);
