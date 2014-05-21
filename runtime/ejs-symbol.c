/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-symbol.h"
#include "ejs-gc.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-ops.h"

#define EJSVAL_IS_SYMBOL(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Symbol_specops))
#define EJSVAL_TO_SYMBOL(v)     ((EJSSymbol*)EJSVAL_TO_OBJECT(v))

// ECMA262: 19.4.2.2 Symbol.for ( key ) 
static ejsval
_ejs_Symbol_for (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval key = _ejs_undefined;
    if (argc > 0) key = args[0];

    // 1. Let stringKey be ToString(key). 
    // 2. ReturnIfAbrupt(stringKey). 
    ejsval stringKey = ToString(key);
    
    // 3. For each element e of the GlobalSymbolRegistry List, 
    // a. If SameValue(e.[[key]], stringKey) is true, then return e.[[symbol]]. 
    // 4. Assert: GlobalSymbolRegistry does not current contain an entry for stringKey. 
    // 5. Let newSymbol be a new unique Symbol value whose [[Description]] is stringKey. 
    ejsval newSymbol = _ejs_null; // XXX

    // 6. Append the record { [[key]]: stringKey, [[symbol]]: newSymbol) to the GlobalSymbolRegistry List. 
    // 7. Return newSymbol. 
    return newSymbol;
}

// ECMA262: 19.4.2.7 Symbol.keyFor ( sym ) 
static ejsval
_ejs_Symbol_keyFor(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval sym = _ejs_undefined;
    if (argc > 0) sym = args[0];

    // 1. If Type(sym) is not Symbol, then throw a TypeError exception. 
    if (!EJSVAL_IS_SYMBOL(sym))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Symbol.keyFor called with non-symbol argument");
        
    // 2. For each element e of the GlobalSymbolRegistry List (see 19.4.2.2), 
    //    a. If SameValue(e.[[symbol]], sym) is true, then return e.[[key]]. 
    // 3. Assert: GlobalSymbolRegistry does not current contain an entry for sym. 

    // XXX

    // 4. Return undefined. 
    return _ejs_undefined;
}

// ECMA262: 19.4.3.2 Symbol.prototype.toString ()
static ejsval
_ejs_Symbol_prototype_toString(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let s be the this value. 
    ejsval s = _this;

    EJSSymbol* sym;

    // 2. If Type(s) is Symbol, then let sym be s. 
    if (EJSVAL_IS_SYMBOL(s))
        sym = EJSVAL_TO_SYMBOL(s);
    // 3. Else, 
    //    a. If s does not have a [[SymbolData]] internal slot, then throw a TypeError exception. 
    //    b. Let sym be the value of s’s [[SymbolData]] internal slot. 
    else
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Symbol.prototype.toString called with non-symbol this");

    // 4. Let desc be the value of sym’s [[Description]] attribute. 
    ejsval desc = sym->description;

    // 5. If desc is undefined, then let desc be the empty string. 
    if (EJSVAL_IS_UNDEFINED(desc))
        desc = _ejs_atom_empty;

    // 6. Assert: Type(desc) is String. 
    // 7. Let result be the result of concatenating the strings "Symbol(", desc, and ")". 
    ejsval result = _ejs_string_concatv (_ejs_atom_Symbol,
                                         _ejs_string_new_utf8("("),
                                         desc,
                                         _ejs_string_new_utf8(")"),
                                         _ejs_null);
    // 8. Return result. 
    return result;
}

