var j = 0;
outer: while (j < 10) {
    var i = 0;
    while (i < 10) {
        console.log("hello world");
        i = i + 1;
        if (i == 5) break outer;
    }

    j = j + 1;
}
