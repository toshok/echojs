// File generated with util/buildShaders.py. Do not edit //

J3D.ShaderSource = {};

J3D.ShaderSource.Depth = "\
//#name Depth\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
varying float depth;\n\
\n\
void main(void) {\n\
vec4 p = mMatrix * vec4(aVertexPosition, 1.0);\n\
gl_Position = pMatrix * vMatrix * p;\n\
depth = gl_Position.z/gl_Position.w;\n\
}\n\
\n\
//#fragment\n\
varying float depth;\n\
\n\
void main(void) {\n\
float d = 1.0 - depth;\n\
\n\
gl_FragColor = vec4(d, d, d, 1.0);\n\
}\n\
";

J3D.ShaderSource.Gouraud = "\
//#name Gouraud\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
uniform float specularIntensity;\n\
uniform float shininess;\n\
\n\
varying vec3 vLight;\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
vec4 p = mMatrix * vec4(aVertexPosition, 1.0);\n\
gl_Position = pMatrix * vMatrix * p;\n\
vTextureCoord = getTextureCoord(aTextureCoord);\n\
vec3 n = normalize( nMatrix * aVertexNormal );\n\
vLight = computeLights(p, n, specularIntensity, shininess);\n\
}\n\
\n\
//#fragment\n\
uniform vec4 color;\n\
uniform sampler2D colorTexture;\n\
uniform bool hasColorTexture;\n\
\n\
varying vec3 vLight;\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
vec4 tc = color;\n\
if(hasColorTexture) tc *= texture2D(colorTexture, vTextureCoord);\n\
gl_FragColor = vec4(tc.rgb * vLight, color.a);\n\
}\n\
";

J3D.ShaderSource.Lightmap = "\
//#name Lightmap\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
uniform vec4 lightmapAtlas;\n\
\n\
varying vec2 vTextureCoord;\n\
varying vec2 vTextureCoord2;\n\
\n\
void main(void) {\n\
vTextureCoord = getTextureCoord(aTextureCoord);\n\
vTextureCoord2 = aTextureCoord2 * lightmapAtlas.xy + lightmapAtlas.zw;\n\
\n\
gl_Position = mvpMatrix() * vec4(aVertexPosition, 1.0);\n\
}\n\
\n\
//#fragment\n\
uniform vec4 color;\n\
uniform sampler2D colorTexture;\n\
uniform sampler2D lightmapTexture;\n\
\n\
varying vec2 vTextureCoord;\n\
varying vec2 vTextureCoord2;\n\
\n\
void main(void) {\n\
\n\
vec4 tc = texture2D(colorTexture, vTextureCoord);\n\
vec4 lm = texture2D(lightmapTexture, vTextureCoord2);\n\
\n\
if(tc.a < 0.1) discard;\n\
else gl_FragColor = vec4(color.rgb * tc.rgb * lm.rgb, 1.0);\n\
}\n\
";

J3D.ShaderSource.Normal2Color = "\
//#name Normal2Color\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
varying vec3 vColor;\n\
\n\
void main(void) {\n\
gl_Position = mvpMatrix() * vec4(aVertexPosition, 1.0);\n\
vColor = normalize( aVertexNormal / 2.0 + vec3(0.5) );\n\
}\n\
\n\
//#fragment\n\
varying vec3 vColor;\n\
\n\
void main(void) {\n\
gl_FragColor = vec4(vColor, 1.0);\n\
}\n\
";

J3D.ShaderSource.Phong = "\
//#name Phong\n\
//#description Classic phong shader\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
varying vec4 vPosition;\n\
varying vec3 vLight;\n\
varying vec2 vTextureCoord;\n\
varying vec3 vNormal;\n\
\n\
void main(void) {\n\
vTextureCoord = getTextureCoord(aTextureCoord);\n\
vNormal = nMatrix * aVertexNormal;\n\
vPosition = mMatrix * vec4(aVertexPosition, 1.0);\n\
gl_Position = pMatrix * vMatrix * vPosition;\n\
gl_PointSize = 5.0;\n\
}\n\
\n\
//#fragment\n\
uniform vec4 color;\n\
uniform sampler2D colorTexture;\n\
uniform bool hasColorTexture;\n\
uniform float specularIntensity;\n\
uniform float shininess;\n\
\n\
varying vec4 vPosition;\n\
varying vec3 vLight;\n\
varying vec2 vTextureCoord;\n\
varying vec3 vNormal;\n\
\n\
void main(void) {\n\
vec4 tc = color;\n\
if(hasColorTexture) tc *= texture2D(colorTexture, vTextureCoord);\n\
vec3 l = computeLights(vPosition, vNormal, specularIntensity, shininess);\n\
gl_FragColor = vec4(tc.rgb * l, color.a);\n\
}\n\
";

