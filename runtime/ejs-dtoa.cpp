#include <stdio.h>
#include "external-deps/double-conversion/double-conversion.h"

using namespace double_conversion;

extern "C" {

void
_ejs_dtoa(double d, char* buf, size_t buf_size) {
  StringBuilder builder(buf, buf_size);
  DoubleToStringConverter::EcmaScriptConverter().ToShortest(d, &builder);
  builder.Finalize();
}

// Number.prototype.toPrecision digits (ES semantics, correct decimal
// rounding); precision must be 1..100 (validated by the caller)
void
_ejs_dtoa_precision(double d, int precision, char* buf, size_t buf_size) {
  StringBuilder builder(buf, buf_size);
  DoubleToStringConverter::EcmaScriptConverter().ToPrecision(d, precision, &builder);
  builder.Finalize();
}

};
