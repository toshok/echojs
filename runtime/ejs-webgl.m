/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifdef IOS

#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>

#include "ejs-value.h"
#include "ejs-jsobjc.h"
#include "ejs-objc.h"
#include "ejs-webgl.h"
#include "ejs-error.h"
#include "ejs-ops.h"
#include "ejs-array.h"
#include "ejs-typedarrays.h"

@implementation WebGLObject
-(id)initWithGLId:(GLuint)glid
{
  _glId = glid;
  return self;
}

-(NSString*)description
{
  return [NSString stringWithFormat:@"<%@ glId=%u>", [self class], _glId];
}

-(GLuint)glId
{
  return _glId;
}
@end

@implementation WebGLBuffer
-(void)dealloc
{
    [self deleteBuffer];
    [super dealloc];
}

-(void)deleteBuffer
{
    glDeleteBuffers(1, &_glId);
    _glId = 0;    
}
@end

@implementation WebGLFramebuffer
-(void)dealloc
{
    [self deleteFramebuffer];
    [super dealloc];
}

-(void)deleteFramebuffer
{
    glDeleteFramebuffers(1, &_glId);
    _glId = 0;
}
@end

@implementation WebGLProgram
-(void)dealloc
{
    [self deleteProgram];
    [super dealloc];
}


-(void)deleteProgram
{
    glDeleteProgram(_glId);
    _glId = 0;    
}
@end

@implementation WebGLRenderbuffer
-(void)dealloc
{
    [self deleteRenderbuffer];
    [super dealloc];
}

-(void)deleteRenderbuffer
{
    glDeleteRenderbuffers(1, &_glId);
    _glId = 0;
}
@end

@implementation WebGLShader
-(void)dealloc
{
    [self deleteShader];
    [super dealloc];
}

-(void)deleteShader
{
    glDeleteShader(_glId);
    _glId = 0;
}
@end

@implementation WebGLTexture
-(void)dealloc
{
    [self deleteTexture];
    [super dealloc];
}

-(void)deleteTexture
{
    glDeleteTextures(1, &_glId);
    _glId = 0;
}
@end

@implementation WebGLActiveInfo
-(id)initWithName:(const char*)n type:(GLenum)t size:(GLint)s
{
  _name = strdup(n ? n : "");
  _type = t;
  _size = s;
  return self;
}

-(void)dealloc
{
  free (_name);
  [super dealloc];
}

-(GLint)size
{
  return _size;
}
-(GLenum)type
{
  return _type;
}
-(const char*)name
{
  return _name;
}
@end

typedef struct  {
    EJSObject obj;
    id peer;
} EJSWebGLObject;

static id
get_peer (ejsval obj)
{
    return ((EJSWebGLObject*)EJSVAL_TO_OBJECT(obj))->peer;
}

static void
finalize_release_private_data (EJSObject* obj)
{
	id peer = ((EJSWebGLObject*)obj)->peer;
	[peer release];
}

EJSSpecOps WebGLRenderingContext_specops;
EJSSpecOps WebGLBuffer_specops;
EJSSpecOps WebGLFramebuffer_specops;
EJSSpecOps WebGLRenderbuffer_specops;
EJSSpecOps WebGLProgram_specops;
EJSSpecOps WebGLShader_specops;
EJSSpecOps WebGLTexture_specops;
EJSSpecOps WebGLActiveInfo_specops;
EJSSpecOps WebGLUniformLocation_specops;

static ejsval WebGLActiveInfo__proto__;

static ejsval
webglactiveinfo_get_size (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
	WebGLActiveInfo *info = (WebGLActiveInfo*)get_peer(_this);

	return NUMBER_TO_EJSVAL ([info size]);
}

static ejsval
webglactiveinfo_get_type (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
	WebGLActiveInfo *info = (WebGLActiveInfo*)get_peer(_this);

	return NUMBER_TO_EJSVAL ([info type]);
}

static ejsval
webglactiveinfo_get_name (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
	WebGLActiveInfo *info = (WebGLActiveInfo*)get_peer(_this);

	return _ejs_string_new_utf8 ([info name]);
}

#define SPEW(x) x
#define CHECK_GL_ERRORS 1

#define WEBGL_UNPACK_FLIP_Y_WEBGL 0x9240
#define WEBGL_UNPACK_PREMULTIPLY_ALPHA_WEBGL 0x9241
#define WEBGL_UNPACK_COLORSPACE_CONVERSION_WEBGL 0x9243

#if CHECK_GL_ERRORS
static GLenum glerr;
#define CHECK_GL do {							\
    if ((glerr = glGetError ()) != GL_NO_ERROR) {			\
         NSLog (@"GL error = %d, at %s, line %d", glerr, __PRETTY_FUNCTION__, __LINE__);	\
         abort();								\
    }									\
} while (0)
#else
#define CHECK_GL
#endif

static BOOL unpack_flip_y = NO;

#define JSMETHODNAME(meth) webglrenderingcontext_func_##meth
#define JSMETHOD(meth) static ejsval JSMETHODNAME(meth) (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
#define THROW_ARG_COUNT_EXCEPTION(expected_count) _ejs_throw_nativeerror_utf8 (EJS_ERROR/*XXX*/, "argument count mismatch")


/*
 readonly attribute HTMLCanvasElement canvas;
 readonly attribute GLsizei drawingBufferWidth;
 readonly attribute GLsizei drawingBufferHeight;
 
 WebGLContextAttributes getContextAttributes();
 boolean isContextLost();
 
 DOMString[ ] getSupportedExtensions();
 object getExtension(DOMString name);
 */

//    void activeTexture(GLenum texture);
JSMETHOD (activeTexture) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	GLenum texture = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	SPEW(NSLog (@"glActiveTexture (%d)", texture);)
	glActiveTexture (texture);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void attachShader(WebGLProgram program, WebGLShader shader);
JSMETHOD (attachShader) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
	WebGLShader *shader = (WebGLShader*)get_peer (args[1]);
    
	SPEW(NSLog (@"glAttachShader (program = %d, shader = %d)", [program glId], [shader glId]);)
    
	glAttachShader ([program glId], [shader glId]);
	CHECK_GL;
    
	return _ejs_undefined;
}

/*
 void bindAttribLocation(WebGLProgram program, GLuint index, DOMString name);
 */

//    void bindBuffer(GLenum target, WebGLBuffer buffer);
JSMETHOD (bindBuffer) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLuint buffer_id = 0;
	if (!EJSVAL_IS_PRIMITIVE(args[1])) {
		WebGLBuffer *buffer = (WebGLBuffer*)get_peer (args[1]);
		buffer_id = [buffer glId];
	}
    
	SPEW(NSLog (@"glBindBuffer (%d, %d)", target, buffer_id);)
	glBindBuffer (target, buffer_id);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void bindFramebuffer(GLenum target, WebGLFramebuffer framebuffer);
JSMETHOD (bindFramebuffer) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLuint framebuffer = 0;
	if (!EJSVAL_IS_PRIMITIVE(args[1])) {
		WebGLFramebuffer *buffer = (WebGLFramebuffer*)get_peer (args[1]);
		framebuffer = [buffer glId];
	}
    
	glBindFramebuffer (target, framebuffer);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void bindRenderbuffer(GLenum target, WebGLRenderbuffer renderbuffer);
JSMETHOD (bindRenderbuffer) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLuint buffer_id = 0;
	if (!EJSVAL_IS_PRIMITIVE (args[1])) {
		WebGLRenderbuffer *buffer = (WebGLRenderbuffer*)get_peer (args[1]);
		buffer_id = [buffer glId];
	}
    
	glBindRenderbuffer (target, buffer_id);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void bindTexture(GLenum target, WebGLTexture texture);
JSMETHOD (bindTexture) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
    
	GLuint texture_id = 0;
	if (!EJSVAL_IS_PRIMITIVE (args[1])) {
		// XXX further type check here to see if it's a WebGLTexture
		WebGLTexture *texture = (WebGLTexture*)get_peer (args[1]);
		texture_id = [texture glId];
	}
    
	SPEW(NSLog (@"glBindTexture");)
	glBindTexture (target, texture_id);
	CHECK_GL;
	
	return _ejs_undefined;
}

