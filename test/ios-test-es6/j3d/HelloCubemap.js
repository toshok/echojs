var fs = require('fs');

//XXX this should be var j3d_all = require("j3d/j3d-all");
var j3d_all = require("j3d-all");
var J3D = j3d_all.J3D;
var gl = j3d_all.gl;

var engine, scene, cubemap;
var root, monkey, camera;
var mx = 1.5, my = 1.5;
var isMouseDown = false;

exports.run = function (canvas) {

  engine = new J3D.Engine(canvas);
  engine.setClearColor(J3D.Color.white);

  camera = new J3D.Transform();
  camera.camera = new J3D.Camera({far: 100});
  camera.position.z = 5;
  engine.camera = camera;

  root = new J3D.Transform();
  root.add(camera);
  engine.scene.add(root);

  cubemap = new J3D.Cubemap({
    left: "demo/models/textures/skybox/left.jpg",
    right: "demo/models/textures/skybox/right.jpg",
    up: "demo/models/textures/skybox/up.jpg",
    down: "demo/models/textures/skybox/down.jpg",
    back: "demo/models/textures/skybox/back.jpg",
    front: "demo/models/textures/skybox/front.jpg"
  });

  engine.scene.addSkybox(cubemap);

  monkey = new J3D.Transform();
  monkey.renderer = J3D.BuiltinShaders.fetch("Reflective");
  monkey.renderer.uCubemap = cubemap;
  J3D.Loader.loadJSON("demo/models/monkeyhi.js", function(j) { monkey.geometry = new J3D.Mesh(j); } );
  engine.scene.add(monkey);

  /*document.onmousemove = onMouseMove;*/
};

function onMouseMove(e) {
  mx = ( e.clientX / window.innerWidth  ) * 2 - 1;
  my = ( e.clientY / window.innerHeight ) * 2 - 1;
}

exports.draw = function () {
  //requestAnimationFrame(draw);

  if (!isNaN(mx) && !isNaN(my)) {
    // Rotate the root to which camera is attached
    root.rotation.x += my * J3D.Time.deltaTime / 2000;
    root.rotation.y += mx * J3D.Time.deltaTime / 3000;
  }

  engine.render();
};




