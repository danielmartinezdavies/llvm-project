//===--- MapReduceCheck.cpp - clang-tidy --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MapReduceCheck.h"

namespace clang {
	namespace tidy {
		namespace grppi {
			//Map
			bool Map::addElement(const Expr *expr, ASTContext *Context) {
				BeforeThanCompare<SourceLocation> isBefore(Context->getSourceManager());
				if (!isBefore(Lexer::getLocForEndOfToken(mapFunction->getEndLoc(), 1, Context->getSourceManager(),
														 LangOptions()), expr->getEndLoc())) {
					Element.push_back(expr);
					return true;
				}
				return false;
			}

			bool Map::isWithin(const Expr *expr, ASTContext *Context) const {
				BeforeThanCompare<SourceLocation> isBefore(Context->getSourceManager());
				if (!isBefore(Lexer::getLocForEndOfToken(mapFunction->getEndLoc(), 0, Context->getSourceManager(),
														 LangOptions()), expr->getEndLoc())) {
					if (!isBefore(expr->getBeginLoc(), mapFunction->getBeginLoc())) {
						return true;
					}
				}
				return false;
			}

			BinaryOperator *Map::getBinaryOperator() const {
				if (auto *BO = dyn_cast<BinaryOperator>(this->mapFunction)) {
					return BO;
				}
				return nullptr;
			}

			bool Map::isCompoundAssignmentBO() const {
				BinaryOperator *BO = this->getBinaryOperator();
				if (BO != nullptr) {
					if (BO->isCompoundAssignmentOp()) {
						return true;
					}
				}
				return false;
			}

			std::string Map::getOperatorAsString(SourceManager &SM) const {
				auto BO = this->getBinaryOperator();
				return Lexer::getSourceText(CharSourceRange::getTokenRange(BO->getOperatorLoc(), BO->getOperatorLoc()),
											SM,
											LangOptions()).str().substr(0, 1);
			}


			//Reduce
			std::string Reduce::getOperatorAsString() const {
				if (binary_operator == nullptr) return "operator";
				if (binary_operator->getOpcode() == BO_AddAssign || binary_operator->getOpcode() == BO_Add) {
					return "+";
				}
				if (binary_operator->getOpcode() == BO_MulAssign || binary_operator->getOpcode() == BO_Mul) {
					return "*";
				}

				return "No valid operator";
			}

			std::string Reduce::getIdentityAsString() const {
				if (binary_operator == nullptr) return "\"identity\"";
				if (binary_operator->getOpcode() == BO_AddAssign || binary_operator->getOpcode() == BO_Add) {
					return "0L";
				}
				if (binary_operator->getOpcode() == BO_MulAssign || binary_operator->getOpcode() == BO_Mul) {
					return "1L";
				}
				return "No identity";
			}

			//IntegerForLoop
			const Expr *IntegerForLoopExplorer::getLoopContainer(Expr *write) {
				return write;
			}

			std::string IntegerForLoopExplorer::getArrayBeginOffset() const {
				std::string result = Lexer::getSourceText(CharSourceRange::getTokenRange(start_expr->getSourceRange()),
														  Context->getSourceManager(),
														  LangOptions()).str();
				if (result == "0") return "";
				return result;
			}

			std::string IntegerForLoopExplorer::getArrayEndString() const {
				return "std::begin(";
			}

			std::string IntegerForLoopExplorer::getArrayEndOffset() const {
				std::string result = Lexer::getSourceText(CharSourceRange::getTokenRange(end_expr->getSourceRange()),
														  Context->getSourceManager(),
														  LangOptions()).str();
				if (result == "0") return "";
				return result;
			}

			std::string IntegerForLoopExplorer::getEndInputAsString(const DeclRefExpr *inputName) {
				//returns size
				std::unique_ptr<const uint64_t> startP = getStartValue(), endP = getEndValue();
				if (startP != nullptr && endP != nullptr) {
					return std::to_string(*endP - *startP);
				}
				std::string end = getArrayEndOffset(), start = getArrayBeginOffset();
				if (end == "" && start == "") return "0";
				if (end != "" && start != "") return end + " - " + start;

				return end + start;
			}

