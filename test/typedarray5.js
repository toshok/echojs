var buff = new ArrayBuffer(4);
var view = new DataView(buff);

function dumpStuff(view) {
    console.log("byte offset: " + view.byteOffset);
    console.log("byte length: " + view.byteLength);

    for (var i = 0; i < view.byteLength; i++) console.log(view[i]);
}

view[0] = 1;
view[1] = 3;
view[2] = 5;
view[3] = 7;

dumpStuff(view);

var view2 = new DataView(buff, 2, 2);
view2[0] = 111;
view2[1] = 113;

dumpStuff(view2);
dumpStuff(view);