//    void blendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
JSMETHOD (blendColor) {
	if (argc != 4)
		THROW_ARG_COUNT_EXCEPTION(4);
    
	// FIXME check args
	GLclampf red = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLclampf green = (GLenum)EJSVAL_TO_NUMBER (args[1]);
	GLclampf blue = (GLenum)EJSVAL_TO_NUMBER (args[2]);
	GLclampf alpha = (GLenum)EJSVAL_TO_NUMBER (args[3]);
    
	glBlendColor (red, green, blue, alpha);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void blendEquation(GLenum mode);
JSMETHOD (blendEquation) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	// FIXME check args
	GLenum mode = (GLenum)EJSVAL_TO_NUMBER (args[0]);
    
	glBlendEquation (mode);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
JSMETHOD (blendEquationSeparate) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	GLenum modeRGB = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLenum modeAlpha = (GLenum)EJSVAL_TO_NUMBER (args[1]);
    
	glBlendEquationSeparate (modeRGB, modeAlpha);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void blendFunc(GLenum sfactor, GLenum dfactor);
JSMETHOD (blendFunc) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	GLenum sfactor = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLenum dfactor = (GLenum)EJSVAL_TO_NUMBER (args[1]);
    
	glBlendFunc (sfactor, dfactor);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, 
//                           GLenum srcAlpha, GLenum dstAlpha);
JSMETHOD (blendFuncSeparate) {
	if (argc != 4)
		THROW_ARG_COUNT_EXCEPTION(4);
    
	// FIXME check args
	GLenum srcRGB = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLenum dstRGB = (GLenum)EJSVAL_TO_NUMBER (args[1]);
    
	GLenum srcAlpha = (GLenum)EJSVAL_TO_NUMBER (args[2]);
	GLenum dstAlpha = (GLenum)EJSVAL_TO_NUMBER (args[3]);
    
	glBlendFuncSeparate (srcRGB, dstRGB, srcAlpha, dstAlpha);
	CHECK_GL;
    
	return _ejs_undefined;
}


//    void bufferData(GLenum target, GLsizeiptr size, GLenum usage);
//    void bufferData(GLenum target, ArrayBufferView data, GLenum usage);
//    void bufferData(GLenum target, ArrayBuffer data, GLenum usage);
JSMETHOD (bufferData) {
	if (argc != 3)
		THROW_ARG_COUNT_EXCEPTION(3);
    
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLenum usage = (GLenum)EJSVAL_TO_NUMBER (args[2]);
    
	GLsizeiptr bufferSize = 0;
	GLvoid *bufferData = NULL;
    
	if (EJSVAL_IS_NUMBER (args[1])) {
		SPEW(NSLog (@"arg[1] is a number");)
		bufferSize = (GLsizeiptr)EJSVAL_TO_NUMBER (args[1]);
	}
	else if (!EJSVAL_IS_PRIMITIVE(args[1])) {
		EJSObject* obj = EJSVAL_TO_OBJECT (args[1]);
		if (EJSOBJECT_IS_TYPEDARRAY(obj)) {
			SPEW(NSLog (@"arg[1] is an array buffer view subclass");)
            bufferSize = (GLsizeiptr)EJSTYPEDARRAY_BYTE_LEN(obj);
			bufferData = _ejs_typedarray_get_data(obj);
			//SPEW(NSLog (@"  using [%lu-%lu]", [bufferView byteOffset], [bufferView byteLength] + [bufferView byteOffset]);)
		}
		else if (EJSOBJECT_IS_ARRAYBUFFER (obj)) {
			SPEW(NSLog (@"arg[1] is an array buffer subclass");)
                bufferSize = (GLsizeiptr)EJSARRAYBUFFER_BYTE_LEN(obj);
			bufferData = _ejs_arraybuffer_get_data(obj);
		}
	}
	else {
		NSLog (@"webgl is lame 56");
		abort();
	}
    
	SPEW(NSLog(@"glBufferData");)
	glBufferData (target, bufferSize, bufferData, usage);
	CHECK_GL;
    
	return _ejs_undefined;
}

/*
 J3D    void bufferSubData(GLenum target, GLintptr offset, ArrayBufferView data);
 J3D    void bufferSubData(GLenum target, GLintptr offset, ArrayBuffer data);
 
 GLenum checkFramebufferStatus(GLenum target);
 */
//    void clear(GLbitfield mask);
JSMETHOD (clear) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	// FIXME check args
	GLbitfield mask = (GLenum)EJSVAL_TO_NUMBER (args[0]);
    
	glClear (mask);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void clearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
JSMETHOD (clearColor) {
	if (argc != 4)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	// FIXME check args
	GLclampf red = (GLclampf)EJSVAL_TO_NUMBER (args[0]);
	GLclampf green = (GLclampf)EJSVAL_TO_NUMBER (args[1]);
	GLclampf blue = (GLclampf)EJSVAL_TO_NUMBER (args[2]);
	GLclampf alpha = (GLclampf)EJSVAL_TO_NUMBER (args[3]);
    
	glClearColor (red, green, blue, alpha);
	CHECK_GL;
    
	return _ejs_undefined;
}


//    void clearDepth(GLclampf depth);
JSMETHOD (clearDepth) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	GLclampf depth = (GLclampf)EJSVAL_TO_NUMBER (args[0]);
    
	glClearDepthf (depth);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void clearStencil(GLint s);
JSMETHOD (clearStencil) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	GLint s = (GLint)EJSVAL_TO_NUMBER (args[0]);
    
	glClearStencil (s);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
JSMETHOD (colorMask) {
	if (argc != 4)
		THROW_ARG_COUNT_EXCEPTION(4);
    
	// FIXME check args
	GLboolean red = (GLboolean)EJSVAL_TO_BOOLEAN (args[0]);
	GLboolean green = (GLboolean)EJSVAL_TO_BOOLEAN (args[1]);
	GLboolean blue = (GLboolean)EJSVAL_TO_BOOLEAN (args[2]);
	GLboolean alpha = (GLboolean)EJSVAL_TO_BOOLEAN (args[3]);
    
	glColorMask (red, green, blue, alpha);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void compileShader(WebGLShader shader);
JSMETHOD (compileShader) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	// FIXME check args
	WebGLShader *shader = (WebGLShader*)get_peer (args[0]);
    
	glCompileShader ([shader glId]);
	CHECK_GL;
    
	return _ejs_undefined;
}

/*
 void copyTexImage2D(GLenum target, GLint level, GLenum internalformat, 
 GLint x, GLint y, GLsizei width, GLsizei height, 
 GLint border);
 void copyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
 GLint x, GLint y, GLsizei width, GLsizei height);
 
 */
//    WebGLBuffer createBuffer();
JSMETHOD (createBuffer) {
	if (argc != 0)
		THROW_ARG_COUNT_EXCEPTION(0);
    
	GLuint buf;
	glGenBuffers (1, &buf);
	CHECK_GL;

    EJSWebGLObject *obj = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)obj, _ejs_Object__proto__, &WebGLBuffer_specops);
    
    obj->peer = [[WebGLBuffer alloc] initWithGLId:buf];

    return OBJECT_TO_EJSVAL(obj);
}

//    WebGLFramebuffer createFramebuffer();
JSMETHOD (createFramebuffer) {
	if (argc != 0)
		THROW_ARG_COUNT_EXCEPTION(0);
    
	GLuint buf;
	glGenFramebuffers (1, &buf);
	CHECK_GL;

    EJSWebGLObject *obj = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)obj, _ejs_Object__proto__, &WebGLFramebuffer_specops);
    
    obj->peer = [[WebGLFramebuffer alloc] initWithGLId:buf];

    return OBJECT_TO_EJSVAL(obj);
}

//    WebGLRenderbuffer createRenderbuffer();
JSMETHOD (createRenderbuffer) {
	if (argc != 0)
		THROW_ARG_COUNT_EXCEPTION(0);
    
	GLuint buf;
	glGenRenderbuffers (1, &buf);
	CHECK_GL;

    EJSWebGLObject *obj = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)obj, _ejs_Object__proto__, &WebGLRenderbuffer_specops);
    
    obj->peer = [[WebGLRenderbuffer alloc] initWithGLId:buf];

    return OBJECT_TO_EJSVAL(obj);
}

//    WebGLProgram createProgram();
JSMETHOD (createProgram) {
	if (argc != 0)
		THROW_ARG_COUNT_EXCEPTION(0);
 
	GLuint programId = glCreateProgram();
	CHECK_GL;

    EJSWebGLObject *obj = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)obj, _ejs_Object__proto__, &WebGLProgram_specops);
    
    obj->peer = [[WebGLProgram alloc] initWithGLId:programId];

    return OBJECT_TO_EJSVAL(obj);
}

//    WebGLShader createShader(GLenum type);
JSMETHOD (createShader) {
	SPEW(NSLog (@"createShader (ctx = %p)", [EAGLContext currentContext]);)
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	GLenum shaderType = (GLenum)EJSVAL_TO_NUMBER (args[0]);
    
	GLuint shaderId = glCreateShader(shaderType);
	CHECK_GL;

    EJSWebGLObject *obj = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)obj, _ejs_Object__proto__, &WebGLShader_specops);
    
    obj->peer = [[WebGLShader alloc] initWithGLId:shaderId];

    return OBJECT_TO_EJSVAL(obj);
}

//    WebGLTexture createTexture();
JSMETHOD (createTexture) {
	if (argc != 0)
		THROW_ARG_COUNT_EXCEPTION(0);
    
	GLuint tex;
	glGenTextures (1, &tex);
	CHECK_GL;

    EJSWebGLObject *obj = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)obj, _ejs_Object__proto__, &WebGLTexture_specops);
    
    obj->peer = [[WebGLTexture alloc] initWithGLId:tex];

    return OBJECT_TO_EJSVAL(obj);
}

