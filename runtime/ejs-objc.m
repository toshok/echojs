/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#if IOS
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif
#import <objc/runtime.h>
#import <objc/message.h>

#include "ejs-function.h"
#import "ejs-jsobjc.h"
#include "ejs-objc.h"
#include "ejs-string.h"
#include "ejs-error.h"

#if IOS
#include "ejs-webgl.h"
#endif

#define SPEW(x)

// the keys we use for our associated objects
#define JSCTX_KEY (void*)1
#define JSPEER_KEY (void*)2
#define JSCTOR_KEY (void*)3
#define JSMETHODMAP_KEY (void*)4
#define MONKEYPATCHED_RETAIN_RELEASE (void*)5
#define JSRETAINCOUNT_KEY (void*)6

#define RO_DONT_ENUM_PERMANENT (EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_NOT_CONFIGURABLE)

id
get_objc_id (CKObject* obj)
{
	CKObject *handle = [obj objectForPropertyNS:@"_ck_handle"];
	return handle ? ((EJSObjcHandle*)[handle jsObject])->handle : nil;
}


void
set_jspeer (id objc_obj, void*  jspeer)
{
    objc_setAssociatedObject(objc_obj, JSPEER_KEY, jspeer, OBJC_ASSOCIATION_RETAIN);
}

void*
get_jspeer (id objc_obj)
{
    return (void*)objc_getAssociatedObject(objc_obj, JSPEER_KEY);
}

void
set_jsctor (Class cls, void *ctor)
{
    objc_setAssociatedObject(cls, JSCTOR_KEY, ctor, OBJC_ASSOCIATION_RETAIN);
}

void*
get_jsctor (Class cls)
{
    return (void*)objc_getAssociatedObject (cls, JSCTOR_KEY);
}


ejsval _ejs_ObjcHandle;
ejsval _ejs_ObjcHandle_proto;
EJSSpecOps _ejs_objchandle_specops;

static EJS_NATIVE_FUNC(_ejs_ObjcHandle_impl) {
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_objc_handle_new (id handle)
{
    if (handle == nil)
        _ejs_throw_nativeerror_utf8(EJS_ERROR, "_ejs_objc_handle_new passed a nil objc id");

    EJSObjcHandle *rv = _ejs_gc_new(EJSObjcHandle);
    _ejs_init_object ((EJSObject*)rv, _ejs_ObjcHandle_proto, &_ejs_objchandle_specops);

    rv->handle = [handle retain];

    return OBJECT_TO_EJSVAL(rv);
}

CKObject*
create_objc_handle_object (id objc_id)
{
    ejsval handle = _ejs_objc_handle_new(objc_id);

    return [CKObject objectWithJSObject:EJSVAL_TO_OBJECT(handle)];
}

id
ejs_objc_handle_get_id (ejsval handleval)
{
    EJSObjcHandle* handleobj = (EJSObjcHandle*)EJSVAL_TO_OBJECT(handleval);
    return handleobj->handle;
}

ejsval _ejs_CoffeeKitObject;
ejsval _ejs_CoffeeKitObject_proto;
EJSSpecOps _ejs_coffeekitobject_specops;

static EJS_NATIVE_FUNC(_ejs_CoffeeKitObject_impl) {
    EJS_NOT_IMPLEMENTED();
}

static EJS_NATIVE_FUNC(_ejs_CoffeeKitObject_setHandle) {
	CKObject* thisObject = [CKObject objectWithJSObject:EJSVAL_TO_OBJECT(*_this)];
    
	SPEW(NSLog (@"made it to coffeekit_object_set_handle, argumentCount = %u", argc);)

    EJSObjcHandle* handleArg = (EJSObjcHandle*)EJSVAL_TO_OBJECT (args[0]);

	if (handleArg->handle == nil) {
        _ejs_throw_nativeerror_utf8(EJS_ERROR, "setHandle called with nil handle");
	}

	[thisObject definePropertyNS:@"_ck_handle"
     object:[CKObject objectWithJSObject:(EJSObject*)handleArg]
		    attributes:EJS_PROP_NOT_WRITABLE | EJS_PROP_NOT_CONFIGURABLE];

	set_jspeer (handleArg->handle, thisObject);

    return _ejs_undefined;
}


static EJS_NATIVE_FUNC(_ejs_CoffeeKitObject_prototype_toString) {
	CKObject* thisObject = [CKObject objectWithJSObject:EJSVAL_TO_OBJECT(*_this)];
    NSString* desc = [NSString stringWithFormat:@"%@",  get_objc_id(thisObject)];
    return STRING_TO_EJSVAL([[CKString stringWithNSString:desc] jsString]);
}

void
_ejs_objc_init(ejsval global)
{
    _ejs_objchandle_specops =  _ejs_Object_specops;
    _ejs_objchandle_specops.class_name = "ObjcHandle";

    _ejs_gc_add_root (&_ejs_ObjcHandle_proto);
    _ejs_ObjcHandle_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_objchandle_specops);

    _ejs_ObjcHandle = _ejs_function_new_native (_ejs_null, _ejs_atom_ObjcHandle, _ejs_ObjcHandle_impl);

    _ejs_object_setprop (_ejs_ObjcHandle, _ejs_atom_prototype,  _ejs_ObjcHandle_proto);
    _ejs_object_setprop (_ejs_ObjcHandle_proto, _ejs_atom_constructor,  _ejs_ObjcHandle);

    _ejs_object_setprop (global, _ejs_atom_ObjcHandle, _ejs_ObjcHandle);

    _ejs_coffeekitobject_specops =  _ejs_Object_specops;
    _ejs_coffeekitobject_specops.class_name = "PirouetteObject";

    _ejs_gc_add_root (&_ejs_CoffeeKitObject_proto);
    _ejs_CoffeeKitObject_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_coffeekitobject_specops);

    _ejs_CoffeeKitObject = _ejs_function_new_native (_ejs_null, _ejs_atom_PirouetteObject, (EJSClosureFunc)_ejs_CoffeeKitObject_impl);

    _ejs_object_setprop (_ejs_CoffeeKitObject, _ejs_atom_prototype,  _ejs_CoffeeKitObject_proto);
    _ejs_object_setprop (_ejs_CoffeeKitObject_proto, _ejs_atom_constructor,  _ejs_CoffeeKitObject);

    EJS_INSTALL_ATOM_FUNCTION(_ejs_CoffeeKitObject, setHandle, _ejs_CoffeeKitObject_setHandle);
    EJS_INSTALL_ATOM_FUNCTION(_ejs_CoffeeKitObject_proto, toString, _ejs_CoffeeKitObject_prototype_toString);

    _ejs_object_setprop (global, _ejs_atom_PirouetteObject, _ejs_CoffeeKitObject);
}




