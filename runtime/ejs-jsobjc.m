/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-ops.h"
#import "ejs-jsobjc.h"
#include "ejs-objc.h"
#include "ejs-object.h"
#include "ejs-array.h"
#include "ejs-string.h"

#define SPEW(x)

@implementation EJSObjcString

-(id)initWithJSString:(EJSPrimString*)str
{
    self = [super init];
    _jsstr = str;
#if spidermonkey
    JS_AddStringRoot ([_ctx jsContext], &_jsstr);
#endif
    _nsstr = nil;
    return self;
}

-(id)initWithNSString:(NSString*)str
{
    self = [super init];
    _jsstr = NULL;
    _nsstr = [str retain]; // XXX should we copy this?
    return self;
}

-(id)initWithUTF8CString:(const char*)str
{
    self = [super init];
    _jsstr = NULL;
    _nsstr = [[NSString alloc] initWithUTF8String:str];
    return self;
}

+(id)stringWithJSString:(EJSPrimString*)str
{
	return [[[EJSObjcString alloc] initWithJSString:str] autorelease];
}

+(id)stringWithNSString:(NSString*)str
{
	return [[[EJSObjcString alloc] initWithNSString:str] autorelease];
}

+(id)stringWithUTF8CString:(const char*)str
{
	return [[[EJSObjcString alloc] initWithUTF8CString:str] autorelease];
}

-(void)dealloc
{
	SPEW(NSLog(@"deallocating %@", self);)

#if spidermonkey
	if (_jsstr)
		JS_RemoveStringRoot ([_ctx jsContext], &_jsstr);
#endif
	[_nsstr release];    
	[super dealloc];
}

-(EJSPrimString*)jsString
{
	if (!_jsstr) {
		unichar* buf = (jschar*)malloc([_nsstr length] * sizeof(unichar));

		[_nsstr getCharacters:buf range:NSMakeRange(0, [_nsstr length])];

		_jsstr = EJSVAL_TO_STRING(_ejs_string_new_ucs2_len ((jschar*)buf, [_nsstr length]));

#if spidermonkey
		JS_AddStringRoot ([_ctx jsContext], &_jsstr);
#endif
        free (buf);
	}

	return _jsstr;
}

-(NSString*)nsString
{
	if (!_nsstr) {
		const jschar *chars;
		size_t length = _jsstr->length;
        chars = _ejs_primstring_flatten(_jsstr)->data.flat;
		_nsstr = [[NSString alloc] initWithCharacters:(unichar*)chars length:length];
	}
    
	return _nsstr;
}

-(const char*)UTF8String
{
	if (!_nsstr) {
		const jschar *chars;
		size_t length = _jsstr->length;
        chars = _ejs_primstring_flatten(_jsstr)->data.flat;
		_nsstr = [[NSString alloc] initWithCharacters:(unichar*)chars length:length];
	}
    
    return [_nsstr UTF8String];
}

-(NSString*)description
{
    return [NSString stringWithFormat:@"<EJSObjcString(%@)>", [self nsString]];
}

@end



@implementation EJSObjcValue

-(id)initWithJSValue:(ejsval)val
{
    self = [super init];

    _val = val;
#if spidermonkey
    JS_AddValueRoot ([_ctx jsContext], &_val);
#endif
    return self;
}

-(void)dealloc
{
    SPEW(NSLog(@"deallocating %@", self);)

#if spidermonkey
    JS_RemoveValueRoot ([_ctx jsContext], &_val);
#endif

    [super dealloc];    
}

-(BOOL)isNull
{
	return EJSVAL_IS_NULL(_val);
}

-(BOOL)isBool
{
	return EJSVAL_IS_BOOLEAN(_val);
}

-(BOOL)isNumber
{
	return EJSVAL_IS_NUMBER(_val);
}

-(BOOL)isObject
{
	return EJSVAL_IS_OBJECT(_val);
}

-(BOOL)isString
{
	return EJSVAL_IS_STRING(_val);
}

-(BOOL)isUndefined
{
	return EJSVAL_IS_UNDEFINED(_val);
}

-(BOOL)isFunction
{
    return EJSVAL_IS_FUNCTION(_val);
}

-(id)objectValue
{
	return [EJSObjcObject objectWithJSObject:EJSVAL_TO_OBJECT(_val)];
}

-(double)numberValue
{
	return ToDouble (_val);
}