//    void cullFace(GLenum mode);
JSMETHOD (cullFace) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	SPEW(NSLog (@"glCullFace");)
	GLenum mode = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	glCullFace (mode);
	CHECK_GL;
    
	return _ejs_undefined;
}

// void deleteBuffer(WebGLBuffer buffer);
JSMETHOD (deleteBuffer) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	WebGLBuffer *buffer = (WebGLBuffer*)get_peer (args[0]);
    
	[buffer deleteBuffer];
    
	return _ejs_undefined;
}

// void deleteFramebuffer(WebGLFramebuffer framebuffer);
JSMETHOD (deleteFramebuffer) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	WebGLFramebuffer *framebuffer = (WebGLFramebuffer*)get_peer (args[0]);
    
	[framebuffer deleteFramebuffer];
    
	return _ejs_undefined;
}

// void deleteProgram(WebGLProgram program);
JSMETHOD (deleteProgram) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
    
	[program deleteProgram];
    
	return _ejs_undefined;
}

// void deleteRenderbuffer(WebGLRenderbuffer renderbuffer);
JSMETHOD (deleteRenderbuffer) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	WebGLRenderbuffer *buffer = (WebGLRenderbuffer*)get_peer (args[0]);
    
	[buffer deleteRenderbuffer];
    
	return _ejs_undefined;
}

// void deleteShader(WebGLShader shader);
JSMETHOD (deleteShader) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	WebGLShader *shader = (WebGLShader*)get_peer (args[0]);
    
	[shader deleteShader];
    
	return _ejs_undefined;
}

// void deleteTexture(WebGLTexture texture);
JSMETHOD (deleteTexture) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	WebGLTexture *texture = (WebGLTexture*)get_peer (args[0]);
    
	[texture deleteTexture];
    
	return _ejs_undefined;
}

//    void depthFunc(GLenum func);
JSMETHOD (depthFunc) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	glDepthFunc ((GLenum)EJSVAL_TO_NUMBER (args[0]));
	CHECK_GL;
    
	return _ejs_undefined;
}


//    void depthMask(GLboolean flag);
JSMETHOD (depthMask) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	GLboolean flag = (GLboolean)EJSVAL_TO_BOOLEAN (args[0]);
	SPEW(NSLog (@"glDepthMask");)
	glDepthMask (flag);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void depthRange(GLclampf zNear, GLclampf zFar);
JSMETHOD (depthRange)
{
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	GLclampf zNear = (GLclampf)EJSVAL_TO_NUMBER (args[0]);
	GLclampf zFar = (GLclampf)EJSVAL_TO_NUMBER (args[1]);
	SPEW(NSLog (@"glDepthRange");)
	glDepthRangef (zNear, zFar);
	CHECK_GL;
    
	return _ejs_undefined;
}
/*
 void detachShader(WebGLProgram program, WebGLShader shader);
 */
//    void disable(GLenum cap);
JSMETHOD (disable) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	glDisable ((GLenum)EJSVAL_TO_NUMBER (args[0]));
	CHECK_GL;
    
	return _ejs_undefined;
}

/*
 void disableVertexAttribArray(GLuint index);
 */
JSMETHOD (disableVertexAttribArray) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	GLuint indx = (GLuint)EJSVAL_TO_NUMBER (args[0]);
	SPEW(NSLog (@"glDisableVertexAttribArray");)
	glDisableVertexAttribArray (indx);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void drawArrays(GLenum mode, GLint first, GLsizei count);
JSMETHOD (drawArrays) {
	if (argc != 3)
		THROW_ARG_COUNT_EXCEPTION(3);
    
	GLenum mode = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLint first = (GLint)EJSVAL_TO_NUMBER (args[1]);
	GLsizei count = (GLsizei)EJSVAL_TO_NUMBER (args[2]);
    
	SPEW(NSLog (@"glDrawArrays");)
	glDrawArrays (mode, first, count);
	CHECK_GL;
    
	return _ejs_undefined;
}


//    void drawElements(GLenum mode, GLsizei count, GLenum type, GLintptr offset);
JSMETHOD (drawElements) {
	if (argc != 4)
		THROW_ARG_COUNT_EXCEPTION(4);
    
	GLenum mode = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLsizei count = (GLsizei)EJSVAL_TO_NUMBER (args[1]);
	GLenum type = (GLenum)EJSVAL_TO_NUMBER (args[2]);
	GLintptr offset = (GLintptr)EJSVAL_TO_NUMBER (args[3]);
    
	SPEW(NSLog (@"glDrawElements");)
	glDrawElements (mode, count, type, (GLvoid*)offset);
	CHECK_GL;
    
	return _ejs_undefined;
}


//    void enable(GLenum cap);
JSMETHOD (enable) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	glEnable ((GLenum)EJSVAL_TO_NUMBER (args[0]));
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void enableVertexAttribArray(GLuint index);
JSMETHOD (enableVertexAttribArray) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	GLuint indx;
	indx = (GLuint)EJSVAL_TO_NUMBER(args[0]);
	SPEW(NSLog (@"glEnableVertexAttribArray(%u)", indx);)
	glEnableVertexAttribArray (indx);
	CHECK_GL;
    
	return _ejs_undefined;
}

/*
 void finish();
 void flush();
 */

//    void framebufferRenderbuffer(GLenum target, GLenum attachment, 
//                                 GLenum renderbuffertarget, 
//                                 WebGLRenderbuffer renderbuffer);
JSMETHOD (framebufferRenderbuffer) {
	if (argc != 4)
		THROW_ARG_COUNT_EXCEPTION(4);
    
	// FIXME check args
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLenum attachment = (GLenum)EJSVAL_TO_NUMBER (args[1]);
	GLenum renderbuffertarget = (GLenum)EJSVAL_TO_NUMBER (args[2]);
	WebGLRenderbuffer *renderbuffer = (WebGLRenderbuffer*)get_peer (args[3]);
    
	glFramebufferRenderbuffer (target, attachment, renderbuffertarget, [renderbuffer glId]);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, 
//                              WebGLTexture texture, GLint level);
JSMETHOD (framebufferTexture2D) {
	if (argc != 5)
		THROW_ARG_COUNT_EXCEPTION(5);
    
	// FIXME check args
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLenum attachment = (GLenum)EJSVAL_TO_NUMBER (args[1]);
	GLenum textarget = (GLenum)EJSVAL_TO_NUMBER (args[2]);
	WebGLTexture *texture = (WebGLTexture*)get_peer (args[3]);
	GLint level = (GLint)EJSVAL_TO_NUMBER (args[4]);
    
	glFramebufferTexture2D (target, attachment, textarget, [texture glId], level);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void frontFace(GLenum mode);
JSMETHOD (frontFace) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	glFrontFace ((GLenum)EJSVAL_TO_NUMBER (args[0]));
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void generateMipmap(GLenum target);
JSMETHOD (generateMipmap) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	glGenerateMipmap (target);
	glGetError();
	CHECK_GL;

	return _ejs_undefined;
}


//    WebGLActiveInfo getActiveAttrib(WebGLProgram program, GLuint index);
JSMETHOD (getActiveAttrib) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
	GLuint index = (GLuint)EJSVAL_TO_NUMBER (args[1]);
    
	SPEW(NSLog (@"getActiveAttrib index = %d", index);)
    
	GLint bufSize = 0;
    
	glGetProgramiv ([program glId],
                    GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                    &bufSize);
	CHECK_GL;

	GLchar *name = (GLchar*)calloc (1, (size_t)bufSize);
	GLint size;
	GLenum type;
    
	glGetActiveAttrib([program glId],
                      index,
                      bufSize,
                      NULL,
                      &size,
                      &type,
                      name);
	CHECK_GL;

	SPEW(NSLog (@"getActiveAttrib index = %d, name = %s, type = %d, size = %d", index, name, type, size);)

    EJSWebGLObject *obj = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)obj, WebGLActiveInfo__proto__, &WebGLActiveInfo_specops);
    
    obj->peer = [[WebGLActiveInfo alloc] initWithName:name type:type size:size];

    free (name);

    return OBJECT_TO_EJSVAL(obj);
}

//    WebGLActiveInfo getActiveUniform(WebGLProgram program, GLuint index);
JSMETHOD (getActiveUniform) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
	GLuint index = (GLuint)EJSVAL_TO_NUMBER (args[1]);
    
	SPEW(NSLog (@"getActiveUniform index = %d", index);)
    
	GLint bufSize = 0;
    
	glGetProgramiv ([program glId],
			GL_ACTIVE_UNIFORM_MAX_LENGTH,
			&bufSize);
	CHECK_GL;
    
	GLchar *name = (GLchar*)calloc (1, (size_t)bufSize);
	GLint size;
	GLenum type;
    
	glGetActiveUniform([program glId],
			   index,
			   bufSize,
			   NULL,
			   &size,
			   &type,
			   name);
	CHECK_GL;

    EJSWebGLObject *obj = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)obj, WebGLActiveInfo__proto__, &WebGLActiveInfo_specops);
    
    obj->peer = [[WebGLActiveInfo alloc] initWithName:name type:type size:size];

    free (name);

    return OBJECT_TO_EJSVAL(obj);
}