static EJS_NATIVE_FUNC(_ejs_objc_requireFramework) {
    // this needs to go away, replaced by a pragma that the compiler can react to
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_objc_allocInstance) {
    if (argc == 0)
        return _ejs_null;

    ejsval clsname = args[0];

    if (!EJSVAL_IS_STRING(clsname))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "args0 for allocInstance must be a string");

    char *clsname_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[0]));
    Class cls = objc_getClass (clsname_utf8);
    NSLog (@"allocating instance of %s/%p\n", clsname_utf8, cls);
    free (clsname_utf8);
    
    id instance = class_createInstance (cls, 0);
    NSLog (@"class_createInstance returned %@\n", instance);
    ejsval rv = _ejs_objc_handle_new (instance); // this retains
    [instance release]; // so we release instance here
    
    return rv;
}

static EJS_NATIVE_FUNC(_ejs_objc_staticCall) {
    const char *clsname_cstr = [[CKString stringWithJSString:EJSVAL_TO_STRING(args[0])] UTF8String];
    const char *selector_cstr = [[CKString stringWithJSString:EJSVAL_TO_STRING(args[1])] UTF8String];

    Class cls = objc_getClass (clsname_cstr);
    SEL sel = sel_getUid (selector_cstr);

	return OBJECT_TO_EJSVAL([create_objc_handle_object (objc_msgSend (cls, sel)) jsObject]);
}

static EJS_NATIVE_FUNC(_ejs_objc_getInstanceVariable) {
	SPEW(NSLog (@"in _icall_objc_getInstanceVariable");)
	if (argc != 2) {
		NSLog (@"getInstanceVariable requires 2 args");
		// XXX throw an exception here
		abort();
	}

	id handle = get_objc_id ([CKObject objectWithJSObject:EJSVAL_TO_OBJECT(args[0])]);

	if (!handle) {
		NSLog (@"getInstanceVariable first parameter has no handle");
		// XXX throw an exception here
		abort();
	}
    char* ivar_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[1]));
	id ivar_value = nil;

	SPEW(NSLog (@"    %@:%s", handle, ivar_utf8););

	object_getInstanceVariable (handle, ivar_utf8, (void**)&ivar_value);
	free (ivar_utf8);
    
	SPEW(NSLog (@"    returning value = %@", ivar_value);)
    
	if (ivar_value)
		return OBJECT_TO_EJSVAL([create_objc_handle_object (ivar_value) jsObject]);
	else
		return _ejs_null;
}

static EJS_NATIVE_FUNC(_ejs_objc_setInstanceVariable) {
	SPEW(NSLog (@"in _icall_objc_setInstanceVariable");)
	if (argc != 3) {
		NSLog (@"setInstanceVariable requires 3 args");
		// XXX throw an exception here
		abort();
	}

	id handle = get_objc_id ([[CKValue valueWithJSValue:args[0]] objectValue]);
	if (!handle) {
		NSLog (@"setInstanceVariable first parameter has no handle");
		// XXX throw an exception here
		abort();
	}
    char* ivar_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[1]));

	id value_handle = get_objc_id ([[CKValue valueWithJSValue:args[2]] objectValue]);

	SPEW(NSLog (@"  setting %s on %@ to %@ ", ivar_utf8, handle, value_handle);)

	object_setInstanceVariable (handle, ivar_utf8, value_handle);
	free (ivar_utf8);

    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(invokeSelectorFromJS);

static EJS_NATIVE_FUNC(_ejs_objc_selectorInvoker) {
    if (!EJSVAL_IS_STRING(args[0])) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "non-string passed to objc.selectorInvoker");
    }

    char *selname_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[0]));

    //NSLog (@"in _ejs_objc_selectorInvoker (%s)", selname_utf8);

    CKObject* func = [CKObject makeFunctionNS:[NSString stringWithFormat:@"__ejs_invoke_%s_from_js", selname_utf8] withCallback:(EJSClosureFunc)invokeSelectorFromJS argCount:0];

    free (selname_utf8);

    [func definePropertyNS:@"_ck_sel"
          value:[CKValue valueWithJSValue:args[0]]
          attributes:RO_DONT_ENUM_PERMANENT];

    return OBJECT_TO_EJSVAL([func jsObject]);
}

static EJS_NATIVE_FUNC(_ejs_objc_getTypeEncoding) {
    EJS_NOT_IMPLEMENTED();
}

@interface MethodInfo : NSObject {
    NSString* _name;
    SEL _sel;
    NSString* _sig;
    NSMethodSignature* _methodSig;
}

-(id) initWithName:(NSString*)name_str selector:(SEL)selector typeSig:(const char*)type_str;
-(NSMethodSignature*) resolveSigWithClass:(Class)cls staticMethod:(BOOL)static_method;
-(NSString*) name;
-(SEL) sel;
-(NSString*) sig;
-(NSString*) description;

@end