J3D.ShaderSource.Reflective = "\
//#name Reflective\n\
//#description Based on Cg tutorial: http://http.developer.nvidia.com/CgTutorial/cg_tutorial_chapter07.html\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
\n\
varying vec3 vNormal;\n\
varying vec3 refVec;\n\
\n\
void main(void) {\n\
gl_Position = mvpMatrix() * vec4(aVertexPosition, 1.0);\n\
vNormal = normalize(nMatrix * aVertexNormal);\n\
vec3 incident = normalize( (vec4(aVertexPosition, 1.0) * mMatrix).xyz - uEyePosition);\n\
refVec = reflect(incident, vNormal);\n\
}\n\
\n\
//#fragment\n\
uniform samplerCube uCubemap;\n\
\n\
varying vec3 refVec;\n\
\n\
void main(void) {\n\
gl_FragColor = textureCube(uCubemap, refVec);\n\
}\n\
";

J3D.ShaderSource.Skybox = "\
//#name Skybox\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
uniform float mid;\n\
\n\
varying vec3 vVertexPosition;\n\
\n\
void main(void) {\n\
gl_Position = pMatrix * vMatrix * vec4(uEyePosition + aVertexPosition * mid, 1.0);\n\
vVertexPosition = aVertexPosition;\n\
}\n\
\n\
//#fragment\n\
uniform samplerCube uCubemap;\n\
\n\
varying vec3 vVertexPosition;\n\
\n\
void main(void) {\n\
gl_FragColor = textureCube(uCubemap, vVertexPosition);\n\
}\n\
";

J3D.ShaderSource.Toon = "\
//#name Toon\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
varying float vLight;\n\
varying vec2 vTextureCoord;\n\
\n\
float cli(vec4 p, vec3 n, lightSource light){\n\
vec3 ld;\n\
if(light.type == 0) return 0.0;\n\
else if(light.type == 1) ld = -light.direction;\n\
else if(light.type == 2) ld = normalize(light.position - p.xyz);\n\
return max(dot(n, ld), 0.0);\n\
}\n\
\n\
float lightIntensity(vec4 p, vec3 n) {\n\
float s = cli(p, n, uLight[0]);\n\
s += cli(p, n, uLight[1]);\n\
s += cli(p, n, uLight[2]);\n\
s += cli(p, n, uLight[3]);\n\
return s;\n\
}\n\
\n\
void main(void) {\n\
vec4 p = mMatrix * vec4(aVertexPosition, 1.0);\n\
gl_Position = pMatrix * vMatrix * p;\n\
gl_PointSize = 10.0;\n\
vTextureCoord = getTextureCoord(aTextureCoord);\n\
vec3 n = normalize( nMatrix * aVertexNormal );\n\
vLight = lightIntensity(p, n);\n\
}\n\
\n\
//#fragment\n\
uniform vec4 uColor;\n\
uniform sampler2D uColorSampler;\n\
\n\
varying float vLight;\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
vec4 tc = texture2D(uColorSampler, vec2(vLight, 0.5) );\n\
gl_FragColor = vec4(tc.rgb, 1.0);\n\
}\n\
";

J3D.ShaderSource.Vignette = "\
//#name Vignette\n\
//#author bartekd\n\
\n\
//#vertex\n\
//#include BasicFilterVertex\n\
\n\
//#fragment\n\
//#include CommonFilterInclude\n\
uniform sampler2D uTexture;\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
vec2 m = vec2(0.5, 0.5);\n\
float d = distance(m, vTextureCoord) * 1.0;\n\
vec3 c = texture2D(uTexture, vTextureCoord).rgb * (1.0 - d * d);\n\
gl_FragColor = vec4(c.rgb, 1.0);\n\
}\n\
";

J3D.ShaderSource.BasicFilterVertex = "\
//#name BasicFilterVertex\n\
//#description A basic vertex shader for filters that use a full screen quad and have all the logic in fragment shader\n\
attribute vec2 aVertexPosition;\n\
attribute vec2 aTextureCoord;\n\
\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
gl_Position = vec4(aVertexPosition, 0.0, 1.0);\n\
vTextureCoord = aTextureCoord;\n\
}\n\
\n\
";

