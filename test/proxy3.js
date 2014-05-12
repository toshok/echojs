// from MDN
let validator = {
  set: function(obj, prop, value) {
    if (prop === 'age') {
      if (!Number.isInteger(value)) {
        throw new TypeError('The age is not an integer');
      }
      if (value > 200) {
        throw new RangeError('The age seems invalid');
      }
    }

    // The default behavior to store the value
    obj[prop] = value;
  }
};

let person = new Proxy({}, validator);

person.age = 100;
console.log(person.age); // 100
try {
    person.age = 'young'; // Throws an exception
    console.log(person.age);
}
catch (e) {
    console.log(e);
}
try {
    person.age = 300; // Throws an exception
    console.log(person.age);
}
catch (e) {
    console.log(e);
}
