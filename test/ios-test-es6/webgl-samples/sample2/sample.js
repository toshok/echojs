importScripts ("../sylvester.js",
	       "../glUtils.js",
	       "webgl-demo.js");

var fragment_shader_source =
  "void main(void) {" +
  "   gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);" +
  "}";

var vertex_shader_source =
  "attribute vec3 aVertexPosition;" +
  "uniform mat4 uMVMatrix;" +
  "uniform mat4 uPMatrix;" +

"void main(void) { " +
  "   gl_Position = uPMatrix * uMVMatrix * vec4(aVertexPosition, 1.0);" +
  "}";

exports.run = start;

exports.draw = drawScene;
