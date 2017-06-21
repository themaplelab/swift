%module(directors="1") swiftc

%include "std_string.i"

// char** adapter adapted from swig docs - version in docs is out of date and no longer works
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
    // TODO can we do a typemap instead like for std::string?
    int len = strlen(str);
    char *copy = new char[len+1];
    memcpy(copy, str, len+1);
    return StringRef(copy);
}
ArrayRef<const char*> make_string_arrayref(const char** vec, int length) {
    // TODO this may be broken because we need to make a copy. See make_stringref above
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
        DiagnosticEngine::formatDiagnosticText(llvm::dbgs(), FormatString, FormatArgs);
        llvm::dbgs() << "\n";
    }
};

// For some reason these methods are not linked into any swift library.
// We need them to make rtti for DiagnosticConsumer available.
// Definitions are copied from swift.
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

SILModule *constructSILModule(ModuleDecl *module, SILOptions &options) {
    return SILModule::constructSIL(module, options).release();
}

void setSILModule(CompilerInstance *instance, SILModule *module) {
    instance->setSILModule(std::unique_ptr<SILModule>(module));
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
%ignore swift::SILModule::functions;
%ignore swift::SILModule::zombieFunctions;
%ignore swift::SILModule::zombieFunctionNames;
%ignore swift::SILModule::Types;
%ignore swift::SILModule::constructSIL; // TODO we will need a wrapper for this
%ignore swift::SILModule::createEmptyModule;
%ignore swift::SILModule::getWitnessTables;
%ignore swift::SILModule::getDefaultWitnessTables;
%ignore swift::SILModule::getVTables;
%ignore swift::SILModule::getGlobals;
%ignore swift::SILModule::getSILGlobals;
%ignore swift::SILModule::getCoverageMaps;
%ignore swift::SILModule::getFunctions;
%ignore swift::RequirementRepr::getLayoutConstraint;
%ignore swift::ExtensionDecl::getMembers;
%ignore swift::LiteralExpr::shallowClone;
%ignore swift::ObjectLiteralExpr::create;
%ignore swift::ValueDecl::getFormalAccessScope;
%ignore swift::NominalTypeDecl::getStoredProperties;
%ignore swift::NominalTypeDecl::getMembers;
%ignore swift::EnumDecl::getAllElements;
%ignore swift::EnumDecl::getAllCases;
%ignore swift::ProtocolDecl::walkInheritedProtocols;
%ignore swift::ProtocolDecl::getDeclaredType;
%ignore swift::AbstractFunctionDecl::getForeignErrorConvention;
%ignore getDeserializationCallbacks;

%feature("director") swift::ASTWalker;

StringRef make_stringref(const char* str);
ArrayRef<const char*> make_string_arrayref(const char** vec, int length);
DiagnosticConsumer* make_BasicDiagnosticConsumer();
void initialize_llvm(int argc, char **argv);

%include "llvm/Support/Debug.h"
%include "swift/Basic/Compiler.h"
%include "swift/AST/ASTContext.h"
%include "swift/AST/ASTWalker.h"
%include "swift/Frontend/Frontend.h"
%include "swift/AST/Module.h"

#define LLVM_LIBRARY_VISIBILITY
%include "swift/SIL/SILModule.h"
%include "swift/AST/SILOptions.h"

#define alignas(T)
#define final
%include "swift/AST/Expr.h"
%include "swift/AST/Decl.h"

SILModule *constructSILModule(ModuleDecl *module, SILOptions &options);
void setSILModule(CompilerInstance *instance, SILModule *module);
