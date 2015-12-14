#include <stdio.h>
#include "double-conversion/double-conversion.h"

using namespace double_conversion;

extern "C" {

void
_ejs_dtoa(double d, char* buf, size_t buf_size) {
  StringBuilder builder(buf, buf_size);
  DoubleToStringConverter::EcmaScriptConverter().ToShortest(d, &builder);
  builder.Finalize();
}

};