			std::unique_ptr<const uint64_t> IntegerForLoopExplorer::getStartValue() {
				if (auto *start = dyn_cast<IntegerLiteral>(
						start_expr->IgnoreParenImpCasts())) {
					return std::make_unique<const uint64_t>(start->getValue().getZExtValue());
				}
				return nullptr;
			}

			/*
			 * Returns end value of loop, where iterator_variable < end
			 * */
			std::unique_ptr<const uint64_t> IntegerForLoopExplorer::getEndValue() {
				if (auto *end = dyn_cast<IntegerLiteral>(
						end_expr->IgnoreParenImpCasts())) {
					return std::make_unique<const uint64_t>(end->getValue().getZExtValue());
				}
				return nullptr;
			}


			bool IntegerForLoopExplorer::isRequiredMinSize() {
				std::unique_ptr<const uint64_t> start, end;
				start = getStartValue();
				end = getEndValue();

				if (start != nullptr && end != nullptr) {
					if (*end - *start < LoopSizeMin) {
						return false;
					}
				}
				return true;
			}

			bool IntegerForLoopExplorer::VisitArray(CustomArray array) {
				const DeclRefExpr *base = getPointer(array.getOriginal());
				if (isa<ArraySubscriptExpr>(array.getOriginal())) {
					//if regular arraysubscript expression, check for valid pointer
					if (base == nullptr) {
						Check.diag(array.getOriginal()->getBeginLoc(),
								   Diag::label + "Pointer is invalid");
						parallelizable = false;
						return true;
					}
					if (!base->getType()->isArrayType()) {
						PointerHasValidLastValue((const VarDecl *) base->getDecl(), array.getOriginal());
					}
				}
				bool isInput = addToReadArraySubscriptList(array, Context);
				if (isInput) {
					//add Input and Element for possible future map
					if (!MapList.empty()) {
						Map &currentMap = MapList[MapList.size() - 1];
						//Dont add element to placeholder if it is inside of current map
						if (!currentMap.isWithin(array.getOriginal(), Context)) {
							placeHolderMap.Element.push_back(array.getOriginal());
							addInput(placeHolderMap, array.getOriginal());
						}
						//add Input and Element for current maps if possible
						bool addedElem = currentMap.addElement(array.getOriginal(), Context);
						if (addedElem) {
							addInput(currentMap, array.getOriginal());
						}
					} else {
						placeHolderMap.Element.push_back(array.getOriginal());
						addInput(placeHolderMap, array.getOriginal());
					}
				}
				return true;
			}

			bool IntegerForLoopExplorer::VisitArraySubscriptExpr(ArraySubscriptExpr *ase) {
				CustomArray array(ase->getBase(), ase->getIdx(), ase);
				return VisitArray(array);
			}

			bool IntegerForLoopExplorer::VisitCXXOperatorCallExpr(CXXOperatorCallExpr *OO) {
				if (OO->getOperator() == OO_Subscript) {
					CustomArray array(OO->getArg(0), OO->getArg(1), OO);
					return VisitArray(array);
				}
				return true;
			}

			bool IntegerForLoopExplorer::addInput(Map &map, const Expr *expr) {
				bool isRepeated = false;
				for (auto &currentInput:map.Input) {
					const auto *inputPointer = getPointer(currentInput);
					const auto *exprVar = getPointer(expr);
					if (exprVar == nullptr || inputPointer == nullptr) parallelizable = false;
					else if (exprVar->getNameInfo().getName() == inputPointer->getNameInfo().getName())
						isRepeated = true;
				}
				if (!isRepeated) {
					map.Input.push_back(expr);
					return true;
				}
				return false;
			}

