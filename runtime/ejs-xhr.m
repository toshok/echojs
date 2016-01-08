/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <assert.h>
#import <Foundation/Foundation.h>

#include "ejs-jsobjc.h"
#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-xhr.h"
#include "ejs-string.h"
#include "ejs-error.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"

extern int _ejs_runloop_darwin_refcount;

#define THROW_ARG_COUNT_EXCEPTION(expected_count) _ejs_throw_nativeerror_utf8 (EJS_ERROR/*XXX*/, "argument count mismatch")

typedef enum {
    UNSENT,
    OPENED,
    HEADERS_RECEIVED,
    LOADING,
    DONE
} ReadyState;

typedef enum {
    INVALID_STATE_ERR = 11,
    SECURITY_ERR = 18,
    NETWORK_ERR = 19,
    ABORT_ERR = 20
} XHRExceptionCode;

@interface XHRException : NSException

@property (assign) XHRExceptionCode code;

-(id)initWithCode:(XHRExceptionCode)code;

+(id)exceptionWithCode:(XHRExceptionCode)code;

@end

@implementation XHRException

@synthesize code;

-(id)initWithCode:(XHRExceptionCode)aCode
{
    self = [super init];
    if (self) {
        self.code = aCode;
    }
    return self;
}

+(id)exceptionWithCode:(XHRExceptionCode)code
{
    return [[[XHRException alloc] initWithCode:code] autorelease];
}

@end



@interface XmlHttpRequest : NSObject<NSURLConnectionDelegate> {
@private
    ReadyState state;
    BOOL send_flag;
    NSURLConnection *connection;
    NSMutableData *responseData;
}

@property (assign) NSInteger statusCode;
@property (retain) NSURL*    url;
@property (copy)   NSString* method;
@property (retain) CKObject* readystatechange;
@property (assign) BOOL error;

-(id)init;

-(void)openURL:(NSString*)url withMethod:(NSString*)method;
-(void)send;
-(void)sendWithString:(NSString*)data;
#ifndef IOS
-(void)sendWithDocument:(NSXMLDocument*)data;
#endif
-(void)invokeReadyStateChange;

-(void)abort;

-(ReadyState)state;

-(NSString*)statusText NS_RETURNS_NOT_RETAINED;
-(NSString*)responseText NS_RETURNS_RETAINED;

/* NSURLConnection delegate methods */
- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response;
- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data;
- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error;

@end


@implementation XmlHttpRequest

@synthesize statusCode;
@synthesize url;
@synthesize method;
@synthesize readystatechange;
@synthesize error;

- (id)init {
	self = [super init];
	if (self) {
		state = UNSENT;
		error = NO;
		send_flag = NO;
        url = nil;
        method = nil;
        readystatechange = nil;
	}
	return self;
}

-(void)openURL:(NSString*)aUrl withMethod:(NSString*)aMethod
{
	url = [NSURL URLWithString:aUrl];
	method = aMethod;
    
	send_flag = NO;
	state = OPENED;
}

-(void)send
{
#if false
	if (state != OPENED)
		@throw [XHRException exceptionWithCode:INVALID_STATE_ERR];

	if (send_flag)
		@throw [XHRException exceptionWithCode:INVALID_STATE_ERR];
#endif
	error = NO;
    
	NSURLRequest *urlRequest = [NSURLRequest requestWithURL:url];
	connection = [[NSURLConnection alloc] initWithRequest:urlRequest delegate:self startImmediately:YES];

    _ejs_runloop_darwin_refcount++;
}

-(void)sendWithString:(NSString*)data
{
	abort ();
}

#ifndef IOS
-(void)sendWithDocument:(NSXMLDocument*)data
{
	abort ();
}
#endif

- (ReadyState) state
{
	return state;
}

-(NSString*)statusText
{
	return [NSHTTPURLResponse localizedStringForStatusCode:statusCode];
}

-(NSString*)responseText
{
	if (!responseData)
		return [[NSString string] retain];
    
	return [[NSString alloc] initWithData:responseData encoding:NSUTF8StringEncoding/*XXX this should come from the response headers... */];
}

-(void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
	NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse*)response;
	statusCode = [httpResponse statusCode];
    
	if (responseData)
		[responseData setLength:0];
}

-(void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
	if (!responseData)
		responseData = [[NSMutableData data] retain];

	[responseData appendData:data];
}

-(void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)anError
{
    NSLog(@"didFailWithError:%@\n", anError);
	abort (); // XXX
}

-(void)proxyDidFinishLoading
{
	[self invokeReadyStateChange];
}

-(void)connectionDidFinishLoading:(NSURLConnection *)connection
{
	state = DONE;
    [self performSelectorOnMainThread:@selector(proxyDidFinishLoading) withObject:nil waitUntilDone:NO];
}

