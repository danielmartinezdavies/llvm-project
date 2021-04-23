//===--- MapReduceCheck.h - clang-tidy --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//@author danielmartinezdavies
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_GRPPI_MAPREDUCECHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_GRPPI_MAPREDUCECHECK_H

#include "../ClangTidyCheck.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include "clang/Rewrite/Core/Rewriter.h"
#include <clang/AST/RecursiveASTVisitor.h>
#include <iostream>
#include <string>
#include <vector>

using namespace clang::ast_matchers;
namespace clang {
	namespace tidy {
		namespace grppi {

/// FIXME: Write a short description.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/misc-map-reduce.html


			static std::vector<const Stmt *> forLoopList;

			namespace LoopConstant {
				static std::string startElement = "grppi_";
			}

			namespace Diag {
				static std::string label = "GrPPI: ";
			}

			namespace Functions {
				static bool areSameExpr(const ASTContext *Context, const Expr *First, const Expr *Second) {
					if (!First || !Second)
						return false;
					llvm::FoldingSetNodeID FirstID, SecondID;
					First->Profile(FirstID, *Context, true);
					Second->Profile(SecondID, *Context, true);
					return FirstID == SecondID;
				}

				//Consider removing
				/*static bool alreadyExploredForLoop(const Stmt *FS, const ASTContext *Context) {
					std::cout << forLoopList.size() << std::endl;
					for (const Stmt *currentFS : forLoopList) {
						if (currentFS == FS) {
							return true;
						}
					}

					return false;
				}*/

				static bool isSameVariable(const DeclarationName DN, const DeclarationName DN2) {
					if (!DN.isEmpty() && !DN2.isEmpty() && DN == DN2) {
						return true;
					}
					return false;
				}

				static inline bool isSameVariable(const ValueDecl *First, const ValueDecl *Second) {
					return First && Second &&
						   First->getCanonicalDecl() == Second->getCanonicalDecl();
				}

				template<class T>
				static bool hasElement(const std::vector<T> &list, const T &elem) {
					if (list.end() != std::find(list.begin(), list.end(), elem)) {
						return true;
					}
					return false;
				}
			}

			class CustomArray {
				public:
				CustomArray(const Expr *base, const Expr *index, const Expr *original)
						: base(base), index(index), original(original) {}

				const Expr *getBase() const { return base; }

				const Expr *getIndex() const { return index; }

				const Expr *getOriginal() const { return original; }

				private:
				const Expr *base;
				const Expr *index;
				const Expr *original;
			};

			class Prep : public PPCallbacks {
				public:
				Prep(const SourceManager &SM) : SM(SM) {}

				virtual void InclusionDirective(SourceLocation HashLoc, const Token &IncludeTok, StringRef FileName,
												bool IsAngled, CharSourceRange FilenameRange, const FileEntry *File,
												StringRef SearchPath,
												StringRef RelativePath, const Module *Imported,
												SrcMgr::CharacteristicKind FileType) override {

					std::string include_text = Lexer::getSourceText(FilenameRange, SM,
																	LangOptions()).str();
					//std::cout << "Includes: " << include_text << std::endl;
				}

				const SourceManager &SM;
			};



			class Pattern {
				public:
				Pattern(std::vector<const Expr *> Input, const Expr *Output)
						: Input(Input), Output(Output) {}

				std::vector<const Expr *> Input;
				const Expr *Output;

				void
				removeFromRewriter(SourceRange &, int &, std::vector<Pattern>::iterator, SourceManager &, Rewriter &);

			};

			class Map : public Pattern {
				public:
				Map(std::vector<const Expr *> Element, std::vector<const Expr *> Input, const Expr *Output,
					Expr *mapFunction)
						: Pattern(Input, Output), Element(Element), mapFunction(mapFunction) {}

				bool addElement(const Expr *expr, ASTContext *Context);

				bool isWithin(const Expr *expr, ASTContext *Context) const;

				BinaryOperator *getBinaryOperator() const;

				bool isCompoundAssignmentBO() const;

				std::string getOperatorAsString(SourceManager &) const;

				std::vector<const Expr *> Element;

				Expr *mapFunction;

