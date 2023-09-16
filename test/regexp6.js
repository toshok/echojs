// skip-if: true
// esprima uses the JS regexp ctor while parsing regexps to validate them.  node can't deal with the regexp in this file.

// kanga

try {
    console.log("𠮷".match(/./u)[0].length === 2);
} catch (err) {
    console.log(err);
}
