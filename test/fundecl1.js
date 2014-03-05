/*
if (typeof(console) === "undefined") {
  var console = {
    log: print
  };
}
*/

function foodecl(a) {
    function bar () { console.log ("whu"); };
    bar();

    if (a) {
	function bar () { console.log ("hi"); };
    }
    else {
	function bar () { console.log ("bye"); };
    }

    bar();
}

function fooexp(a) {
    var bar = function () { console.log ("whu"); };
    bar();

    if (a) {
	var bar = function () { console.log ("hi"); };
    }
    else {
	var bar = function () { console.log ("bye"); };
    }

    bar();
}

foodecl(true);
foodecl(false);
fooexp(true);
fooexp(false);