@implementation MethodInfo
-(id) initWithName:(NSString*)name_str selector:(SEL)selector typeSig:(const char*)type_str
{
    self = [super init];
    _name = [name_str retain];
    _sel = selector;
    _sig = [[NSString alloc] initWithUTF8String:type_str];
    return self;
}

-(NSMethodSignature*) resolveSigWithClass:(Class)cls staticMethod:(BOOL)static_method
{
    if (_methodSig)
        return _methodSig;

    //  NSLog (@"resolveSigWithClass %@ (static = %s)", _name, static_method ? "YES" : "NO");
    Method meth = static_method ? class_getClassMethod (cls, _sel) : class_getInstanceMethod (cls, _sel);

    if (!meth) {
        NSString* str = [NSString stringWithFormat:@"failed to locate method `%s' on class %@", sel_getName(_sel), cls];
        _ejs_throw_nativeerror_utf8(EJS_ERROR, [str UTF8String]);
    }

    //  NSLog (@"method type encoding = %s", method_getTypeEncoding (meth));

    _methodSig = [[NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding (meth)] retain];
  
    return _methodSig;
}

-(NSString*) name
{
    return _name;
}

-(SEL) sel
{
    return _sel;
}

-(NSString*) sig
{
    return _sig;
}

-(NSString*) description
{
    return [NSString stringWithFormat:@"<MethodInfo name=%@ sel=%s sig=%@>", _name, sel_getName(_sel), _sig];
}
@end

static const char*
get_selector_name_from_function (CKObject* function)
{
    return [function utf8StringForPropertyNS:@"_ck_sel"];
}

static const char*
get_type_sig_from_function (CKObject* function)
{
    return [function utf8StringForPropertyNS:@"_ck_typeSig"];
}

static const char*
get_type_encoding_from_function (CKObject* function)
{
    return [function utf8StringForPropertyNS:@"_ck_typeEncoding"];
}

static SEL
get_selector_from_function (CKObject* function)
{
    return sel_getUid(get_selector_name_from_function (function));
}

static bool
is_function_exported (CKObject* func)
{
    CKValue* propval = [func valueForPropertyNS:@"_ck_exported"];
    
    return [propval isBool] && [propval boolValue];
}

static CKObject*
get_object_prototype_method ( NSString *name)
{
    EJS_NOT_IMPLEMENTED();
#if notyet
	CKObject* object_proto = [[[ctx globalObject] objectForPropertyNS:@"Object"] prototype];
	return [object_proto objectForPropertyNS:name];
#endif
}

static CKValue*
marshal_id_as_jsvalue (id objc_id, BOOL protect)
{
    if (objc_id == nil) {
        SPEW(NSLog (@"returning null");)
        return [CKValue nullValue];
    }

    if ([objc_id isKindOfClass:[NSString class]]) {
        return [CKValue nsStringValue:(NSString*)objc_id];
    }

    CKObject* ctor_obj = NULL;
    Class return_class = object_getClass (objc_id);

    do {
        SPEW(NSLog (@"return_class = %@", return_class);)

        ctor_obj = get_jsctor(return_class);

        //          NSLog (@"ctor_obj = %p", ctor_obj);

        if (!ctor_obj) {
            return_class = class_getSuperclass (return_class);
            if (return_class == NULL) {
                NSLog (@"no further superclasses, bailing...");
                abort();
            }
        }
        
    } while (ctor_obj == NULL);

    // this will retain objc_id
    CKObject* handle = create_objc_handle_object (objc_id);
    CKValue* handle_val = [CKValue objectValue:handle];
    //        NSLog (@"handle = %p", handle);

    CKInvocation *inv = [CKInvocation invocationWithConstructor:ctor_obj argCount:1];
    
    [inv setArgument:handle_val atIndex:0];
    
    // and this will create a ref to the handle_val so that the lifetime of the objc_id will be at least
    // as long as the lifetime of jspeer.
    CKValue* rv = [inv invoke];
    CKObject* jspeer = [rv objectValue];
    if (jspeer == NULL)
        [NSException raise:@"Exception marshaling id as jsvalue" format:@"%@", [inv exception]];

    //        NSLog (@"peer = %p", jspeer);
    set_jspeer (objc_id, jspeer);

    return rv;
}

static CKValue*
marshal_cgrect_as_jsvalue (CGRect *rect)
{
	CKString* x_ = [CKString stringWithUTF8CString:"x"];
	CKString* y_ = [CKString stringWithUTF8CString:"y"];
	CKString* width_ = [CKString stringWithUTF8CString:"width"];
	CKString* height_ = [CKString stringWithUTF8CString:"height"];

    CKObject* peer = [CKObject objectWithJSObject:EJSVAL_TO_OBJECT(_ejs_object_create(_ejs_null))];
    
    [peer defineProperty:x_ value:[CKValue numberValue:rect->origin.x]
          attributes:RO_DONT_ENUM_PERMANENT];

    [peer defineProperty:y_ value:[CKValue numberValue:rect->origin.y]
          attributes:RO_DONT_ENUM_PERMANENT];

    [peer defineProperty:width_ value:[CKValue numberValue:rect->size.width]
          attributes:RO_DONT_ENUM_PERMANENT];
    
    [peer defineProperty:height_ value:[CKValue numberValue:rect->size.height]
          attributes:RO_DONT_ENUM_PERMANENT];

    return [CKValue objectValue:peer];
}

static NSArray*
marshal_jsarray_as_nsarray (CKObject *o)
{
	int length = [o arrayLength];
        int i;
    
        NSMutableArray *nsarray = [NSMutableArray arrayWithCapacity:length];
    
        for (i = 0; i < length; i ++) {
                CKObject* el = [o objectForPropertyNS:[NSString stringWithFormat:@"%d", i]];
                [nsarray addObject:get_objc_id(el)];
        }
    
        return [NSArray arrayWithArray:nsarray];
}

