// generator: babel-node

// 'Stolen' from the Mozilla's developers docs.

console.log ([1, 2, 3, 4, 5].copyWithin (0, 3));
// [4, 5, 3, 4, 5]

console.log ([1, 2, 3, 4, 5].copyWithin (0, 3, 4));
// [4, 2, 3, 4, 5]

console.log ([1, 2, 3, 4, 5].copyWithin (0, -2, -1));
// [4, 2, 3, 4, 5]

