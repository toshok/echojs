
class SuperClass {
  sayHi () {
    console.log ("hi from SuperClass");
  }
}

class TestClass extends SuperClass {
  sayHi() {
    console.log ("hi from TestClass");
    super();
  }
}


let t = new TestClass();
t.sayHi();
