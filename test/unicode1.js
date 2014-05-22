// kangax's unicode codepoint escapes test

try {
  console.log('\\u{1d306}' == '\\ud834\\udf06');
} catch (e) {
  console.log(e);
}
