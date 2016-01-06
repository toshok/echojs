import * as fs from   '@node-compat/fs';

import { J3D, gl } from './j3d-all';

var plasmaShader =
"   uniform sampler2D uTexture;	\n"+
"	varying vec2 vTextureCoord;\n"+
"\n"+
"	void main(void) {\n"+
"		vec2 ca = vec2(0.1, 0.2);\n"+
"		vec2 cb = vec2(0.7, 0.9);\n"+
"		float da = distance(vTextureCoord, ca);\n"+
"		float db = distance(vTextureCoord, cb);\n"+
"		\n"+
"		float t = uTime * 0.5;\n"+
"		\n"+
"		float c1 = sin(da * cos(t) * 16.0 + t * 4.0);\n"+
"		float c2 = cos(vTextureCoord.y * 8.0 + t);\n"+
"		float c3 = cos(db * 14.0) + sin(t);\n"+
"	\n"+
"		float p = (c1 + c2 + c3) / 3.0;\n"+
"	\n"+
"		gl_FragColor = texture2D(uTexture, vec2(p, p));\n"+
"	}";

var engine, ctex, textures, post;

export function run(canvas) {

    engine = new J3D.Engine(canvas);

    textures = [
	new J3D.Texture("demo/models/textures/colorramp1.png"),
	new J3D.Texture("demo/models/textures/colorramp2.png"),
	new J3D.Texture("demo/models/textures/colorramp3.png")
    ];

    ctex = 1;

    post = new J3D.Postprocess(engine);
    post.filter = new J3D.Shader("PlasmaFilter", J3D.ShaderSource.BasicFilterVertex, plasmaShader, {
        fragmentIncludes: ["CommonFilterInclude"],
        vertexIncludes: ["CommonInclude"]
    });
    post.render = function() {
	J3D.Time.tick();
	this.renderEffect(textures[ctex].tex);
    };
}

export function draw() {
    post.render();
}

export function tap() {
    ctex = (ctex + 1) % 3;
    // XXX the first color ramp png is broken for some reason.. so let's just toggle between the other two for now.
    if (ctex == 0)
        ctex = 1;
}
