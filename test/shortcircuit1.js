function fail(rv) {
    console.log("FAIL");
    return rv;
}

var s = "hi";

if (typeof s !== "string" && fail(true)) {
} else {
    console.log("PASS");
}

if (typeof s === "string" || fail(false)) {
    console.log("PASS");
} else {
}