/*
 WebGLShader[ ] getAttachedShaders(WebGLProgram program);
 */

//    GLint getAttribLocation(WebGLProgram program, DOMString name);
JSMETHOD (getAttribLocation) {
	if (argc != 2)
	  THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
    char *name = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[1]));

    SPEW(LOG ("glGetAttribLocation (%s)\n", name));
	GLint rv = glGetAttribLocation ([program glId], name);
	CHECK_GL;
    
    SPEW(LOG ("glGetAttribLocation (%s) = %d\n", name, rv));
	free (name);

    return NUMBER_TO_EJSVAL(rv);
}

/*
 any getParameter(GLenum pname);
 any getBufferParameter(GLenum target, GLenum pname);
 
 GLenum getError();
 
 any getFramebufferAttachmentParameter(GLenum target, GLenum attachment, 
 GLenum pname);
 */
//    DOMString getProgramInfoLog(WebGLProgram program);
JSMETHOD (getProgramInfoLog) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	// FIXME check args
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
    
	GLint info_log_length;
	GLchar *program_log;
    
	glGetProgramiv ([program glId],
			GL_INFO_LOG_LENGTH,
			&info_log_length);
	CHECK_GL;
    
	program_log = (GLchar*)calloc (1,(size_t)(info_log_length+1));
    
	glGetProgramInfoLog ([program glId],
			     info_log_length,
			     NULL,
			     program_log);
	CHECK_GL;
    
	ejsval jsstr = _ejs_string_new_utf8(program_log);
	free (program_log);

	return jsstr;
}

//    any getProgramParameter(WebGLProgram program, GLenum pname);
JSMETHOD (getProgramParameter) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
	GLenum pname = (GLenum)EJSVAL_TO_NUMBER (args[1]);
	GLint param = 0;
    
	glGetProgramiv ([program glId],
			pname,
			&param);
	CHECK_GL;
    
	switch (pname) {
        case GL_VALIDATE_STATUS:
        case GL_LINK_STATUS:
        case GL_DELETE_STATUS:
            return BOOLEAN_TO_EJSVAL(param == GL_TRUE);
            
        case GL_ATTACHED_SHADERS:
        case GL_ACTIVE_ATTRIBUTES:
            SPEW(NSLog (@"program has %d active %s", param, pname == GL_ATTACHED_SHADERS ? "shaders" : "attributes");)
            return NUMBER_TO_EJSVAL(param);
        case GL_ACTIVE_UNIFORMS:
            SPEW(NSLog (@"program id = %d, GL_ACTIVE_UNIFORMS = %d", [program glId], param);)
            return NUMBER_TO_EJSVAL(param);
        default:
            SPEW(NSLog (@"unhandled getProgramParameter pname %d", pname);)
            abort();
	}
}

/*
 any getRenderbufferParameter(GLenum target, GLenum pname);
 */

//    DOMString getShaderInfoLog(WebGLShader shader);
JSMETHOD (getShaderInfoLog) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	// FIXME check args
	WebGLShader *shader = (WebGLShader*)get_peer (args[0]);
	
	GLint info_log_length;
	GLchar *shader_log;
    
	glGetShaderiv ([shader glId],
		       GL_INFO_LOG_LENGTH,
		       &info_log_length);
	CHECK_GL;
    
	shader_log = (GLchar*)calloc (1,(size_t)(info_log_length+1));
    
	glGetShaderInfoLog ([shader glId],
			    info_log_length,
			    NULL,
			    shader_log);
	CHECK_GL;
    
	ejsval jsstr = _ejs_string_new_utf8 (shader_log);
	free (shader_log);
    
    return jsstr;
}

//    any getShaderParameter(WebGLShader shader, GLenum pname);
JSMETHOD (getShaderParameter) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	WebGLShader *shader = (WebGLShader*)get_peer (args[0]);
	GLenum pname = (GLenum)EJSVAL_TO_NUMBER (args[1]);
	GLint param;
    
	glGetShaderiv ([shader glId],
		       pname,
		       &param);
	CHECK_GL;
    
	switch (pname) {
        case GL_SHADER_TYPE:
            return NUMBER_TO_EJSVAL(param);
        case GL_DELETE_STATUS:
        case GL_COMPILE_STATUS:
            return BOOLEAN_TO_EJSVAL(param != 0);
        default:
            NSLog (@"unhandled getShaderParameter pname %d", pname);
            abort();
	}
}

//    DOMString getShaderSource(WebGLShader shader);
JSMETHOD (getShaderSource) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	// FIXME check args
	WebGLShader *shader = (WebGLShader*)get_peer (args[0]);
    
	GLint source_length;
	GLchar *shader_source;
    
	glGetShaderiv ([shader glId],
		       GL_SHADER_SOURCE_LENGTH,
		       &source_length);
	CHECK_GL;
    
	shader_source = (GLchar*)calloc (1,(size_t)(source_length+1));
    
	glGetShaderSource ([shader glId],
			   source_length,
			   NULL,
			   shader_source);
	CHECK_GL;
    
	ejsval jsstr = _ejs_string_new_utf8 (shader_source);
	free (shader_source);
    
	return jsstr;
}

/*
 any getTexParameter(GLenum target, GLenum pname);
 
 any getUniform(WebGLProgram program, WebGLUniformLocation location);
 */

//    WebGLUniformLocation getUniformLocation(WebGLProgram program, DOMString name);
JSMETHOD (getUniformLocation) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
    char *name = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[1]));
    
	GLint loc = glGetUniformLocation ([program glId],
                                      name);
	SPEW(NSLog(@"glGetUniformLocation (%d, %s) => %d", [program glId], name, loc);)

	CHECK_GL;
    
	free(name);

    EJSWebGLObject *obj = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)obj, _ejs_Object__proto__, &WebGLUniformLocation_specops);
    
    obj->peer = (id)loc;

    return OBJECT_TO_EJSVAL(obj);
}

/*
 any getVertexAttrib(GLuint index, GLenum pname);
 
 GLsizeiptr getVertexAttribOffset(GLuint index, GLenum pname);
 
 void hint(GLenum target, GLenum mode);
 GLboolean isBuffer(WebGLBuffer buffer);
 GLboolean isEnabled(GLenum cap);
 GLboolean isFramebuffer(WebGLFramebuffer framebuffer);
 GLboolean isProgram(WebGLProgram program);
 GLboolean isRenderbuffer(WebGLRenderbuffer renderbuffer);
 GLboolean isShader(WebGLShader shader);
 GLboolean isTexture(WebGLTexture texture);
 void lineWidth(GLfloat width);
 */
//    void linkProgram(WebGLProgram program);
JSMETHOD (linkProgram) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(1);
    
	// FIXME check args
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
    
	SPEW(NSLog (@"glLinkProgram (%d)", [program glId]);)
    
	glLinkProgram ([program glId]);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void pixelStorei(GLenum pname, GLint param);
JSMETHOD (pixelStorei) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(2);
    
	// FIXME check args
	GLenum pname = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLint param = (GLint)EJSVAL_TO_NUMBER (args[1]);
    
	if (pname == WEBGL_UNPACK_FLIP_Y_WEBGL) {
		unpack_flip_y = param != 0;
		return _ejs_undefined;
	}
	else if (pname == WEBGL_UNPACK_PREMULTIPLY_ALPHA_WEBGL ||
		 pname == WEBGL_UNPACK_COLORSPACE_CONVERSION_WEBGL) {
		// FIXME
		NSLog(@"unhandled webgl specific pixelStorei interface.  ignoring");
		return _ejs_undefined;
	}
    
    
	glPixelStorei (pname, param);
	CHECK_GL;
    
	return _ejs_undefined;
}

/*
 void polygonOffset(GLfloat factor, GLfloat units);
 
 void readPixels(GLint x, GLint y, GLsizei width, GLsizei height, 
 GLenum format, GLenum type, ArrayBufferView pixels);
 */

//    void renderbufferStorage(GLenum target, GLenum internalformat, 
//                             GLsizei width, GLsizei height);
JSMETHOD (renderbufferStorage) {
	if (argc != 4)
		THROW_ARG_COUNT_EXCEPTION(4);
    
	// FIXME check args
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLenum internalformat = (GLenum)EJSVAL_TO_NUMBER (args[1]);
	GLsizei width = (GLsizei)EJSVAL_TO_NUMBER (args[2]);
	GLsizei height = (GLsizei)EJSVAL_TO_NUMBER (args[3]);
    
	glRenderbufferStorage (target, internalformat, width, height);
	CHECK_GL;
    
	return _ejs_undefined;
}

/*
 void sampleCoverage(GLclampf value, GLboolean invert);
 void scissor(GLint x, GLint y, GLsizei width, GLsizei height);
 */

