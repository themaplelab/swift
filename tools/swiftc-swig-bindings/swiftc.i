%module example

%{
    
#include "swift/Frontend/Frontend.h"
#include "llvm/ADT/StringRef.h"

using namespace llvm;
using namespace swift;

StringRef make_stringref(const char* str);

%}

// unique_ptr handling in SWIG is not fully there yet
// we don't need these methods so just ignore them.
%ignore swift::CompilerInstance::setSILModule(std::unique_ptr<SILModule>);
%ignore swift::CompilerInstance::takeSILModule();

StringRef make_stringref(const char* str) {
    return StringRef(str);
}

%include "swift/Frontend/Frontend.h"