-(EJSObjcString*)stringValue
{
    return [EJSObjcString stringWithJSString:EJSVAL_TO_STRING(ToString(_val))];
}

-(NSString*)nsStringValue
{
	return [[self stringValue] nsString];
}

-(const char*)utf8StringValue
{
	return [[self stringValue] UTF8String];
}


-(BOOL)boolValue
{
	return EJSVAL_TO_BOOLEAN (_val);
}

+(id)undefinedValue
{
	return [[[EJSObjcValue alloc] initWithJSValue:_ejs_undefined] autorelease];
}

+(id)nullValue
{
	return [[[EJSObjcValue alloc] initWithJSValue:_ejs_null] autorelease];
}

+(id)numberValue:(double)num
{
	return [[[EJSObjcValue alloc] initWithJSValue:NUMBER_TO_EJSVAL(num)] autorelease];
}

+(id)objectValue:(EJSObjcObject*)obj
{
	return [[[EJSObjcValue alloc] initWithJSValue:(obj == NULL ? _ejs_null : OBJECT_TO_EJSVAL([obj jsObject]))] autorelease];    
}

+(id)jsStringValue:(EJSObjcString*)str
{
	return [[[EJSObjcValue alloc] initWithJSValue:STRING_TO_EJSVAL([str jsString])] autorelease];
}

+(id)nsStringValue:(NSString*)str
{
	return [[[EJSObjcValue alloc] initWithJSValue:STRING_TO_EJSVAL([[EJSObjcString stringWithNSString:str] jsString])] autorelease];
}

+(id)utf8StringValue:(const char*)str
{
	return [[[EJSObjcValue alloc] initWithJSValue:STRING_TO_EJSVAL([[EJSObjcString stringWithUTF8CString:str] jsString])] autorelease];
}

+(id)valueWithJSValue:(ejsval)val
{
	return [[[EJSObjcValue alloc] initWithJSValue:val] autorelease];
}




-(ejsval)jsValue
{
	return _val;
}

-(NSString*)description
{
	return [NSString stringWithFormat:@"<EJSObjcValue val=\"%@\">", [self nsStringValue]];
}

@end


@implementation EJSObjcObject

-(id)initWithJSObject:(EJSObject*)obj
{
	if (obj == NULL) {
		NSLog (@"null object");
        abort();
    }
    
	self = [super init];
	_obj = obj;
#if spidermonkey
	JS_AddObjectRoot ([_ctx jsContext], &_obj);
#endif
	return self;
}

-(void)wrap
{
#if spidermonkey
	if (!JS_WrapObject ([_ctx jsContext], &_obj))
		abort();
#endif
}

#if spidermonkey
-(void)defineClass:(NSString*)name withClass:(JSClass*)classp constructor:(JSNative)ctor ctorArgCount:(uintN)nargs properties:(JSPropertySpec*)properties functions:(JSFunctionSpec*)functions staticProperties:(JSPropertySpec*)staticProperties staticFunctions:(JSFunctionSpec*)staticFunctions
{
	JS_InitClass ([_ctx jsContext], _obj, NULL,
		      classp, ctor, nargs,
		      properties, functions,
		      staticProperties, staticFunctions);
}

-(void)defineFunctions:(JSFunctionSpec*)functions
{
	JS_DefineFunctions ([_ctx jsContext], _obj, functions);
}

-(void)defineProperties:(JSPropertySpec*)properties
{
	JS_DefineProperties ([_ctx jsContext], _obj, properties);
}
#endif

-(void)dealloc
{
	SPEW(NSLog(@"deallocating %@", self);)

#if notyet
	JS_RemoveObjectRoot ([_ctx jsContext], &_obj);
#endif
	[super dealloc];
}

+(id)objectWithJSObject:(EJSObject*)obj
{
	return [[[EJSObjcObject alloc] initWithJSObject:obj] autorelease];
}

+(EJSObjcObject*)makeFunction:(EJSObjcString*)name withCallback:(EJSClosureFunc)callback argCount:(uint32_t)nargs
{
	return [self makeFunctionNS:[name nsString] withCallback:callback argCount:nargs];
}

+(EJSObjcObject*)makeFunctionNS:(NSString*)name withCallback:(EJSClosureFunc)callback argCount:(uint32_t)nargs
{
	return [[[EJSObjcObject alloc] initWithJSObject:EJSVAL_TO_OBJECT(_ejs_function_new_utf8 (_ejs_null, [name UTF8String], callback))] autorelease];
}

