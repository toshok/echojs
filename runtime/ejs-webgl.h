
#ifndef __EJS_WEBGL_H__
#define __EJS_WEBGL_H__

#import "ejs-jsobjc.h"
#import <OpenGLES/ES2/gl.h>

extern EJSSpecOps WebGLRenderingContext_specops;
extern EJSSpecOps WebGLBuffer_specops;
extern EJSSpecOps WebGLFramebuffer_specops;
extern EJSSpecOps WebGLRenderbuffer_specops;
extern EJSSpecOps WebGLProgram_specops;
extern EJSSpecOps WebGLShader_specops;
extern EJSSpecOps WebGLTexture_specops;
extern EJSSpecOps WebGLActiveInfo_specops;
extern EJSSpecOps WebGLUniformLocation_specops;

ejsval _ejs_objc_allocateWebGLRenderingContext (ejsval env, ejsval _this, uint32_t argc, ejsval *args);

void _ejs_webgl_init(ejsval global);


@interface WebGLObject : NSObject {
  GLuint _glId;
}
-(id)initWithGLId:(GLuint)glid;
-(GLuint)glId;
@end

@interface WebGLBuffer : WebGLObject {
}
//-(id)initWithGLId:(GLint)id;
-(void)deleteBuffer;
-(void)dealloc;
@end

@interface WebGLFramebuffer : WebGLObject {
}
//-(id)initWithGLId:(GLint)id;
-(void)deleteFramebuffer;
-(void)dealloc;
@end

@interface WebGLProgram : WebGLObject {
}
//-(id)initWithGLId:(GLint)id;
-(void)deleteProgram;
-(void)dealloc;
@end

@interface WebGLRenderbuffer : WebGLObject {
}
//-(id)initWithGLId:(GLint)id;
-(void)deleteRenderbuffer;
-(void)dealloc;
@end

@interface WebGLShader : WebGLObject {
}
//-(id)initWithGLId:(GLint)id;
-(void)deleteShader;
-(void)dealloc;
@end

@interface WebGLTexture : WebGLObject {
}
//-(id)initWithGLId:(GLint)id;
-(void)deleteTexture;
-(void)dealloc;
@end

@interface WebGLActiveInfo : NSObject {
  GLint _size;
  GLenum _type;
  char *_name;
}
-(id)initWithName:(const char*)n type:(GLenum)t size:(GLint)s;
-(void)dealloc;
-(GLint)size;
-(GLenum)type;
-(const char*)name;
@end

#endif /* __EJS_WEBGL_H__ */
