
var buff = new ArrayBuffer (4);
var view = new DataView (buff);

function dumpStuff (view)
{
  console.log ("byte offset: " + view.byteOffset);
  console.log ("byte length: " + view.byteLength);

  for (var i = 0; i < view.byteLength; i++)
    console.log (view.getInt8 (i));
}

view.setInt8 (0, 1);
view.setInt8 (1, 3);
view.setInt8 (2, 5);
view.setInt8 (3, 7);

dumpStuff (view);

var view2 = new DataView (buff, 2, 2);
view2.setInt8 (0, 111);
view2.setInt8 (1, 113);

dumpStuff (view2);