				void removeFromRewriter(SourceRange &currentRange, int &offset, SourceManager &SM, Rewriter &rewriter) {
					currentRange = SourceRange(
							this->mapFunction->getBeginLoc().getLocWithOffset(offset),
							Lexer::getLocForEndOfToken(this->mapFunction->getEndLoc(),
													   offset, SM, LangOptions()));
					rewriter.RemoveText(currentRange);
					offset -= rewriter.getRangeSize(currentRange);
				}
			};

			class Reduce : public Pattern {
				public:
				Reduce(std::vector<const Expr *> Input, const Expr *Output, std::vector<const Expr *> Element,
					   const BinaryOperator *binary_operator,
					   const Expr *original_expr)
						: Pattern(Input, Output), Element(Element), binary_operator(binary_operator),
						  original_expr(original_expr) {}

				std::vector<const Expr *> Element;
				const BinaryOperator *binary_operator;
				const Expr *original_expr;


				std::string getOperatorAsString() const;

				std::string getIdentityAsString() const;

				void removeFromRewriter(SourceRange &currentRange, int &offset, SourceManager &SM, Rewriter &rewriter) {
					currentRange = SourceRange(
							this->original_expr->getBeginLoc().getLocWithOffset(offset),
							Lexer::getLocForEndOfToken(this->original_expr->getEndLoc(),
													   offset, SM, LangOptions()));
					rewriter.RemoveText(currentRange);
					offset -= rewriter.getRangeSize(currentRange);
				}
			};

			class LoopExplorer{

				public:
				ASTContext *Context;
				ClangTidyCheck &Check;

				public:
				const Stmt *visitingForStmt;

				protected:

				bool parallelizable = true;

				public:
				std::vector<Map> MapList;
				std::vector<Reduce> ReduceList;

				protected:
				Map placeHolderMap = {{}, {}, nullptr, nullptr};

				std::vector<const Stmt *> visitedForLoopList;
				std::vector<const FunctionDecl *> visitedFunctionDeclarationList;
				std::vector<DeclarationName> localVariables;
				std::vector<DeclarationName> exploredPointers;

				const VarDecl *iterator_variable;

				bool isThisExprValid = false;

				const bool verbose;

				const virtual Expr *getLoopContainer(Expr *write) = 0;

				public:
				LoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
						std::vector<const Stmt *> visitedForLoopList,
				std::vector<const FunctionDecl *> visitedFunctionDeclarationList,
						std::vector<DeclarationName> localVariables, bool isThisExprValid,
				const Stmt *visitingForStmt, const VarDecl *iterator, const bool verbose)
				: Context(Context), Check(Check), visitingForStmt(visitingForStmt),
				visitedForLoopList(visitedForLoopList),
				visitedFunctionDeclarationList(visitedFunctionDeclarationList),
				localVariables(localVariables), iterator_variable(iterator),
				isThisExprValid(isThisExprValid), verbose(verbose) {}

				LoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
						std::vector<const Stmt *> visitedForLoopList,
				const Stmt *visitingForStmt, const VarDecl *iterator, const bool verbose)
				: Context(Context), Check(Check), visitingForStmt(visitingForStmt),
				visitedForLoopList(visitedForLoopList), iterator_variable(iterator), verbose(verbose) {
				}
				virtual ~LoopExplorer() = default;


				virtual bool isValidWrite(Expr *write);
				bool isLocalCallee(const Expr *callee);
				const DeclRefExpr *getPointer(const Expr *S);

				virtual const Stmt* getVisitingForLoopBody() = 0;
				virtual bool haveSameVisitingForLoopHeaderExpressions(const Stmt*) = 0;

				const StringRef getSourceText(const Expr*) const;
				const StringRef getSourceText(const SourceRange) const;

				// Auxiliary Functions
				bool PointerHasValidLastValue(const VarDecl *pointerVarDecl, const Expr *expr);

				bool isParallelizable();
				bool isMapPattern();
				bool isReducePattern();
				bool isMapReducePattern();
				bool isMapReducePattern(std::vector<Map> MapList, std::vector<Reduce> ReduceList);

				bool isMapReducePattern(std::shared_ptr<LoopExplorer> le);

				virtual bool isMapAssignment(Expr *write) = 0;
				Expr *isReduceCallExpr(const Expr *expr);
				std::vector<const Expr *> getReduceElementsFromCallExpr(const Expr *expr);
				virtual bool isLoopElem(Expr *write) = 0;
				Reduce *isReduceAssignment(const BinaryOperator *BO);
				bool isRepeatedForStmt(const Stmt *FS);
				void appendForLoopList();

