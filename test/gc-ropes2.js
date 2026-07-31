function build() {
    var parts = [];
    for (var i = 0; i < 50; i++) parts.push("x" + i);
    var s = "";
    for (var i = 0; i < 50; i++) s = s + "," + parts[i];
    return s;
}
var out = build();
console.log(out.length);
console.log(out.substring(0, 30));