			/*
			 * Returns 3 for subscript where index is integer literal less than the start value or larger than the end value of the for loop
			 * Returns 2 for subscript where index is integer literal within bounds
			 * Returns 1 for subscript where index is iterator variable
			 * Returns -1 if index cannot be studied due to start_expr or end_expr of loop
			 * Returns 0 for anything else
			 * */
			int IntegerForLoopExplorer::isValidArraySubscript(CustomArray array) {
				std::unique_ptr<const uint64_t> startP = getStartValue();
				if (startP == nullptr) {
					return -1;
				}
				int start = *startP;

				std::unique_ptr<const uint64_t> endP = getEndValue();
				if (endP == nullptr) {
					return -1;
				}
				int end = *endP;


				if (auto *index = dyn_cast<IntegerLiteral>(
						array.getIndex()->IgnoreParenImpCasts())) {
					int indexValue = index->getValue().getZExtValue();
					if (indexValue < start || indexValue >= end) return 3;
					return 2;
				}
				if (auto *dre = dyn_cast<DeclRefExpr>(
						array.getIndex()->IgnoreParenImpCasts())) {
					if (dre->getDecl() == iterator_variable) {
						return 1;
					}
				}
				parallelizable = false;
				Check.diag(array.getOriginal()->getBeginLoc(),
						   Diag::label + "Pointer has invalid subscript");
				return 0;
			}

			bool IntegerForLoopExplorer::addToReadArraySubscriptList(CustomArray array, ASTContext *context) {
				if (isValidArraySubscript(array) == 3) return false;
				const Expr *base = array.getBase();
				const Expr *index = array.getIndex();
				for (CustomArray write_array : writeArraySubscriptList) {
					if (Functions::areSameExpr(context, write_array.getBase(), base)) {
						if (!Functions::areSameExpr(context, write_array.getIndex(), index) && isValidArraySubscript(array) != -1) {
							Check.diag(
									write_array.getIndex()->getBeginLoc(),
									Diag::label + "Inconsistent array subscription makes for loop "
									"parallelization unsafe");
							parallelizable = false;
							return false;
						} else {
							// read is actually write
							if (array.getOriginal()->getSourceRange() == write_array.getOriginal()->getSourceRange()) {
								return false;
							}
						}
					}
				}
				readArraySubscriptList.push_back(array);
				return true;
			}

			bool IntegerForLoopExplorer::addToWriteArraySubscriptList(CustomArray array, ASTContext *context) {
				if (isValidArraySubscript(array) == 2) return false;
				const Expr *base = array.getBase();
				const Expr *index = array.getIndex();
				for (CustomArray read_array : readArraySubscriptList) {
					if (Functions::areSameExpr(context, read_array.getBase(), base)) {
						if (!Functions::areSameExpr(context, read_array.getIndex(), index) && isValidArraySubscript(array) != -1) {
							Check.diag(
									array.getOriginal()->getBeginLoc(),
									Diag::label + "Inconsistent array subscription makes for loop "
									"parallelization unsafe");
							parallelizable = false;
							return false;
						}
					}
				}
				for (CustomArray write_array : writeArraySubscriptList) {
					if (Functions::areSameExpr(context, write_array.getBase(), base)) {
						if (!Functions::areSameExpr(context, write_array.getIndex(), index)) {
							Check.diag(
									write_array.getOriginal()->getBeginLoc(),
									Diag::label + "Inconsistent array subscription makes for loop "
									"parallelization unsafe");
							parallelizable = false;
							return false;
						}
							// same array subscript but in different location
						else {
							return true;
						}
					}
				}
				writeArraySubscriptList.push_back(array);
				return true;
			}

			bool IntegerForLoopExplorer::HandleArrayMapAssignment(CustomArray array) {
				// if not a local subscript to local array, make sure it is a valid write
				const DeclRefExpr *ArraySubscriptPointer = getPointer(array.getOriginal());
				if (ArraySubscriptPointer == nullptr) {
					Check.diag(array.getOriginal()->getBeginLoc(),
							   Diag::label + "Write to pointer subscript too complex to analyze makes "
							   "loop parallelization unsafe");
					parallelizable = false;
					return false;
				} else if (!isLocalVariable(
						ArraySubscriptPointer->getDecl()->getDeclName())) {
					addToWriteArraySubscriptList(array, Context);
					if (isValidArraySubscript(array) == 1) return true;
					if (isValidArraySubscript(array) == -1) {
						Check.diag(array.getOriginal()->getBeginLoc(),
								   Diag::label + "Index cannot be analysed properly. Parallelization may still be possible");
						return true;
					}
				}
				return false;
			}

