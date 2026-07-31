try { throw new Error("x"); } catch { console.log("caught"); }
try { try { throw 1; } catch { throw 2; } } catch (e) { console.log("outer", e); }
let n = 0;
function attempt() { try { n++; if (n < 3) throw n; return n; } catch { return attempt(); } }
console.log(attempt());
