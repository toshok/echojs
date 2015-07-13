
function log(message) {
    if (message == undefined)
        message = "none";

    console.log(message);
}

// Simple timeouts
setTimeout(log);
setTimeout(log, -99, "-99");
setTimeout(log, 0, "0-1");
setTimeout(log, 0, "0-2");
setTimeout(log, 20, "20");
setTimeout(log, 30, "30");

// Canceled timeouts.
var t1 = setTimeout(log, 0, "0-3");
var t2 = setTimeout(log, 100, "100");
clearTimeout(t1);
clearTimeout(t2);

// A late-canceled one.
var t3 = setTimeout(log, 40, "40");
setTimeout(function() { clearTimeout(t3); });