static EJS_NATIVE_FUNC(invokeSelectorFromJS) {
    CKObject *thisObj = [CKObject objectWithJSObject:EJSVAL_TO_OBJECT(*_this)];
    
#if old
    SEL sel = get_selector_from_function (func);
#else
    SEL sel = sel_getUid([[CKValue valueWithJSValue:args[0]] utf8StringValue]);
#endif
    id handle = get_objc_id (thisObj);
    Method meth;
    const char *typeEncoding = NULL;
    void** free_ptrs = NULL;

    if (handle == nil) {
        const char *register_cstr = [thisObj utf8StringForPropertyNS:@"_ck_register"];
        
        if (register_cstr) {
            Class cls = objc_getClass (register_cstr);
            meth = class_getClassMethod (cls, sel);
            if (meth != NULL)
                handle = cls;
        }
    }
    else {
        meth = class_getInstanceMethod (object_getClass (handle), sel);
    }

    SPEW(NSLog (@"invoking %s on %@\n", sel_getName (sel), handle);)

#if false
    typeEncoding = get_type_encoding_from_function (func);
#endif
    if (!typeEncoding) {
        SPEW(NSLog (@"getting type encoding from objc");)
        typeEncoding = method_getTypeEncoding (meth);
        SPEW(NSLog (@" it was %s", typeEncoding);)
        if (!typeEncoding) {
            NSString* str = [NSString stringWithFormat:@"failed to locate selector `%s'.  this is usually due to a framework not being linked against.", sel_getName (sel)];
            _ejs_throw_nativeerror_utf8(EJS_ERROR, [str UTF8String]);
        }
    }
    SPEW(NSLog (@"  type = %s", typeEncoding);)

        if (handle == NULL) {
        _ejs_throw_nativeerror_utf8(EJS_ERROR, "NULL handle in invokeSelectorFromJS");
    }
        
    if (sel == NULL) {
        _ejs_throw_nativeerror_utf8(EJS_ERROR, "NULL selector in invokeSelectorFromJS");
    }

    
    NSMethodSignature *sig = [NSMethodSignature signatureWithObjCTypes:typeEncoding];
    NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];

    if ((argc + 2 - 1 /* the -1 to get rid of the selector passed as arg0 */) != [sig numberOfArguments]) {
        NSLog (@"Incorrect number of arguments to objective-C method %s on %@ (type encoding %s).  expected = %lu, offered = %u", sel_getName(sel), handle, typeEncoding, (unsigned long)[sig numberOfArguments], argc + 2);
        return _ejs_null;
    }

    free_ptrs = (void**)calloc(sizeof(void*), argc);
    [inv setTarget:handle];
    [inv setSelector:sel];

    int i;
    for (i = 0; i < argc-1; i ++) {
        int objc_arg_num = i+2;
        const char* arg_type = [sig getArgumentTypeAtIndex:objc_arg_num];
        CKValue* val = [CKValue valueWithJSValue:args[i+1]];
        
		SPEW(NSLog(@"marshaling arg[%d], type %s", i, arg_type);)

        switch (arg_type[0]) {
        case _C_ID: {
            void* arg_ptr = NULL;

            if ([val isString]) {
                // create an NSString for this arg
                arg_ptr = [[val stringValue] nsString];
                //            NSLog (@"arg %d: string marshalling of %@", i, arg_ptr);
            }
            else if ([val isNull] || [val isUndefined]) {
                arg_ptr = nil;
                //            NSLog (@"arg %d: object marshalling of %@", i, arg_ptr);
            }
            else if ([val isObject]) {
                // fetch the objc handle from this arg
                CKObject* o = [val objectValue];
                arg_ptr = get_objc_id (o);
                if (arg_ptr == NULL) {
                    if ([o isArray]) {
                        // let's assume we marshal this as an NSArray for now... XXX
                        arg_ptr = marshal_jsarray_as_nsarray (o);
                    }
                }
				SPEW(NSLog (@"arg %d: object marshalling of %@", i, arg_ptr);)
            }
            else {
#if notyet_spidermonkey
                NSLog (@"arg %d: skipping @ marshaling arg (unhandled JS type %d)", i, JSValueGetType (cx, JS_ARGV(cx,vp)[i]));
                NSLog (@"selector: %s", sel_getName (sel));
                int j;
                for (j = 0; j < argc; j ++) {
                    NSLog (@"arg %d: JS type %d", j, JSValueGetType (cx, JS_ARGV(cx,vp)[i]));
                }
#else
				NSLog (@"error marshaling 1");
#endif
                abort();
            }
                
            [inv setArgument:&arg_ptr atIndex:objc_arg_num];
            break;
        }
                
        case _C_SEL: {
            if ([val isObject] && [[val objectValue] isFunction]) {
                SEL sel = get_selector_from_function ([val objectValue]);
                SPEW(NSLog (@"arg %d: marshaling selector %s", i, sel ? sel_getName (sel) : "<NULL>");)
                [inv setArgument:&sel atIndex:objc_arg_num];
            }
            else if ([val isString]) {
                const char *sel_str = [[val stringValue] UTF8String];
                SPEW(NSLog (@"arg %d: marshaling selector %s", i, sel_str);)
                SEL sel = sel_getUid (sel_str);
                [inv setArgument:&sel atIndex:objc_arg_num];
            }
            else {
#if notyet_spidermonkey
                NSLog (@"arg %d: skipping selector marshaling arg (unhandled JS type %d)", i, JSValueGetType (cx, JS_ARGV(cx,vp)[i]));
#else
				NSLog (@"error marshaling 2");
#endif
                abort();
            }
            break;
        }

        case _C_FLT: {
            float v = (float)[val numberValue];
			SPEW(NSLog (@"arg %d: marshaling float = %g", i, v);)
            [inv setArgument:&v atIndex:objc_arg_num];
            break;
        }

        case _C_DBL: {
            double v = (double)[val numberValue];
			SPEW(NSLog (@"arg %d: marshaling double = %g", i, v);)
            [inv setArgument:&v atIndex:objc_arg_num];
            break;
        }

        case _C_INT: {
            int32_t v = (int32_t)[val numberValue];
			SPEW(NSLog (@"arg %d: marshaling int = %d", i, v);)
            [inv setArgument:&v atIndex:objc_arg_num];
            break;
        }
                
        case _C_UINT: {
            uint32_t v = (uint32_t)[val numberValue];
			SPEW(NSLog (@"arg %d: marshaling uint = %u", i, v);)
            [inv setArgument:&v atIndex:objc_arg_num];
            break;
        }
                
        case _C_LNG_LNG: {
            int64_t v = (int64_t)[val numberValue];
            [inv setArgument:&v atIndex:objc_arg_num];
            break;
        }

        case _C_ULNG_LNG: {
            uint64_t v = (uint64_t)[val numberValue];
            [inv setArgument:&v atIndex:objc_arg_num];
            break;
        }
                
        case _C_CHR: {
#if !IOS
            char v = (char)[val numberValue];
#else
            int32_t v = (int32_t)[val numberValue];
#endif
            [inv setArgument:&v atIndex:objc_arg_num];
            break;
        }
                
        case _C_CHARPTR: {
            const char* jsstr = [[val stringValue] UTF8String];
            [inv setArgument:&jsstr atIndex:objc_arg_num];
            break;
        }
                
        case _C_STRUCT_B: {
            // marshal by duck typing, not by the JS type, so we can accept object literals
                
            if (!strncmp (&arg_type[1], "CGRect", strlen ("CGRect"))
                || !strncmp (&arg_type[1], "NSRect", strlen ("NSRect"))) {
                // FIXME: handle both x,y,width,height and position,size?
                //JSValueRef* js_exc = NULL;

                CKObject* rectobj = [val objectValue];
                CGRect *rect = (CGRect*)malloc (sizeof (CGRect));

                rect->origin.x = [rectobj floatForPropertyNS:@"x"];
                rect->origin.y = [rectobj floatForPropertyNS:@"y"];
                rect->size.width = [rectobj floatForPropertyNS:@"width"];
                rect->size.height = [rectobj floatForPropertyNS:@"height"];
                    
                NSLog (@"marshaling rectangle %g %g %g %g", rect->origin.x, rect->origin.y, rect->size.width, rect->size.height);
                    
                free_ptrs[i] = rect;
                    
                [inv setArgument:rect atIndex:objc_arg_num];
            }
#if IOS
            else if (!strncmp (&arg_type[1], "UIEdgeInsets", strlen ("UIEdgeInsets"))) {
                    
                CKObject* insetsobj = [val objectValue];

                UIEdgeInsets *insets = (UIEdgeInsets*)malloc (sizeof (UIEdgeInsets));
                insets->top = [insetsobj floatForPropertyNS:@"top"];
                insets->left = [insetsobj floatForPropertyNS:@"left"];;
                insets->bottom = [insetsobj floatForPropertyNS:@"bottom"];;
                insets->right = [insetsobj floatForPropertyNS:@"right"];;

                free_ptrs[i] = insets;
                    
                [inv setArgument:insets atIndex:objc_arg_num];
            }
            else if (!strncmp (&arg_type[1], "UIOffset", strlen ("UIOffset"))) {
                    
                CKObject* offsetobj = [val objectValue];

                UIOffset *offset = (UIOffset*)malloc (sizeof (UIOffset));
                offset->horizontal = [offsetobj floatForPropertyNS:@"horizontal"];
                offset->vertical = [offsetobj floatForPropertyNS:@"vertical"];
                    
                free_ptrs[i] = offset;

                [inv setArgument:offset atIndex:objc_arg_num];
            }
#endif
            else {
                NSLog (@"skipping marshaling arg %d (unhandled struct type %s)", objc_arg_num, arg_type);
                abort();
            }
            break;
        }
                
        default:
            NSLog (@"skipping marshaling arg %d (unhandled objc type %s)", objc_arg_num, arg_type);
            abort();
            break;
        }
    }
    
    SPEW(NSLog (@" start objc invoke");)
    [inv invoke];
    SPEW(NSLog (@" end objc invoke");)
    
    // free the marshaled structs
    for (i = 0; i < argc; i ++)
        if (free_ptrs[i])
            free (free_ptrs[i]);
    free(free_ptrs);
    
    
    const char *return_type = [sig methodReturnType];
    // bail early if the return type is void
    if (return_type[0] == _C_VOID) {
		return _ejs_undefined;
	}
    
    NSUInteger return_length = [sig methodReturnLength];
    void* return_buffer = (void *)malloc(return_length);
    CKValue* rv;
    
    [inv getReturnValue:return_buffer];
    
    // marshal the rv
    switch (return_type[0]) {
    case _C_ID: {
        SPEW(NSLog (@"@ return type, creating JS peer, return_buffer = %p, value = %p", return_buffer, *(id*)return_buffer);)
        rv = marshal_id_as_jsvalue(*(id*)return_buffer, YES);
        break;
    }
            
    case _C_STRUCT_B: {
        if (!strncmp (&return_type[1], "CGRect", strlen ("CGRect"))
            || !strncmp (&return_type[1], "NSRect", strlen ("NSRect"))) {
            // XXX this should return an instance of foundation.NSRect
            // but for now we just return a new empty object with the
            // right fields set.
            rv = marshal_cgrect_as_jsvalue ((CGRect*)return_buffer);
        }
        else {
            NSString* str = [NSString stringWithFormat:@"unhandled struct return type `%s'", return_type];
            _ejs_throw_nativeerror_utf8(EJS_ERROR, [str UTF8String]);
        }
        break;
    }
            
    case _C_CHR:
        rv = [CKValue numberValue:*(int8_t*)return_buffer];
        break;            
    case _C_UCHR:
        rv = [CKValue numberValue:*(uint8_t*)return_buffer];
        break;            
    case _C_SHT:
        rv = [CKValue numberValue:*(int16_t*)return_buffer];
        break;            
    case _C_USHT:
        rv = [CKValue numberValue:*(uint16_t*)return_buffer];
        break;            
    case _C_INT:
        rv = [CKValue numberValue:*(int32_t*)return_buffer];
        break;            
    case _C_UINT:
        rv = [CKValue numberValue:*(uint32_t*)return_buffer];
        break;
    case _C_LNG_LNG:
        rv = [CKValue numberValue:*(int64_t*)return_buffer];
        break;
    case _C_ULNG_LNG:
        rv = [CKValue numberValue:*(uint64_t*)return_buffer];
        break;
    case _C_FLT:
	rv = [CKValue numberValue:*(float*)return_buffer];
	break;
    case _C_DBL:
	rv = [CKValue numberValue:*(double*)return_buffer];
	break;

    default: {
        NSString* str = [NSString stringWithFormat:@"unhandled return type `%s'", return_type];
        _ejs_throw_nativeerror_utf8(EJS_ERROR, [str UTF8String]);
    }
    }
        
    free (return_buffer);
    return [rv jsValue];
}

