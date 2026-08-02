console.log("a-b-c".replaceAll("-", "+"));
console.log("aaa".replaceAll("a", "bb"));
console.log("abc".replaceAll("z", "+"));
console.log("".replaceAll("a", "b"));

// non-overlapping matches
console.log("aaa".replaceAll("aa", "b"));
console.log("aaaa".replaceAll("aa", "b"));

// empty search inserts at every position, including start and end
console.log("xy".replaceAll("", "-"));
console.log("".replaceAll("", "-"));

// $-substitutions
console.log("a-b".replaceAll("-", "[$&]"));
console.log("a-b".replaceAll("-", "$`"));
console.log("a-b".replaceAll("-", "$'"));
console.log("a-b".replaceAll("-", "$$"));

// functional replacement gets (matched, position, string)
console.log("a-b-c".replaceAll("-", function (m, p, s) { return "(" + m + "," + p + "," + s + ")"; }));

// global regexps delegate to regexp replace
console.log("a1b2".replaceAll(/[0-9]/g, "#"));

// non-global regexps throw
try { "a1b2".replaceAll(/[0-9]/, "#"); } catch (e) { console.log("non-global:", e.constructor.name); }

// search and replacement go through ToString
console.log("a1b".replaceAll(1, 2));

console.log("".replaceAll.length);