//    void shaderSource(WebGLShader shader, DOMString source);
JSMETHOD (shaderSource) {
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
	// FIXME check args
	WebGLShader *shader = (WebGLShader*)get_peer (args[0]);
    char *source = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[1]));
    
	glShaderSource ([shader glId],
                    1,
                    (const GLchar**)&source,
                    NULL);
	CHECK_GL;
    
	free (source);
    
	return _ejs_undefined;
}

/*
 void stencilFunc(GLenum func, GLint ref, GLuint mask);
 void stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
 void stencilMask(GLuint mask);
 void stencilMaskSeparate(GLenum face, GLuint mask);
 void stencilOp(GLenum fail, GLenum zfail, GLenum zpass);
 void stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
 */

//    void texImage2D(GLenum target, GLint level, GLenum internalformat, 
//                    GLsizei width, GLsizei height, GLint border, GLenum format, 
//                    GLenum type, ArrayBufferView pixels);
//    void texImage2D(GLenum target, GLint level, GLenum internalformat,
//                    GLenum format, GLenum type, ImageData pixels);
//    void texImage2D(GLenum target, GLint level, GLenum internalformat,
//                    GLenum format, GLenum type, HTMLImageElement image) raises (DOMException);
//    void texImage2D(GLenum target, GLint level, GLenum internalformat,
//                    GLenum format, GLenum type, HTMLCanvasElement canvas) raises (DOMException);
//    void texImage2D(GLenum target, GLint level, GLenum internalformat,
//                    GLenum format, GLenum type, HTMLVideoElement video) raises (DOMException);
JSMETHOD (texImage2D) {
	if (argc != 9 && argc != 6)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
	GLenum target;
	GLint level;
	GLint internalFormat;
	GLsizei width;
	GLsizei height;
	GLint border;
	GLenum format;
	GLenum type;
	GLvoid *pixels;
	BOOL free_pixels = NO;
	CGContextRef bitmapContext = NULL;
    
	target = (GLenum)EJSVAL_TO_NUMBER(args[0]);
	level = (GLint)EJSVAL_TO_NUMBER(args[1]);
	internalFormat = (GLint)EJSVAL_TO_NUMBER(args[2]);

	if (argc == 6) {
		border = 0; /* XXX */
		format = (GLenum)EJSVAL_TO_NUMBER(args[3]);
		type = (GLenum)EJSVAL_TO_NUMBER(args[4]);
		NSLog(@"1");
		UIImage* uiimage = (UIImage*)get_objc_id ([[CKValue valueWithJSValue:args[5]] objectValue]);
		NSLog(@"2, uiimage = %@", uiimage);
        
		CGImageRef cgimage = [uiimage CGImage];
		CGSize imageSize = [uiimage size];
		width = (GLsizei)imageSize.width;
		height = (GLsizei)imageSize.height;
        
		SPEW(NSLog (@"creating bitmapContext with CGImageRef = %p, colorspace = %p, width = %d, height = %d", cgimage, CGImageGetColorSpace(cgimage), width, height);)
		SPEW(NSLog (@"colorspace model = %d", CGColorSpaceGetModel(CGImageGetColorSpace(cgimage)));)
		SPEW(NSLog (@"bits per component = %lu", CGImageGetBitsPerComponent(cgimage));)
		SPEW(NSLog (@"bytes per row = %lu", CGImageGetBytesPerRow(cgimage));)

		bitmapContext = CGBitmapContextCreate (NULL,
						       (size_t)width,
						       (size_t)height,
						       8, //CGImageGetBitsPerComponent(cgimage),
						       width * 4, //CGImageGetBytesPerRow(cgimage),
						       CGColorSpaceCreateDeviceRGB(),//CGImageGetColorSpace(cgimage),
						       format == GL_RGBA ? kCGImageAlphaPremultipliedLast : kCGImageAlphaNoneSkipLast); //CGImageGetBitmapInfo(cgimage));
        
        
		if (unpack_flip_y) {
			CGAffineTransform flipVertical = CGAffineTransformMake(
									       1, 0, 0, -1, 0, height
									       );
			CGContextConcatCTM(bitmapContext, flipVertical);
		}
        
		CGContextDrawImage (bitmapContext,
				    CGRectMake (0, 0, width, height),
				    cgimage);
		//        Xcode says this is wrong.
		//        CGImageRelease(cgimage);
        
		pixels = CGBitmapContextGetData (bitmapContext);
	}
	else {
		width = (GLsizei)EJSVAL_TO_NUMBER(args[3]);
		height = (GLsizei)EJSVAL_TO_NUMBER(args[4]);
		border = (GLint)EJSVAL_TO_NUMBER(args[5]);
		format = (GLenum)EJSVAL_TO_NUMBER(args[6]);
		type = (GLenum)EJSVAL_TO_NUMBER(args[7]);

		if (EJSVAL_IS_NULL (args[8])) {
			pixels = calloc (1, (size_t)(width * height * 4)); // XXX we don't always need 32bpp, do we?
			free_pixels = YES;
		}
		else if (EJSVAL_IS_OBJECT(args[8])) {
			EJSObject *obj = EJSVAL_TO_OBJECT (args[8]);
			if (!EJSOBJECT_IS_TYPEDARRAY(obj))
				_ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "");

			//    If pixels is non-null, the type of
			//    pixels must match the type of the data
			//    to be read. If it is UNSIGNED_BYTE, a
			//    Uint8Array must be supplied; if it is
			//    UNSIGNED_SHORT_5_6_5,
			//    UNSIGNED_SHORT_4_4_4_4, or
			//    UNSIGNED_SHORT_5_5_5_1, a Uint16Array
			//    must be supplied. If the types do not
			//    match, an INVALID_OPERATION error is
			//    generated.
			if (type == GL_UNSIGNED_BYTE && EJSTYPEDARRAY_ELEMENT_TYPE(obj) != EJS_TYPEDARRAY_UINT8)
				_ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "");
			if ((type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_5_5_5_1) && 
			    EJSTYPEDARRAY_ELEMENT_TYPE(obj) != EJS_TYPEDARRAY_UINT16)
				_ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "");

			pixels = _ejs_typedarray_get_data (obj);
		}
	}

	SPEW(NSLog(@"glTexImage2D (target=%d format=%d type=%d internalFormat=%d w=%d, h=%d)", target, format, type, internalFormat, width, height);)
	glTexImage2D (target, level, internalFormat, width, height, border, format, type, pixels);
	CHECK_GL;
    
	if (bitmapContext)         
		CGContextRelease(bitmapContext);
	if (free_pixels)
		free (pixels);

	return _ejs_undefined;
}