static CKObject*
get_js_peer (id obj, id *class_out, BOOL *static_method)
{
    CKObject* jspeer = get_jspeer (obj);
    if (jspeer == NULL) {
        jspeer = get_jsctor (obj);
        if (jspeer == NULL) {
            SPEW(NSLog (@"null peer somehow made it to the method tramp...");)
            abort ();
        }
        else {
            *class_out = obj;
            *static_method = YES;
        }
    }
    else {
        *class_out = [obj class];
        *static_method = NO;
    }
    
    return jspeer;
}

static void*
coffeekit_method_tramp (id obj, SEL sel, ...)
{
    const char *sel_name = sel_getName(sel);
    id cls;
    BOOL static_method = false;
    va_list ap;
    
    SPEW(NSLog (@"coffeekit_method_tramp (%@, %s)", obj, sel_name);)
    
    CKObject* jspeer = get_js_peer (obj, &cls, &static_method);
    
    NSString* sel_nsstr = [NSString stringWithUTF8String:sel_name];
    NSMutableDictionary *method_map = objc_getAssociatedObject (cls, JSMETHODMAP_KEY);
    if (method_map == NULL) {
        SPEW(NSLog (@"null method_map somehow made it to the method tramp...");)
        abort ();
    }
    
    MethodInfo* meth_info = [method_map objectForKey:sel_nsstr];
    NSString* name_nsstr = [meth_info name];
    NSMethodSignature* sig = [meth_info resolveSigWithClass:cls staticMethod:static_method];
    
    SPEW(NSLog (@"method_map[%@] = %@, looking up on jspeer %@", sel_nsstr, name_nsstr, jspeer);)
    
    CKObject* func = [jspeer objectForPropertyNS:name_nsstr];
    
    SPEW(NSLog (@"found it!");)
    
    if (!func || ![func isFunction]) {
        SPEW(NSLog (@"it's not a function, returning");)
        return NULL;
    }

#if notyet    
    int func_arity = [func functionArity];
    int jsarg_count = MIN(func_arity, [sig numberOfArguments] - 2);
#else
    int jsarg_count = [sig numberOfArguments] - 2;
#endif
    int jsarg_num;
    
    CKInvocation* inv = [CKInvocation invocationWithFunction:func argCount:jsarg_count thisObject:jspeer];

    [inv setThisObject:jspeer];
    [inv setFunction:func];
     
    va_start (ap, sel);
    
    for (jsarg_num = 0; jsarg_num < jsarg_count; jsarg_num++) {
        const char *arg_type = [sig getArgumentTypeAtIndex:jsarg_num + 2];
        switch (arg_type[0]) {
            case _C_ID: {
                id objc_id = va_arg (ap, id);
                [inv setArgument:marshal_id_as_jsvalue(objc_id, YES) atIndex:jsarg_num];
                break;
            }
            case _C_STRUCT_B: {
                if (!strncmp (&arg_type[1], "CGRect", strlen ("CGRect")) || !strncmp (&arg_type[1], "NSRect", strlen ("NSRect"))) {
                    CGRect rect = va_arg (ap, CGRect);
                    [inv setArgument:marshal_cgrect_as_jsvalue(&rect) atIndex:jsarg_num];
                }
                else {
                    NSString* str = [NSString stringWithFormat:@"unhandled tramp arg marshalling for struct type `%s'", arg_type];
                    _ejs_throw_nativeerror_utf8(EJS_ERROR, [str UTF8String]);
                }
                break;
            }
            case _C_INT: {
                int32_t i = va_arg(ap, int32_t);
                [inv setArgument:[CKValue numberValue:i] atIndex:jsarg_num];
                break;
            }
            case _C_UINT: {
                uint32_t i = va_arg(ap, uint32_t);
                [inv setArgument:[CKValue numberValue:i] atIndex:jsarg_num];
                break;
            }
            case _C_LNG_LNG: {
                int64_t i = va_arg(ap, int64_t);
                [inv setArgument:[CKValue numberValue:i] atIndex:jsarg_num];
                break;
            }
            case _C_ULNG_LNG: {
                uint64_t i = va_arg(ap, uint64_t);
                [inv setArgument:[CKValue numberValue:i] atIndex:jsarg_num];
                break;
            }
            default: {
                /*id objc_id =*/ va_arg (ap, id);

                NSLog (@"unhandled tramp arg marshaling for type %s", arg_type);
                [inv setArgument:[CKValue undefinedValue] atIndex:jsarg_num];
                break;
            }
        }
    }
    
    va_end(ap);
    
    CKValue* rv = [inv invoke];

    if (rv == NULL) {
      NSLog(@"WTF");
      NSLog(@"WTF");
      NSLog(@"WTF");
      NSLog(@"WTF");
      NSLog(@"WTF");
      NSLog(@"WTF");
      NSLog(@"WTF");
      NSLog(@"WTF");
        [NSException raise:@"Exception calling JS method" format:@"%@", [inv exception]];
    }
    
    // marshal the JS return value to objective-c
    const char *return_type = [sig methodReturnType];
    
    // bail out early (and ignore the JS return value) if the return type is void
    if (return_type[0] == _C_VOID) return NULL;
    
    switch (return_type[0]) {
        case _C_ID: {
            if ([rv isString]) return [[rv stringValue] nsString];
            return [rv isObject] ? get_objc_id([rv objectValue]) : NULL;
        }
        case _C_CHR: {
            return [rv isBool] ? (void*)(NSInteger)[rv boolValue] : (void*)(NSInteger)[rv numberValue];
        }
        case _C_INT:
        case _C_LNG:
        case _C_LNG_LNG: {
            return [rv isNumber] ? (void*)(NSInteger)[rv numberValue] : (void*)(NSInteger)0;
        }
        case _C_UINT:
        case _C_ULNG:
        case _C_ULNG_LNG: {
            return [rv isNumber] ? (void*)(NSUInteger)[rv numberValue] : (void*)(NSUInteger)0;
        }
        case _C_CLASS: {
            //    NSLog (@"_C_CLASS return value marshalling");

            CKObject* o = [rv objectValue];

            const char* register_cstr = [o utf8StringForPropertyNS:@"_ck_register"];
            
            if (!register_cstr) {
                NSLog (@"invalid or missing _ck_register on a object returned for where Class JS->objc marshalling was assumed");
                return NULL;
            }
            
            SPEW(NSLog (@"objective C class for return value is %s", register_cstr);)
            
            Class cls = objc_getClass (register_cstr);
            
            return cls;
        }
    }
    
    // XXX
    NSLog (@"XXX");
    abort();
    return nil;
}

