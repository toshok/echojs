// empty things
EJS_ATOM2(,empty)
EJS_ATOM2([],empty_array)
EJS_ATOM2({},empty_object)

EJS_ATOM2(/,slash)

// common properties/methods
EJS_ATOM(__proto__)
EJS_ATOM(constructor)
EJS_ATOM(length)
EJS_ATOM(prototype)
EJS_ATOM(message)
EJS_ATOM(name)

EJS_ATOM(object)
EJS_ATOM(function)
EJS_ATOM(string)
EJS_ATOM(boolean)
EJS_ATOM(number)


EJS_ATOM(Empty)

EJS_ATOM(null)
EJS_ATOM(undefined)
EJS_ATOM(NaN)

EJS_ATOM(true)
EJS_ATOM(false)

EJS_ATOM(toJSON)

EJS_ATOM(byteOffset)
EJS_ATOM(byteLength)
EJS_ATOM(buffer)

// property descriptors
EJS_ATOM(value)
EJS_ATOM(writable)
EJS_ATOM(configurable)
EJS_ATOM(enumerable)
EJS_ATOM(get)
EJS_ATOM(set)

// class names
EJS_ATOM(Array)
EJS_ATOM(Boolean)
EJS_ATOM(Date)
EJS_ATOM(Function)
EJS_ATOM(JSON)
EJS_ATOM(Math)
EJS_ATOM(Number)
EJS_ATOM(Object)
EJS_ATOM(RegExp)
EJS_ATOM(String)

EJS_ATOM(XMLHttpRequest)

EJS_ATOM(ArrayBuffer)
EJS_ATOM(Int8Array)
EJS_ATOM(Uint8Array)
EJS_ATOM(Uint8ClampedArray)
EJS_ATOM(Int16Array)
EJS_ATOM(Uint16Array)
EJS_ATOM(Int32Array)
EJS_ATOM(Uint32Array)
EJS_ATOM(Float32Array)
EJS_ATOM(Float64Array)
EJS_ATOM(BYTES_PER_ELEMENT)

// error types
EJS_ATOM(Error)
EJS_ATOM(EvalError)
EJS_ATOM(RangeError)
EJS_ATOM(ReferenceError)
EJS_ATOM(SyntaxError)
EJS_ATOM(TypeError)
EJS_ATOM(URIError)

EJS_ATOM(console)
EJS_ATOM(require)
EJS_ATOM(exports)

// our ejs-specific stuff
EJS_ATOM(__ejs)
EJS_ATOM(GC)

EJS_ATOM(argv)
EJS_ATOM(process)

// Object method/property names
EJS_ATOM(getPrototypeOf)
EJS_ATOM(getOwnPropertyDescriptor)
EJS_ATOM(getOwnPropertyNames)
EJS_ATOM(create)
EJS_ATOM(defineProperty)
EJS_ATOM(defineProperties)
EJS_ATOM(seal)
EJS_ATOM(freeze)
EJS_ATOM(preventExtensions)
EJS_ATOM(isSealed)
EJS_ATOM(isFrozen)
EJS_ATOM(isExtensible)
EJS_ATOM(keys)
EJS_ATOM(toString)
EJS_ATOM(toLocaleString)
EJS_ATOM(valueOf)
EJS_ATOM(hasOwnProperty)
EJS_ATOM(isPrototypeOf)
EJS_ATOM(propertyIsEnumerable)


// Math method names
EJS_ATOM(abs)
EJS_ATOM(acos)
EJS_ATOM(asin)
EJS_ATOM(atan)
EJS_ATOM(atan2)
EJS_ATOM(ceil)
EJS_ATOM(cos)
EJS_ATOM(exp)
EJS_ATOM(floor)
EJS_ATOM(log)
EJS_ATOM(max)
EJS_ATOM(min)
EJS_ATOM(pow)
EJS_ATOM(random)
EJS_ATOM(round)
EJS_ATOM(sin)
EJS_ATOM(sqrt)
EJS_ATOM(tan)

// Array method names
EJS_ATOM(isArray)
EJS_ATOM(push)
EJS_ATOM(pop)
EJS_ATOM(shift)
EJS_ATOM(unshift)
EJS_ATOM(concat)
EJS_ATOM(slice)
EJS_ATOM(splice)
EJS_ATOM(indexOf)
EJS_ATOM(join)
EJS_ATOM(forEach)
EJS_ATOM(map)
EJS_ATOM(every)
EJS_ATOM(some)
EJS_ATOM(reduce)
EJS_ATOM(reduceRight)

// global functions
EJS_ATOM(isNaN)
EJS_ATOM(isFinite)
EJS_ATOM(parseInt)
EJS_ATOM(parseFloat)
EJS_ATOM(decodeURI)
EJS_ATOM(decodeURIComponent)
EJS_ATOM(encodeURI)
EJS_ATOM(encodeURIComponent)

// JSON functions
EJS_ATOM(parse)
EJS_ATOM(stringify)

// String functions
EJS_ATOM(charAt)
EJS_ATOM(charCodeAt)
//EJS_ATOM(concat)
//EJS_ATOM(indexOf)
EJS_ATOM(lastIndexOf)
EJS_ATOM(localeCompare)
EJS_ATOM(match)
EJS_ATOM(replace)
EJS_ATOM(search)
//EJS_ATOM(slice)
EJS_ATOM(split)
EJS_ATOM(substr)
EJS_ATOM(substring)
EJS_ATOM(toLocaleLowerCase)
EJS_ATOM(toLocaleUpperCase)
EJS_ATOM(toLowerCase)
//EJS_ATOM(toString)
EJS_ATOM(toUpperCase)
EJS_ATOM(trim)
//EJS_ATOM(valueOf)
EJS_ATOM(fromCharCode)

// Number functions
//EJS_ATOM(valueOf)
//EJS_ATOM(toString)

// Boolean functions
//EJS_ATOM(valueOf)
//EJS_ATOM(toString)

// Regexp functions and properties
EJS_ATOM(exec)
//EJS_ATOM(match)
EJS_ATOM(test)
//EJS_ATOM(toString)
EJS_ATOM(source)
EJS_ATOM(global)
EJS_ATOM(lastIndex)
EJS_ATOM(multiline)
EJS_ATOM(ignoreCase)

// Date functions
//EJS_ATOM(toString)
EJS_ATOM(getTime)
EJS_ATOM(getTimezoneOffset)

// Function functions
//EJS_ATOM(toString)
EJS_ATOM(apply)
EJS_ATOM(call)
EJS_ATOM(bind)


// ArrayBuffer functions
//EJS_ATOM(slice)

// console functions
//EJS_ATOM(log)
EJS_ATOM(warn)

// gc functions
EJS_ATOM(collect)

// process functions/properties
EJS_ATOM(exit)
EJS_ATOM(chdir)
EJS_ATOM(cwd)
EJS_ATOM(env)

// XMLHttpRequest functions
EJS_ATOM(open)
EJS_ATOM(send)
EJS_ATOM(setRequestHeader)
EJS_ATOM(abort)
EJS_ATOM(getResponseHeader)
EJS_ATOM(getAllResponseHeaders)

// XMLHttpRequest properties
EJS_ATOM(readyState)
EJS_ATOM(status)
EJS_ATOM(statusText)
EJS_ATOM(responseText)
EJS_ATOM(responseXML)
EJS_ATOM(onreadystatechange)

EJS_ATOM(eval)