// ECMA262: 19.4.1
static ejsval
_ejs_Symbol_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_NULL(_this) || EJSVAL_IS_UNDEFINED(_this)) {
        // 19.4.1.1 Symbol ( [ description ] )
        ejsval description = _ejs_undefined;
        if (argc > 0) description = args[0];
        return _ejs_symbol_new(description);
    }
    else {
        // 19.4.1.2 new Symbol ( ...argumentsList ) 
        EJS_NOT_IMPLEMENTED();
    }
}

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Symbol, x, _ejs_Symbol_##x, EJS_PROP_NOT_ENUMERABLE)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Symbol_prototype, x, _ejs_Symbol_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_PROPERTY(x) 
#define WELL_KNOWN_SYMBOL(x) EJS_MACRO_START                            \
    _ejs_gc_add_root (&_ejs_Symbol_##x);                                \
    _ejs_Symbol_##x = _ejs_symbol_new(_ejs_atom_Symbol_##x);            \
    _ejs_object_define_value_property (_ejs_Symbol, _ejs_atom_##x, _ejs_Symbol_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_NOT_CONFIGURABLE); \
    EJS_MACRO_END

ejsval _ejs_Symbol EJSVAL_ALIGNMENT;
ejsval _ejs_Symbol_prototype EJSVAL_ALIGNMENT;

ejsval _ejs_Symbol_create EJSVAL_ALIGNMENT;
ejsval _ejs_Symbol_hasInstance EJSVAL_ALIGNMENT;
ejsval _ejs_Symbol_isConcatSpreadable EJSVAL_ALIGNMENT;
ejsval _ejs_Symbol_isRegExp EJSVAL_ALIGNMENT;
ejsval _ejs_Symbol_iterator EJSVAL_ALIGNMENT;
ejsval _ejs_Symbol_toPrimitive EJSVAL_ALIGNMENT;
ejsval _ejs_Symbol_toStringTag EJSVAL_ALIGNMENT;
ejsval _ejs_Symbol_unscopables EJSVAL_ALIGNMENT;

void
_ejs_Symbol_create_impl(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "1"); // XXX
}

void
_ejs_symbol_init(ejsval global)
{
    _ejs_Symbol = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Symbol, (EJSClosureFunc)_ejs_Symbol_impl);
    _ejs_object_setprop (global, _ejs_atom_Symbol, _ejs_Symbol);

    _ejs_gc_add_root (&_ejs_Symbol_prototype);
    _ejs_Symbol_prototype = _ejs_object_new(_ejs_null, &_ejs_Object_specops); // XXX
    _ejs_object_setprop (_ejs_Symbol,       _ejs_atom_prototype,  _ejs_Symbol_prototype);

    PROTO_METHOD(toString);
    //PROTO_METHOD(valueOf);

    OBJ_METHOD(for);
    OBJ_METHOD(keyFor);

    WELL_KNOWN_SYMBOL(create);
    WELL_KNOWN_SYMBOL(hasInstance);
    WELL_KNOWN_SYMBOL(isConcatSpreadable);
    WELL_KNOWN_SYMBOL(isRegExp);
    WELL_KNOWN_SYMBOL(iterator);
    WELL_KNOWN_SYMBOL(toPrimitive);
    WELL_KNOWN_SYMBOL(toStringTag);
    WELL_KNOWN_SYMBOL(unscopables);

    _ejs_object_define_value_property (_ejs_Symbol_prototype, _ejs_Symbol_toStringTag, _ejs_atom_Symbol, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Symbol, create, _ejs_Symbol_create_impl, EJS_PROP_NOT_ENUMERABLE);
}

ejsval
_ejs_symbol_new (ejsval description)
{
    EJSSymbol* rv = _ejs_gc_new (EJSSymbol);

    _ejs_init_object((EJSObject*)rv, _ejs_Symbol_prototype, &_ejs_Symbol_specops);

    rv->description = EJSVAL_IS_UNDEFINED(description) ? description : ToString(description);

    return OBJECT_TO_EJSVAL(rv);
}

uint32_t
_ejs_symbol_hash (ejsval obj)
{
    EJSSymbol* sym = EJSVAL_TO_SYMBOL(obj);
    if ((sym->hashcode & 1) == 0) {
        // just use the symbol's pointer
        sym->hashcode = ((uint32_t)(uintptr_t)sym) | 1;
    }
    return sym->hashcode;
}

static EJSObject*
_ejs_symbol_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSSymbol);
}

static void
_ejs_symbol_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    scan_func(((EJSSymbol*)obj)->description);
}

EJS_DEFINE_CLASS(Symbol,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT,
                 OP_INHERIT,
                 OP_INHERIT,
                 OP_INHERIT,
                 OP_INHERIT,
                 OP_INHERIT,
                 OP_INHERIT,
                 OP_INHERIT,
                 OP_INHERIT,
                 OP_INHERIT,
                 _ejs_symbol_specop_allocate,
                 OP_INHERIT,
                 _ejs_symbol_specop_scan
                 )