			bool IntegerForLoopExplorer::isLoopElem(Expr *write) {
				if (auto *BO_LHS = dyn_cast<ArraySubscriptExpr>(write->IgnoreParenImpCasts())) {
					CustomArray a(BO_LHS->getBase(), BO_LHS->getIdx(), BO_LHS);
					return HandleArrayMapAssignment(a);
				}
				if (auto *OO = dyn_cast<CXXOperatorCallExpr>(write->IgnoreParenImpCasts())) {
					if (OO->getOperator() == OO_Subscript) {
						CustomArray a(OO->getArg(0), OO->getArg(1), OO);
						return HandleArrayMapAssignment(a);
					}
				}
				return false;
			}

			/*
			 * Checks to see if variable instance was used as index in either readArraySubscriptList and writeArraySubscriptList
			 * Does not modify boolean member variable "parallelizable"
			 * */
			bool IntegerForLoopExplorer::isVariableUsedInArraySubscript(const DeclRefExpr *dre) {
				bool found = false;
				for (auto readElem: readArraySubscriptList) {
					const Expr *index = readElem.getIndex();
					if (auto *var = dyn_cast<DeclRefExpr>(index->IgnoreParenImpCasts())) {
						if (dre == var) {
							found = true;
							break;
						}
					}
				}
				for (auto writeElem: writeArraySubscriptList) {
					const Expr *index = writeElem.getIndex();
					if (auto *var = dyn_cast<DeclRefExpr>(index->IgnoreParenImpCasts())) {
						if (dre == var) {
							found = true;
							break;
						}
					}
				}

				return found;
			}

			//ContainerForLoop
			bool ContainerForLoopExplorer::VisitCXXOperatorCallExpr(CXXOperatorCallExpr *OO) {
				DeclRefExpr *DRE = isValidDereference(OO);
				if (DRE != nullptr) {
					if (!Functions::hasElement(writeList, DRE)) {
						if (!MapList.empty()) {
							Map &currentMap = MapList[MapList.size() - 1];
							//Dont add element to placeholder if it is inside of current map
							if (!currentMap.isWithin(DRE, Context)) {
								placeHolderMap.Element.push_back(DRE);
							}
							currentMap.addElement(DRE, Context);
						} else {
							placeHolderMap.Element.push_back(DRE);
						}
					}
				} else {
					parallelizable = false;
					Check.diag(OO->getBeginLoc(),
							   Diag::label + "Invalid dereference");
				}
				return true;
			}

			DeclRefExpr *ContainerForLoopExplorer::isValidDereference(Expr *expr) {
				if (auto *OO = dyn_cast<CXXOperatorCallExpr>(expr->IgnoreParenImpCasts())) {
					if (OO->getOperator() == OO_Star) {
						if (auto *dre = dyn_cast<DeclRefExpr>(OO->getArg(0)->IgnoreParenImpCasts())) {
							if (dre->getFoundDecl() == iterator_variable) {
								return dre;
							}
						}
					}
				}
				if (auto *UO = dyn_cast<UnaryOperator>(expr->IgnoreParenImpCasts())) {
					if (UO->getOpcode() == UO_Deref) {
						if (auto *dre = dyn_cast<DeclRefExpr>(UO->getSubExpr()->IgnoreParenImpCasts())) {
							if (dre->getFoundDecl() == iterator_variable) {
								return dre;
							}
						}
					}
				}
				return nullptr;
			}

			bool ContainerForLoopExplorer::isLoopElem(Expr *write) {
				DeclRefExpr *elem = isValidDereference(write);
				if (elem != nullptr) {
					writeList.push_back(elem);
					return true;
				}
				return false;
			}

			const Expr *ContainerForLoopExplorer::getLoopContainer(Expr *write) {
				return LoopContainer;
			}