				std::vector<DeclarationName> getValidParameterList(FunctionDecl *functionDeclaration);

				void appendVisitedFunctionDeclarationList(std::vector<const FunctionDecl *> visitedFunctionDeclarations);

				bool isLocalVariable(DeclarationName DN);

				//Transformation
				virtual std::string getArrayBeginOffset() const;
				virtual std::string getArrayEndOffset() const;
				virtual std::string getArrayEndString() const;

				virtual bool isVariableUsedInArraySubscript(const DeclRefExpr *dre);

				virtual std::string getMultipleInputTransformation();

				virtual std::string getBeginInputAsString(const DeclRefExpr *inputName);
				virtual std::string getEndInputAsString(const DeclRefExpr *inputName);
				virtual std::string getOutputAsString(const DeclRefExpr *output);
				virtual std::string getElementAsString(const DeclRefExpr *elem) const;


				std::string getBeginInputTransformation(const DeclRefExpr *expr);
				std::string getCloseBeginInputTransformation(const DeclRefExpr *expr);
				std::string getEndInputTransformation(const DeclRefExpr *expr);
				std::string getCloseEndInputTransformation(const DeclRefExpr *expr);
				std::string getStartOffsetString();
				std::string getEndOffsetString();


				//Pattern Transformation
				std::string getPatternTransformationInput(Pattern &pattern);
				std::string getPatternTransformationInputEnd(const Pattern &pattern);
				std::string getMapTransformationOutput(const Pattern &pattern);
				std::string getMapTransformationLambdaParameters(Map &map);

				template<typename Pattern>
				void removePatternFromRewriter(SourceRange &currentRange, int &offset, std::vector<Pattern> PatterList,
											   typename std::vector<Pattern>::iterator pattern, SourceManager &SM,
											   Rewriter &rewriter) {
					for (typename std::vector<Pattern>::iterator otherPattern = PatterList.begin();
						 otherPattern != PatterList.end(); otherPattern++) {
						if (otherPattern != pattern) {
							otherPattern->removeFromRewriter(currentRange, offset, SM, rewriter);
						}
					}
				}

				template<typename Pattern>
				void removePatternFromRewriter(SourceRange &currentRange, int &offset, std::vector<Pattern> PatterList,
											   SourceManager &SM, Rewriter &rewriter) {
					for (typename std::vector<Pattern>::iterator otherPattern = PatterList.begin();
						 otherPattern != PatterList.end(); otherPattern++) {
						otherPattern->removeFromRewriter(currentRange, offset, SM, rewriter);
					}
				}

				std::string getMapTransformationLambdaBody(const std::vector<Map>::iterator &map);
				std::string getReduceTransformationLambdaParameters(const Reduce &reduce);
				std::string getReduceTransformationLambdaBody(const std::vector<Reduce>::iterator &reduce);

				std::string getMapTransformation();
				std::string getReduceTransformation();
				std::string getMapReduceTransformation();
				std::string getMapReduceTransformation(LoopExplorer&, LoopExplorer&);
			};

			static std::vector<std::shared_ptr<LoopExplorer>> LoopExplorerList;

			// Loop Visitor
			template<class LoopType>
			class LoopVisitor : public LoopExplorer,  public RecursiveASTVisitor<LoopType> {

				public:
				LoopVisitor(ASTContext *context, ClangTidyCheck &check,
							const std::vector<const Stmt *> &visitedForLoopList,
							const std::vector<const FunctionDecl *> &visitedFunctionDeclarationList,
							const std::vector<DeclarationName> &localVariables, bool isThisExprValid,
							const Stmt *visitingForStmt, const VarDecl *iterator, const bool verbose)
						: LoopExplorer(context, check, visitedForLoopList, visitedFunctionDeclarationList,
									   localVariables, isThisExprValid, visitingForStmt, iterator, verbose) {}

				LoopVisitor(ASTContext *context, ClangTidyCheck &check,
							const std::vector<const Stmt *> &visitedForLoopList, const Stmt *visitingForStmt,
							const VarDecl *iterator, const bool verbose) : LoopExplorer(context, check,
																						visitedForLoopList,
																						visitingForStmt, iterator,
																						verbose) {}