-(EJSObjcPropertyNameArray*)propertyNames
{
	return [EJSObjcPropertyNameArray arrayWithObjectProperties:self];
}

-(EJSObjcValue*)valueForProperty:(EJSObjcString*)name
{
    ejsval objval = OBJECT_TO_EJSVAL(_obj);
	return [EJSObjcValue valueWithJSValue:_ejs_object_getprop_utf8 (objval, [name UTF8String])];
}

-(EJSObjcValue*)valueForPropertyNS:(NSString*)name
{
    ejsval objval = OBJECT_TO_EJSVAL(_obj);
    return [EJSObjcValue valueWithJSValue:_ejs_object_getprop_utf8(objval, [name UTF8String])];
}

-(EJSObjcObject*)objectForProperty:(EJSObjcString*)name
{
	EJSObjcValue* val = [self valueForProperty:name];
	if (![val isObject])
		return nil;

	return [val objectValue];
}
-(EJSObjcObject*)objectForPropertyNS:(NSString*)name
{
	EJSObjcValue* val = [self valueForPropertyNS:name];
	if (![val isObject])
		return nil;
    
	return [val objectValue];
}

-(EJSObjcString*)jsStringForProperty:(EJSObjcString*)name
{
	EJSObjcValue* val = [self valueForProperty:name];
	if (![val isString])
		return nil;

	return [val stringValue];
}
-(EJSObjcString*)jsStringForPropertyNS:(NSString*)name
{
	EJSObjcValue* val = [self valueForPropertyNS:name];
	if (![val isString])
		return nil;
    
	return [val stringValue];
}

-(NSString*)nsStringForProperty:(EJSObjcString*)name
{
	EJSObjcValue* val = [self valueForProperty:name];
	if (![val isString])
		return nil;
    
	return [[val stringValue] nsString];
}
-(NSString*)nsStringForPropertyNS:(NSString*)name
{
	EJSObjcValue* val = [self valueForPropertyNS:name];
	if (![val isString])
		return nil;
    
	return [[val stringValue] nsString];
}

-(const char*)utf8StringForProperty:(EJSObjcString*)name
{
	EJSObjcValue* val = [self valueForProperty:name];
	if (![val isString])
		return NULL;
    
	return [[val stringValue] UTF8String];
}
-(const char*)utf8StringForPropertyNS:(NSString*)name
{
	EJSObjcValue* val = [self valueForPropertyNS:name];
	if (![val isString])
		return NULL;

	return [[val stringValue] UTF8String];
}

-(int)intForProperty:(EJSObjcString*)name
{
	EJSObjcValue* val = [self valueForProperty:name];
	return [val isNumber] ? (int)[val numberValue] : 0;
}
-(int)intForPropertyNS:(NSString*)name
{
	EJSObjcValue* val = [self valueForPropertyNS:name];
	return [val isNumber] ? (int)[val numberValue] : 0;
}

-(float)floatForProperty:(EJSObjcString*)name
{
	EJSObjcValue* val = [self valueForProperty:name];
	return [val isNumber] ? (float)[val numberValue] : 0.0f;
}
-(float)floatForPropertyNS:(NSString*)name
{
	EJSObjcValue* val = [self valueForPropertyNS:name];
	return [val isNumber] ? (float)[val numberValue] : 0.0f;
}


-(void)defineProperty:(EJSObjcString*)name value:(EJSObjcValue*)val attributes:(uint32_t)attrs
{
    _ejs_object_define_value_property (OBJECT_TO_EJSVAL(_obj), STRING_TO_EJSVAL([name jsString]), [val jsValue], attrs);
}

-(void)defineProperty:(EJSObjcString*)name object:(EJSObjcObject*)obj attributes:(uint32_t)attrs
{
    _ejs_object_define_value_property (OBJECT_TO_EJSVAL(_obj), STRING_TO_EJSVAL([name jsString]), OBJECT_TO_EJSVAL([obj jsObject]), attrs);
}

-(void)definePropertyNS:(NSString*)name value:(EJSObjcValue*)val attributes:(uint32_t)attrs
{
    _ejs_object_define_value_property (OBJECT_TO_EJSVAL(_obj), STRING_TO_EJSVAL([[EJSObjcString stringWithNSString:name] jsString]), [val jsValue], attrs);
}