static id
coffeekit_ctor_tramp (id obj, SEL sel, ...)
{
    SPEW(NSLog (@"coffeekit_ctor_tramp (%@, %s)", obj, sel_getName(sel));)

    Class cls = [obj class];

    CKObject* js_ctor = get_jsctor(cls);

    CKInvocation* inv = [CKInvocation invocationWithConstructor:js_ctor argCount:1];

	CKObject* handle = create_objc_handle_object(obj);
	CKValue* handle_val = [CKValue objectValue:handle];

    [inv setArgument:handle_val atIndex:0];
    
    CKValue* rv = [inv invoke];
    if (rv == NULL)
        [NSException raise:@"Exception creating new JS wrapper" format:@"%@", [inv exception]];
	if (![rv isObject])
        [NSException raise:@"Constructor returned something other than an object" format:@"%@", [inv exception]];
	  

    CKObject* peer = [rv objectValue];

	SPEW(NSLog (@"ctor created peer %@", peer);)

    set_jspeer (obj, peer);

    return obj;
}

static CKObject*
register_members (Class cls, CKObject* obj, NSMutableDictionary* method_map)
{
	CKObject* ctor = NULL;
    EJSObject* _obj = [obj jsObject];

    for (_EJSPropertyMapEntry* s = _obj->map->head_insert; s; s = s->next_insert) {
        if (_ejs_property_desc_has_getter(s->desc) || 
            _ejs_property_desc_has_setter(s->desc)) {
			if (_ejs_property_desc_has_getter(s->desc)) {

                char *utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(s->name));
                CKString* name = [CKString stringWithUTF8CString:utf8];

                CKObject *getter = [CKObject objectWithJSObject:EJSVAL_TO_OBJECT(s->desc->getter)];
                NSLog (@"there was a getter for %@", [name nsString]);
				CKValue* ck_ivar = [getter valueForPropertyNS:@"_ck_ivar"];

				if ([ck_ivar isString]) {
                    const char *ivar_name = [[ck_ivar stringValue] UTF8String];
                    NSLog (@" and there's an ivar named %s", ivar_name);
                    if (NO == class_addIvar (cls, ivar_name, sizeof(id), log2(sizeof(id)), "@")) {
                        NSLog (@"failed to add ivar");
                    }
                }
			}
		}
		else {
            char *utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(s->name));
            CKString* name = [CKString stringWithUTF8CString:utf8];
            free (utf8);
            NSString* name_nsstr = [name nsString];
        
			CKObject* propobj = [obj objectForProperty:name];
            
			if (!propobj || ![propobj isFunction]) {
                SPEW (NSLog (@"property %@ isn't a function, skipping for now", name_nsstr);)
				continue;
			}
            
			if (NSOrderedSame == [name_nsstr compare:@"constructor"]) {
				ctor = propobj;
                
				[method_map setObject:name_nsstr forKey:@"init"];
                
				class_addMethod (cls, sel_getUid ("init"), (IMP)coffeekit_ctor_tramp, "@@:");
			}
			else {
				if (!is_function_exported (propobj)) {
                    SPEW(NSLog (@"function %@ isn't exported, skipping", name_nsstr);)
					continue;
				}
                
				const char *sel_name = get_selector_name_from_function (propobj);
				const char *type_sig = get_type_sig_from_function (propobj);

				SEL sel = sel_getUid(sel_name);
                
				SPEW(NSLog (@"adding method %s/%s in class %s", sel_name, type_sig, class_getName(cls));)
                
				MethodInfo *info = [[MethodInfo alloc] initWithName:name_nsstr selector:sel typeSig:type_sig];
				[method_map setObject:info forKey:[NSString stringWithUTF8String:sel_name]];
				[info release];
                
				class_addMethod (cls, sel, (IMP)coffeekit_method_tramp, type_sig);
			}
		}
	}

	return ctor;
}

