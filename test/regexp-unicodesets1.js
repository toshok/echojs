// the v flag (unicodeSets): set algebra, string members, and flag
// surface.  Only Unicode-version-stable assertions, so the
// node-generated baseline agrees regardless of the host's ICU.

// union, difference, intersection
console.log(/^[\p{L}--\p{Lu}]$/v.test("a"));
console.log(/^[\p{L}--\p{Lu}]$/v.test("A"));
console.log(/^[\p{L}&&\p{Script=Greek}]$/v.test("α"));
console.log(/^[[a-z][0-9]]+$/v.test("a0z9"));
console.log(/^[[a-z]--[aeiou]]$/v.test("b"));
console.log(/^[[a-z]--[aeiou]]$/v.test("e"));
console.log(/^[[a-z]&&[^aeiou]]$/v.test("z"));

// string literals in classes
console.log(/^[\q{abc|xy}]$/v.test("abc"));
console.log(/^[\q{abc|xy}]$/v.test("xy"));
console.log(/^[\q{abc|xy}]$/v.test("ab"));
console.log(/^[\q{a}b]$/v.test("b"));
console.log("1abc2".replace(/[\q{abc}0-9]/gv, "#"));

// properties of strings (flag sequences have been stable for years)
console.log(/^\p{RGI_Emoji_Flag_Sequence}$/v.test("\u{1F1FA}\u{1F1F8}"));
console.log(/^\p{RGI_Emoji_Flag_Sequence}$/v.test("a"));

// negation (never with strings)
console.log(/^[^a-z]$/v.test("A"));
console.log(/^[^\p{ASCII}]$/v.test("α"));

// astral ranges and escapes
console.log(/^[\u{1F600}-\u{1F64F}]$/v.test("\u{1F600}"));
console.log(/^[\\--\-]$/v.source.length > 0);

// flag surface
console.log(new RegExp("a", "v").unicodeSets);
console.log(new RegExp("a", "v").unicode);
console.log(new RegExp("a", "gvy").flags);
console.log(/a/gimvy.flags);

// errors
for (const [pat, flags] of [
    ["[^\\q{abc}]", "v"],   // negated class with strings
    ["\\P{RGI_Emoji}", "v"], // negated string property
    ["[a--b&&c]", "v"],      // mixed operators
    ["a", "uv"],             // both unicode flags
]) {
    try {
        new RegExp(pat, flags);
        console.log("no throw for " + pat);
    } catch (e) {
        console.log(e instanceof SyntaxError);
    }
}