			//Range For Loop
			bool RangeForLoopExplorer::VisitDeclRefExpr(DeclRefExpr *DRE) {
				/*std::cout <<  Lexer::getSourceText(CharSourceRange::getTokenRange(DRE->getSourceRange()), Context->getSourceManager(),
												   LangOptions()).str() << std::endl;*/
				//std::cout << "L1" << std::endl;
				bool continueExploring = LoopExplorer::VisitDeclRefExpr(DRE);
				if (DRE->getFoundDecl() == iterator_variable) {
					//std::cout << "L:" << DRE << std::endl;
					/*std::cout <<  Lexer::getSourceText(CharSourceRange::getTokenRange(DRE->getSourceRange()), Context->getSourceManager(),
													   LangOptions()).str() << std::endl;*/
					for(auto &elem : writeList){
						//std::cout << "K:" << elem;
						/*std::cout <<  Lexer::getSourceText(CharSourceRange::getTokenRange(elem->getSourceRange()), Context->getSourceManager(),
														   LangOptions()).str() << std::endl;*/
					}
					if (!Functions::hasElement(writeList, DRE)) {
						//std::cout << "L3" << std::endl;
						if (!MapList.empty()) {
							//std::cout << "L4" << std::endl;
							Map &currentMap = MapList[MapList.size() - 1];
							//Dont add element to placeholder if it is inside of current map
							if (!currentMap.isWithin(DRE, Context)) {
								placeHolderMap.Element.push_back(DRE);
							}
							currentMap.addElement(DRE, Context);
						} else {
							placeHolderMap.Element.push_back(DRE);
						}
					}
				}
				return continueExploring;
			}

			DeclRefExpr *RangeForLoopExplorer::isElemDeclRefExpr(Expr *expr) {
				if (auto *dre = dyn_cast<DeclRefExpr>(
						expr->IgnoreParenImpCasts())) {
					if (dre->getDecl() == iterator_variable) {
						if (iterator_variable->getType()->isReferenceType()) {
							return dre;
						}

					}
				}
				return nullptr;
			}

			bool RangeForLoopExplorer::isLoopElem(Expr *write) {
				DeclRefExpr *elem = isElemDeclRefExpr(write);
				if (elem != nullptr) {
					writeList.push_back(elem);
					return true;
				}
				return false;
			}

			const Expr *RangeForLoopExplorer::getLoopContainer(Expr *write) {
				return LoopContainer;
			}

			std::string RangeForLoopExplorer::getMultipleInputTransformation() {
				return "grppi::zip( ";
			}

			std::string RangeForLoopExplorer::getBeginInputAsString(const DeclRefExpr *inputName) {
				return inputName->getNameInfo().getName().getAsString();
			}

			std::string RangeForLoopExplorer::getEndInputAsString(const DeclRefExpr *inputName) {
				return "";
			}

			std::string RangeForLoopExplorer::getOutputAsString(const DeclRefExpr *output) {
				return output->getNameInfo().getName().getAsString();
			}

			//MapReduceCheck
//
// Matcher
//
			const auto getForRestrictions = [] {
				return hasBody(unless(anyOf(hasDescendant(gotoStmt()),
											hasDescendant(breakStmt()),
											hasDescendant(returnStmt()),
											hasDescendant(unaryOperator(hasOperatorName("*"),
																		hasDescendant(binaryOperator()))),
											hasDescendant(binaryOperator(isAssignmentOperator(),
																		 anyOf(hasDescendant(binaryOperator(
																				 isAssignmentOperator())),
																			   hasDescendant(unaryOperator(
																					   anyOf(hasOperatorName("++"),
																							 hasOperatorName("--")))))))
				)));
			};
			const auto getIteratorForType = [] {
				return /*expr(unless(hasType(builtinType())))*/
						anyOf(hasDescendant(callExpr(hasArgument(0, declRefExpr().bind("array")))),
							  callExpr(hasArgument(0, declRefExpr().bind("array"))),
							  hasDescendant(cxxMemberCallExpr().bind("member")),
							  cxxMemberCallExpr().bind("member")
						);
			};