-(void)definePropertyNS:(NSString*)name object:(EJSObjcObject*)obj attributes:(uint32_t)attrs
{
    _ejs_object_define_value_property (OBJECT_TO_EJSVAL(_obj), STRING_TO_EJSVAL([[EJSObjcString stringWithNSString:name] jsString]), OBJECT_TO_EJSVAL([obj jsObject]), attrs);
}


-(BOOL)isFunction
{
    return _obj->ops == &_ejs_Function_specops;
}

-(BOOL)isConstructor
{
	if (![self isFunction])
		return NO;

    EJS_NOT_IMPLEMENTED();
#if notnet
	// XXX this relies on spidermonkey's internals that JSFunction == JSObject..
	JSFunction *fun = (JSFunction*)_obj;
	return (JS_GetFunctionFlags(fun) & JSFUN_CONSTRUCTOR) != 0;
#endif
}

-(BOOL)isArray
{
    return _obj->ops == &_ejs_Array_specops || _obj->ops == &_ejs_sparsearray_specops;
}

-(jsuint)arrayLength
{
    return ((EJSArray*)_obj)->array_length;
}

-(uint16_t)functionArity
{
    EJS_NOT_IMPLEMENTED();
#if notyet
	return JS_GetFunctionArity((JSFunction*)_obj);
#endif
}

-(EJSObjcObject*)prototype
{
    ejsval p = _ejs_object_getprop (OBJECT_TO_EJSVAL(_obj), _ejs_atom_prototype);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(p))
        return nil;

    return [EJSObjcObject objectWithJSObject:EJSVAL_TO_OBJECT(p)];
}

-(void*)privateData
{
    EJS_NOT_IMPLEMENTED();
#if spidermonkey
	return JS_GetPrivate ([_ctx jsContext], _obj);
#endif
}
-(void)setPrivateData:(void*)data
{
    EJS_NOT_IMPLEMENTED();
#if spidermonkey
	JS_SetPrivate ([_ctx jsContext], _obj, data);
#endif
}

-(EJSObject*)jsObject
{
	return _obj;
}

-(void)toggleJSRefToStrong
{
    EJS_NOT_IMPLEMENTED();
#if notyet
	JS_AddObjectRoot ([_ctx jsContext], &_obj);
#endif
}
                    
-(void)toggleJSRefToWeak
{
    EJS_NOT_IMPLEMENTED();
#if notyet
  	JS_RemoveObjectRoot ([_ctx jsContext], &_obj);
#endif
}

-(NSString*)description
{
    EJSPrimString* jsstr = EJSVAL_TO_STRING(ToString(OBJECT_TO_EJSVAL(_obj)));
	NSString *valstr = [[EJSObjcString stringWithJSString:jsstr] nsString];
	return [NSString stringWithFormat:@"<EJSObjcObject \"%@\">", valstr];
}

@end

@implementation EJSObjcPropertyNameArray

-(EJSObjcPropertyNameArray*)initWithObjectProperties:(EJSObjcObject*)obj
{
	self = [super init];

    EJSObject* _obj = [obj jsObject];
    EJSPropertyMap *map = _obj->map;
    _count = map->inuse;
    if (_count == 0) {
        _names = NULL;
    }
    else {
        _names = (EJSObjcString**)malloc(_count * sizeof(EJSObjcString*));
        uint32_t i = 0;
        for (_EJSPropertyMapEntry* s = map->head_insert; s; s = s->next_insert) {
            ejsval name = s->name;
            char* utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(name));

            _names[i++] = [[EJSObjcString alloc] initWithUTF8CString:utf8];
            free (utf8);
        }
    }

    return self;
}

+(EJSObjcPropertyNameArray*)arrayWithObjectProperties:(EJSObjcObject*)obj
{
	return [[[EJSObjcPropertyNameArray alloc] initWithObjectProperties:obj] autorelease];
}

-(void)dealloc
{
	if (_count > 0) {
		for (uint32_t i = 0; i < _count; i ++) {
			[_names[i] release];
		}
		free (_names);
	}
    [super dealloc];
}

-(uint32_t)count
{
	return _count;
}

-(EJSObjcString*)nameAtIndex:(uint32_t)index
{
	return _names[index]; // XXX we should clone+autorelease this...
}
@end

@implementation EJSObjcInvocation

-(EJSObjcInvocation*)initWithFunction:(EJSObjcObject*)func argCount:(NSUInteger)argCount thisObject:(EJSObjcObject*)obj
{
	self = [super init];
	_func = [func retain];
	_thisObj = [obj retain];
	isFuncCtor = NO;
	_argCount = argCount;
	_args = _argCount == 0 ? NULL : (EJSObjcValue**)calloc(_argCount, sizeof(EJSObjcValue*));
	return self;
}

