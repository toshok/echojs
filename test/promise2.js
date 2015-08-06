// generator: traceur
// skip-if: runloop_impl == 'noop'

Promise.resolve(5).then((value) => {
    console.log(value);
    throw new Error(value);
}).catch((err) => {
    console.log(err);
    return 8;
}).then((value) => {
    console.log(value);
});