-(void)invokeReadyStateChange
{
	if (readystatechange) {
		CKInvocation* inv = [CKInvocation invocationWithFunction:readystatechange argCount:0 thisObject:NULL];

		[inv invoke];
        _ejs_runloop_darwin_refcount--;
	}
}

-(void)abort
{
}

@end

typedef struct {
    EJSObject obj;
    id peer;
} EJSXMLHttpRequest;

static id
get_peer (ejsval obj)
{
    return ((EJSXMLHttpRequest*)EJSVAL_TO_OBJECT(obj))->peer;
}

static void
finalize_release_private_data (EJSObject* obj)
{
	id peer = ((EJSXMLHttpRequest*)obj)->peer;
	[peer release];
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_open) {
	/* the forms the user can invoke this method using:
	 * 
	 * void open(DOMString method, DOMString url);
	 * void open(DOMString method, DOMString url, boolean async);
	 * void open(DOMString method, DOMString url, boolean async, DOMString? user);
	 * void open(DOMString method, DOMString url, boolean async, DOMString? user, DOMString? password);
	 */
    
	// Note: we only allow async mode.  if the user requests sync mode we throw an exception

	if (argc < 2)
		THROW_ARG_COUNT_EXCEPTION(-1);

	XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer (*_this);
    
	NSString *method = [[CKValue valueWithJSValue:args[0]] nsStringValue];
	NSString *url = [[CKValue valueWithJSValue:args[1]] nsStringValue];
    
	[xhr openURL:url withMethod:method];
    
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_send) {
	/* the forms the user can invoke this method using:
	 *
	 * void send();
	 * void send(Document data);
	 * void send([AllowAny] DOMString? data);
	 */

	if (argc > 1)
		THROW_ARG_COUNT_EXCEPTION(-1);

	XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer (*_this);

	@try {
		if (argc == 0) {
			[xhr send];
		}
		else {
			if (EJSVAL_IS_STRING(args[0])) {
				EJS_NOT_IMPLEMENTED(); // FIXME handle this.. string case
			}
			else {
				EJS_NOT_IMPLEMENTED(); // FIXME handle this... NSXmlDocument case
			}
		}
	}
	@catch (NSException *objc_exc) {
        printf ("say whaaaaa\n");
#if notyet
		return throwXHRException(cx, objc_exc);
#endif
	}

    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_setRequestHeader) {
	/*
	 * When the setRequestHeader(header, value) method is invoked, the user agent must run these steps:
	 * 
	 * If the state is not OPENED raise an INVALID_STATE_ERR exception and terminate these steps.
	 *
	 * If the send() flag is true raise an INVALID_STATE_ERR exception and terminate these steps.
	 * 
	 * If any code point in header is higher than U+00FF LATIN SMALL LETTER Y WITH DIAERESIS or after deflating header it does not match the field-name production raise a SYNTAX_ERR exception and terminate these steps. Otherwise let header be the result of deflating header.
	 * 
	 * If any code point in value is higher than U+00FF LATIN SMALL LETTER Y WITH DIAERESIS or after deflating value it does not match the field-value production raise a SYNTAX_ERR exception and terminate these steps. Otherwise let value be the result of deflating value.
	 * 
	 * The empty string is legal and represents the empty header value.
	 * 
	 * Terminate these steps if header is a case-insensitive match for one of the following headers:
	 * 
	 * Accept-Charset
	 * Accept-Encoding
	 * Connection
	 * Content-Length
	 * Cookie
	 * Cookie2
	 * Content-Transfer-Encoding
	 * Date
	 * Expect
	 * Host
	 * Keep-Alive
	 * Referer
	 * TE
	 * Trailer
	 * Transfer-Encoding
	 * Upgrade
	 * User-Agent
	 * Via
	 * … or if the start of header is a case-insensitive match for Proxy- or Sec- (including when header is just Proxy- or Sec-).
	 * 
	 * The above headers are controlled by the user agent to let it control those aspects of transport. This guarantees data integrity to some extent. Header names starting with Sec- are not allowed to be set to allow new headers to be minted that are guaranteed not to come from XMLHttpRequest.
	 * 
	 * If header is not in the author request headers list append header with its associated value to the list and terminate these steps.
	 * 
	 * If header is in the author request headers list either use multiple headers, combine the values or use a combination of those (section 4.2, RFC 2616). [RFC2616]
	 *
	 */

	// FIXME
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_abort) {
	/*
	 * When the abort() method is invoked, the user agent must run these steps (unless otherwise noted):
	 * 
	 * Abort the send() algorithm.
	 * 
	 * The user agent should cancel any network activity for which the object is responsible.
	 * 
	 * If there are any tasks from the object's XMLHttpRequest task source in one of the task queues, then remove those tasks.
	 * 
	 * Set the response entity body to null.
	 * 
	 * Empty the list of author request headers.
	 * 
	 * Set the error flag to true.
	 * 
	 * If the state is UNSENT, OPENED with the send() flag being false, or DONE go to the next step.
	 * 
	 * Otherwise run these substeps:
	 *
	 * Switch the state to DONE.
	 *
	 * Set the send() flag to false.
	 * 
	 * Dispatch a readystatechange event.
	 * 
	 * A future version of this specification will dispatch an abort event here.
	 * 
	 * Switch the state to UNSENT.
	 * 
	 * No readystatechange event is dispatched.
	 */

	if (argc != 0)
		THROW_ARG_COUNT_EXCEPTION(-1);
    
	XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer (*_this);
    
	[xhr abort];

    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_getResponseHeader) {
	/*
	 * When the getResponseHeader(header) is invoked, the user agent must run these steps:
	 * 
	 * If the state is UNSENT or OPENED return null and terminate these steps.
	 * 
	 * If the error flag is true return null and terminate these steps.
	 *
	 * If any code point in header is higher than U+00FF LATIN SMALL LETTER Y WITH DIAERESIS return null and terminate these steps.
	 * 
	 * Let header be the result of deflating header.
	 * 
	 * If header is a case-insensitive match for Set-Cookie or Set-Cookie2 return null and terminate these steps.
	 * 
	 * If header is a case-insensitive match for multiple HTTP response headers, return the inflated values of these headers as a single concatenated string separated from each other by a U+002C COMMA U+0020 SPACE character pair and terminate these steps.
	 * 
	 * If header is a case-insensitive match for a single HTTP response header, return the inflated value of that header and terminate these steps.
	 * 
	 * Return null.
	 * 
	 */

	if (argc != 1)
		THROW_ARG_COUNT_EXCEPTION(-1);

	XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer (*_this);

	ReadyState state = [xhr state];
    
	if (state == UNSENT || state == OPENED || [xhr error]) {
        return _ejs_null;
	}


	/* FIXME
	 * If any code point in header is higher than U+00FF LATIN SMALL LETTER Y WITH DIAERESIS return null and terminate these steps.
	 * 
	 * Let header be the result of deflating header.
	 * 
	 * If header is a case-insensitive match for Set-Cookie or Set-Cookie2 return null and terminate these steps.
	 * 
	 * If header is a case-insensitive match for multiple HTTP response headers, return the inflated values of these headers as a single concatenated string separated from each other by a U+002C COMMA U+0020 SPACE character pair and terminate these steps.
	 * 
	 * If header is a case-insensitive match for a single HTTP response header, return the inflated value of that header and terminate these steps.
	 */
    return _ejs_null;
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_getAllResponseHeaders) {
	/*
	 * When the getAllResponseHeaders() method is invoked, the user agent must run the following steps:
	 * 
	 * If the state is UNSENT or OPENED return the empty string and terminate these steps.
	 * 
	 * If the error flag is true return the empty string and terminate these steps.
	 * 
	 * Return all the HTTP headers, excluding headers that are a case-insensitive match for Set-Cookie or Set-Cookie2, inflated, as a single string, with each header line separated by a U+000D CR U+000A LF pair, excluding the status line, and with each header name and header value separated by a U+003A COLON U+0020 SPACE pair.
	 */

	XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer (*_this);
	ReadyState state = [xhr state];
    ejsval rv;
    
	if (state == UNSENT || state == OPENED || [xhr error])
		rv = _ejs_atom_empty;
	else {
		// FIXME
		rv = _ejs_atom_empty;
	}
    
    return rv;
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_get_readyState) {
	XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer(*_this);
	return NUMBER_TO_EJSVAL ([xhr state]);
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_get_status) {
	/*
	 * The status attribute must return the result of running these steps:
	 * 
	 * If the state is UNSENT or OPENED return 0 and terminate these steps.
	 * 
	 * If the error flag is true return 0 and terminate these steps.
	 * 
	 * Return the HTTP status code.
	 */
	XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer(*_this);
	ReadyState state = [xhr state];
    
	if (state == UNSENT || state == OPENED || [xhr error]) {
        return NUMBER_TO_EJSVAL(0);
	}

    return NUMBER_TO_EJSVAL ([xhr statusCode]);
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_get_statusText) {
	/*
	 * The statusText attribute must return the result of running these steps:
	 * 
	 * If the state is UNSENT or OPENED return the empty string and terminate these steps.
	 *
	 * If the error flag is true return the empty string and terminate these steps.
	 * 
	 * Return the HTTP status text.
	 */
	XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer(*_this);
	ReadyState state = [xhr state];
	ejsval rv;
    
	if (state == UNSENT || state == OPENED || [xhr error])
		rv = _ejs_atom_empty;
	else {
		NSString *status = [[xhr statusText] autorelease];
		rv = STRING_TO_EJSVAL([[CKString stringWithNSString:status] jsString]);
	}
    
    return rv;
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_get_responseText) {
	/*
	 * The responseText attribute must return the result of running these steps:
	 * 
	 * If the state is not LOADING or DONE return the empty string and terminate these steps.
	 *
	 * Return the text response entity body.
	 */
	XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer(*_this);
	ReadyState state = [xhr state];
    ejsval rv;
    
	if (state != LOADING && state != DONE)
		rv = _ejs_atom_empty;
	else {
		NSString *resp = [[xhr responseText] autorelease];
		rv = STRING_TO_EJSVAL([[CKString stringWithNSString:resp] jsString]);
	}

    return rv;
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_get_responseXML) {
    EJS_NOT_IMPLEMENTED();
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_get_onreadystatechange) {
    XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer(*_this);
	CKObject* readystatechange = [xhr readystatechange];

    return readystatechange ? OBJECT_TO_EJSVAL([readystatechange jsObject]): _ejs_null;
}

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_prototype_set_onreadystatechange) {
    XmlHttpRequest *xhr = (XmlHttpRequest*)get_peer (*_this);
    xhr.readystatechange = [[CKValue valueWithJSValue:args[0]] objectValue];
    return _ejs_undefined;
}