//    void texParameterf(GLenum target, GLenum pname, GLfloat param);
JSMETHOD (texParameterf) {
	if (argc != 3)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
	// FIXME check args
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLenum pname = (GLenum)EJSVAL_TO_NUMBER (args[1]);
	GLfloat param = (GLfloat)EJSVAL_TO_NUMBER (args[2]);
    
	glTexParameterf (target, pname, param);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void texParameteri(GLenum target, GLenum pname, GLint param);
JSMETHOD (texParameteri) {
	if (argc != 3)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
	// FIXME check args
	GLenum target = (GLenum)EJSVAL_TO_NUMBER (args[0]);
	GLenum pname = (GLenum)EJSVAL_TO_NUMBER (args[1]);
	GLint param = (GLint)EJSVAL_TO_NUMBER (args[2]);
    
	glTexParameteri (target, pname, param);
	CHECK_GL;
    
	return _ejs_undefined;
}

/*
 void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
 GLsizei width, GLsizei height, 
 GLenum format, GLenum type, ArrayBufferView pixels);
 void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
 GLenum format, GLenum type, ImageData pixels);
 void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
 GLenum format, GLenum type, HTMLImageElement image) raises (DOMException);
 void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
 GLenum format, GLenum type, HTMLCanvasElement canvas) raises (DOMException);
 void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
 GLenum format, GLenum type, HTMLVideoElement video) raises (DOMException);
 */

//    void uniform1f(WebGLUniformLocation location, GLfloat x);
//    void uniform2f(WebGLUniformLocation location, GLfloat x, GLfloat y);
//    void uniform3f(WebGLUniformLocation location, GLfloat x, GLfloat y, GLfloat z);
//    void uniform4f(WebGLUniformLocation location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

static ejsval
uniform_f (size_t c, uint32_t argc, ejsval *args)
{
	int loc = -1;
    
	if (argc != (c + 1))
		THROW_ARG_COUNT_EXCEPTION(-1);
    
#if notyet
	//if (EJSVAL_IS_OBJECTOfClass (ctx, args[0], WebGLUniformLocationClass)) {
#endif
	if (!EJSVAL_IS_PRIMITIVE (args[0])) {
		loc = (int)get_peer (args[0]);
	}
	else if (EJSVAL_IS_NUMBER (args[0])) {
		loc = EJSVAL_TO_NUMBER (args[0]);
	}
    
	if (loc == -1) {
		NSLog (@"invalid location specified.");
		abort();
	}
    
	GLfloat gl_args[4];
    
	if (c > 0) gl_args[0] = (GLfloat)EJSVAL_TO_NUMBER (args[1]);
	if (c > 1) gl_args[1] = (GLfloat)EJSVAL_TO_NUMBER (args[2]);
	if (c > 2) gl_args[2] = (GLfloat)EJSVAL_TO_NUMBER (args[3]);
	if (c > 3) gl_args[3] = (GLfloat)EJSVAL_TO_NUMBER (args[4]);
    
	switch (c) {
        case 1: glUniform1f (loc, gl_args[0]); break;
        case 2: glUniform2f (loc, gl_args[0], gl_args[1]); break;
        case 3: glUniform3f (loc, gl_args[0], gl_args[1], gl_args[2]); break;
        case 4: glUniform4f (loc, gl_args[0], gl_args[1], gl_args[2], gl_args[3]); break;
        default: abort();
	}
	CHECK_GL;
    
	return _ejs_undefined;
}

JSMETHOD (uniform1f) { return uniform_f (1, argc, args); }
JSMETHOD (uniform2f) { return uniform_f (2, argc, args); }
JSMETHOD (uniform3f) { return uniform_f (3, argc, args); }
JSMETHOD (uniform4f) { return uniform_f (4, argc, args); }


//    void uniform1i(WebGLUniformLocation location, GLint x);
//    void uniform2i(WebGLUniformLocation location, GLint x, GLint y);
//    void uniform3i(WebGLUniformLocation location, GLint x, GLint y, GLint z);
//    void uniform4i(WebGLUniformLocation location, GLint x, GLint y, GLint z, GLint w);
static ejsval
uniform_i (size_t c, uint32_t argc, ejsval *args)
{
	int loc = -1;
    
	if (argc != (c + 1))
		THROW_ARG_COUNT_EXCEPTION(-1);
    
#if notyet
	//if (EJSVAL_IS_OBJECTOfClass (ctx, args[0], WebGLUniformLocationClass)) {
#endif
	if (!EJSVAL_IS_PRIMITIVE (args[0])) {
		loc = (int)get_peer (args[0]);
	}
	else if (EJSVAL_IS_NUMBER (args[0])) {
		loc = EJSVAL_TO_NUMBER (args[0]);
	}
        
	if (loc == -1) {
		NSLog (@"invalid location specified.");
		abort();
	}
        
	GLint gl_args[4];
        
	if (c > 0) gl_args[0] = (GLint)EJSVAL_TO_NUMBER (args[1]);
	if (c > 1) gl_args[1] = (GLint)EJSVAL_TO_NUMBER (args[2]);
	if (c > 2) gl_args[2] = (GLint)EJSVAL_TO_NUMBER (args[3]);
	if (c > 3) gl_args[3] = (GLint)EJSVAL_TO_NUMBER (args[4]);
        
	switch (c) {
        case 1: glUniform1i (loc, gl_args[0]); break;
        case 2: glUniform2i (loc, gl_args[0], gl_args[1]); break;
        case 3: glUniform3i (loc, gl_args[0], gl_args[1], gl_args[2]); break;
        case 4: glUniform4i (loc, gl_args[0], gl_args[1], gl_args[2], gl_args[3]); break;
        default: abort();            
	}
	CHECK_GL;
        
	return _ejs_undefined;
}
    
JSMETHOD (uniform1i) { return uniform_i (1, argc, args); }
JSMETHOD (uniform2i) { return uniform_i (2, argc, args); }
JSMETHOD (uniform3i) { return uniform_i (3, argc, args); }
JSMETHOD (uniform4i) { return uniform_i (4, argc, args); }


//    void uniform1fv(WebGLUniformLocation location, Float32Array v);
//    void uniform1fv(WebGLUniformLocation location, float[] v);
//    void uniform2fv(WebGLUniformLocation location, Float32Array v);
//    void uniform2fv(WebGLUniformLocation location, float[] v);
//    void uniform3fv(WebGLUniformLocation location, Float32Array v);
//    void uniform3fv(WebGLUniformLocation location, float[] v);
//    void uniform4fv(WebGLUniformLocation location, Float32Array v);
//    void uniform4fv(WebGLUniformLocation location, float[] v);
static ejsval
uniform_fv (size_t c, uint32_t argc, ejsval *args)
{
	int loc = -1;
    
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
#if notyet
	//if (EJSVAL_IS_OBJECTOfClass (ctx, args[0], WebGLUniformLocationClass)) {
#endif
	if (!EJSVAL_IS_PRIMITIVE (args[0])) {
		loc = (int)get_peer (args[0]);
	}
	else if (EJSVAL_IS_NUMBER (args[0])) {
		loc = EJSVAL_TO_NUMBER (args[0]);
	}
    
	if (loc == -1) {
		NSLog (@"invalid location specified.");
		abort();
	}
    
	EJSObject* arrayObj = EJSVAL_TO_OBJECT (args[1]);
	EJSObject* bufferView;
    if (EJSOBJECT_IS_TYPEDARRAY(arrayObj) && EJSTYPEDARRAY_ELEMENT_TYPE(arrayObj) == EJS_TYPEDARRAY_FLOAT32) {
		bufferView = arrayObj;
	}
	else if (EJSOBJECT_IS_ARRAY(arrayObj)) {
		bufferView = EJSVAL_TO_OBJECT(_ejs_typedarray_new_from_array (EJS_TYPEDARRAY_FLOAT32, OBJECT_TO_EJSVAL(arrayObj)));
	}
	else {
		NSLog(@"non array-like object passed to uniform*fv");
		abort();
	}

	GLfloat* array_buffer = (GLfloat*)_ejs_typedarray_get_data(bufferView);
	GLint count = EJSTYPEDARRAY_LEN(bufferView);
    
	count /= c;
    
	switch (c) {
        case 1:
		SPEW(NSLog(@"glUniform1fv (loc = %d, count = %d, array_buffer = %p", loc, count, array_buffer);)
		glUniform1fv (loc, count, array_buffer);
		break;
        case 2:
		SPEW(NSLog(@"glUniform2fv (loc = %d, count = %d, array_buffer = %p", loc, count, array_buffer);)
		glUniform2fv (loc, count, array_buffer);
		break;
        case 3:
		SPEW(NSLog(@"glUniform3fv (loc = %d, count = %d, array_buffer = %p", loc, count, array_buffer);)
		glUniform3fv (loc, count, array_buffer);
		break;
        case 4:
		SPEW(NSLog(@"glUniform4fv (loc = %d, count = %d, array_buffer = %p", loc, count, array_buffer);)
		glUniform4fv (loc, count, array_buffer);
		break;
        default:
		abort();
	}
	CHECK_GL;
    
	return _ejs_undefined;
}

JSMETHOD (uniform1fv) { return uniform_fv (1, argc, args); }
JSMETHOD (uniform2fv) { return uniform_fv (2, argc, args); }
JSMETHOD (uniform3fv) { return uniform_fv (3, argc, args); }
JSMETHOD (uniform4fv) { return uniform_fv (4, argc, args); }

//    void uniform1iv(WebGLUniformLocation location, Int32Array v);
//    void uniform1iv(WebGLUniformLocation location, int[] v);
//    void uniform2iv(WebGLUniformLocation location, Int32Array v);
//    void uniform2iv(WebGLUniformLocation location, int[] v);
//    void uniform3iv(WebGLUniformLocation location, Int32Array v);
//    void uniform3iv(WebGLUniformLocation location, int[] v);
//    void uniform4iv(WebGLUniformLocation location, Int32Array v);
//    void uniform4iv(WebGLUniformLocation location, int[] v);
static ejsval
uniform_iv (size_t c, uint32_t argc, ejsval *args)
{
	int loc = -1;
    
	if (argc != 2)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
#if notyet
	//if (EJSVAL_IS_OBJECTOfClass (ctx, args[0], WebGLUniformLocationClass)) {
#endif
	if (!EJSVAL_IS_PRIMITIVE (args[0])) {
		loc = (int)get_peer (args[0]);
	}
	else if (EJSVAL_IS_NUMBER (args[0])) {
		loc = EJSVAL_TO_NUMBER (args[0]);
	}
    
	if (loc == -1) {
		NSLog (@"invalid location specified.");
		abort();
	}

	EJSObject* arrayObj = EJSVAL_TO_OBJECT (args[1]);
	EJSObject* bufferView;
	if (EJSOBJECT_IS_TYPEDARRAY(arrayObj) && EJSTYPEDARRAY_ELEMENT_TYPE(arrayObj) == EJS_TYPEDARRAY_INT32) {
		bufferView = arrayObj;
	}
	else if (EJSOBJECT_IS_ARRAY(arrayObj)) {
		bufferView = EJSVAL_TO_OBJECT(_ejs_typedarray_new_from_array (EJS_TYPEDARRAY_INT32, OBJECT_TO_EJSVAL(arrayObj)));
	}
	else {
		NSLog(@"non array-like object passed to uniform*fv");
		abort();
	}

	GLint* array_buffer = (GLint*)_ejs_typedarray_get_data(bufferView);
	GLint count = EJSTYPEDARRAY_LEN(bufferView);
    
	count /= c;
    
	switch (c) {
        case 1: glUniform1iv (loc, count, array_buffer); break;
        case 2: glUniform2iv (loc, count, array_buffer); break;
        case 3: glUniform3iv (loc, count, array_buffer); break;
        case 4: glUniform4iv (loc, count, array_buffer); break;
        default: abort();
	}
	CHECK_GL;
    
	return _ejs_undefined;
}

JSMETHOD (uniform1iv) { return uniform_iv (1, argc, args); }
JSMETHOD (uniform2iv) { return uniform_iv (2, argc, args); }
JSMETHOD (uniform3iv) { return uniform_iv (3, argc, args); }
JSMETHOD (uniform4iv) { return uniform_iv (4, argc, args); }

//    void uniformMatrix2fv(WebGLUniformLocation location, GLboolean transpose, 
//                          Float32Array value);
//    void uniformMatrix2fv(WebGLUniformLocation location, GLboolean transpose, 
//                          float[] value);
//    void uniformMatrix3fv(WebGLUniformLocation location, GLboolean transpose, 
//                          Float32Array value);
//    void uniformMatrix3fv(WebGLUniformLocation location, GLboolean transpose, 
//                          float[] value);
//    void uniformMatrix4fv(WebGLUniformLocation location, GLboolean transpose, 
//                          Float32Array value);
//    void uniformMatrix4fv(WebGLUniformLocation location, GLboolean transpose, 
//                          float[] value);
static ejsval
uniformMatrix_fv (size_t c, uint32_t argc, ejsval *args)
{
	int loc;
    
	if (argc != 3)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
#if notyet
	//if (EJSVAL_IS_OBJECTOfClass (ctx, args[0], WebGLUniformLocationClass)) {
#endif
	if (!EJSVAL_IS_PRIMITIVE (args[0])) {
		loc = (int)get_peer (args[0]);
	}
	else if (EJSVAL_IS_NUMBER (args[0])) {
		loc = EJSVAL_TO_NUMBER (args[0]);
	}
	else {
        const char* arg_as_str = [[CKValue valueWithJSValue:args[0]] utf8StringValue];
		NSLog (@"need more rubusness 1, js arg0 = %s", arg_as_str);
		abort();
	}
    
	GLboolean transpose = EJSVAL_TO_BOOLEAN (args[1]);

	EJSObject* arrayObj = EJSVAL_TO_OBJECT (args[2]);
	EJSObject* bufferView;
	if (EJSOBJECT_IS_TYPEDARRAY(arrayObj) && EJSTYPEDARRAY_ELEMENT_TYPE(arrayObj) == EJS_TYPEDARRAY_FLOAT32) {
		bufferView = arrayObj;
	}
	else if (EJSOBJECT_IS_ARRAY(arrayObj)) {
		bufferView = EJSVAL_TO_OBJECT(_ejs_typedarray_new_from_array (EJS_TYPEDARRAY_FLOAT32, OBJECT_TO_EJSVAL(arrayObj)));
	}
	else {
		NSLog(@"non array-like object passed to uniform*fv");
		abort();
	}

	GLfloat* array_buffer = (GLfloat*)_ejs_typedarray_get_data(bufferView);
	GLint array_length = EJSTYPEDARRAY_LEN(bufferView);
    
	GLint count = array_length / (c * c);
    
	switch (c) {
        case 2:
		SPEW(NSLog(@"glUniformMatrix2fv(loc = %d, count = %d, transpose = %d, array_buffer = %p)", loc, count, transpose, array_buffer);)
		SPEW({
		    NSLog(@"matrix = ");
		    int i;
		    for (i = 0; i < array_length; i ++) {
		      NSLog(@"  [%d] = %g", i, array_buffer[i]);
		    }
		})            
		glUniformMatrix2fv (loc, count, transpose, array_buffer);
		break;
        case 3:
		SPEW(NSLog(@"glUniformMatrix3fv(loc = %d, count = %d, transpose = %d, array_buffer = %p)", loc, count, transpose, array_buffer);)
		SPEW({
		    NSLog(@"matrix = ");
		    int i;
		    for (i = 0; i < array_length; i ++) {
		      NSLog(@"  [%d] = %g", i, array_buffer[i]);
		    }
		})            
		glUniformMatrix3fv (loc, count, transpose, array_buffer);
		break;
        case 4:
		SPEW(NSLog(@"glUniformMatrix4fv(loc = %d, count = %d, transpose = %d, array_buffer = %p)", loc, count, transpose, array_buffer);)
		SPEW({
		    NSLog(@"matrix = ");
		    int i;
		    for (i = 0; i < array_length; i ++) {
		      NSLog(@"  [%d] = %g", i, array_buffer[i]);
		    }
		})
		glUniformMatrix4fv (loc, count, transpose, array_buffer);
		break;
        default:
		abort();
	}
	CHECK_GL;
    
	return _ejs_undefined;
}

JSMETHOD (uniformMatrix2fv) { return uniformMatrix_fv (2, argc, args); }
JSMETHOD (uniformMatrix3fv) { return uniformMatrix_fv (3, argc, args); }
JSMETHOD (uniformMatrix4fv) { return uniformMatrix_fv (4, argc, args); }

/*
 void vertexAttrib1f(GLuint indx, GLfloat x);
 void vertexAttrib1fv(GLuint indx, Float32Array values);
 void vertexAttrib1fv(GLuint indx, float[] values);
 void vertexAttrib2f(GLuint indx, GLfloat x, GLfloat y);
 void vertexAttrib2fv(GLuint indx, Float32Array values);
 void vertexAttrib2fv(GLuint indx, float[] values);
 void vertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z);
 void vertexAttrib3fv(GLuint indx, Float32Array values);
 void vertexAttrib3fv(GLuint indx, float[] values);
 void vertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
 void vertexAttrib4fv(GLuint indx, Float32Array values);
 void vertexAttrib4fv(GLuint indx, float[] values);
 */

//    void vertexAttribPointer(GLuint indx, GLint size, GLenum type, 
//                             GLboolean normalized, GLsizei stride, GLintptr offset);
JSMETHOD (vertexAttribPointer) {
	if (argc != 6)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
	// FIXME check args
	GLuint indx = (GLuint)EJSVAL_TO_NUMBER (args[0]);
	GLint size = (GLint)EJSVAL_TO_NUMBER (args[1]);
	GLenum type = (GLenum)EJSVAL_TO_NUMBER (args[2]);
	GLboolean normalized = (GLboolean)EJSVAL_TO_BOOLEAN (args[3]);
	GLsizei stride = (GLsizei)EJSVAL_TO_NUMBER (args[4]);
	GLintptr offset = (GLintptr)EJSVAL_TO_NUMBER (args[5]);
    
	SPEW(NSLog (@"glVertexAttribPointer (%d, %d, %d, %d, %d, %ld)", indx, size, type, normalized, stride, offset);)
	glVertexAttribPointer (indx, size, type, normalized, stride, (GLvoid*)offset);
	CHECK_GL;
    
	return _ejs_undefined;
}


//    void useProgram(WebGLProgram program);
JSMETHOD (useProgram) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
	// FIXME check args
	GLuint program_id = 0;
	if (!EJSVAL_IS_PRIMITIVE (args[0])) {
		WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
		program_id = [program glId];
	}
    
	SPEW(NSLog (@"glUseProgram (%d)", program_id);)
    
	glUseProgram (program_id);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void validateProgram(WebGLProgram program);
JSMETHOD (validateProgram) {
	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
#if notyet
	if (!JSValueIsInstanceOfConstructor (ctx, args[0], [WebGLProgram jsConstructor], exception))
		return throwTypeError (ctx, exception);
#endif
    
	WebGLProgram *program = (WebGLProgram*)get_peer (args[0]);
	glValidateProgram ([program glId]);
	CHECK_GL;
    
	return _ejs_undefined;
}

//    void viewport(GLint x, GLint y, GLsizei width, GLsizei height);
JSMETHOD (viewport) {
	if (argc != 4)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
	// FIXME make sure the args are numbers
	GLint x = (GLint)EJSVAL_TO_NUMBER (args[0]);
	GLint y = (GLint)EJSVAL_TO_NUMBER (args[1]);
	GLsizei width = (GLsizei)EJSVAL_TO_NUMBER (args[2]);
	GLsizei height = (GLsizei)EJSVAL_TO_NUMBER (args[3]);
    
	SPEW(NSLog (@"glViewPort (%d %d %d %d)", x, y, width, height);)
	glViewport (x, y, width, height);
	CHECK_GL;
    
	return _ejs_undefined;
}

typedef struct {
	const char *name;
	int constant;
} webgl_constant;

static const webgl_constant constants[] =  {
#define WEBGL_CONSTANT(n,v) { #n, (v) },
#include "ejs-webgl-constants-sorted.h"
#undef WEBGL_CONSTANT
};

static int compare_constants (const void *a, const void *b)
{
	const char *key = (const char*)a;
	const webgl_constant* webgl_const = (const webgl_constant*)b;
    
	return strcmp (key, webgl_const->name);
}

static EJSBool
webglrenderingcontext_specop_has_property (ejsval O, ejsval P)
{
    char *prop = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(P));

	webgl_constant* constant_info = (webgl_constant*)bsearch(prop, constants, sizeof(constants)/sizeof(webgl_constant), sizeof(webgl_constant), compare_constants);
    
	if (constant_info == NULL) {
		free (prop);
        return _ejs_object_specops.has_property (O, P);
	}

	free (prop);
    
    return EJS_TRUE;
}


