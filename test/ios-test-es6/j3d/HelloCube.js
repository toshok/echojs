import * as fs from   '@node-compat/fs';
import { J3D, gl, v3 } from './j3d-all';

var engine, cube, light;
var ambient, camera;
var draw;

export function run(canvas) {
    engine = new J3D.Engine(canvas);

    engine.setClearColor(J3D.Color.white);

    ambient = new J3D.Transform();
    ambient.light = new J3D.Light(J3D.AMBIENT);
    ambient.light.color = new J3D.Color(0.5, 0.5, 0.5, 1);

    light = new J3D.Transform();
    light.light = new J3D.Light(J3D.DIRECT);
    light.light.color = new J3D.Color(0.5, 0.5, 0.5, 1);
    light.light.direction = new v3(1, 0, 1).norm();

    cube = new J3D.Transform();
    cube.geometry = J3D.Primitive.Cube(1, 1, 1);
    cube.renderer = J3D.BuiltinShaders.fetch("Phong");

    /*
     *  Optimization tip
     *
     *  To make sure the uniform value will be set only once,
     assign uniform values to "su" property instead of assigning them directly to the renderer.
     It helps performance, but you can't change this value continously once it's set.
     *
     "su" stands for "static uniforms". If you want to reload them once set "renderer.reloadStaticUniforms = true"

     If the same property exists in "su" and in the renderer - the latter will override the former.
     */
    //cube.renderer.su.color = new J3D.Color(0,0,0,1);
    cube.renderer.color = new J3D.Color(1,1,0,1);

    camera = new J3D.Transform();
    camera.camera = new J3D.Camera();
    camera.position.z = 4;
    engine.camera = camera;

    engine.scene.add(camera, cube, light, ambient);
}

export function update() {
    //console.log ("in HelloCube.update");
    cube.rotation.x += Math.PI * J3D.Time.deltaTime / 6000;
    cube.rotation.y += Math.PI/2 * J3D.Time.deltaTime / 3000;
}

export function draw() {
    //console.log ("in HelloCube.draw");
    //requestAnimationFrame(draw);
    engine.render();
}
