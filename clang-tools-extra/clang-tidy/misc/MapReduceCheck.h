//===--- MapReduceCheck.h - clang-tidy --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//@author danielmartinezdavies
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_MAPREDUCECHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_MAPREDUCECHECK_H

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
		namespace misc {

/// FIXME: Write a short description.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/misc-map-reduce.html


			static std::vector<const Stmt *> forLoopList;

			namespace LoopConstant{
				static std::string startElement = "grppi_";
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

				static bool alreadyExploredForLoop(const Stmt *FS) {
					for (const Stmt *currentFS : forLoopList) {
						if (currentFS == FS)
							return true;
					}
					return false;
				}

				static bool isSameVariable(const DeclarationName DN, const DeclarationName DN2) {
					if (!DN.isEmpty() && !DN2.isEmpty() && DN == DN2) {
						return true;
					}
					return false;
				}

				static bool isSameVariable(const ValueDecl *First, const ValueDecl *Second) {
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
												SrcMgr::CharacteristicKind FileType) {

					std::string include_text = Lexer::getSourceText(FilenameRange, SM,
																	LangOptions()).str();
					//std::cout << "Includes: " << include_text << std::endl;
				}

				const SourceManager &SM;
			};


			class MapReduceCheck : public ClangTidyCheck {
				public:
				MapReduceCheck(StringRef name, ClangTidyContext *context)
						: ClangTidyCheck(name, context) {}

				void registerMatchers(ast_matchers::MatchFinder *Finder) override;

				void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
										 Preprocessor *ModuleExpanderPP) override {
					std::unique_ptr<Prep> prep_callback(new Prep(SM));
					PP->addPPCallbacks(std::move(prep_callback));
				}

				void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

				void ProcessIntegerForLoop(const ForStmt *integerForLoop, const MatchFinder::MatchResult &Result);

				void ProcessIteratorForLoop(const ForStmt *iteratorForLoop, const MatchFinder::MatchResult &Result);

				void ProcessRangeForLoop(const CXXForRangeStmt *rangeForLoop, const MatchFinder::MatchResult &Result);

				template<class LoopExplorer, class Loop>
				void addDiagnostic(LoopExplorer currentMap, Loop loop) {
					if (currentMap.isReducePattern() && currentMap.isParallelizable()) {
						diag(loop->getBeginLoc(),
							 "Reduce pattern detected. Loop can be parallelized.");
					} else if (currentMap.isMapPattern() && currentMap.isParallelizable()) {
						diag(loop->getBeginLoc(),
							 "Map pattern detected. Loop can be parallelized.",
							 DiagnosticIDs::Remark)
								<< FixItHint::CreateReplacement(loop->getSourceRange(),
																currentMap.getMapTransformation());

					} else {
						diag(loop->getBeginLoc(), "No parallelization possible.");
					}
				}
			};

			class Pattern {
				public:
				Pattern( std::vector<const Expr *> Input, const Expr *Output)
				:  Input(Input), Output(Output) {}
				std::vector<const Expr *> Input;
				const Expr *Output;
			};

			class Map : public Pattern {
				public:
				Map(std::vector<const Expr *> Element, std::vector<const Expr *> Input, const Expr *Output,
					Expr *mapFunction)
						: Element(Element), mapFunction(mapFunction), Pattern(Input, Output) {}

				bool addElement(const Expr *expr, ASTContext *Context);

				bool isWithin(const Expr *expr, ASTContext *Context) const;

				std::vector<const Expr *> Element;

				Expr *mapFunction;
			};

			class Reduce : public Pattern{
				public:
				Reduce(std::vector<const Expr *> Input, const Expr* Output, const BinaryOperator* binary_operator)
					:  binary_operator(binary_operator), Pattern(Input, Output){}

				const BinaryOperator* binary_operator;

			};