static ejsval
webglrenderingcontext_specop_get (ejsval O, ejsval P)
{
    char *prop = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(P));

	webgl_constant* constant_info = (webgl_constant*)bsearch(prop, constants, sizeof(constants)/sizeof(webgl_constant), sizeof(webgl_constant), compare_constants);
    
	if (constant_info == NULL) {
		free (prop);
        return _ejs_object_specops.get (O, P);
	}

	free (prop);
    
	return NUMBER_TO_EJSVAL (constant_info->constant);
}

ejsval
_ejs_objc_allocateWebGLRenderingContext (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
	EAGLContext* eagl_context = (EAGLContext*)get_objc_id ([[CKValue valueWithJSValue:args[0]] objectValue]);
    
	SPEW(NSLog (@"EAGLContext = %p\n", eagl_context);)

    EJSWebGLObject *glContext = _ejs_gc_new(EJSWebGLObject);
    _ejs_init_object ((EJSObject*)glContext, _ejs_Object__proto__, &WebGLRenderingContext_specops);

    glContext->peer = eagl_context; // XXX retain?

    ejsval obj = OBJECT_TO_EJSVAL(glContext);

#define WEBGL_FUNC(name, nArgs)  EJS_INSTALL_FUNCTION_FLAGS(obj, #name, JSMETHODNAME(name), EJS_PROP_NOT_WRITABLE | EJS_PROP_NOT_CONFIGURABLE)
	WEBGL_FUNC(activeTexture, 1);

	WEBGL_FUNC(attachShader, 2);
    
	WEBGL_FUNC(bindBuffer, 2);
	WEBGL_FUNC(bindFramebuffer, 2);
	WEBGL_FUNC(bindRenderbuffer, 2);
    
	WEBGL_FUNC(bindTexture, 2);
    
	WEBGL_FUNC(blendColor, 4);
	WEBGL_FUNC(blendEquation, 1);
	WEBGL_FUNC(blendEquationSeparate, 2);
	WEBGL_FUNC(blendFunc, 2);
	WEBGL_FUNC(blendFuncSeparate, 4);
    
	WEBGL_FUNC(bufferData, 4);
    
	WEBGL_FUNC(clear, 1);
	WEBGL_FUNC(clearColor, 4);
	WEBGL_FUNC(clearDepth, 1);
	WEBGL_FUNC(clearStencil, 1);
    
	WEBGL_FUNC(colorMask, 4);
    
	WEBGL_FUNC(compileShader, 1);
    
	WEBGL_FUNC(createBuffer, 0);
	WEBGL_FUNC(createFramebuffer, 0);
	WEBGL_FUNC(createRenderbuffer, 0);
    
	WEBGL_FUNC(createProgram, 0);
	WEBGL_FUNC(createShader, 1);
	WEBGL_FUNC(createTexture, 0);
    
	WEBGL_FUNC(cullFace, 1);
    
	WEBGL_FUNC (deleteBuffer, 1);
	WEBGL_FUNC (deleteFramebuffer, 1);
	WEBGL_FUNC (deleteProgram, 1);
	WEBGL_FUNC (deleteRenderbuffer, 1);
	WEBGL_FUNC (deleteShader, 1);
	WEBGL_FUNC (deleteTexture, 1);

	WEBGL_FUNC(depthFunc, 1);
	WEBGL_FUNC(depthMask, 1);
	WEBGL_FUNC(depthRange, 2);
	WEBGL_FUNC(disable, 1);
    
	WEBGL_FUNC(disableVertexAttribArray, 1);
    
	WEBGL_FUNC(drawArrays, 3);
	WEBGL_FUNC(drawElements, 4);
    
	WEBGL_FUNC(enable, 1);
	WEBGL_FUNC(enableVertexAttribArray, 1);
    
	WEBGL_FUNC(framebufferRenderbuffer, 4);
	WEBGL_FUNC(framebufferTexture2D, 5);
    
	WEBGL_FUNC(frontFace, 1);
	WEBGL_FUNC(generateMipmap, 1);
	WEBGL_FUNC(getActiveAttrib, 2);
	WEBGL_FUNC(getActiveUniform, 2);
	WEBGL_FUNC(getAttribLocation, 2);
	WEBGL_FUNC(getProgramInfoLog, 1);
	WEBGL_FUNC(getProgramParameter, 2);
	WEBGL_FUNC(getShaderInfoLog, 1);
	WEBGL_FUNC(getShaderParameter, 2);
	WEBGL_FUNC(getShaderSource, 1);
    
	WEBGL_FUNC(getUniformLocation, 2);
    
	WEBGL_FUNC(linkProgram, 1);
	WEBGL_FUNC(pixelStorei, 2);
    
	WEBGL_FUNC(renderbufferStorage, 4);
    
	WEBGL_FUNC(shaderSource, 2);
    
	WEBGL_FUNC(texImage2D, 9);
	WEBGL_FUNC(texParameterf, 3);
	WEBGL_FUNC(texParameteri, 3);
    
	WEBGL_FUNC(uniform1f, 2);
	WEBGL_FUNC(uniform2f, 3);
	WEBGL_FUNC(uniform3f, 4);
	WEBGL_FUNC(uniform4f, 5);
    
	WEBGL_FUNC(uniform1fv, 2);
	WEBGL_FUNC(uniform2fv, 2);
	WEBGL_FUNC(uniform3fv, 2);
	WEBGL_FUNC(uniform4fv, 2);
    
	WEBGL_FUNC(uniformMatrix2fv, 3);
	WEBGL_FUNC(uniformMatrix3fv, 3);
	WEBGL_FUNC(uniformMatrix4fv, 3);
    
	WEBGL_FUNC(uniform1i, 2);
	WEBGL_FUNC(uniform2i, 3);
	WEBGL_FUNC(uniform3i, 4);
	WEBGL_FUNC(uniform4i, 5);
    
	WEBGL_FUNC(uniform1iv, 2);
	WEBGL_FUNC(uniform2iv, 2);
	WEBGL_FUNC(uniform3iv, 2);
	WEBGL_FUNC(uniform4iv, 2);
    
	WEBGL_FUNC(vertexAttribPointer, 6);
	WEBGL_FUNC(useProgram, 1);
	WEBGL_FUNC(validateProgram, 1);
    
	WEBGL_FUNC(viewport, 4);

#undef WEBGL_FUNC

	[EAGLContext setCurrentContext:eagl_context];
    
	return obj;
}