ejsval _ejs_XMLHttpRequest EJSVAL_ALIGNMENT;
ejsval _ejs_XMLHttpRequest_prototype EJSVAL_ALIGNMENT;

static EJS_NATIVE_FUNC(_ejs_XMLHttpRequest_impl) {

    if (EJSVAL_IS_UNDEFINED(newTarget)) {
        printf ("XMLHttpRequest called as a function\n");
        EJS_NOT_IMPLEMENTED();
    }
    else {
        // called as a constructor
        if (argc != 0)
            THROW_ARG_COUNT_EXCEPTION(-1);

        EJS_ASSERT(EJSVAL_IS_UNDEFINED(*_this));

        *_this = OrdinaryCreateFromConstructor(EJSVAL_IS_UNDEFINED(newTarget) ? _ejs_XMLHttpRequest : newTarget, _ejs_XMLHttpRequest_prototype, &_ejs_Object_specops);

        EJSXMLHttpRequest* xhr = (EJSXMLHttpRequest*)EJSVAL_TO_OBJECT(*_this);
        id peer = [[XmlHttpRequest alloc] init];

        xhr->peer = peer;

        return *_this;
    }
}

void
_ejs_xmlhttprequest_init(ejsval global)
{
    _ejs_XMLHttpRequest = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_XMLHttpRequest, (EJSClosureFunc)_ejs_XMLHttpRequest_impl);
    _ejs_object_setprop (global, _ejs_atom_XMLHttpRequest, _ejs_XMLHttpRequest);

    _ejs_gc_add_root (&_ejs_XMLHttpRequest_prototype);
    _ejs_XMLHttpRequest_prototype = _ejs_object_new(_ejs_null, &_ejs_Object_specops);
    _ejs_object_setprop (_ejs_XMLHttpRequest,       _ejs_atom_prototype,  _ejs_XMLHttpRequest_prototype);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_XMLHttpRequest_prototype, x, _ejs_XMLHttpRequest_prototype_##x, EJS_PROP_NOT_ENUMERABLE)
