import * as fs from   '@node-compat/fs';
import { J3D, gl } from './j3d-all';

var mx = 2, my = 2;
var jsonScene;
var root;
var rings = [];
var engine;

export function run(canvas) {
    engine = new J3D.Engine(canvas);

    J3D.Loader.loadJSON("demo/models/text.json", function(jsmeshes) {
	J3D.Loader.loadJSON("demo/models/textScene.json", function(jsscene) {
	    jsonScene = jsscene;
	    J3D.Loader.parseJSONScene(jsscene, jsmeshes, engine);

	    root = engine.scene.find("root");

	    rings[0] = engine.scene.find("root/ring");
	    rings[1] = engine.scene.find("root/ring/ring");
	    rings[2] = engine.scene.find("root/ring/ring/ring");
	    rings[3] = engine.scene.find("root/ring/ring/ring/ring");
	    rings[4] = engine.scene.find("root/ring/ring/ring/ring/ring");
	    rings[5] = engine.scene.find("root/ring/ring/ring/ring/ring/ring");
	    rings[6] = engine.scene.find("root/ring/ring/ring/ring/ring/ring/ring");
	});
    });

    /*document.onmousemove = onMouseMove;*/
}

function onMouseMove(e) {
    mx = ( e.clientX / window.innerWidth  ) * 2 - 1;
    my = ( e.clientY / window.innerHeight ) * 2 - 1;
}

export function draw() {
    //  requestAnimationFrame(draw);

    if(!mx) mx = 0;
    if(!my) my = 0;

    var sx = mx * J3D.Time.deltaTime / 1000;
    var sy = my * J3D.Time.deltaTime / 1000;

    root.rotation.y += sx;

    rings[0].rotation.y += sx;
    rings[1].rotation.y -= sx * 0.1;
    rings[2].rotation.y -= sx * 0.12;
    rings[3].rotation.y -= sx * 0.14;
    rings[4].rotation.y -= sx * 0.16;
    rings[5].rotation.y -= sx * 0.18;
    rings[6].rotation.y -= sx * 0.2;

    rings[0].rotation.x += sy;
    rings[1].rotation.x -= sy * 0.1;
    rings[2].rotation.x -= sy * 0.12;
    rings[3].rotation.x -= sy * 0.14;
    rings[4].rotation.x -= sy * 0.16;
    rings[5].rotation.x -= sy * 0.18;
    rings[6].rotation.x -= sy * 0.2;

    engine.render();
}