-(EJSObjcInvocation*)initWithConstructor:(EJSObjcObject*)ctor argCount:(NSUInteger)argCount
{
	self = [super init];
	_func = [ctor retain];
	_thisObj = nil;
	isFuncCtor = YES;
	_argCount = argCount;
	_args = _argCount == 0 ? NULL : (EJSObjcValue**)calloc(_argCount, sizeof(EJSObjcValue*));
	return self;    
}

+(EJSObjcInvocation*)invocationWithFunction:(EJSObjcObject*)func argCount:(NSUInteger)argCount thisObject:(EJSObjcObject*)obj
{
	return [[[EJSObjcInvocation alloc] initWithFunction:func argCount:argCount thisObject:obj] autorelease];
}

+(EJSObjcInvocation*)invocationWithConstructor:(EJSObjcObject*)ctor argCount:(NSUInteger)argCount
{
	return [[[EJSObjcInvocation alloc] initWithConstructor:ctor argCount:argCount] autorelease];    
}

-(void)dealloc
{
	SPEW(NSLog(@"deallocating %@", self);)

	[_thisObj release];
	[_func release];
	[_exc release];
	if (_args) {
		int i;
		for (i = 0; i < _argCount; i ++)
			[_args[i] release];
		free (_args);
	}
    [super dealloc];
}


-(void)setThisObject:(EJSObjcObject*)obj
{
	if (_thisObj == obj)
		return;
    
	[_thisObj release];
	_thisObj = [obj retain];
}

-(void)setFunction:(EJSObjcObject*)func
{
	isFuncCtor = NO;
	if (_func == func)
		return;
    
	[_func release];
	_func = [func retain];    
}

-(void)setConstructor:(EJSObjcObject*)ctor
{
	isFuncCtor = YES;
	if (_func == ctor)
		return;
    
	[_func release];
	_func = [ctor retain];    
}

-(void)setArgument:(EJSObjcValue*)arg atIndex:(NSUInteger)index
{
	if (index > _argCount) {
		NSLog(@"too many arguments..");
		// XXX this should throw.
		abort();
	}

	if (_args[index] == arg)
		return;
    
	[_args[index] release];
	_args[index] = [arg retain];
}

-(EJSObjcValue*)invoke
{
	ejsval rv = _ejs_undefined;
	ejsval* jsargs = _argCount == 0 ? NULL : (ejsval*)malloc(_argCount * sizeof(ejsval));
	NSUInteger i;

	for (i = 0; i < _argCount; i ++) {
		jsargs[i] = [_args[i] jsValue];
#if spidermonkey
		JS_AddValueRoot([_ctx jsContext], &jsargs[i]);
#endif
	}

	if (isFuncCtor) {
        ejsval o = _ejs_object_create (OBJECT_TO_EJSVAL([[_func prototype] jsObject]));
        _ejs_invoke_closure (OBJECT_TO_EJSVAL([_func jsObject]), &o, _argCount, jsargs, _ejs_undefined);
        rv = o;
	}
	else {
        ejsval thisArg = [[EJSObjcValue objectValue:_thisObj] jsValue];
        rv = _ejs_invoke_closure ([[EJSObjcValue objectValue:_func] jsValue],
                                  &thisArg,
                                  _argCount, jsargs, _ejs_undefined);
	}

	if (jsargs) {
#if notyet
		for (i = 0; i < _argCount; i ++)
			JS_RemoveValueRoot([_ctx jsContext], &jsargs[i]);
#endif
		free (jsargs);
	}
#if spidermonkey
	if (!call_succeeded) {
		jsval jsexc;

		if (!JS_GetPendingException ([_ctx jsContext], &jsexc)) {
			NSLog (@"there was no pending exception?  wtf...");
			abort(); // XXX what do we do here?
		}

		NSString *exc_nsstr = jsvalue_to_nsstr ([_ctx jsContext], jsexc);
		NSLog (@"there was an exception calling the JS %s:\n%@", isFuncCtor ? "constructor" : "function", exc_nsstr);
		_exc = [[EJSObjcValue valueWithJSValue:jsexc context:_ctx] retain];
		return NULL;
	}
#endif

	return [EJSObjcValue valueWithJSValue:rv];
}

-(EJSObjcValue*)exception
{
	return _exc;
}
@end