				public:
				// Traverse Function
				bool TraverseLambdaExpr(LambdaExpr *LE) {
					parallelizable = false;
					return true;
				}

				bool VisitCXXThrowExpr(CXXThrowExpr *CXXTE) {
					parallelizable = false;
					if (verbose) {
						Check.diag(CXXTE->getBeginLoc(),
								   Diag::label + "Throw exception makes loop parallelization unsafe");
					}
					return true;
				}

				bool VisitGotoStmt(GotoStmt *GS) {
					parallelizable = false;
					if (verbose) {
						Check.diag(GS->getBeginLoc(),
								   Diag::label + "Goto statement makes loop parallelization unsafe");
					}
					return true;
				}

				// Visit Functions
				bool VisitForStmt(ForStmt *FS) {
					if (!isRepeatedForStmt(FS)) {
						visitedForLoopList.push_back(FS);
					}
					return true;
				}

				// New local variables
				bool VisitVarDecl(VarDecl *VD) {
					// find declaration of all local variables
					if (!VD->hasGlobalStorage() && !VD->getType()->isReferenceType()) {
						localVariables.push_back(VD->getDeclName());
					}
					if (VD->getName().startswith(LoopConstant::startElement)) {
						parallelizable = false;
					}
					return true;
				}

				bool VisitDeclRefExpr(DeclRefExpr *DRE) {
					if (DRE->getDecl()->getName().startswith(LoopConstant::startElement)) {
						parallelizable = false;
					}
					if (Functions::isSameVariable(DRE->getDecl(), iterator_variable)) {
						if (!isVariableUsedInArraySubscript(DRE)) {
							parallelizable = false;
							if (verbose) {
								Check.diag(DRE->getBeginLoc(),
										   Diag::label +
										   "Loop variable used outside of array subscript making loop parallelization impossible");
							}
						}
					}
					return true;

				}

				// Jump statements
				bool VisitCXXConstructExpr(CXXConstructExpr *CXXCE) {
					// Check constructor
					if (CXXCE->getConstructor() != nullptr) {
						// Only explore only if not explored before
						if (!Functions::hasElement<const FunctionDecl *>(
								visitedFunctionDeclarationList, CXXCE->getConstructor())) {
							std::vector<DeclarationName> functionVariables;
							/*LoopType constructorExpr(Context, Check, visitedForLoopList,
													 visitedFunctionDeclarationList,
													 functionVariables, true, visitingForStmt, iterator_variable,
													 verbose);
							constructorExpr.TraverseStmt(CXXCE->getConstructor()->getBody());
							if (!constructorExpr.isParallelizable()) {
								parallelizable = false;
							}
							constructorExpr.appendForLoopList();
							visitedFunctionDeclarationList.push_back(CXXCE->getConstructor());*/
						}
						// Check Destructor
						if (CXXCE->getConstructor()->getParent() != nullptr &&
							CXXCE->getConstructor()->getParent()->getDestructor() != nullptr) {
							auto *destructor = CXXCE->getConstructor()->getParent()->getDestructor();
							if (!Functions::hasElement<const FunctionDecl *>(
									visitedFunctionDeclarationList, destructor)) {

								std::vector<DeclarationName> functionVariables;
								/*LoopType destructorExpr(Context, Check, visitedForLoopList,
														visitedFunctionDeclarationList,
														functionVariables, true, visitingForStmt,
														iterator_variable, verbose);
								destructorExpr.TraverseStmt(destructor->getBody());
								if (!destructorExpr.isParallelizable()) {
									parallelizable = false;
								}
								destructorExpr.appendForLoopList();
								visitedFunctionDeclarationList.push_back(destructor);*/
							}
						}
					}
					return true;
				}

				bool VisitCXXOperatorCallExpr(CXXOperatorCallExpr *OO) {
					if (verbose) {
						Check.diag(OO->getBeginLoc(),
								   Diag::label + "Overloaded operator may be unsafe");
					}
					return true;
				}

