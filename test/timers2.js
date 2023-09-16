// skip-if: runloop_impl == 'noop'

var sum = 0;

var t1 = setInterval(function () {
    if (sum >= 69) {
        clearInterval(t1);
        return;
    }

    console.log(sum++);
}, 20);

var t2 = setInterval(function () {
    sum++;
});
clearInterval(t2);
