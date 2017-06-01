// need to figure out how to get cmake to build this into a library
#include "llvm/ADT/StringRef.h"
using namespace llvm;

StringRef make_stringref(const char* str) {
    return StringRef(str);
}

