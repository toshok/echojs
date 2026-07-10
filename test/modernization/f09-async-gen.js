async function* g() { yield 1; } g().next().then((r) => console.log(r.value));
