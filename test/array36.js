// the array to be sorted
var list = ["Delta", "alpha", "CHARLIE", "bravo"];

// temporary holder of position and sort-value
var map = list.map(function (e, i) {
    return { index: i, value: e.toLowerCase() };
});

// sorting the map containing the reduced values
map.sort(function (a, b) {
    return +(a.value > b.value) || +(a.value === b.value) - 1;
});

// container for the resulting order
var result = map.map(function (e) {
    return list[e.index];
});

console.log(result.join());
