/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#import "Foundation/Foundation.h"

#include "ejs.h"
#include "ejs-string.h"
#include "ejs-function.h"

@class EJSObjcObject;
@class EJSObjcValue;

@interface EJSObjcString : NSObject {
  EJSPrimString*  _jsstr;
  NSString*  _nsstr;
}

-(EJSObjcString*)initWithJSString:(EJSPrimString*)str;
-(EJSObjcString*)initWithNSString:(NSString*)str;
-(EJSObjcString*)initWithUTF8CString:(const char*)str;

+(EJSObjcString*)stringWithJSString:(EJSPrimString*)str;
+(EJSObjcString*)stringWithNSString:(NSString*)str;
+(EJSObjcString*)stringWithUTF8CString:(const char*)str;

-(void)dealloc;

-(EJSPrimString*)jsString;
-(NSString*)nsString;
-(const char*)UTF8String;

-(NSString*)description;

@end


@interface EJSObjcValue : NSObject {
    ejsval _val;
}


-(EJSObjcValue*)initWithJSValue:(ejsval)val;

-(void)dealloc;

-(BOOL)isNull;
-(BOOL)isBool;
-(BOOL)isNumber;
-(BOOL)isObject;
-(BOOL)isString;
-(BOOL)isUndefined;
-(BOOL)isFunction;


-(EJSObjcObject*)objectValue;
-(double)numberValue;
-(BOOL)boolValue;
-(EJSObjcString*)stringValue;
-(NSString*)nsStringValue;
-(const char*)utf8StringValue;

+(EJSObjcValue*)undefinedValue;
+(EJSObjcValue*)nullValue;
+(EJSObjcValue*)numberValue:(double)num;
+(EJSObjcValue*)objectValue:(EJSObjcObject*)obj;

+(EJSObjcValue*)jsStringValue:(EJSObjcString*)str;
+(EJSObjcValue*)nsStringValue:(NSString*)str;
+(EJSObjcValue*)utf8StringValue:(const char*)str;

+(EJSObjcValue*)valueWithJSValue:(ejsval)val;

-(ejsval)jsValue;

-(NSString*)description;

@end

@interface EJSObjcPropertyNameArray : NSObject {
  EJSObjcString** _names;
  uint32_t _count;
}


-(EJSObjcPropertyNameArray*)initWithObjectProperties:(EJSObjcObject*)obj;
+(EJSObjcPropertyNameArray*)arrayWithObjectProperties:(EJSObjcObject*)obj;

-(uint32_t)count;
-(EJSObjcString*)nameAtIndex:(uint32_t)index;
-(void)dealloc;

@end

@interface EJSObjcObject : NSObject {
  EJSObject* _obj;
}


-(EJSObjcObject*)initWithJSObject:(EJSObject*)obj;

-(void)wrap;

-(void)dealloc;

#if spidermonkey
-(void)defineFunctions:(JSFunctionSpec*)functions;
-(void)defineProperties:(JSPropertySpec*)properties;

-(void)defineClass:(NSString*)name withClass:(JSClass*)classp constructor:(EJSClosureFunc)ctor ctorArgCount:(uint32_t)nargs properties:(JSPropertySpec*)properties functions:(JSFunctionSpec*)functions staticProperties:(JSPropertySpec*)staticProperties staticFunctions:(JSFunctionSpec*)staticFunctions;
#endif

+(EJSObjcObject*)objectWithJSObject:(EJSObject*)obj;

+(EJSObjcObject*)makeFunction:(EJSObjcString*)name withCallback:(EJSClosureFunc)callback argCount:(uint32_t)nargs;
+(EJSObjcObject*)makeFunctionNS:(NSString*)name withCallback:(EJSClosureFunc)callback argCount:(uint32_t)nargs;

-(EJSObjcValue*)valueForProperty:(EJSObjcString*)name;
-(EJSObjcValue*)valueForPropertyNS:(NSString*)name;

-(EJSObjcObject*)objectForProperty:(EJSObjcString*)name;
-(EJSObjcObject*)objectForPropertyNS:(NSString*)name;

-(EJSObjcString*)jsStringForProperty:(EJSObjcString*)name;
-(EJSObjcString*)jsStringForPropertyNS:(NSString*)name;

-(NSString*)nsStringForProperty:(EJSObjcString*)name;
-(NSString*)nsStringForPropertyNS:(NSString*)name;

-(const char*)utf8StringForProperty:(EJSObjcString*)name;
-(const char*)utf8StringForPropertyNS:(NSString*)name;

-(int)intForProperty:(EJSObjcString*)name;
-(int)intForPropertyNS:(NSString*)name;

-(float)floatForProperty:(EJSObjcString*)name;
-(float)floatForPropertyNS:(NSString*)name;

-(EJSObjcPropertyNameArray*)propertyNames;

-(void)defineProperty:(EJSObjcString*)name value:(EJSObjcValue*)val attributes:(uint32_t)attrs;
-(void)definePropertyNS:(NSString*)name value:(EJSObjcValue*)val attributes:(uint32_t)attrs;

-(void)defineProperty:(EJSObjcString*)name object:(EJSObjcObject*)obj attributes:(uint32_t)attrs;
-(void)definePropertyNS:(NSString*)name object:(EJSObjcObject*)obj attributes:(uint32_t)attrs;


-(BOOL)isFunction;
-(BOOL)isConstructor;
-(BOOL)isArray;

-(jsuint)arrayLength;
-(uint16_t)functionArity;;

-(EJSObjcObject*)prototype;

-(void*)privateData;
-(void)setPrivateData:(void*)data;

-(EJSObject*)jsObject;

-(void)toggleJSRefToStrong;
-(void)toggleJSRefToWeak;

@end

@interface EJSObjcInvocation : NSObject {
  EJSObjcObject* _thisObj;
  EJSObjcObject* _func;
  NSUInteger _argCount;
  EJSObjcValue** _args;
  EJSObjcValue* _exc;
  BOOL isFuncCtor;
}


-(EJSObjcInvocation*)initWithFunction:(EJSObjcObject*)func argCount:(NSUInteger)argCount thisObject:(EJSObjcObject*)obj;
-(EJSObjcInvocation*)initWithConstructor:(EJSObjcObject*)ctor argCount:(NSUInteger)argCount;

+(EJSObjcInvocation*)invocationWithFunction:(EJSObjcObject*)func argCount:(NSUInteger)argCount thisObject:(EJSObjcObject*)obj;
+(EJSObjcInvocation*)invocationWithConstructor:(EJSObjcObject*)ctor argCount:(NSUInteger)argCount;

-(void)dealloc;


-(void)setThisObject:(EJSObjcObject*)obj;
-(void)setFunction:(EJSObjcObject*)func;
-(void)setConstructor:(EJSObjcObject*)ctor;

-(void)setArgument:(EJSObjcValue*)arg atIndex:(NSUInteger)index;

-(EJSObjcValue*)invoke NS_RETURNS_NOT_RETAINED;
-(EJSObjcValue*)exception NS_RETURNS_NOT_RETAINED;

@end

id get_objc_id (EJSObjcObject* obj); // XXX this needs to go away
