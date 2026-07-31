function mk() {
    var x = null;
    return { set: function (v) { x = v; }, get: function () { return x; } };
}
var c = mk();
function churn(n) { var t = []; for (var i = 0; i < n; i++) t.push({ p: i }); return t.length; }
churn(2000);
c.set({ fresh: 42 });
churn(2000);
console.log(c.get().fresh);