				bool VisitCallExpr(CallExpr *CE) {
					// visit
					bool isLocCallee = false;
					if (const auto *callee = dyn_cast<MemberExpr>(CE->getCallee())) {
						isLocCallee = isLocalCallee(callee);
					}

					FunctionDecl *functionDeclaration = CE->getDirectCallee();
					if (functionDeclaration != nullptr &&
						functionDeclaration->getBody() != nullptr) {
						std::vector<DeclarationName> validParameterList =
								getValidParameterList(functionDeclaration);
						if (!isVisitedFunctionDecl(functionDeclaration)) {

							visitedFunctionDeclarationList.push_back(functionDeclaration);

							/*LoopType callExpr(Context, Check, visitedForLoopList,
											  visitedFunctionDeclarationList, validParameterList,
											  isLocCallee, visitingForStmt, iterator_variable, verbose);
							callExpr.TraverseStmt(functionDeclaration->getBody());
							if (!callExpr.isParallelizable()) {
								if (verbose) {
									Check.diag(CE->getBeginLoc(),
											   Diag::label + "Call expression unsafe");
								}
							}
							callExpr.appendForLoopList();*/
						}
					} else {
						if (verbose) {
							Check.diag(CE->getBeginLoc(),
									   Diag::label + "Call expression unexplorable, could be unsafe");
						}
					}
					return true;
				}

				// Write and read
				bool VisitUnaryOperator(UnaryOperator *UO) {
					if (UO->isIncrementOp() || UO->isDecrementOp()) {
						if (isMapAssignment(UO->getSubExpr())) {
							/*Map m(placeHolderMap.Element, placeHolderMap.Input, getOutput(UO), UO);
							MapList.push_back(m);*/
						}
						isValidWrite(UO->getSubExpr());
						return true;
					} else if (UO->getOpcode() == UO_Deref) {
						if (!isMapAssignment(UO->IgnoreParenImpCasts())) { parallelizable = false; }


					}
					return true;
				}

				// Writes and reads
				bool VisitBinaryOperator(BinaryOperator *BO) {
					//'=' assign operator
					//'+=', etc
					// BO->getOverloadedOperator(BO->getOpcode());getBeginLoc()
					if (BO->isAssignmentOp()) {
						// write variables
						Expr *LHS = BO->getLHS();
						Reduce *r = isReduceAssignment(BO);
						if (r != nullptr) {
							ReduceList.push_back(*r);
						} else if (isMapAssignment(LHS)) {
							Map m(placeHolderMap.Element, placeHolderMap.Input, getLoopContainer(LHS), BO);
							MapList.push_back(m);
						} else {
							isValidWrite(LHS);
						}
					}
					return true;
				}

				bool isVisitedFunctionDecl(FunctionDecl *FD) {
					for (const FunctionDecl *currentFunctionDecl :
							visitedFunctionDeclarationList) {
						if (currentFunctionDecl == FD) {
							return true;
						}
					}
					return false;
				}
			};


			class IntegerForLoopExplorer : public LoopVisitor<IntegerForLoopExplorer> {
				private:
				std::vector<CustomArray> readArraySubscriptList;
				std::vector<CustomArray> writeArraySubscriptList;

				const Expr *start_expr;
				const Expr *end_expr;

				const uint64_t LoopSizeMin = 0;


				public:
				IntegerForLoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
									   std::vector<const Stmt *> visitedForLoopList,
									   const Stmt *visitingForStmt, const Expr *start_expr, const Expr *end_expr,
									   const VarDecl *iterator, const uint64_t LoopSizeMin, const bool verbose)
						: LoopVisitor(Context, Check, visitedForLoopList, visitingForStmt, iterator, verbose),
						  start_expr(start_expr),
						  end_expr(end_expr), LoopSizeMin(LoopSizeMin) {
					if (!isRequiredMinSize()) parallelizable = false;
				}

				IntegerForLoopExplorer(
						ASTContext *Context, ClangTidyCheck &Check,
						std::vector<const Stmt *> visitedForLoopList,
						std::vector<const FunctionDecl *> visitedFunctionDeclarationList,
						std::vector<DeclarationName> localVariables, bool isThisExprValid,
						const Stmt *visitingForStmt, const VarDecl *iterator, const bool verbose)
						: LoopVisitor(Context, Check, visitedForLoopList,
									  visitedFunctionDeclarationList, localVariables,
									  isThisExprValid, visitingForStmt, iterator, verbose) {}

				const Expr *getLoopContainer(Expr *write) override;

				const Stmt* getVisitingForLoopBody() override;
				bool haveSameVisitingForLoopHeaderExpressions(const Stmt*) override;

