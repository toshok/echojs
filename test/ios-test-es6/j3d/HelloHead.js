import * as fs from   '@node-compat/fs';
import { J3D } from './j3d-all';

var gl;

var mx, my = 0;
var engine;
var path = "demo/models/014/";

var base, lee, eyeLeft, eyeRight, head, cam;
var tgry, tgrz, tgpz;
var auto;

var defphong, cover, reflective, toon, glitter, darkglass;
var dispersion;
var shd = 0;

var t = 0;
var shadersToLoad;

export function run(canvas) {
    console.log("Starting...");

    engine = new J3D.Engine(canvas);

    gl = engine.gl;

    shadersToLoad = 4;

    J3D.Loader.loadGLSL("demo/shaders/GoldHead.glsl", function(s) {
	reflective = s;
	setup();
    });

    J3D.Loader.loadGLSL("demo/shaders/Glitter.glsl", function(s) {
	glitter = s;
	setup();
    });

    J3D.Loader.loadGLSL("demo/shaders/DarkGlass.glsl", function(s) {
	darkglass = s;
	setup();
    });

    J3D.Loader.loadGLSL("demo/shaders/Stripes.glsl", function(s) {
	toon = s;
	setup();
    });
}

function setup() {
    shadersToLoad--;
    if(shadersToLoad > 0) return;

    console.log("Shaders loaded...");

    var cubemap = new J3D.Cubemap({
	left: path + "cubemap/front.jpg",
	right: path + "cubemap/back.jpg",
	up: path + "cubemap/left.jpg",
	down: path + "cubemap/right.jpg",
	back: path + "cubemap/top.jpg",
	front: path + "cubemap/top.jpg"
    });

    var headTex = new J3D.Texture(path + "map_col.jpg");

    var particleTex = new J3D.Texture(path + "particle01.png");
    particleTex.wrapMode = gl.CLAMP_TO_EDGE;

    var ramp = new J3D.Texture(path + "goldramp.png");

    reflective.uCubemap = cubemap;
    reflective.uColorTexture = headTex;

    darkglass.uCubemap = cubemap;
    darkglass.uColorTexture = headTex;
    darkglass.chromaticDispertion = [0.02, 0.0, -0.02];
    darkglass.bias = 0.0;
    darkglass.scale = 1.8;
    darkglass.power = 3.0;

    glitter.uColorSampler = ramp;
    glitter.uParticle = particleTex;

    toon.uColorSampler = ramp;

    J3D.Loader.loadJSON(path + "leeperry.js", function(jsmeshes) {
        J3D.Loader.loadJSON(path + "leeperryScene.js", function(jsscene) {
            jsscene.path = path;
            J3D.Loader.parseJSONScene(jsscene, jsmeshes, engine);

            base = engine.scene.find("base");
            cam = engine.scene.find("base/camera");

            lee = engine.scene.find("headbase");

            head = engine.scene.find("headbase/head");

            defphong = head.renderer;

            console.log("Models loaded...");

            setRenderer(darkglass, gl.TRIANGLES);
            setTransparency(false);
        });
    });

    //  document.onmousemove = onMouseMove;
    //  document.onmousedown = changeShaderManual;

    //  auto = setInterval(changeShader, 5000);
}

function setRenderer(r, m) {
    if (r != null) {
	head.renderer = r;
    } else {
	head.renderer = defphong;
    }

    if (m != null) {
	head.renderer.drawMode = m;
    }
}

function setTransparency(t, s, d) {
    head.geometry.setTransparency(t, s, d);
}

function changeShaderManual() {
    //  clearInterval(auto);
    changeShader();
}

function changeShader() {
    shd = (shd + 1) % 5;
    switch (shd) {
    case 0:
	setRenderer(null, gl.TRIANGLES);
	setTransparency(false);
	break;
    case 1:
	setRenderer(darkglass, gl.TRIANGLES);
	setTransparency(false);
	break;
    case 2:
	setRenderer(reflective, gl.TRIANGLES);
	setTransparency(false);
	break;
    case 3:
	setRenderer(toon, gl.TRIANGLES);
	setTransparency(false);
	break;
    case 4:
	setRenderer(glitter, gl.POINTS);
	setTransparency(true, gl.SRC_ALPHA, gl.DST_ALPHA);
	break;
    }
}

function onMouseMove(e) {
    mx = ( e.clientX / window.innerWidth  ) * 2 - 1;
    my = ( e.clientY / window.innerHeight ) * 2 - 1;
}

export function draw() {
    //requestAnimationFrame(draw);

    if(isNaN(mx)) mx = 0;
    if(isNaN(my)) my = 0;

    tgry = mx * Math.PI / 3.0;
    tgrz = -mx * Math.PI / 15.0;
    tgpz = my - 1;

    lee.position.z += (tgpz - lee.position.z) / 10;

    lee.rotation.y += (tgry - lee.rotation.y) / 2;
    lee.rotation.z += (tgrz - lee.rotation.z) / 12;
    lee.rotation.x = Math.sin(t) * 0.1;

    t += 0.1;

    engine.render();
}

export function tap() {
    changeShader();
}
