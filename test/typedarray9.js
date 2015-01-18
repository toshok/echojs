function logArrayElements(element, index, array) {
    console.log("a[" + index + "] = " + element);
}

let arr = new Int32Array(3);
arr[0] = 2;
arr[1] = 5;
arr[2] = 9;

arr.forEach(logArrayElements);
// logs:
// a[0] = 2
// a[1] = 5
// a[2] = 9

let arr2 = new Int8Array(3);
arr2[0] = 7;
arr2[2] = 120;

arr2.forEach(logArrayElements);
// logs:
// a[0] = 7
// a[1] = 0
// a[2] = 120