static Class
register_js_class (CKObject* proto,
                   const char *register_name,
                   const char *super_register_name)
{
	id cls = objc_getClass (register_name);
    
	if (cls) {
		// if the class was already registered (it's builtin),
		// just add the corresponding ctor
	        if (objc_getAssociatedObject (cls, MONKEYPATCHED_RETAIN_RELEASE) == NULL) {
//                      Method retain = class_getInstanceMethod (cls, sel_getUid ("retain"));
//                      Method release = class_getInstanceMethod (cls, sel_getUid ("release"));
            
			objc_setAssociatedObject (cls, MONKEYPATCHED_RETAIN_RELEASE, (id)1, OBJC_ASSOCIATION_ASSIGN);
            
//                      class_replaceMethod(cls, sel_getUid("retainCount"), (IMP)monkey_retainCount, "I@:");
//                      class_replaceMethod(cls, sel_getUid("retain"), (IMP)monkey_retain, "@@:");
//                      class_replaceMethod(cls, sel_getUid("release"), (IMP)monkey_release, "v@:");
		}
        
		if (get_jsctor(cls) == NULL) {
			CKObject* propobj = [proto objectForPropertyNS:@"constructor"];
			if (!propobj || ![propobj isFunction]) {
				NSLog (@"constructor is missing or is not a function");
				return cls;
			}
        
			// 	  NSLog (@"storing ctor in builtin class");
			set_jsctor(cls, propobj);
		}
		return cls;
	}
    
	cls = objc_getProtocol(register_name);
	if (cls) {
		NSLog (@"%s is not a class, it's a protocol", register_name);
		// XXX we really need to throw an exception here...
		return cls;
	}

	SPEW(NSLog (@"register_name = %s, super_name = %s", register_name, super_register_name);)

	id sup = objc_lookUpClass (super_register_name);
    
	cls = objc_allocateClassPair (sup, register_name, 0);
    
	Class metaclass = object_getClass (cls);
    
	NSMutableDictionary* method_map = [NSMutableDictionary dictionary];
    

    SPEW(NSLog (@"registering instance members");)
	// register the instance methods and properties
	CKObject* ctor = register_members (cls, proto, method_map);
    
    SPEW(NSLog (@"registering static members");)
	// register the static methods and properties
	if (ctor)
		register_members (metaclass, ctor, method_map);

	objc_registerClassPair (cls);

#if notyet    
	objc_setAssociatedObject (cls, JSCTX_KEY, ctx, OBJC_ASSOCIATION_RETAIN);
#endif
	if (ctor)
		set_jsctor(cls, ctor);
    
	objc_setAssociatedObject (cls, JSMETHODMAP_KEY, method_map, OBJC_ASSOCIATION_RETAIN);
    
	/*
	  class_addMethod(cls, sel_getUid("retain"), (IMP)monkey_retain, "@@:");
	  class_addMethod(cls, sel_getUid("release"), (IMP)monkey_release, "v@:");
	*/

	SPEW(NSLog (@"associated ctor %p with class %s/%p", ctor, register_name, cls);)
    
	return cls;
}