			void MapReduceCheck::registerMatchers(MatchFinder *Finder) {
				//IntegerForLoop
				Finder->addMatcher(
						forStmt(
								hasLoopInit(
										declStmt(hasSingleDecl(
												varDecl(hasInitializer(
														expr(hasType(isInteger())).bind("start_expr"))).bind(
														"initVar")))
												.bind("initFor")),
								hasCondition(binaryOperator(hasOperatorName("<"),
															hasLHS(ignoringParenImpCasts(declRefExpr(
																	to(varDecl(hasType(isInteger())).bind(
																			"condVar"))))),
															hasRHS(expr(expr(hasType(isInteger())).bind("end_expr"))))
													 .bind("condFor")),
								hasIncrement(unaryOperator(hasOperatorName("++"),
														   hasUnaryOperand(declRefExpr(
																   to(varDecl(hasType(isInteger())).bind("incVar")))))
													 .bind("incFor")),
								getForRestrictions())
								.bind("forLoop"),
						this);
				//ContainerForLoop
				Finder->addMatcher(
						forStmt(
								hasLoopInit(
										declStmt(hasSingleDecl(
												varDecl(hasInitializer(getIteratorForType())).bind(
														"initVar")))
												.bind("initFor")),
								hasCondition(anyOf(binaryOperator(hasOperatorName("!="),
																  hasLHS(ignoringParenImpCasts(declRefExpr(
																		  to(varDecl(
																				  hasInitializer(
																						  getIteratorForType())).bind(
																				  "condVar"))))),
																  hasRHS(getIteratorForType()))
														   .bind("condFor"),
												   hasDescendant(cxxOperatorCallExpr(hasOverloadedOperatorName("!="),
																					 hasArgument(0,
																								 declRefExpr(to(varDecl(
																										 hasInitializer(
																												 getIteratorForType())).bind(
																										 "condVar")))),
																					 hasArgument(1,
																								 getIteratorForType()))))),
								hasIncrement(anyOf(unaryOperator(hasOperatorName("++"),
																 hasUnaryOperand(declRefExpr(
																		 to(varDecl(
																				 hasInitializer(
																						 getIteratorForType())).bind(
																				 "incVar"))))),
												   cxxOperatorCallExpr(hasOverloadedOperatorName("++"),
																	   hasArgument(0, declRefExpr(to(varDecl(
																			   hasInitializer(
																					   getIteratorForType())).bind(
																			   "incVar"))))))),
								getForRestrictions())
								.bind("iteratorForLoop"),
						this);
				//RangeForLoop
				Finder->addMatcher(cxxForRangeStmt(getForRestrictions()).bind("rangeForLoop"),
								   this);
			}

			void MapReduceCheck::ProcessIntegerForLoop(const ForStmt *IntegerForLoop,
													   const MatchFinder::MatchResult &Result) {

				std::cout << "Processing Integer loop" << std::endl;
				const auto *IncVar = Result.Nodes.getNodeAs<VarDecl>("incVar");
				const auto *CondVar = Result.Nodes.getNodeAs<VarDecl>("condVar");
				const auto *InitVar = Result.Nodes.getNodeAs<VarDecl>("initVar");


				const auto *start_expr = Result.Nodes.getNodeAs<Expr>("start_expr");
				if (start_expr == nullptr)
					return;

				const auto *end_expr = Result.Nodes.getNodeAs<Expr>("end_expr");
				if (end_expr == nullptr)
					return;


				if (!Functions::isSameVariable(IncVar, CondVar) || !Functions::isSameVariable(IncVar, InitVar))
					return;

				//Contains bug
				/*if (Functions::alreadyExploredForLoop(IntegerForLoop, Result.Context))
					return;*/


				forLoopList.push_back(IntegerForLoop);
				//IntegerForLoop->dump();

				IntegerForLoopExplorer currentMap(Result.Context, *this,
												  std::vector<const Stmt *>(), IntegerForLoop->getBody(), start_expr,
												  end_expr,
												  InitVar, IntegerForLoopSizeMin);

				currentMap.TraverseStmt(const_cast<Stmt *>(IntegerForLoop->getBody()));

				currentMap.appendForLoopList();


				addDiagnostic(currentMap, IntegerForLoop);
			}

