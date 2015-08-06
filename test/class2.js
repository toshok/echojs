// generator: babel-node

class SuperClass {
  constructor () {
    console.log ("constructing SuperClass");
  }

  sayHi () {
    console.log ("hi from SuperClass");
  }
}

class TestClass extends SuperClass {
  constructor () {
    console.log ("constructing TestClass");
    super();
  }

  sayHi() {
    console.log ("hi from TestClass");
    super.sayHi();
  }
}


let t = new TestClass();
t.sayHi();
