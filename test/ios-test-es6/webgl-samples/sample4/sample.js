importScripts ("../sylvester.js",
	       "../glUtils.js",
	       "webgl-demo.js");

var fragment_shader_source =
  "varying lowp vec4 vColor;" +
  "void main(void) {" +
  "   gl_FragColor = vColor;" +
  "}";

var vertex_shader_source =
  "attribute vec3 aVertexPosition;" +
  "attribute vec4 aVertexColor;" +

  "uniform mat4 uMVMatrix;" +
  "uniform mat4 uPMatrix;" +

  "varying lowp vec4 vColor;" +

  "void main(void) {" +

  "  gl_Position = uPMatrix * uMVMatrix * vec4(aVertexPosition, 1.0);" +
  "  vColor = aVertexColor;" +
  "}";

exports.run = start;

exports.draw = drawScene;
