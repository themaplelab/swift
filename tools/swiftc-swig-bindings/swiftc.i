%module(directors="1") swiftc

%include "std_string.i"

// from swig docs
/* This tells SWIG to treat char ** as a special case when used as a parameter
   in a function call */
%typemap(in) char ** (jint size) {
  int i = 0;
  size = jenv->GetArrayLength((jarray)$input);
  $1 = (char **) malloc((size+1)*sizeof(char *));
  /* make a copy of each string */
  for (i = 0; i<size; i++) {
    jstring j_string = (jstring)jenv->GetObjectArrayElement((jobjectArray)$input, i);
    const char * c_string = jenv->GetStringUTFChars(j_string, 0);
    $1[i] = (char*)malloc((strlen(c_string)+1)*sizeof(char));
    strcpy($1[i], c_string);
    jenv->ReleaseStringUTFChars(j_string, c_string);
    jenv->DeleteLocalRef(j_string);
  }
  $1[i] = 0;
}

/* This cleans up the memory we malloc'd before the function call */
%typemap(freearg) char ** {
  int i;
  for (i=0; i<size$argnum-1; i++)
    free($1[i]);
  free($1);
}

/* These 3 typemaps tell SWIG what JNI and Java types to use */
%typemap(jni) char ** "jobjectArray"
%typemap(jtype) char ** "String[]"
%typemap(jstype) char ** "String[]"

/* These 2 typemaps handle the conversion of the jtype to jstype typemap type and visa versa */
%typemap(javain) char ** "$javainput"
%typemap(javaout) char ** {
    return $jnicall;
}

%{
    
#include "swift/AST/DiagnosticConsumer.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/ASTWalker.h"
#include "swift/AST/Module.h"
#include "swift/Frontend/Frontend.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ArrayRef.h"
#include "swift/Basic/LLVMInitialize.h"
#include "swift/SIL/SILModule.h"
#include "llvm/Support/Debug.h"
#include <cstring>
#include <iostream>

using namespace swift;

StringRef make_stringref(const char* str) {
    int len = strlen(str);
    char *copy = new char[len+1];
    memcpy(copy, str, len+1);
    return StringRef(copy);
}
ArrayRef<const char*> make_string_arrayref(const char** vec, int length) {
    return ArrayRef<const char*>(vec, length);
}

class BasicDiagnosticConsumer : public swift::DiagnosticConsumer {
public:
    BasicDiagnosticConsumer() {}
  void handleDiagnostic(SourceManager &SM, SourceLoc Loc,
                        DiagnosticKind Kind,
                        StringRef FormatString,
                        ArrayRef<DiagnosticArgument> FormatArgs,
                        const DiagnosticInfo &Info) override {
        llvm::dbgs() << "Diagnostic: ";
        DiagnosticEngine::formatDiagnosticText(llvm::dbgs(), FormatString,
FormatArgs);
        llvm::dbgs() << "\n";
    }
};
DiagnosticConsumer::~DiagnosticConsumer() { }
llvm::SMLoc DiagnosticConsumer::getRawLoc(SourceLoc loc) {
  return loc.Value;
}
DiagnosticConsumer* make_BasicDiagnosticConsumer() {
    return new BasicDiagnosticConsumer();
}

void initialize_llvm(int argc, char **argv) {
    INITIALIZE_LLVM(argc, argv);
}
%}

// SWIG chokes on these items
// we dont need them (yet) so just ignore them.
%ignore swift::CompilerInstance::setSILModule(std::unique_ptr<SILModule>);
%ignore swift::CompilerInstance::takeSILModule();
%ignore swift::ASTContext::getForeignRepresentationInfo(NominalTypeDecl *nominal, ForeignLanguage language, const DeclContext *dc);
%ignore swift::ASTContext::addModuleLoader(std::unique_ptr<ModuleLoader> loader, bool isClang = false);
%ignore swift::ASTContext::TypeCheckerDebug;
%ignore swift::ASTContext::SourceMgr;
%ignore swift::ASTContext::Diags;
%ignore swift::ASTContext::Impl;
%ignore swift::ASTContext::LLVMSourceMgr;
%ignore swift::ModuleDecl::forAllVisibleModules;
%ignore swift::ModuleDecl::collectLinkLibraries;
%ignore swift::FileUnit::forAllVisibleModules;
%ignore swift::FileUnit::collectLinkLibraries;
%ignore swift::SourceFile::collectLinkLibraries;
%ignore swift::SourceFile::getKind;
%ignore swift::FileUnit::getKind;
%ignore swift::MagicIdentifierLiteralExpr::getKind;
%ignore swift::Expr::isStaticallyDerivedMetatype;
%ignore swift::SILModule::BPA;

%feature("director") swift::ASTWalker;

StringRef make_stringref(const char* str);
ArrayRef<const char*> make_string_arrayref(const char** vec, int length);
DiagnosticConsumer* make_BasicDiagnosticConsumer();

%include "llvm/Support/Debug.h"
%include "swift/Basic/Compiler.h"
%include "swift/AST/ASTContext.h"
%include "swift/AST/ASTWalker.h"
%include "swift/Frontend/Frontend.h"
%include "swift/AST/Module.h"

#define LLVM_LIBRARY_VISIBILITY
//%include "swift/SIL/SILModule.h"

#define alignas(T)
%include "swift/AST/Expr.h"

void initialize_llvm(int argc, char **argv);