EJSSpecOps WebGLRenderingContext_specops;
EJSSpecOps WebGLBuffer_specops;
EJSSpecOps WebGLFramebuffer_specops;
EJSSpecOps WebGLRenderbuffer_specops;
EJSSpecOps WebGLProgram_specops;
EJSSpecOps WebGLShader_specops;
EJSSpecOps WebGLTexture_specops;
EJSSpecOps WebGLActiveInfo_specops;
EJSSpecOps WebGLUniformLocation_specops;

void
_ejs_webgl_init(ejsval global)
{
    WebGLRenderingContext_specops = _ejs_object_specops;
    WebGLRenderingContext_specops.class_name = "WebGLRenderingContext";
    WebGLRenderingContext_specops.has_property = webglrenderingcontext_specop_has_property;
    WebGLRenderingContext_specops.get = webglrenderingcontext_specop_get;
    WebGLRenderingContext_specops.finalize = finalize_release_private_data;

    EJSSpecOps webgl_obj_specops = _ejs_object_specops;
    webgl_obj_specops.finalize = finalize_release_private_data;

#define WEBGL_SPECOPS(n) n##_specops = webgl_obj_specops; n##_specops.class_name = #n

    WEBGL_SPECOPS(WebGLBuffer);
    WEBGL_SPECOPS(WebGLFramebuffer);
    WEBGL_SPECOPS(WebGLRenderbuffer);
    WEBGL_SPECOPS(WebGLProgram);
    WEBGL_SPECOPS(WebGLShader);
    WEBGL_SPECOPS(WebGLTexture);
    WEBGL_SPECOPS(WebGLActiveInfo);

    WebGLUniformLocation_specops = _ejs_object_specops;
    WebGLUniformLocation_specops.class_name = "WebGLUniformLocation";

    WebGLActiveInfo__proto__ = _ejs_object_create (_ejs_Object_prototype);
    EJS_INSTALL_GETTER (WebGLActiveInfo__proto__, "size", webglactiveinfo_get_size);
    EJS_INSTALL_GETTER (WebGLActiveInfo__proto__, "type", webglactiveinfo_get_type);
    EJS_INSTALL_GETTER (WebGLActiveInfo__proto__, "name", webglactiveinfo_get_name);
}

#endif

