// babel seems to have problems with new.target.  guess I need a newer version?
// generator: none

function foo() {
    console.log(new.target);
}

foo();
new foo();