#define PROTO_GETTER(x) EJS_INSTALL_ATOM_GETTER(_ejs_XMLHttpRequest_prototype, x, _ejs_XMLHttpRequest_prototype_get_##x)
#define PROTO_ACCESSORS(x) EJS_INSTALL_ATOM_ACCESSORS(_ejs_XMLHttpRequest_prototype, x, _ejs_XMLHttpRequest_prototype_get_##x, _ejs_XMLHttpRequest_prototype_set_##x)

    PROTO_METHOD(open);
    PROTO_METHOD(send);
    PROTO_METHOD(setRequestHeader);
    PROTO_METHOD(abort);
    PROTO_METHOD(getResponseHeader);
    PROTO_METHOD(getAllResponseHeaders);

    PROTO_GETTER(readyState);
    PROTO_GETTER(status);
    PROTO_GETTER(statusText);
    PROTO_GETTER(responseText);
    PROTO_GETTER(responseXML);
    PROTO_ACCESSORS(onreadystatechange);

#undef PROTO_METHOD
#undef PROTO_GETTER
#undef PROTO_ACCESSORS
}

static EJSObject*
_ejs_xmlhttprequest_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSXMLHttpRequest);
}

EJS_DEFINE_CLASS(XMLHttpRequest,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_xmlhttprequest_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 OP_INHERIT  // [[Scan]]
                 )
