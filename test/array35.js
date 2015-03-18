var stringArray = ['Blue', 'Humpback', 'Beluga'];
var numericStringArray = ['80', '9', '700'];
var numberArray = [40, 1, 5, 200];
var mixedNumericArray = ['80', '9', '700', 40, 1, 5, 200];

function compareNumbers(a, b) {
  return a - b;
}

// again, assumes a print function is defined
console.log('stringArray:', stringArray.join());
console.log('Sorted:', stringArray.sort().join());

console.log('numberArray:', numberArray.join());
console.log('Sorted without a compare function:', numberArray.sort().join());
console.log('Sorted with compareNumbers:', numberArray.sort(compareNumbers).join());

console.log('numericStringArray:', numericStringArray.join());
console.log('Sorted without a compare function:', numericStringArray.sort().join());
console.log('Sorted with compareNumbers:', numericStringArray.sort(compareNumbers).join());

console.log('mixedNumericArray:', mixedNumericArray.join());
console.log('Sorted without a compare function:', mixedNumericArray.sort().join());
console.log('Sorted with compareNumbers:', mixedNumericArray.sort(compareNumbers).join());