static EJS_NATIVE_FUNC(_ejs_objc_registerJSClass) {
    // unused ejsval ctor = args[0]
    CKObject* proto = [[CKValue valueWithJSValue:args[1]] objectValue];
    char *register_cstr = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[2]));
    char *super_register_cstr = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[3]));

    register_js_class (proto, register_cstr, super_register_cstr);

    free (register_cstr);
    free (super_register_cstr);

    return _ejs_undefined;
}

#if IOS
static EJS_NATIVE_FUNC(_ejs_objc_UIApplicationMain) {
    NSString* delegate_name = [[CKString stringWithJSString:EJSVAL_TO_STRING(args[2])] nsString];
    
    NSLog (@"About to call UIApplicationMain (..., %@)!", delegate_name);
    UIApplicationMain(0, NULL, nil, delegate_name); // XXX get argv/argc/principal from @args
    exit(0);
}

#else
static EJS_NATIVE_FUNC(_ejs_objc_NSApplicationMain) {
    NSLog (@"About to call NSApplicationMain!");
    NSApplicationMain(0, NULL); // XXX populate from args
    exit(0);
}
#endif

ejsval
_ejs_objc_module_func (ejsval exports)
{
    EJS_INSTALL_FUNCTION(exports, "requireFramework", _ejs_objc_requireFramework);
    EJS_INSTALL_FUNCTION(exports, "allocInstance", _ejs_objc_allocInstance);
    EJS_INSTALL_FUNCTION(exports, "staticCall", _ejs_objc_staticCall);
    EJS_INSTALL_FUNCTION(exports, "getInstanceVariable", _ejs_objc_getInstanceVariable);
    EJS_INSTALL_FUNCTION(exports, "setInstanceVariable", _ejs_objc_setInstanceVariable);
    EJS_INSTALL_FUNCTION(exports, "selectorInvoker", _ejs_objc_selectorInvoker);
    EJS_INSTALL_FUNCTION(exports, "getTypeEncoding", _ejs_objc_getTypeEncoding);
    EJS_INSTALL_FUNCTION(exports, "registerJSClass", _ejs_objc_registerJSClass);
#if IOS
    EJS_INSTALL_FUNCTION(exports, "allocateWebGLRenderingContext", _ejs_objc_allocateWebGLRenderingContext);
    EJS_INSTALL_FUNCTION(exports, "UIApplicationMain", _ejs_objc_UIApplicationMain);
#else
    EJS_INSTALL_FUNCTION(exports, "NSApplicationMain", _ejs_objc_NSApplicationMain);
#endif

    _ejs_objc_init (exports);

    return _ejs_undefined;
}

