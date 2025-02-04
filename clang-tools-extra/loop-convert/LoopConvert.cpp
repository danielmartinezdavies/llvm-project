
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include <clang/Frontend/CompilerInstance.h>
#include <iostream>
#include "clang/Tooling/Refactoring.h"

using namespace clang::tooling;
using namespace llvm;
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang;
using namespace clang::ast_matchers;

StatementMatcher LoopMatcher =
    forStmt(hasLoopInit(declStmt(hasSingleDecl(varDecl(
                hasInitializer(integerLiteral(equals(0)))))))).bind("forLoop");

class LoopPrinter : public MatchFinder::MatchCallback {
public :
  Rewriter &r;
  LoopPrinter(Rewriter &r): r(r){};
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (const ForStmt *FS = Result.Nodes.getNodeAs<clang::ForStmt>("forLoop")) {
      auto & Context = Result.Context;
      SourceManager &SM = Context->getSourceManager();

      //Rewriter r(SM, LangOptions());
      r.InsertTextAfter(FS->getBeginLoc(), "/*Hello*/");
      //r.overwriteChangedFiles();
      const RewriteBuffer *RewriteBuf = r.getRewriteBufferFor(SM.getMainFileID());
      llvm::outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
    }
      //FS->dump();
  }
};

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

int main(int argc, const char **argv) {
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!ExpectedParser) {
    // Fail gracefully for unsupported options.
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser& OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  std::vector< std::unique_ptr< ASTUnit >> ASTs;
  Tool.buildASTs(ASTs);
  for(auto & ast: ASTs){

    auto & SM = ast->getSourceManager();
    SourceLocation line = SM.translateLineCol(SM.getMainFileID(), 2, 0);
    ast->getASTContext().getTranslationUnitDecl();
    Rewriter r(SM, LangOptions());
    r.InsertTextAfter(line, "//Im sorry");
    LoopPrinter Printer(r);

    MatchFinder Finder;
    Finder.addMatcher(LoopMatcher, &Printer);
    Tool.run(newFrontendActionFactory(&Finder).get());
    r.overwriteChangedFiles();
    //r.overwriteChangedFiles();
  }


  return 0;
}

