var a = [5, 4, 3, 2, 1];

for (var i in a) {
    console.log(typeof i);
    if (a.hasOwnProperty(i)) console.log(a[i]);
}
