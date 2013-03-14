/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#import "Foundation/Foundation.h"

#include "ejs.h"
#include "ejs-string.h"
#include "ejs-function.h"

@class CKObject;
@class CKValue;

@interface CKString : NSObject {
  EJSPrimString*  _jsstr;
  NSString*  _nsstr;
}

-(CKString*)initWithJSString:(EJSPrimString*)str;
-(CKString*)initWithNSString:(NSString*)str;
-(CKString*)initWithUTF8CString:(const char*)str;

+(CKString*)stringWithJSString:(EJSPrimString*)str;
+(CKString*)stringWithNSString:(NSString*)str;
+(CKString*)stringWithUTF8CString:(const char*)str;

-(void)dealloc;

-(EJSPrimString*)jsString;
-(NSString*)nsString;
-(const char*)UTF8String;

-(NSString*)description;

@end


@interface CKValue : NSObject {
    ejsval _val;
}


-(CKValue*)initWithJSValue:(ejsval)val;

-(void)dealloc;

-(BOOL)isNull;
-(BOOL)isBool;
-(BOOL)isNumber;
-(BOOL)isObject;
-(BOOL)isString;
-(BOOL)isUndefined;
-(BOOL)isFunction;


-(CKObject*)objectValue;
-(double)numberValue;
-(BOOL)boolValue;
-(CKString*)stringValue;
-(NSString*)nsStringValue;
-(const char*)utf8StringValue;

+(CKValue*)undefinedValue;
+(CKValue*)nullValue;
+(CKValue*)numberValue:(double)num;
+(CKValue*)objectValue:(CKObject*)obj;

+(CKValue*)jsStringValue:(CKString*)str;
+(CKValue*)nsStringValue:(NSString*)str;
+(CKValue*)utf8StringValue:(const char*)str;

+(CKValue*)valueWithJSValue:(ejsval)val;

-(ejsval)jsValue;

-(NSString*)description;

@end

@interface CKPropertyNameArray : NSObject {
  CKString** _names;
  uint32_t _count;
}


-(CKPropertyNameArray*)initWithObjectProperties:(CKObject*)obj;
+(CKPropertyNameArray*)arrayWithObjectProperties:(CKObject*)obj;

-(uint32_t)count;
-(CKString*)nameAtIndex:(uint32_t)index;
-(void)dealloc;

@end

@interface CKObject : NSObject {
  EJSObject* _obj;
}


-(CKObject*)initWithJSObject:(EJSObject*)obj;

-(void)wrap;

-(void)dealloc;

#if spidermonkey
-(void)defineFunctions:(JSFunctionSpec*)functions;
-(void)defineProperties:(JSPropertySpec*)properties;

-(void)defineClass:(NSString*)name withClass:(JSClass*)classp constructor:(EJSClosureFunc)ctor ctorArgCount:(uint32_t)nargs properties:(JSPropertySpec*)properties functions:(JSFunctionSpec*)functions staticProperties:(JSPropertySpec*)staticProperties staticFunctions:(JSFunctionSpec*)staticFunctions;
#endif

+(CKObject*)objectWithJSObject:(EJSObject*)obj;

+(CKObject*)makeFunction:(CKString*)name withCallback:(EJSClosureFunc)callback argCount:(uint32_t)nargs;
+(CKObject*)makeFunctionNS:(NSString*)name withCallback:(EJSClosureFunc)callback argCount:(uint32_t)nargs;

-(CKValue*)valueForProperty:(CKString*)name;
-(CKValue*)valueForPropertyNS:(NSString*)name;

-(CKObject*)objectForProperty:(CKString*)name;
-(CKObject*)objectForPropertyNS:(NSString*)name;

-(CKString*)jsStringForProperty:(CKString*)name;
-(CKString*)jsStringForPropertyNS:(NSString*)name;

-(NSString*)nsStringForProperty:(CKString*)name;
-(NSString*)nsStringForPropertyNS:(NSString*)name;

-(const char*)utf8StringForProperty:(CKString*)name;
-(const char*)utf8StringForPropertyNS:(NSString*)name;

-(int)intForProperty:(CKString*)name;
-(int)intForPropertyNS:(NSString*)name;

-(float)floatForProperty:(CKString*)name;
-(float)floatForPropertyNS:(NSString*)name;

-(CKPropertyNameArray*)propertyNames;

-(void)defineProperty:(CKString*)name value:(CKValue*)val attributes:(uint32_t)attrs;
-(void)definePropertyNS:(NSString*)name value:(CKValue*)val attributes:(uint32_t)attrs;

-(void)defineProperty:(CKString*)name object:(CKObject*)obj attributes:(uint32_t)attrs;
-(void)definePropertyNS:(NSString*)name object:(CKObject*)obj attributes:(uint32_t)attrs;


-(BOOL)isFunction;
-(BOOL)isConstructor;
-(BOOL)isArray;

-(jsuint)arrayLength;
-(uint16_t)functionArity;;

-(CKObject*)prototype;

-(void*)privateData;
-(void)setPrivateData:(void*)data;

-(EJSObject*)jsObject;

-(void)toggleJSRefToStrong;
-(void)toggleJSRefToWeak;

@end

@interface CKInvocation : NSObject {
  CKObject* _thisObj;
  CKObject* _func;
  NSUInteger _argCount;
  CKValue** _args;
  CKValue* _exc;
  BOOL isFuncCtor;
}


-(CKInvocation*)initWithFunction:(CKObject*)func argCount:(NSUInteger)argCount thisObject:(CKObject*)obj;
-(CKInvocation*)initWithConstructor:(CKObject*)ctor argCount:(NSUInteger)argCount;

+(CKInvocation*)invocationWithFunction:(CKObject*)func argCount:(NSUInteger)argCount thisObject:(CKObject*)obj;
+(CKInvocation*)invocationWithConstructor:(CKObject*)ctor argCount:(NSUInteger)argCount;

-(void)dealloc;


-(void)setThisObject:(CKObject*)obj;
-(void)setFunction:(CKObject*)func;
-(void)setConstructor:(CKObject*)ctor;

-(void)setArgument:(CKValue*)arg atIndex:(NSUInteger)index;

-(CKValue*)invoke NS_RETURNS_NOT_RETAINED;
-(CKValue*)exception NS_RETURNS_NOT_RETAINED;

@end
