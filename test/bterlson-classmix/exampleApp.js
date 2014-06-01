import { mix } from "mix"

class A {
    constructor() { console.log('constructing a') }
    aMethod() { console.log('a method') }
    static aStaticMethod() { console.log('static a method') }
}

class B {
    constructor() { console.log('constructing b') }
    bMethod() { console.log('b method') }
    static bStaticMethod() { console.log('static b method') }
}
 
class Foo extends mix(A, B) {
    constructor() {
        super.A();
        super.B();
    }
 
    fooMethod() {
        console.log('fooMethod');
    }
 
    aMethod() {
        console.log('subclass aMethod');
        super();
    }

    static bStaticMethod() {
        console.log('subclass static bMethod');
        super();
    }
}

// Can call any method on A.prototype or B.prototype
// (first one wins)
var foo = new Foo();
foo.fooMethod();
foo.aMethod();
foo.bMethod();
 
// Dynamic changes to prototypes works as expected
A.prototype.newAMethod = function() { console.log('new A method');};
B.prototype.newBMethod = function() { console.log('new B method');};
foo.newAMethod();
foo.newBMethod();

// Static methods are also inherited
Foo.aStaticMethod();
Foo.bStaticMethod();
