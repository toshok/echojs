// generator: babel-node

var mySet = new Set();

mySet.add(1);
mySet.add(5);
mySet.add("some text");

console.log(mySet.has(1)); // true
console.log(mySet.has(3)); // false, 3 has not been added to the set
console.log(mySet.has(5)); // true
console.log(mySet.has(Math.sqrt(25))); // true
console.log(mySet.has("Some Text".toLowerCase())); // true

console.log(mySet.size); // 3

mySet.delete(5); // removes 5 from the set
console.log(mySet.has(5)); // false, 5 has been removed

console.log(mySet.size); // 2, we just removed one value
