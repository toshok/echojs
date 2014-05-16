// kangax test for regexp y flag

try {
    var re = new RegExp('\\w');
    var re2 = new RegExp('\\w', 'y');
    re.exec('xy');
    re2.exec('xy');
    console.log (re.exec('xy')[0] === 'x' && re2.exec('xy')[0] === 'y');
} catch (e) {
    console.log(e);
}