// Loop Explorer
			template<class LoopType>
			class LoopExplorer : public RecursiveASTVisitor<LoopType> {
				protected:
				ASTContext *Context;
				ClangTidyCheck &Check;

				const Stmt *visitingForStmtBody;

				bool parallelizable = true;
				std::vector<Map> MapList;
				std::vector<Reduce> ReduceList;
				Map placeHolderMap = {{}, {}, nullptr, nullptr};


				std::vector<const Stmt *> visitedForLoopList;
				std::vector<const FunctionDecl *> visitedFunctionDeclarationList;
				std::vector<DeclarationName> localVariables;
				std::vector<DeclarationName> exploredPointers;

				const VarDecl *iterator_variable;

				bool isThisExprValid = false;


				LoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
							 std::vector<const Stmt *> visitedForLoopList,
							 std::vector<const FunctionDecl *> visitedFunctionDeclarationList,
							 std::vector<DeclarationName> localVariables, bool isThisExprValid,
							 const Stmt *visitingForStmtBody, const VarDecl *iterator)
						: Context(Context), Check(Check), visitingForStmtBody(visitingForStmtBody),
						  visitedForLoopList(visitedForLoopList),
						  visitedFunctionDeclarationList(visitedFunctionDeclarationList),
						  localVariables(localVariables), iterator_variable(iterator),
						  isThisExprValid(isThisExprValid) {}

				virtual ~LoopExplorer() = default;

				const virtual Expr *getOutput(Expr *write) = 0;

				public:
				LoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
							 std::vector<const Stmt *> visitedForLoopList,
							 const Stmt *visitingForStmtBody, const VarDecl *iterator)
						: Context(Context), Check(Check), visitingForStmtBody(visitingForStmtBody),
						  visitedForLoopList(visitedForLoopList), iterator_variable(iterator) {
				}

				// Traverse Function
				bool TraverseLambdaExpr(LambdaExpr *LE) {
					parallelizable = false;
					return true;
				}

				bool VisitCXXThrowExpr(CXXThrowExpr *CXXTE) {
					parallelizable = false;
					Check.diag(CXXTE->getBeginLoc(),
							   "Throw exception makes loop parallelization unsafe");
					return true;
				}

				bool VisitGotoStmt(GotoStmt *GS) {
					parallelizable = false;
					Check.diag(GS->getBeginLoc(),
							   "Goto statement makes loop parallelization unsafe");
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
							Check.diag(DRE->getBeginLoc(),
									   "Loop variable used outside of array subscript making loop parallelization impossible");
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
							LoopType constructorExpr(Context, Check, visitedForLoopList,
													 visitedFunctionDeclarationList,
													 functionVariables, true, visitingForStmtBody, iterator_variable);
							constructorExpr.TraverseStmt(CXXCE->getConstructor()->getBody());
							if (!constructorExpr.isParallelizable()) {
								parallelizable = false;
							}
							constructorExpr.appendForLoopList();
							visitedFunctionDeclarationList.push_back(CXXCE->getConstructor());
						}
						// Check Destructor
						if (CXXCE->getConstructor()->getParent() != nullptr &&
							CXXCE->getConstructor()->getParent()->getDestructor() != nullptr) {
							auto *destructor = CXXCE->getConstructor()->getParent()->getDestructor();
							if (!Functions::hasElement<const FunctionDecl *>(
									visitedFunctionDeclarationList, destructor)) {

								std::vector<DeclarationName> functionVariables;
								LoopType destructorExpr(Context, Check, visitedForLoopList,
														visitedFunctionDeclarationList,
														functionVariables, true, visitingForStmtBody,
														iterator_variable);
								destructorExpr.TraverseStmt(destructor->getBody());
								if (!destructorExpr.isParallelizable()) {
									parallelizable = false;
								}
								destructorExpr.appendForLoopList();
								visitedFunctionDeclarationList.push_back(destructor);
							}
						}
					}
					return true;
				}

				bool VisitCXXOperatorCallExpr(CXXOperatorCallExpr *OO) {
					Check.diag(OO->getBeginLoc(),
							   "Overloaded operator may be unsafe");
					return true;
				}

				bool VisitCallExpr(CallExpr *CE) {
					// visit
					bool isLocCallee = false;
					if (auto *callee = dyn_cast<MemberExpr>(CE->getCallee())) {
						isLocCallee = isLocalCallee(callee);
					}

					FunctionDecl *functionDeclaration = CE->getDirectCallee();
					if (functionDeclaration != nullptr &&
						functionDeclaration->getBody() != nullptr) {
						std::vector<DeclarationName> validParameterList =
								getValidParameterList(functionDeclaration);
						if (!isVisitedFunctionDecl(functionDeclaration)) {

							visitedFunctionDeclarationList.push_back(functionDeclaration);

							LoopType callExpr(Context, Check, visitedForLoopList,
											  visitedFunctionDeclarationList, validParameterList,
											  isLocCallee, visitingForStmtBody, iterator_variable);
							callExpr.TraverseStmt(functionDeclaration->getBody());
							if (!callExpr.isParallelizable()) {
								Check.diag(CE->getBeginLoc(),
										   "Call expression unsafe");
							}
							callExpr.appendForLoopList();
						}
					} else {
						Check.diag(CE->getBeginLoc(),
								   "Call expression unexplorable, could be unsafe");
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
						// TODO implement dereference
						parallelizable = false;
						if (auto *BO_LHS =
								dyn_cast<DeclRefExpr>(UO->getSubExpr()->IgnoreParenImpCasts()))
							return true;
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
							Map m(placeHolderMap.Element, placeHolderMap.Input, getOutput(LHS), BO);
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

				virtual bool isValidWrite(Expr *write) {
					write = write->IgnoreParenImpCasts();
					// possibility of removing
					if (isMapAssignment(write))
						return true;
					if (auto *BO_LHS = dyn_cast<DeclRefExpr>(write)) {
						if (!isLocalVariable(BO_LHS->getDecl()->getDeclName())) {
							Check.diag(write->getBeginLoc(),
									   "Write to variable declared outside for loop statement "
									   "makes loop parallelization unsafe");
							parallelizable = false;
							return false;
						}
						return true;
					} else if (auto *BO_LHS = dyn_cast<MemberExpr>(write)) {
						bool isValidMember = true;
						if (auto *memberDecl = dyn_cast<VarDecl>(BO_LHS->getMemberDecl())) {
							if (memberDecl->hasGlobalStorage()) {
								Check.diag(BO_LHS->getBeginLoc(),
										   "Write to variable stored globally makes loop "
										   "parallelization unsafe");
								parallelizable = false;
								isValidMember = false;
							}
						}
						bool isValidBase = isValidWrite(BO_LHS->getBase());
						if (!isValidMember)
							return false;
						return isValidBase;
					} else if (auto *BO_LHS = dyn_cast<CXXThisExpr>(write)) {
						if (!isThisExprValid) {
							Check.diag(BO_LHS->getBeginLoc(), "Write to variable stored globally "
															  "makes loop parallelization unsafe");
							parallelizable = false;
							return false;
						}
						return true;
					} else if (auto *BO_LHS =
							dyn_cast<CXXOperatorCallExpr>(write)) {
						Check.diag(BO_LHS->getBeginLoc(),
								   "Write to overloaded operator could be unsafe");
						return true;
					} else {
						Check.diag(write->getBeginLoc(),
								   "Write to type that is not variable or array subscript of loop variable makes "
								   "for loop parallelization unsafe");
						parallelizable = false;
						return false;
					}
				}

				bool isLocalCallee(Expr *callee) {
					callee = callee->IgnoreParenImpCasts();
					if (auto *BO_LHS = dyn_cast<DeclRefExpr>(callee)) {
						if (!isLocalVariable(BO_LHS->getDecl()->getDeclName())) {
							std::cout << BO_LHS->getDecl()->getDeclName().getAsString()
									  << std::endl;
							return false;
						}
						return true;
					} else if (auto *BO_LHS =
							dyn_cast<ArraySubscriptExpr>(callee)) {
						return false;
					} else if (auto *BO_LHS = dyn_cast<MemberExpr>(callee)) {
						return isLocalCallee(BO_LHS->getBase());
					} else {
						return false;
					}
				}

				const DeclRefExpr *getPointer(const Expr *S) {
					if (const auto *DRE = dyn_cast<DeclRefExpr>(S->IgnoreParenImpCasts())) {
						if (DRE->getDecl() != nullptr) {
							return DRE;
						}
					} else if (const auto *ase =
							dyn_cast<ArraySubscriptExpr>(S->IgnoreParenImpCasts())) {
						return getPointer(ase->getBase());
					} else if (const auto *UO =
							dyn_cast<UnaryOperator>(S->IgnoreParenImpCasts())) {
						return getPointer(UO->getSubExpr());
					} else if (const auto *OO =
							dyn_cast<CXXOperatorCallExpr>(S->IgnoreParenImpCasts())) {
						if (OO->getOperator() == OO_Subscript) {
							return getPointer(OO->getArg(0));
						}
					}
					return nullptr;
				}

				// Auxiliary Functions

				/*
				 * Takes as first parameter the declaration of the pointer
				 * Takes as second parameter the expression where the pointer is used for outputting error message
				 * */
				bool PointerHasValidLastValue(VarDecl *pointerVarDecl, const Expr *expr) {

					if (!Functions::hasElement<DeclarationName>(exploredPointers,
																pointerVarDecl->getDeclName())) {
						exploredPointers.push_back(pointerVarDecl->getDeclName());
						if (!pointerVarDecl->hasGlobalStorage()) {

							const auto hasValidPointerAssignment = [] {
								return cxxNewExpr();
							};
							bool hasValidInit = false;
							if (pointerVarDecl->hasInit()) {
								hasValidInit = true;
								const auto validInit = match(findAll(varDecl(equalsNode(pointerVarDecl), hasInitializer(
										hasValidPointerAssignment()))), *Context);
								if (validInit.size() == 0) hasValidInit = false;
							}
							if (!hasValidInit) {
								Check.diag(expr->getBeginLoc(),
										   "Pointer has invalid initialization");
								parallelizable = false;
							}
							if (auto *functionDecl = dyn_cast<FunctionDecl>(
									pointerVarDecl->getParentFunctionOrMethod())) {
								//

								const auto isVariable = [](VarDecl *VD) {
									return ignoringImpCasts(declRefExpr(to(varDecl(equalsNode(VD)))));
								};
								const auto invalidAssignments =
//                                        match(findAll(
//                                                binaryOperator(isAssignmentOperator(),
//                                                        hasLHS(equalsNode(write)), unless(hasRHS(hasValidPointerAssignment())))),*Context);
//*/
										match(findAll(stmt(anyOf(
												binaryOperator(
														hasLHS(isVariable(pointerVarDecl)),
														unless(hasRHS(hasValidPointerAssignment()))),
												unaryOperator(hasUnaryOperand(isVariable(pointerVarDecl)),
															  hasAnyOperatorName("++", "--"))

										))), *Context);


								if (invalidAssignments.size() > 0) {
									Check.diag(expr->getBeginLoc(),
											   "Pointer points to potentially unsafe memory space");
									parallelizable = false;
									return false;
								}
							}
						} else {
							Check.diag(expr->getBeginLoc(),
									   "Global pointer makes parallelization unsafe");
							parallelizable = false;
							return false;
						}
						return true;
					}
					return true;
				}

				bool isParallelizable() { return parallelizable; }

				bool isMapPattern() { return !MapList.empty(); }

				bool isReducePattern() { return !ReduceList.empty(); }

				virtual bool isMapAssignment(Expr *write) {
					return isLoopElem(write);
				};

				virtual bool isLoopElem(Expr *write) = 0;

				Reduce* isReduceAssignment(const BinaryOperator *BO) {
					Expr *LHS = BO->getLHS();
					if (auto *write =
							dyn_cast<DeclRefExpr>(LHS->IgnoreParenImpCasts())) {
						if (write->getType()->isIntegerType() &&
							!isLocalVariable(write->getFoundDecl()->getDeclName())) {
							// invariant += i;
							if (BO->isCompoundAssignmentOp()) {
								if (BO->getOpcode() == BO_AddAssign ||
									BO->getOpcode() == BO_MulAssign) {
									if (isLoopElem(BO->getRHS()))
										return new Reduce({BO->getRHS()}, write, BO);
								}
							}
								// invariant = invariant + i;
								// invariant = i + invariant;
							else if (BO->isAssignmentOp()) {
								Expr *RHS = BO->getRHS();
								if (auto *RHS_BO =
										dyn_cast<BinaryOperator>(RHS->IgnoreParenImpCasts())) {
									if (RHS_BO->getOpcode() == BO_Add ||
										RHS_BO->getOpcode() == BO_Mul) {

										// invariant = invariant + i;
										if (auto *read = dyn_cast<DeclRefExpr>(
												RHS_BO->getLHS()->IgnoreParenImpCasts())) {
											if (Functions::isSameVariable(write->getFoundDecl()->getDeclName(),
																		  read->getFoundDecl()->getDeclName())) {
												if (isLoopElem(RHS_BO->getRHS()))
													return new Reduce({RHS_BO->getRHS()}, write, RHS_BO);
											}
										}
										// invariant = i + invariant;
										else if (auto *read = dyn_cast<DeclRefExpr>(
												RHS_BO->getRHS()->IgnoreParenImpCasts())) {
											if (Functions::isSameVariable(write->getFoundDecl()->getDeclName(),
																		  read->getFoundDecl()->getDeclName())) {

												if (isLoopElem(RHS_BO->getLHS()))
													return new Reduce({RHS_BO->getLHS()}, write, RHS_BO);
											}
										}
									}
								}
							}
						}
					}

					return nullptr;
				}

				bool isRepeatedForStmt(Stmt *FS) {
					for (const Stmt *currentFor : visitedForLoopList) {
						if (currentFor == FS) {
							parallelizable = false;
							Check.diag(FS->getBeginLoc(),
									   "Recursion makes for loop parallelization unsafe");
							return true;
						}
					}
					return false;
				}

				void appendForLoopList() {
					if (parallelizable) {
						forLoopList.insert(forLoopList.begin(), visitedForLoopList.begin(),
										   visitedForLoopList.end());
					}
				}

				std::vector<DeclarationName>
				getValidParameterList(FunctionDecl *functionDeclaration) {
					std::vector<DeclarationName> validParameterList;
					for (unsigned i = 0; i < functionDeclaration->getNumParams(); i++) {
						ParmVarDecl *PVD = functionDeclaration->getParamDecl(i);
						if (PVD->getType()->isBuiltinType()) {
							validParameterList.push_back(PVD->getDeclName());
						}
					}
					return validParameterList;
				}

				void appendVisitedFunctionDeclarationList(
						std::vector<const FunctionDecl *> visitedFunctionDeclarations) {
					visitedFunctionDeclarationList.insert(
							visitedFunctionDeclarationList.begin(),
							visitedFunctionDeclarations.begin(), visitedFunctionDeclarations.end());
				}

				bool isLocalVariable(DeclarationName DN) {
					for (auto declarationName : localVariables) {
						if (Functions::isSameVariable(declarationName, DN)) {
							return true;
						}
					}
					return false;
				}

				//Transformation
				virtual int getArrayBeginOffset() const {
					return 0;
				}

				virtual std::string getArrayEndString() const {
					return "std::end(";
				}

				virtual int getArrayEndOffset() const {
					return 0;
				}

				virtual bool isVariableUsedInArraySubscript(DeclRefExpr *dre) {
					return true;
				}

				/*
				 * Gets beginning iterator for inputs that are not pointers
				 *
				 * */
				std::string getBeginInputTransformation(const DeclRefExpr *expr) {
					if (!expr->getType()->isPointerType() && !expr->getType()->isArrayType()) {
						return "std::begin(";
					}
					return "";
				}

				/*
				 * Closes parenthesis for inputs that require it
				 * */
				std::string getCloseBeginInputTransformation(const DeclRefExpr *expr) {
					if (!expr->getType()->isPointerType() && !expr->getType()->isArrayType()) {
						return ")";
					}
					return "";
				}

				virtual std::string getEndInput(const DeclRefExpr *inputName, std::string endOffsetString) {
					return getCastTransformation(inputName) + getEndInputTransformation(inputName) +
						   inputName->getNameInfo().getName().getAsString()
						   + getCloseEndInputTransformation(inputName) + endOffsetString;
				}

				/*
				 * Gets back iterator for input
				 * Integer for loops have their own ending
				 **/
				std::string getEndInputTransformation(const DeclRefExpr *expr) {
					if (!expr->getType()->isPointerType()) {
						return getArrayEndString();
					}
					return "";
				}

				/*
				 * Closes parenthesis for inputs that require it
				 * */
				std::string getCloseEndInputTransformation(const DeclRefExpr *expr) {
					if (!expr->getType()->isPointerType()) {
						return ")";
					}
					return "";
				}

				/*
				 * Get cast to turn into an iterator
				 * Cast for pointers and C style arrays
				 * A vector or container like variable does not need a cast, so an empty string is returned
				 * */
				std::string getCastTransformation(const DeclRefExpr *expr) {
					if (expr->getType()->isPointerType()) {
						std::string type = expr->getType()->getPointeeType().getAsString();
						return "(std::vector<" + type + ">::iterator) ";
					} else if (expr->getType()->isArrayType()) {
						std::string type = expr->getType()->getAsArrayTypeUnsafe()->getElementType().getAsString();
						return "(std::vector<" + type + ">::iterator) ";
					}
					return "";
				}

				std::string getStartOffsetString(){
					std::string startOffsetString = "";
					if (getArrayBeginOffset() != 0)
						startOffsetString = " + " + std::to_string(getArrayBeginOffset());
					return startOffsetString;
				}
				std::string getEndOffsetString(){
					std::string endOffsetString = "";
					if (getArrayEndOffset() != 0)
						endOffsetString = " + " + std::to_string(getArrayEndOffset());
					return endOffsetString;
				}


				//Pattern Transformation
				virtual std::string getPatternTransformationInput(Pattern &pattern) {
					std::string transformation = "";
					if (pattern.Input.empty()) {
						pattern.Input.push_back(pattern.Output);
					}

					std::string add_comma = ", ";

					//if more than one input, put it in a tuple
					if (pattern.Input.size() > 1) {
						transformation += ", std::make_tuple( ";
						add_comma = "";
					}
					for (auto &input : pattern.Input) {
						const DeclRefExpr *inputName = getPointer(input);
						if (inputName == nullptr) return "input null";

						transformation +=
								add_comma + getCastTransformation(inputName) + getBeginInputTransformation(inputName) +
								inputName->getNameInfo().getName().getAsString()
								+ getCloseBeginInputTransformation(inputName) + getStartOffsetString();
						add_comma = ", ";
					}
					if (pattern.Input.size() > 1) {
						transformation += ")";
					}
					return transformation;
				}
				virtual std::string getPatternTransformationInputEnd(const Pattern &pattern) {
					const Expr *input = pattern.Input[0];
					const DeclRefExpr *inputName = getPointer(input);
					std::string transformation = "";
					transformation +=
							", " + getEndInput(inputName, getEndOffsetString());
					return transformation;
				}

				virtual std::string getMapTransformationOutput(const Pattern &pattern) {
					std::string transformation = "";

					const DeclRefExpr *output = getPointer(pattern.Output);
					if (output == nullptr)
						return "output null";

					transformation += ", " + getBeginInputTransformation(output) +
									  output->getNameInfo().getName().getAsString()
									  + getCloseBeginInputTransformation(output) + getStartOffsetString();

					transformation += ", [=](";

					return transformation;
				}

				virtual std::string getMapTransformationLambdaParameters(const std::vector<Map>::iterator &map) {
					std::string transformation = "";

					std::vector<const Expr *> uniqueElementList;
					if (map->Element.empty()) {
						transformation += "auto " + LoopConstant::startElement;
					}

					for (auto &element:map->Element) {
						const DeclRefExpr *elementVar = getPointer(element);

						if (elementVar == nullptr) parallelizable = false;
						DeclarationName elementName = elementVar->getNameInfo().getName();
						bool isRepeated = false;
						for (auto &elem:uniqueElementList) {
							auto elemVar = getPointer(elem);
							if (elemVar != nullptr &&
								Functions::isSameVariable(elemVar->getNameInfo().getName(), elementName)) {
								isRepeated = true;
							}
						}
						if (!isRepeated) {
							uniqueElementList.push_back(element);
						}

					}
					int numElem = 0;
					for (auto &element:uniqueElementList) {
						if (numElem != 0) {
							transformation += ", ";
						}
						const DeclRefExpr *name = getPointer(element);
						if (name == nullptr) parallelizable = false;
						else {
							transformation +=
									"auto " + LoopConstant::startElement + name->getNameInfo().getName().getAsString();
						}

						numElem++;
					}

					return transformation;
				}
				virtual std::string getMapTransformationLambdaBody(const std::vector<Map>::iterator &map, SourceManager &SM) {
					std::string mapLambda = "";

					Rewriter rewriter(SM, LangOptions());
					int offset = 0;
					SourceRange currentRange;

					// remove all other maps
					for (std::vector<Map>::iterator otherMap = MapList.begin();
						 otherMap != MapList.end(); otherMap++) {
						if (otherMap != map) {
							currentRange = SourceRange(
									otherMap->mapFunction->getBeginLoc().getLocWithOffset(offset),
									Lexer::getLocForEndOfToken(otherMap->mapFunction->getEndLoc(),
															   offset, SM, LangOptions()));
							rewriter.RemoveText(currentRange);
							offset -= rewriter.getRangeSize(currentRange);
						}
					}

					//remove left hand side of assignment if Binary Operator
					if (auto *BO = dyn_cast<BinaryOperator>(map->mapFunction->IgnoreParenImpCasts())) {
						currentRange = SourceRange(
								BO->getBeginLoc().getLocWithOffset(offset),
								BO->getOperatorLoc().getLocWithOffset(offset));
						rewriter.RemoveText(currentRange);
						offset -= rewriter.getRangeSize(currentRange);
					}

					for (const auto *read:map->Element) {

						const DeclRefExpr *elem = getPointer(read);
						if (elem != nullptr) {
							currentRange = SourceRange(read->getSourceRange());
							rewriter.ReplaceText(currentRange,
												 LoopConstant::startElement + elem->getNameInfo().getAsString());
						}
					}

					std::string to_insert = "return ";
					rewriter.InsertTextBefore(
							map->mapFunction->getBeginLoc().getLocWithOffset(offset), to_insert);
					mapLambda = rewriter.getRewrittenText(
							visitingForStmtBody->getSourceRange());

					return mapLambda;
				}

				std::string getMapTransformation() {
					std::string transformation;
					SourceManager &SM = Context->getSourceManager();
					std::vector<Map> PastMapList;
					for (std::vector<Map>::iterator map = MapList.begin(); map != MapList.end(); map++) {
						transformation += "grppi::map(grppi::dynamic_execution()";

						std::string startOffsetString = "";
						if (getArrayBeginOffset() != 0)
							startOffsetString = " + " + std::to_string(getArrayBeginOffset());
						std::string endOffsetString = "";
						if (getArrayEndOffset() != 0) endOffsetString = " + " + std::to_string(getArrayEndOffset());

						//Input
						transformation += getPatternTransformationInput(*map);

						//End iterator of input
						transformation += getPatternTransformationInputEnd(*map);

						//Output
						transformation += getMapTransformationOutput(*map);

						//Parameters for lambda expression
						transformation += getMapTransformationLambdaParameters(map);

						//Lambda body
						std::string mapLambda = getMapTransformationLambdaBody(map, SM);
						transformation += ")" + mapLambda + ");\n";

						//End of current map translation
						PastMapList.push_back(*map);
					}
					//Removes new line characters that may have existed
					transformation.erase(
							std::remove(transformation.begin(), transformation.end(), '\n'),
							transformation.end());

					return transformation;
				}
			};

			class IntegerForLoopExplorer : public LoopExplorer<IntegerForLoopExplorer> {
				private:
				std::vector<CustomArray> readArraySubscriptList;
				std::vector<CustomArray> writeArraySubscriptList;

				int start = 0, end = 0;
				public:
				IntegerForLoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
									   std::vector<const Stmt *> visitedForLoopList,
									   const Stmt *visitingForStmtBody, int start, int end, const VarDecl *iterator)
						: LoopExplorer(Context, Check, visitedForLoopList, visitingForStmtBody, iterator), start(start),
						  end(end) {}

				IntegerForLoopExplorer(
						ASTContext *Context, ClangTidyCheck &Check,
						std::vector<const Stmt *> visitedForLoopList,
						std::vector<const FunctionDecl *> visitedFunctionDeclarationList,
						std::vector<DeclarationName> localVariables, bool isThisExprValid,
						const Stmt *visitingForStmtBody, const VarDecl *iterator)
						: LoopExplorer(Context, Check, visitedForLoopList,
									   visitedFunctionDeclarationList, localVariables,
									   isThisExprValid, visitingForStmtBody, iterator) {}

				const Expr *getOutput(Expr *write) override;

				bool HandleArrayMapAssignment(CustomArray);

				bool isVariableUsedInArraySubscript(DeclRefExpr *dre) override;

				int getArrayBeginOffset() const override;

				std::string getArrayEndString() const override;

				int getArrayEndOffset() const override;

				std::string getEndInput(const DeclRefExpr *inputName, std::string endOffsetString) override;

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


			};

			class ContainerForLoopExplorer : public LoopExplorer<ContainerForLoopExplorer> {
				protected:
				const DeclRefExpr *Output = nullptr;
				std::vector<DeclRefExpr *> writeList;

				bool isLoopElem(Expr *write) override;

				const Expr *getOutput(Expr *write) override;

				public:
				ContainerForLoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
										 std::vector<const Stmt *> visitedForLoopList,
										 const Stmt *visitingForStmtBody, const VarDecl *iterator,
										 const DeclRefExpr *traversalArray)
						: LoopExplorer(Context, Check, visitedForLoopList, visitingForStmtBody, iterator),
						  Output(traversalArray) {
				}

				ContainerForLoopExplorer(
						ASTContext *Context, ClangTidyCheck &Check,
						std::vector<const Stmt *> visitedForLoopList,
						std::vector<const FunctionDecl *> visitedFunctionDeclarationList,
						std::vector<DeclarationName> localVariables, bool isThisExprValid,
						const Stmt *visitingForStmtBody, const VarDecl *iterator)
						: LoopExplorer(Context, Check, visitedForLoopList,
									   visitedFunctionDeclarationList, localVariables,
									   isThisExprValid, visitingForStmtBody, iterator) {}

				bool VisitCXXOperatorCallExpr(CXXOperatorCallExpr *OO);

				DeclRefExpr *isValidDereference(Expr *expr);
			};

			class RangeForLoopExplorer : public LoopExplorer<RangeForLoopExplorer> {
				protected:
				const DeclRefExpr *Output = nullptr;
				std::vector<DeclRefExpr *> writeList;

				DeclRefExpr *isElemDeclRefExpr(Expr *expr);

				bool isLoopElem(Expr *write) override;

				const Expr *getOutput(Expr *write) override;


				public:
				RangeForLoopExplorer(ASTContext *Context, ClangTidyCheck &Check,
									 std::vector<const Stmt *> visitedForLoopList,
									 const Stmt *visitingForStmtBody, const VarDecl *iterator,
									 const DeclRefExpr *traversalArray)
						: LoopExplorer(Context, Check, visitedForLoopList, visitingForStmtBody, iterator),
						  Output(traversalArray) {
				}

				RangeForLoopExplorer(
						ASTContext *Context, ClangTidyCheck &Check,
						std::vector<const Stmt *> visitedForLoopList,
						std::vector<const FunctionDecl *> visitedFunctionDeclarationList,
						std::vector<DeclarationName> localVariables, bool isThisExprValid,
						const Stmt *visitingForStmtBody, const VarDecl *iterator)
						: LoopExplorer(Context, Check, visitedForLoopList,
									   visitedFunctionDeclarationList, localVariables,
									   isThisExprValid, visitingForStmtBody, iterator) {}

				bool VisitDeclRefExpr(DeclRefExpr *DRE);
			};


		} // namespace misc
	} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_MAPREDUCECHECK_H