			void MapReduceCheck::ProcessIteratorForLoop(const ForStmt *iteratorForLoop,
														const MatchFinder::MatchResult &Result) {
				const auto *matchedArray = Result.Nodes.getNodeAs<DeclRefExpr>("array");
				const auto *matchedMember = Result.Nodes.getNodeAs<CXXMemberCallExpr>("member");

				if (matchedArray == nullptr && matchedMember == nullptr) return;

				const auto *IncVar = Result.Nodes.getNodeAs<VarDecl>("incVar");
				const auto *CondVar = Result.Nodes.getNodeAs<VarDecl>("condVar");
				const auto *InitVar = Result.Nodes.getNodeAs<VarDecl>("initVar");
				if (matchedArray == nullptr) {
					if (auto *member = dyn_cast<MemberExpr>(
							matchedMember->getCallee()->IgnoreParenImpCasts())) {
						if (auto *dre = dyn_cast<DeclRefExpr>(
								member->getBase()->IgnoreParenImpCasts())) {
							matchedArray = dre;
						}
					}
				}
				if (matchedArray == nullptr) return;
				//iteratorForLoop->dump();
				if (!Functions::isSameVariable(IncVar, CondVar) || !Functions::isSameVariable(IncVar, InitVar))
					return;
				std::cout << "Processing Iterator loop" << std::endl;
				ContainerForLoopExplorer currentMap(Result.Context, *this, {}, iteratorForLoop->getBody(), IncVar,
													matchedArray);
				currentMap.TraverseStmt(const_cast<Stmt *>(iteratorForLoop->getBody()));
				addDiagnostic(currentMap, iteratorForLoop);
			}

			void MapReduceCheck::ProcessRangeForLoop(const CXXForRangeStmt *rangeForLoop,
													 const MatchFinder::MatchResult &Result) {
				std::cout << "Processing range for loop" << std::endl;
				const auto *iterator = rangeForLoop->getLoopVariable();
				if (const auto *input = dyn_cast<DeclRefExpr>(
						rangeForLoop->getRangeInit()->IgnoreParenImpCasts())) {
					RangeForLoopExplorer currentMap(Result.Context, *this, {}, rangeForLoop->getBody(), iterator,
													input);
					currentMap.TraverseStmt(const_cast<Stmt *>( rangeForLoop->getBody()));
					addDiagnostic(currentMap, rangeForLoop);
				}
			}


			void MapReduceCheck::check(const MatchFinder::MatchResult &Result) {
				// FIXME: Add callback implementation.

				const auto *const IntegerForLoop = Result.Nodes.getNodeAs<ForStmt>("forLoop");
				if (IntegerForLoop != nullptr) {
					ProcessIntegerForLoop(IntegerForLoop, Result);
					//Result.SourceManager->getMainFileID();
				}

				const auto *const iteratorForLoop = Result.Nodes.getNodeAs<ForStmt>("iteratorForLoop");
				if (iteratorForLoop != nullptr) {
					ProcessIteratorForLoop(iteratorForLoop, Result);
				}
				const auto *const rangeForLoop = Result.Nodes.getNodeAs<CXXForRangeStmt>("rangeForLoop");
				if (rangeForLoop != nullptr) {
					ProcessRangeForLoop(rangeForLoop, Result);
				}
			}

			void MapReduceCheck::storeOptions(
					ClangTidyOptions::OptionMap &Opts) {
				Options.store(Opts, "IntegerForLoopSizeMin", IntegerForLoopSizeMin);
			}


		} // namespace grppi
	} // namespace tidy
} // namespace clang


/*const auto dummyForLoop = Result.Nodes.getNodeAs<ForStmt>("dummy");
				if(dummyForLoop != nullptr && Result.Context->getSourceManager().isInMainFile(dummyForLoop->getBeginLoc())){
					dummyForLoop->dump();
					return;
				}*/

/*
StringRef init_expr =
Lexer::getSourceText(CharSourceRange::getTokenRange(init_range), sm,
										   LangOptions());
*/