				bool isArrayLoopElem(CustomArray);

				bool isVariableUsedInArraySubscript(const DeclRefExpr *dre) override;

				bool isRequiredMinSize();

				std::string getArrayBeginOffset() const override;

				std::string getArrayEndOffset() const override;

				std::string getArrayEndString() const override;

				std::string getEndInputAsString(const DeclRefExpr *inputName) override;

				std::unique_ptr<const uint64_t> getStartValue() const;

				std::unique_ptr<const uint64_t> getEndValue() const;

				bool VisitArray(CustomArray);

				bool VisitArraySubscriptExpr(ArraySubscriptExpr *ase);

				bool VisitCXXOperatorCallExpr(CXXOperatorCallExpr *OO);


				protected:
				bool addInput(Map &map, const Expr *expr);

				//Check if arraysubscript is integer literal or iterator. If integer literal, return 2 if out of range. Else if valid, return 1
				int isValidArraySubscript(CustomArray);

				bool addToReadArraySubscriptList(CustomArray, ASTContext *context);

				bool addToWriteArraySubscriptList(CustomArray, ASTContext *context);

				bool isLoopElem(Expr *write) override;

				bool isMapAssignment(Expr *write) override;

			};

			class ContainerForLoopExplorer : public LoopVisitor<ContainerForLoopExplorer> {
				protected:
				const DeclRefExpr *LoopContainer = nullptr;
				std::vector<Expr *> writeList;

				bool isLoopElem(Expr *write) override;

				bool isMapAssignment(Expr *write) override;

				const Expr *getLoopContainer(Expr *write) override;

				const Stmt* getVisitingForLoopBody() override;
				bool haveSameVisitingForLoopHeaderExpressions(const Stmt*) override;

				std::string getElementAsString(const DeclRefExpr *elem) const override;

				public:
				ContainerForLoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
										 std::vector<const Stmt *> visitedForLoopList,
										 const Stmt *visitingForStmt, const VarDecl *iterator,
										 const DeclRefExpr *traversalArray, const bool verbose)
						: LoopVisitor(Context, Check, visitedForLoopList, visitingForStmt, iterator, verbose),
						  LoopContainer(traversalArray) {

				}

				ContainerForLoopExplorer(
						ASTContext *Context, ClangTidyCheck &Check,
						std::vector<const Stmt *> visitedForLoopList,
						std::vector<const FunctionDecl *> visitedFunctionDeclarationList,
						std::vector<DeclarationName> localVariables, bool isThisExprValid,
						const Stmt *visitingForStmt, const VarDecl *iterator, const bool verbose)
						: LoopVisitor(Context, Check, visitedForLoopList,
									  visitedFunctionDeclarationList, localVariables,
									  isThisExprValid, visitingForStmt, iterator, verbose) {}

				bool VisitCXXOperatorCallExpr(CXXOperatorCallExpr *OO);

				bool VisitUnaryOperator(UnaryOperator *UO);

				DeclRefExpr *isValidDereference(Expr *expr);

				bool ExploreDereference(Expr *expr);


			};

			class RangeForLoopExplorer : public LoopVisitor<RangeForLoopExplorer> {
				protected:
				const DeclRefExpr *LoopContainer = nullptr;
				std::vector<DeclRefExpr *> writeList;

				DeclRefExpr *isElemDeclRefExpr(Expr *expr);

				const Stmt* getVisitingForLoopBody() override;
				bool haveSameVisitingForLoopHeaderExpressions(const Stmt *) override;

				bool isLoopElem(Expr *write) override;

				bool isMapAssignment(Expr *write) override;

				const Expr *getLoopContainer(Expr *write) override;

				std::string getMultipleInputTransformation() override;

				std::string getBeginInputAsString(const DeclRefExpr *inputName) override;

				std::string getEndInputAsString(const DeclRefExpr *inputName) override;

				std::string getOutputAsString(const DeclRefExpr *output) override;

				std::string getElementAsString(const DeclRefExpr *elem) const override;

				public:
				RangeForLoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
									 std::vector<const Stmt *> visitedForLoopList,
									 const Stmt *visitingForStmt, const VarDecl *iterator,
									 const DeclRefExpr *traversalArray, const bool verbose)
						: LoopVisitor(Context, Check, visitedForLoopList, visitingForStmt, iterator, verbose),
						  LoopContainer(traversalArray) {
				}

				RangeForLoopExplorer(
						ASTContext *Context, ClangTidyCheck &Check,
						std::vector<const Stmt *> visitedForLoopList,
						std::vector<const FunctionDecl *> visitedFunctionDeclarationList,
						std::vector<DeclarationName> localVariables, bool isThisExprValid,
						const Stmt *visitingForStmt, const VarDecl *iterator, const bool verbose)
						: LoopVisitor(Context, Check, visitedForLoopList,
									  visitedFunctionDeclarationList, localVariables,
									  isThisExprValid, visitingForStmt, iterator, verbose) {}

				bool VisitDeclRefExpr(DeclRefExpr *DRE);
			};

			class MapReduceCheck : public ClangTidyCheck {
				public:
				MapReduceCheck(StringRef name, ClangTidyContext *context)
						: ClangTidyCheck(name, context),
						  IntegerForLoopSizeMin(Options.get("IntegerForLoopSizeMin", 0)),
						  Verbose(Options.get("Verbose", false)) {}

				void registerMatchers(ast_matchers::MatchFinder *Finder) override;

				void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
										 Preprocessor *ModuleExpanderPP) override {
					std::unique_ptr<Prep> prep_callback(new Prep(SM));
					PP->addPPCallbacks(std::move(prep_callback));
				}

				void storeOptions(ClangTidyOptions::OptionMap &Opts) override;

				void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

				void ProcessIntegerForLoop(const ForStmt *integerForLoop, const MatchFinder::MatchResult &Result);

				void ProcessIteratorForLoop(const ForStmt *iteratorForLoop, const MatchFinder::MatchResult &Result);

				void ProcessRangeForLoop(const CXXForRangeStmt *rangeForLoop, const MatchFinder::MatchResult &Result);

				template<class Loop>
				void addDiagnostic(std::shared_ptr<LoopExplorer> currentPattern, Loop loop) {
					std::shared_ptr<LoopExplorer> previousLoopExplorer = nullptr;
					if(!LoopExplorerList.empty())
						 previousLoopExplorer = LoopExplorerList.back();

					LoopExplorerList.push_back(currentPattern);

					//TODO: make sure map reduce map output is not used later
					if (currentPattern->isParallelizable() && previousLoopExplorer != nullptr && currentPattern->isMapReducePattern(previousLoopExplorer)) {
						SourceRange range = SourceRange(previousLoopExplorer->visitingForStmt->getBeginLoc(), loop->getEndLoc());
						diag(previousLoopExplorer->visitingForStmt->getBeginLoc(),
							 Diag::label + "MapReduce pattern detected. Loops can be merged",
							 DiagnosticIDs::Remark) << FixItHint::CreateReplacement(range,
																					currentPattern->getMapReduceTransformation(*previousLoopExplorer, *currentPattern));
					}
					if (currentPattern->isMapReducePattern() && currentPattern->isParallelizable()) {
						diag(loop->getBeginLoc(),
							 Diag::label + "MapReduce pattern detected. Loop can be parallelized.",
							 DiagnosticIDs::Remark) << FixItHint::CreateReplacement(loop->getSourceRange(),
																					currentPattern->getMapReduceTransformation());
					} else if (currentPattern->isReducePattern() && !currentPattern->isMapPattern() &&
							   currentPattern->isParallelizable()) {
						diag(loop->getBeginLoc(),
							 Diag::label + "Reduce pattern detected. Loop can be parallelized.",
							 DiagnosticIDs::Remark)
								<< FixItHint::CreateReplacement(loop->getSourceRange(),
																currentPattern->getReduceTransformation());
					} else if (currentPattern->isMapPattern() && !currentPattern->isReducePattern() &&
							   currentPattern->isParallelizable()) {
						diag(loop->getBeginLoc(),
							 Diag::label + "Map pattern detected. Loop can be parallelized.",
							 DiagnosticIDs::Remark)
								<< FixItHint::CreateReplacement(loop->getSourceRange(),
																currentPattern->getMapTransformation());

					} else {
						diag(loop->getBeginLoc(), Diag::label + "No parallelization possible.");
					}
				}

				private:
				const uint64_t IntegerForLoopSizeMin;
				const bool Verbose = false;

			};


		} // namespace grppi
	} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_MAPREDUCECHECK_H
