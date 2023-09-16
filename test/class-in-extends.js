class B extends class {
    qux() {
        return "bar";
    }
} {
    qux() {
        return super.qux() + this.corge;
    }
}
var obj = {
    qux: B.prototype.qux,
    corge: "ley",
};
console.log(obj.qux() === "barley");
