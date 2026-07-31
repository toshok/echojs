function* g() { try { yield 1; } finally { console.log("fin"); } } let it = g(); it.next(); it.return(5);
