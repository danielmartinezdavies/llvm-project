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


			//LoopExplorer
			bool LoopExplorer::isValidWrite(Expr *write) {
				write = write->IgnoreParenImpCasts();
				// possibility of removing
				if (isMapAssignment(write))
					return true;
				if (auto *BO_LHS = dyn_cast<DeclRefExpr>(write)) {
					if (!isLocalVariable(BO_LHS->getDecl()->getDeclName())) {
						if (verbose) {
							Check.diag(write->getBeginLoc(),
									   Diag::label + "Write to variable declared outside for loop statement "
													 "makes loop parallelization unsafe");
						}
						parallelizable = false;
						return false;
					}
					return true;
				} else if (auto *BO_LHS = dyn_cast<MemberExpr>(write)) {
					bool isValidMember = true;
					if (auto *memberDecl = dyn_cast<VarDecl>(BO_LHS->getMemberDecl())) {
						if (memberDecl->hasGlobalStorage()) {
							if (verbose) {
								Check.diag(BO_LHS->getBeginLoc(),
										   Diag::label + "Write to variable stored globally makes loop "
														 "parallelization unsafe");
							}
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
						if (verbose) {
							Check.diag(BO_LHS->getBeginLoc(), Diag::label + "Write to variable stored globally "
																			"makes loop parallelization unsafe");
						}
						parallelizable = false;
						return false;
					}
					return true;
				} else if (auto *BO_LHS =
						dyn_cast<CXXOperatorCallExpr>(write)) {
					if (verbose) {
						Check.diag(BO_LHS->getBeginLoc(),
								   Diag::label + "Write to overloaded operator could be unsafe");
					}
					return true;
				} else {
					if (verbose) {
						Check.diag(write->getBeginLoc(),
								   Diag::label +
								   "Write to type that is not variable or array subscript of loop variable makes "
								   "for loop parallelization unsafe");
					}
					parallelizable = false;
					return false;
				}
			}

			bool LoopExplorer::isLocalCallee(const Expr *callee) {
				callee = callee->IgnoreParenImpCasts();
				if (auto *BO_LHS = dyn_cast<DeclRefExpr>(callee)) {
					if (!isLocalVariable(BO_LHS->getDecl()->getDeclName())) {
						/*std::cout << BO_LHS->getDecl()->getDeclName().getAsString()
								  << std::endl;*/
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

			const DeclRefExpr *LoopExplorer::getPointer(const Expr *S) {
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
					if (OO->getOperator() == OO_Subscript || OO->getOperator() == OO_Star) {

						return getPointer(OO->getArg(0));
					}
				}
				return nullptr;
			}

			/*
				 * Takes as first parameter the declaration of the pointer
				 * Takes as second parameter the expression where the pointer is used for outputting error message
				 * */
			bool LoopExplorer::PointerHasValidLastValue(const VarDecl *pointerVarDecl, const Expr *expr) {

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
							if (verbose) {
								Check.diag(expr->getBeginLoc(),
										   Diag::label + "Pointer has invalid initialization");
							}
							parallelizable = false;
						}
						if (auto *functionDecl = dyn_cast<FunctionDecl>(
								pointerVarDecl->getParentFunctionOrMethod())) {
							//

							const auto isVariable = [](const VarDecl *VD) {
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
								if (verbose) {
									Check.diag(expr->getBeginLoc(),
											   Diag::label + "Pointer points to potentially unsafe memory space");
								}
								parallelizable = false;
								return false;
							}
						}
					} else {
						if (verbose) {
							Check.diag(expr->getBeginLoc(),
									   Diag::label + "Global pointer makes parallelization unsafe");
						}
						parallelizable = false;
						return false;
					}
					return true;
				}
				return true;
			}

			bool LoopExplorer::isParallelizable() { return parallelizable; }

			bool LoopExplorer::isMapPattern() { return !MapList.empty(); }

			bool LoopExplorer::isReducePattern() { return !ReduceList.empty(); }

			bool LoopExplorer::isMapReducePattern() {
				return isMapReducePattern(this->MapList, this->ReduceList);
			}

			bool LoopExplorer::isMapReducePattern(std::vector<Map> MapList, std::vector<Reduce> ReduceList) {
				if (MapList.size() != 1 || ReduceList.size() != 1) return false;

				Map &m = MapList[0];
				Reduce &r = ReduceList[0];

				const DeclRefExpr *mapVar = getPointer(m.Output);
				const DeclRefExpr *reduceVar = getPointer(r.Input[0]);

				if (mapVar != nullptr && reduceVar != nullptr &&
					Functions::isSameVariable(mapVar->getDecl(), reduceVar->getDecl()))
					return true;

				return false;
			}

			Expr *LoopExplorer::isReduceCallExpr(const Expr *expr) {
				if (auto *callexpr =
						dyn_cast<CallExpr>(expr->IgnoreParenImpCasts())) {
					if (callexpr->getNumArgs() > 0 && callexpr->getNumArgs() <= 2) {
						for (auto arg:callexpr->arguments()) {
							if (isLoopElem(const_cast<Expr *> (arg))) {
								return const_cast<Expr *>(arg);
							}
						}
					}
				}
				return nullptr;
			}

			std::vector<const Expr *> LoopExplorer::getReduceElementsFromCallExpr(const Expr *expr) {
				std::vector<const Expr *> elements{};
				if (auto *callexpr =
						dyn_cast<CallExpr>(expr->IgnoreParenImpCasts())) {
					for (auto arg:callexpr->arguments()) {
						elements.emplace_back(arg);
					}
				}
				return elements;
			}

			Reduce *LoopExplorer::isReduceAssignment(const BinaryOperator *BO) {
				Expr *LHS = BO->getLHS();
				if (auto *write =
						dyn_cast<DeclRefExpr>(LHS->IgnoreParenImpCasts())) {
					if (!isLocalVariable(write->getFoundDecl()->getDeclName()) && !isLoopElem(write)) {

						//callexpr
						if (BO->isAssignmentOp() && !BO->isCompoundAssignmentOp()) {
							Expr *loopElem = isReduceCallExpr(BO->getRHS());
							if (loopElem != nullptr) {
								std::vector<const Expr *> elements = getReduceElementsFromCallExpr(BO->getRHS());
								return new Reduce({getLoopContainer(loopElem)}, write, elements, nullptr, BO);
							}
						}


						// invariant += i;
						if (BO->isCompoundAssignmentOp()) {
							if (BO->getOpcode() == BO_AddAssign ||
								BO->getOpcode() == BO_MulAssign) {

								if (isLoopElem(BO->getRHS()))
									return new Reduce({getLoopContainer(BO->getRHS())}, write, {}, BO, BO);
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
									if (isLoopElem(RHS_BO->getRHS())) {
										if (auto *read = dyn_cast<DeclRefExpr>(
												RHS_BO->getLHS()->IgnoreParenImpCasts())) {
											if (Functions::isSameVariable(write->getFoundDecl()->getDeclName(),
																		  read->getFoundDecl()->getDeclName())) {
												return new Reduce({getLoopContainer(RHS_BO->getRHS())}, write, {},
																  RHS_BO, BO);
											}
										}
									}
										// invariant = i + invariant;
									else if (isLoopElem(RHS_BO->getLHS())) {
										if (auto *read = dyn_cast<DeclRefExpr>(
												RHS_BO->getRHS()->IgnoreParenImpCasts())) {
											if (Functions::isSameVariable(write->getFoundDecl()->getDeclName(),
																		  read->getFoundDecl()->getDeclName())) {
												return new Reduce({getLoopContainer(RHS_BO->getLHS())}, write, {},
																  RHS_BO, BO);
											}
										}
									}
								}
							}
						}
					}
				}

				return nullptr;
			}

			bool LoopExplorer::isRepeatedForStmt(const Stmt *FS) {
				for (const Stmt *currentFor : visitedForLoopList) {
					if (currentFor == FS) {
						parallelizable = false;
						if (verbose) {
							Check.diag(FS->getBeginLoc(),
									   Diag::label + "Recursion makes for loop parallelization unsafe");
						}
						return true;
					}
				}
				return false;
			}

			void LoopExplorer::appendForLoopList() {
				if (parallelizable) {
					forLoopList.insert(forLoopList.begin(), visitedForLoopList.begin(),
									   visitedForLoopList.end());
				}
			}

			std::vector<DeclarationName> LoopExplorer::getValidParameterList(FunctionDecl *functionDeclaration) {
				std::vector<DeclarationName> validParameterList;
				for (unsigned i = 0; i < functionDeclaration->getNumParams(); i++) {
					ParmVarDecl *PVD = functionDeclaration->getParamDecl(i);
					if (PVD->getType()->isBuiltinType()) {
						validParameterList.push_back(PVD->getDeclName());
					}
				}
				return validParameterList;
			}

			void LoopExplorer::appendVisitedFunctionDeclarationList(
					std::vector<const FunctionDecl *> visitedFunctionDeclarations) {
				visitedFunctionDeclarationList.insert(
						visitedFunctionDeclarationList.begin(),
						visitedFunctionDeclarations.begin(), visitedFunctionDeclarations.end());
			}

			bool LoopExplorer::isLocalVariable(DeclarationName DN) {
				for (auto declarationName : localVariables) {
					if (Functions::isSameVariable(declarationName, DN)) {
						return true;
					}
				}
				return false;
			}

			std::string LoopExplorer::getArrayBeginOffset() const {
				return "";
			}

			std::string LoopExplorer::getArrayEndOffset() const {
				return "";
			}

			std::string LoopExplorer::getArrayEndString() const {
				return "std::end(";
			}

			bool LoopExplorer::isVariableUsedInArraySubscript(const DeclRefExpr *dre) {
				return true;
			}

			std::string LoopExplorer::getMultipleInputTransformation() {
				return "std::make_tuple( ";
			}

			std::string LoopExplorer::getBeginInputAsString(const DeclRefExpr *inputName) {
				return getBeginInputTransformation(inputName) +
					   inputName->getNameInfo().getName().getAsString()
					   + getCloseBeginInputTransformation(inputName) + getStartOffsetString();
			}

			std::string LoopExplorer::getEndInputAsString(const DeclRefExpr *inputName) {
				return getEndInputTransformation(inputName) +
					   inputName->getNameInfo().getName().getAsString()
					   + getCloseEndInputTransformation(inputName) + getEndOffsetString();
			}

			std::string LoopExplorer::getOutputAsString(const DeclRefExpr *output) {
				return getBeginInputTransformation(output) +
					   output->getNameInfo().getName().getAsString()
					   + getCloseBeginInputTransformation(output) + getStartOffsetString();;
			}

			std::string LoopExplorer::getElementAsString(const DeclRefExpr *elem) const {
				return elem->getNameInfo().getAsString();
			}

			/*
				 * Gets beginning iterator for inputs that are not pointers
				 *
				 * */
			std::string LoopExplorer::getBeginInputTransformation(const DeclRefExpr *expr) {
				if (!expr->getType()->isPointerType() && !expr->getType()->isArrayType()) {
					return "std::begin(";
				}
				return "";
			}

			/*
			 * Closes parenthesis for inputs that require it
			 */
			std::string LoopExplorer::getCloseBeginInputTransformation(const DeclRefExpr *expr) {
				if (!expr->getType()->isPointerType() && !expr->getType()->isArrayType()) {
					return ")";
				}
				return "";
			}

			/*
			 * Gets back iterator for input
			 * Integer for loops have their own ending
			 */
			std::string LoopExplorer::getEndInputTransformation(const DeclRefExpr *expr) {

				if (expr != nullptr && !expr->getType()->isPointerType()) {
					return getArrayEndString();
				}
				return "";
			}

			/*
			 * Closes parenthesis for inputs that require it
			 *
			 */
			std::string LoopExplorer::getCloseEndInputTransformation(const DeclRefExpr *expr) {
				if (!expr->getType()->isPointerType()) {
					return ")";
				}
				return "";
			}

			/*
			 * Gets the beginning offset as a string in order to obtain the correct iterator as the starting point
			 */
			std::string LoopExplorer::getStartOffsetString() {
				std::string result = getArrayBeginOffset();
				if (result == "") return "";
				return " + " + result;
			}

			std::string LoopExplorer::getEndOffsetString() {
				std::string result = getArrayEndOffset();
				if (result == "") return "";
				return " + " + result;
			}

			std::string LoopExplorer::getPatternTransformationInput(Pattern &pattern) {
				std::string transformation = "";
				if (pattern.Input.empty()) {
					pattern.Input.push_back(pattern.Output);
				}

				std::string add_comma = ", ";

				//if more than one input, put it in a tuple
				if (pattern.Input.size() > 1) {
					add_comma += getMultipleInputTransformation();
				}
				for (auto &input : pattern.Input) {
					const DeclRefExpr *inputName = getPointer(input);
					if (inputName == nullptr) return "input null";

					transformation += add_comma + getBeginInputAsString(inputName);
					add_comma = ", ";
				}
				if (pattern.Input.size() > 1) {
					transformation += ")";
				}
				return transformation;
			}

			std::string LoopExplorer::getPatternTransformationInputEnd(const Pattern &pattern) {
				if (pattern.Input.empty()) return "no input";
				const Expr *input = pattern.Input[0];
				if (input == nullptr) return "no input";

				const DeclRefExpr *inputName = getPointer(input);
				if (inputName == nullptr) return "no input";

				std::string transformation = "";
				std::string endInput = getEndInputAsString(inputName);
				if (endInput != "") {
					transformation += ", " + endInput;
				}

				return transformation;
			}

			std::string LoopExplorer::getMapTransformationOutput(const Pattern &pattern) {
				std::string transformation = "";

				const DeclRefExpr *output = getPointer(pattern.Output);
				if (output == nullptr)
					return "output null";

				transformation += ", " + getOutputAsString(output);

				return transformation;
			}

			std::string LoopExplorer::getMapTransformationLambdaParameters(Map &map) {
				std::string transformation = ", [=](";

				if (map.isCompoundAssignmentBO()) {
					if (!Functions::hasElement(map.Input, map.Output)) {
						map.Input.push_back(map.Output);
					}
				}

				int numElem = 0;
				for (auto &element:map.Input) {
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
				transformation += ")";

				return transformation;
			}

			std::string LoopExplorer::getMapTransformationLambdaBody(const std::vector<Map>::iterator &map) {
				SourceManager &SM = Context->getSourceManager();

				std::string mapLambda = "";

				Rewriter rewriter(SM, LangOptions());
				int offset = 0;
				SourceRange currentRange;

				// remove all other maps
				removePatternFromRewriter(currentRange, offset, MapList, map, SM, rewriter);

				//remove all other reduces
				removePatternFromRewriter(currentRange, offset, ReduceList, SM, rewriter);

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
											 LoopConstant::startElement + getElementAsString(elem));
					}
				}

				if (map->isCompoundAssignmentBO()) {
					const DeclRefExpr *output = getPointer(map->Output);
					if (output == nullptr)
						return "output null";
					std::string end_lambda = " ) " + map->getOperatorAsString(SM) + " " +
											 LoopConstant::startElement +
											 output->getNameInfo().getName().getAsString();
					rewriter.InsertTextAfter(Lexer::getLocForEndOfToken(map->mapFunction->getEndLoc(),
																		offset, SM, LangOptions()), end_lambda);
				}

				std::string to_insert = "return ";
				if (map->isCompoundAssignmentBO()) {
					to_insert += "(";
				}
				rewriter.InsertTextBefore(
						map->mapFunction->getBeginLoc().getLocWithOffset(offset), to_insert);


				mapLambda = rewriter.getRewrittenText(
						getVisitingForLoopBody()->getSourceRange());

				return mapLambda;
			}


			std::string LoopExplorer::getReduceTransformationLambdaParameters(const Reduce &reduce) {
				std::string transformation = ", [=](";
				int numElem = 0;
				if (reduce.Element.size() == 2) {
					for (auto elem : reduce.Element) {
						if (numElem != 0) transformation += ", ";
						const DeclRefExpr *name = getPointer(elem);
						if (name == nullptr) parallelizable = false;
						else
							transformation += "auto " + LoopConstant::startElement +
											  name->getNameInfo().getName().getAsString();
						numElem++;
					}
					transformation += ")";
				} else
					transformation +=
							"auto " + LoopConstant::startElement + "x, auto " + LoopConstant::startElement + "y)";
				return transformation;
			}

			std::string LoopExplorer::getReduceTransformationLambdaBody(const std::vector<Reduce>::iterator &reduce) {
				SourceManager &SM = Context->getSourceManager();
				std::string reduceLambda = "";

				Rewriter rewriter(SM, LangOptions());
				int offset = 0;
				SourceRange currentRange;

				// remove all other reduces
				removePatternFromRewriter(currentRange, offset, ReduceList, reduce, SM, rewriter);

				//remove all other maps
				removePatternFromRewriter(currentRange, offset, MapList, SM, rewriter);

				std::string to_insert = "return ";
				if (reduce->Element.size() == 2) {
					if (auto *BO = dyn_cast<BinaryOperator>(reduce->original_expr->IgnoreParenImpCasts())) {
						currentRange = SourceRange(
								BO->getBeginLoc().getLocWithOffset(offset),
								BO->getOperatorLoc().getLocWithOffset(offset));
						rewriter.RemoveText(currentRange);
						offset -= rewriter.getRangeSize(currentRange);
					}

					for (const auto *read:reduce->Element) {
						const DeclRefExpr *elem = getPointer(read);
						if (elem != nullptr) {
							currentRange = SourceRange(read->getSourceRange());
							rewriter.ReplaceText(currentRange,
												 LoopConstant::startElement + elem->getNameInfo().getAsString());
						}
					}
				} else {
					//remove original reduce expression to be replaced
					reduce->removeFromRewriter(currentRange, offset, SM, rewriter);


					to_insert += LoopConstant::startElement + "x" + reduce->getOperatorAsString() +
								 LoopConstant::startElement + "y;";
				}

				rewriter.InsertTextBefore(
						reduce->original_expr->getBeginLoc().getLocWithOffset(offset), to_insert);


				reduceLambda = rewriter.getRewrittenText(
						getVisitingForLoopBody()->getSourceRange());

				return reduceLambda;
			}

			std::string LoopExplorer::getMapTransformation() {
				std::string transformation;
				std::vector<Map> PastMapList;
				for (std::vector<Map>::iterator map = MapList.begin(); map != MapList.end(); map++) {
					transformation += "grppi::map(grppi::dynamic_execution()";
					//Input
					transformation += getPatternTransformationInput(*map);

					//End iterator of input
					transformation += getPatternTransformationInputEnd(*map);

					//Output
					transformation += getMapTransformationOutput(*map);

					//Parameters for lambda expression
					transformation += getMapTransformationLambdaParameters(*map);

					//Lambda body
					std::string mapLambda = getMapTransformationLambdaBody(map);
					transformation += mapLambda + ");\n";

					//End of current map translation
					PastMapList.push_back(*map);
				}
				//Removes new line characters that may have existed
				transformation.erase(
						std::remove(transformation.begin(), transformation.end(), '\n'),
						transformation.end());

				return transformation;
			}

			std::string LoopExplorer::getReduceTransformation() {
				std::string transformation;
				std::vector<Reduce> PastReduceList;
				for (std::vector<Reduce>::iterator reduce = ReduceList.begin();
					 reduce != ReduceList.end(); reduce++) {
					std::string variable = "";
					const DeclRefExpr *dre = getPointer(reduce->Output);
					if (dre != nullptr) {
						variable = dre->getNameInfo().getAsString();
					}
					transformation += variable + " " + reduce->getOperatorAsString() + "= ";
					transformation += "grppi::reduce(grppi::dynamic_execution()";

					//Input
					transformation += getPatternTransformationInput(*reduce);

					//End iterator of input
					transformation += getPatternTransformationInputEnd(*reduce);

					//Get reduce identity
					transformation += ", " + reduce->getIdentityAsString();

					//Parameters for lambda expression
					transformation += getReduceTransformationLambdaParameters(*reduce);

					//Lambda body
					std::string reduceLambda = getReduceTransformationLambdaBody(reduce);
					transformation += reduceLambda + ");\n";

					//End of current map translation
					PastReduceList.push_back(*reduce);
				}
				//Removes new line characters that may have existed
				transformation.erase(
						std::remove(transformation.begin(), transformation.end(), '\n'),
						transformation.end());

				return transformation;
			}

			std::string LoopExplorer::getMapReduceTransformation() {
				if (MapList.size() != 1 || ReduceList.size() != 1) return "";
				std::vector<Map>::iterator map = MapList.begin();
				std::vector<Reduce>::iterator reduce = ReduceList.begin();

				std::string transformation = "";

				std::string variable = "";

				const DeclRefExpr *dre = getPointer(reduce->Output);
				if (dre != nullptr) {
					variable = dre->getNameInfo().getAsString();
				}
				transformation += variable + " " + reduce->getOperatorAsString() + "= ";
				transformation += "grppi::map_reduce(grppi::dynamic_execution()";

				//Input of map
				transformation += getPatternTransformationInput(*map);

				//End iterator of input of map
				transformation += getPatternTransformationInputEnd(*map);

				//Get reduce identity of reduce
				transformation += ", " + reduce->getIdentityAsString();

				//Parameters for lambda expression
				transformation += getMapTransformationLambdaParameters(*map);

				//Lambda body
				std::string mapLambda = getMapTransformationLambdaBody(map);
				transformation += mapLambda + "";

				//Parameters for lambda expression
				transformation += getReduceTransformationLambdaParameters(*reduce);

				//Lambda body
				std::string reduceLambda = getReduceTransformationLambdaBody(reduce);
				transformation += reduceLambda + ");\n";

				//Removes new line characters that may have existed
				transformation.erase(
						std::remove(transformation.begin(), transformation.end(), '\n'),
						transformation.end());

				return transformation;
			}

			const StringRef LoopExplorer::getSourceText(const Expr *expr) const {
				return getSourceText(expr->getSourceRange());
			}

			const StringRef LoopExplorer::getSourceText(const SourceRange source_range) const {
				return Lexer::getSourceText(CharSourceRange::getTokenRange(source_range), Context->getSourceManager(),
											LangOptions());
			}


			//IntegerForLoop
			const Expr *IntegerForLoopExplorer::getLoopContainer(Expr *write) {
				return write;
			}

			std::string IntegerForLoopExplorer::getArrayBeginOffset() const {
				std::string result = Lexer::getSourceText(
						CharSourceRange::getTokenRange(start_expr->getSourceRange()),
						Context->getSourceManager(),
						LangOptions()).str();
				if (result == "0") return "";
				return result;
			}

			std::string IntegerForLoopExplorer::getArrayEndString() const {
				return "std::begin(";
			}

			std::string IntegerForLoopExplorer::getArrayEndOffset() const {
				std::string result = Lexer::getSourceText(
						CharSourceRange::getTokenRange(end_expr->getSourceRange()),
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

			std::unique_ptr<const uint64_t> IntegerForLoopExplorer::getStartValue() const {
				if (auto *start = dyn_cast<IntegerLiteral>(
						start_expr->IgnoreParenImpCasts())) {
					return std::make_unique<const uint64_t>(start->getValue().getZExtValue());
				}
				return nullptr;
			}

			/*
			 * Returns end value of loop, where iterator_variable < end
			 * */
			std::unique_ptr<const uint64_t> IntegerForLoopExplorer::getEndValue() const {
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
						if (verbose) {
							Check.diag(array.getOriginal()->getBeginLoc(),
									   Diag::label + "Pointer is invalid");
						}
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
				if (verbose) {
					Check.diag(array.getOriginal()->getBeginLoc(),
							   Diag::label + "Pointer has invalid subscript");
				}
				return 0;
			}

			bool IntegerForLoopExplorer::addToReadArraySubscriptList(CustomArray array, ASTContext *context) {
				if (isValidArraySubscript(array) == 3) return false;
				const Expr *base = array.getBase();
				const Expr *index = array.getIndex();
				for (CustomArray write_array : writeArraySubscriptList) {
					if (Functions::areSameExpr(context, write_array.getBase(), base)) {
						if (!Functions::areSameExpr(context, write_array.getIndex(), index) &&
							isValidArraySubscript(array) != -1) {
							if (verbose) {
								Check.diag(
										write_array.getIndex()->getBeginLoc(),
										Diag::label + "Inconsistent array subscription makes for loop "
													  "parallelization unsafe");
							}
							parallelizable = false;
							return false;
						} else {
							// read is actually write
							if (array.getOriginal()->getSourceRange() ==
								write_array.getOriginal()->getSourceRange()) {
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
						if (!Functions::areSameExpr(context, read_array.getIndex(), index) &&
							isValidArraySubscript(array) != -1) {
							if (verbose) {
								Check.diag(
										array.getOriginal()->getBeginLoc(),
										Diag::label + "Inconsistent array subscription makes for loop "
													  "parallelization unsafe");
							}
							parallelizable = false;
							return false;
						}
					}
				}
				for (CustomArray write_array : writeArraySubscriptList) {
					if (Functions::areSameExpr(context, write_array.getBase(), base)) {
						if (!Functions::areSameExpr(context, write_array.getIndex(), index)) {
							if (verbose) {
								Check.diag(
										write_array.getOriginal()->getBeginLoc(),
										Diag::label + "Inconsistent array subscription makes for loop "
													  "parallelization unsafe");
							}
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

			bool IntegerForLoopExplorer::isArrayLoopElem(CustomArray array) {
				// if not a local subscript to local array, make sure it is a valid write
				const DeclRefExpr *ArraySubscriptPointer = getPointer(array.getOriginal());
				if (ArraySubscriptPointer == nullptr) {
					if (verbose) {
						Check.diag(array.getOriginal()->getBeginLoc(),
								   Diag::label + "Write to pointer subscript too complex to analyze makes "
												 "loop parallelization unsafe");
					}
					parallelizable = false;
					return false;
				} else if (!isLocalVariable(
						ArraySubscriptPointer->getDecl()->getDeclName())) {
					if (isValidArraySubscript(array) == 1) return true;
					if (isValidArraySubscript(array) == -1) {
						if (verbose) {
							Check.diag(array.getOriginal()->getBeginLoc(),
									   Diag::label +
									   "Index cannot be analysed properly. Parallelization may still be possible");
						}
						return true;
					}
				}
				return false;
			}

			bool IntegerForLoopExplorer::isMapAssignment(Expr *write) {
				if (auto *BO_LHS = dyn_cast<ArraySubscriptExpr>(write->IgnoreParenImpCasts())) {
					if (isLoopElem(write)) {
						CustomArray a(BO_LHS->getBase(), BO_LHS->getIdx(), BO_LHS);
						addToWriteArraySubscriptList(a, Context);
						return true;
					}
				}
				if (auto *OO = dyn_cast<CXXOperatorCallExpr>(write->IgnoreParenImpCasts())) {
					if (OO->getOperator() == OO_Subscript) {
						if (isLoopElem(write)) {
							CustomArray a(OO->getArg(0), OO->getArg(1), OO);
							addToWriteArraySubscriptList(a, Context);
							return true;
						}
					}
				}
				return false;
			}


			bool IntegerForLoopExplorer::isLoopElem(Expr *write) {
				if (auto *BO_LHS = dyn_cast<ArraySubscriptExpr>(write->IgnoreParenImpCasts())) {
					CustomArray a(BO_LHS->getBase(), BO_LHS->getIdx(), BO_LHS);
					if (isArrayLoopElem(a)) {
						return true;
					}
				}
				if (auto *OO = dyn_cast<CXXOperatorCallExpr>(write->IgnoreParenImpCasts())) {
					if (OO->getOperator() == OO_Subscript) {
						CustomArray a(OO->getArg(0), OO->getArg(1), OO);
						if (isArrayLoopElem(a)) {
							return true;
						}
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

			const Stmt *IntegerForLoopExplorer::getVisitingForLoopBody() {
				if (auto forStmt = dyn_cast<ForStmt>(visitingForStmt)) {
					return forStmt->getBody();
				}
				return nullptr;
			}

			bool IntegerForLoopExplorer::haveSameVisitingForLoopHeaderExpressions(const Stmt *stmt) {
				if (auto forStmt = dyn_cast<ForStmt>(stmt)) {
					if (auto vForStmt = dyn_cast<ForStmt>(visitingForStmt)) {
						if (getSourceText((const Expr *) (forStmt->getInit())) ==
							getSourceText((const Expr *) (vForStmt->getInit())) &&
							getSourceText((const Expr *) forStmt->getCond()) ==
							getSourceText((const Expr *) vForStmt->getCond()) &&
							getSourceText(forStmt->getInc()) == getSourceText(vForStmt->getInc())) {
							return true;
						}
					}
				}
				return false;
			}

			//ContainerForLoop
			bool ContainerForLoopExplorer::VisitCXXOperatorCallExpr(CXXOperatorCallExpr *OO) {
				return ExploreDereference(OO);
			}

			bool ContainerForLoopExplorer::VisitUnaryOperator(UnaryOperator *UO) {
				return ExploreDereference(UO);
			}

			bool ContainerForLoopExplorer::ExploreDereference(Expr *expr) {
				DeclRefExpr *DRE = isValidDereference(expr);
				if (DRE != nullptr) {
					if (!Functions::hasElement(writeList, expr)) {
						if (!MapList.empty()) {
							Map &currentMap = MapList[MapList.size() - 1];
							//Dont add element to placeholder if it is inside of current map
							if (!currentMap.isWithin(DRE, Context)) {
								placeHolderMap.Element.push_back(expr);
							}
							currentMap.addElement(expr, Context);
						} else {
							placeHolderMap.Element.push_back(expr);
						}
					}
				} else {
					parallelizable = false;
					if (verbose) {
						Check.diag(expr->getBeginLoc(),
								   Diag::label + "Invalid dereference");
					}
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


			bool ContainerForLoopExplorer::isMapAssignment(Expr *write) {
				if (isLoopElem(write)) {
					writeList.push_back(write);
					return true;
				}
				return false;
			}

			bool ContainerForLoopExplorer::isLoopElem(Expr *write) {
				DeclRefExpr *elem = isValidDereference(write);
				if (elem != nullptr) {
					return true;
				}
				return false;
			}

			const Expr *ContainerForLoopExplorer::getLoopContainer(Expr *write) {
				return LoopContainer;
			}

			std::string ContainerForLoopExplorer::getElementAsString(const DeclRefExpr *elem) const {
				return LoopContainer->getNameInfo().getAsString();
			}

			const Stmt *ContainerForLoopExplorer::getVisitingForLoopBody() {
				if (auto forStmt = dyn_cast<ForStmt>(visitingForStmt)) {
					return forStmt->getBody();
				}
				return nullptr;
			}

			bool ContainerForLoopExplorer::haveSameVisitingForLoopHeaderExpressions(const Stmt *stmt) {
				if (auto forStmt = dyn_cast<ForStmt>(stmt)) {
					if (auto vForStmt = dyn_cast<ForStmt>(visitingForStmt)) {
						if (getSourceText((const Expr *) (forStmt->getInit())) ==
							getSourceText((const Expr *) (vForStmt->getInit())) &&
							getSourceText((const Expr *) forStmt->getCond()) ==
							getSourceText((const Expr *) vForStmt->getCond()) &&
							getSourceText(forStmt->getInc()) == getSourceText(vForStmt->getInc())){
							return true;
						}
					}
				}
				return false;
			}

			//Range For Loop
			bool RangeForLoopExplorer::VisitDeclRefExpr(DeclRefExpr *DRE) {

				bool continueExploring = LoopVisitor::VisitDeclRefExpr(DRE);
				if (DRE->getFoundDecl() == iterator_variable) {

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

			bool RangeForLoopExplorer::isMapAssignment(Expr *write) {
				DeclRefExpr *elem = isElemDeclRefExpr(write);
				if (elem != nullptr) {
					if (isLoopElem(write)) {
						writeList.push_back(elem);
						return true;
					}
				}
				return false;
			}

			bool RangeForLoopExplorer::isLoopElem(Expr *write) {
				DeclRefExpr *elem = isElemDeclRefExpr(write);
				if (elem != nullptr) {
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

			std::string RangeForLoopExplorer::getElementAsString(const DeclRefExpr *elem) const {
				return LoopContainer->getNameInfo().getAsString();
			}

			const Stmt *RangeForLoopExplorer::getVisitingForLoopBody() {
				if (auto *rangeLoop = dyn_cast<CXXForRangeStmt>(
						visitingForStmt)) {
					return rangeLoop->getBody();

				}
				return nullptr;
			}

			bool RangeForLoopExplorer::haveSameVisitingForLoopHeaderExpressions(const Stmt *stmt) {
				if (auto *rangeLoop = dyn_cast<CXXForRangeStmt>(
						stmt)) {
					if (auto *vRangeLoop = dyn_cast<CXXForRangeStmt>(
							visitingForStmt)) {
						//list.push_back(rangeLoop->getLoopVarStmt());
						if (getSourceText((const Expr *) rangeLoop->getLoopVarStmt()) ==
							getSourceText((const Expr *) (vRangeLoop->getLoopVarStmt())) ) {
							return true;
						}
					}
				}
				return false;
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
																							 hasOperatorName(
																									 "--")))))))
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
															hasRHS(expr(
																	expr(hasType(isInteger())).bind("end_expr"))))
													 .bind("condFor")),
								hasIncrement(unaryOperator(hasOperatorName("++"),
														   hasUnaryOperand(declRefExpr(
																   to(varDecl(hasType(isInteger())).bind(
																		   "incVar")))))
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
												   hasDescendant(
														   cxxOperatorCallExpr(hasOverloadedOperatorName("!="),
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

				auto currentPattern = std::make_shared<IntegerForLoopExplorer>(Result.Context, *this,
																			   std::vector<const Stmt *>(),
																			   IntegerForLoop,
																			   start_expr,
																			   end_expr,
																			   InitVar, IntegerForLoopSizeMin, Verbose);

				currentPattern->TraverseStmt(const_cast<Stmt *>(IntegerForLoop->getBody()));

				currentPattern->appendForLoopList();
				addDiagnostic(std::move(currentPattern), IntegerForLoop);

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
				iteratorForLoop->dump();
				if (!Functions::isSameVariable(IncVar, CondVar) || !Functions::isSameVariable(IncVar, InitVar))
					return;
				std::cout << "Processing Iterator loop" << std::endl;

				auto currentPattern = std::make_shared<ContainerForLoopExplorer>(
						ContainerForLoopExplorer(Result.Context, *this, {}, iteratorForLoop, IncVar,
												 matchedArray, Verbose));

				currentPattern->TraverseStmt(const_cast<Stmt *>(iteratorForLoop->getBody()));
				addDiagnostic(std::move(currentPattern), iteratorForLoop);
			}

			void MapReduceCheck::ProcessRangeForLoop(const CXXForRangeStmt *rangeForLoop,
													 const MatchFinder::MatchResult &Result) {
				std::cout << "Processing range for loop" << std::endl;
				const auto *iterator = rangeForLoop->getLoopVariable();
				if (const auto *input = dyn_cast<DeclRefExpr>(
						rangeForLoop->getRangeInit()->IgnoreParenImpCasts())) {
					auto currentPattern = std::make_shared<RangeForLoopExplorer>(
							RangeForLoopExplorer(Result.Context, *this, {}, rangeForLoop, iterator,
												 input, Verbose));
					currentPattern->TraverseStmt(const_cast<Stmt *>( rangeForLoop->getBody()));
					addDiagnostic(std::move(currentPattern), rangeForLoop);
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
				Options.store(Opts, "Verbose", Verbose);
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