J3D.ShaderSource.CommonFilterInclude = "\
//#name CommonFilterInclude\n\
//#description Common uniforms and function for filters\n\
#ifdef GL_ES\n\
precision highp float;\n\
#endif\n\
\n\
uniform float uTime;\n\
\n\
float whiteNoise(vec2 uv, float scale) {\n\
float x = (uv.x + 0.2) * (uv.y + 0.2) * (10000.0 + uTime);\n\
x = mod( x, 13.0 ) * mod( x, 123.0 );\n\
float dx = mod( x, 0.005 );\n\
return clamp( 0.1 + dx * 100.0, 0.0, 1.0 ) * scale;\n\
}\n\
";

J3D.ShaderSource.CommonInclude = "\
//#name CommonInclude\n\
//#description Collection of common uniforms, functions and structs to include in shaders (both fragment and vertex)\n\
#ifdef GL_ES\n\
precision highp float;\n\
#endif\n\
\n\
struct lightSource {\n\
int type;\n\
vec3 direction;\n\
vec3 color;\n\
vec3 position;\n\
};\n\
\n\
uniform float uTime;\n\
uniform mat4 mMatrix;\n\
uniform mat4 vMatrix;\n\
uniform mat3 nMatrix;\n\
uniform mat4 pMatrix;\n\
uniform vec3 uEyePosition;\n\
uniform lightSource uLight[4];\n\
uniform vec3 uAmbientColor;\n\
uniform vec4 uTileOffset;\n\
\n\
mat4 mvpMatrix() {\n\
return pMatrix * vMatrix * mMatrix;\n\
}\n\
\n\
mat4 mvMatrix() {\n\
return vMatrix * mMatrix;\n\
}\n\
\n\
float luminance(vec3 c) {\n\
return c.r * 0.299 + c.g * 0.587 + c.b * 0.114;\n\
}\n\
\n\
float brightness(vec3 c) {\n\
return c.r * 0.2126 + c.g * 0.7152 + c.b * 0.0722;\n\
}\n\
\n\
vec3 computeLight(vec4 p, vec3 n, float si, float sh, lightSource light){\n\
vec3 ld;\n\
\n\
if(light.type == 0) return vec3(0);\n\
else if(light.type == 1) ld = -light.direction;\n\
else if(light.type == 2) ld = normalize(light.position - p.xyz);\n\
\n\
float dif = max(dot(n, ld), 0.0);\n\
\n\
float spec = 0.0;\n\
\n\
if(si > 0.0) {\n\
vec3 eyed = normalize(uEyePosition - p.xyz);\n\
vec3 refd = reflect(-ld, n);\n\
spec = pow(max(dot(refd, eyed), 0.0), sh) * si;\n\
};\n\
\n\
return light.color * dif + light.color * spec;\n\
}\n\
\n\
vec3 computeLights(vec4 p, vec3 n, float si, float sh) {\n\
vec3 s = uAmbientColor;\n\
s += computeLight(p, n, si, sh, uLight[0]);\n\
s += computeLight(p, n, si, sh, uLight[1]);\n\
s += computeLight(p, n, si, sh, uLight[2]);\n\
s += computeLight(p, n, si, sh, uLight[3]);\n\
return s;\n\
}\n\
\n\
vec2 getTextureCoord(vec2 uv) {\n\
return uv * uTileOffset.xy + uTileOffset.zw;\n\
}\n\
";

J3D.ShaderSource.Modifiers = "\
//#name Modifiers\n\
//#description A collection of modifier functions for geometry (only bend for now)\n\
\n\
vec3 bend(vec3 ip, float ba, vec2 b, float o, float a) {\n\
vec3 op = ip;\n\
\n\
ip.x = op.x * cos(a) - op.y * sin(a);\n\
ip.y = op.x * sin(a) + op.y * cos(a);\n\
\n\
if(ba != 0.0) {\n\
float radius = b.y / ba;\n\
float onp = (ip.x - b.x) / b.y - o;\n\
ip.z = cos(onp * ba) * radius - radius;\n\
ip.x = (b.x + b.y * o) + sin(onp * ba) * radius;\n\
}\n\
\n\
op = ip;\n\
ip.x = op.x * cos(-a) - op.y * sin(-a);\n\
ip.y = op.x * sin(-a) + op.y * cos(-a);\n\
\n\
return ip;\n\
}\n\
";

J3D.ShaderSource.VertexInclude = "\
//#name VertexInclude\n\
//#description Common attributes for a mesh - include this in a vertex shader so you don't rewrite this over and over again\n\
attribute vec3 aVertexPosition;\n\
attribute vec3 aVertexNormal;\n\
attribute vec2 aTextureCoord;\n\
attribute vec2 aTextureCoord2;\n\
attribute vec4 aVertexColor;\n\
";
