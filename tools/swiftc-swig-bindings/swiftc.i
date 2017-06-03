%module swiftc

%{
    
#include "swift/AST/DiagnosticConsumer.h"
#include "swift/AST/ASTContext.h"
#include "swift/Frontend/Frontend.h"
#include "swift/Frontend/PrintingDiagnosticConsumer.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ArrayRef.h"
#include <cstring>

using namespace swift;

StringRef make_stringref(const char* str) {
    int len = strlen(str);
    char *copy = new char[len+1];
    memcpy(copy, str, len+1);
    printf("%s\n", copy);
    return StringRef(copy);
}
ArrayRef<const char*> make_string_arrayref(std::vector<const char*> vec) {
    return ArrayRef<const char*>(vec);
}

%}

// unique_ptr handling in SWIG is not fully there yet
// we don't need these methods/fields (yet) so just ignore them.
%ignore swift::CompilerInstance::setSILModule(std::unique_ptr<SILModule>);
%ignore swift::CompilerInstance::takeSILModule();
%ignore swift::ASTContext::getForeignRepresentationInfo(NominalTypeDecl *nominal, ForeignLanguage language, const DeclContext *dc);
%ignore swift::ASTContext::addModuleLoader(std::unique_ptr<ModuleLoader> loader, bool isClang = false);
%ignore swift::ASTContext::TypeCheckerDebug;
%ignore swift::ASTContext::SourceMgr;
%ignore swift::ASTContext::Diags;
%ignore swift::ASTContext::Impl;
%ignore swift::ASTContext::LLVMSourceMgr;

StringRef make_stringref(const char* str);
ArrayRef<const char*> make_string_arrayref(std::vector<const char*> vec);

%include "swift/AST/DiagnosticConsumer.h"
%include "swift/AST/ASTContext.h"
%include "swift/Frontend/PrintingDiagnosticConsumer.h"
%include "swift/Frontend/Frontend.h"
