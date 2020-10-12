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
        namespace misc {


            std::string Map::startElement = "grppi_";


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


            //IntegerForLoop
            const Expr *IntegerForLoopExplorer::getOutput(Expr *write) {
                return write;
            }

            bool IntegerForLoopExplorer::VisitArraySubscriptExpr(ArraySubscriptExpr *ase) {
                const DeclRefExpr *base = getPointer(ase);
                if (base == nullptr) {
                    Check.diag(ase->getBeginLoc(),
                               "Pointer is invalid");
                    parallelizable = false;
                    return true;
                }
                std::cout << "Type: " << ase->getType().getAsString() << std::endl;

                if (!base->getType()->isArrayType()) {
                    PointerHasValidLastValue((VarDecl *) base->getDecl(), ase);
                }

                const auto PointerDereferences =
                        match(findAll(expr(anyOf(
                                arraySubscriptExpr(
                                        hasAncestor(forStmt(hasDescendant(equalsNode(visitingForStmtBody))))),
                                unaryOperator(hasOperatorName("*"),
                                              hasAncestor(forStmt(equalsNode(visitingForStmtBody))))
                        )).bind("pointer")), *Context);

                bool isInput = addToReadArraySubscriptList(ase, Context);
                if (isInput) {
                    //add Input and Element for possible future map
                    if (!MapList.empty()) {
                        Map &currentMap = MapList[MapList.size() - 1];
                        //Dont add element to placeholder if it is inside of current map
                        if (!currentMap.isWithin(ase, Context)) {
                            placeHolderMap.Element.push_back(ase);
                            addInput(placeHolderMap, ase);
                        }
                        //add Input and Element for current maps if possible
                        bool addedElem = currentMap.addElement(ase, Context);
                        if (addedElem) {
                            addInput(currentMap, ase);
                        }
                    } else {
                        placeHolderMap.Element.push_back(ase);
                        addInput(placeHolderMap, ase);
                    }


                }
                return true;
            }

            bool IntegerForLoopExplorer::addInput(Map &map, Expr *expr) {
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

            int IntegerForLoopExplorer::isValidArraySubscript(ArraySubscriptExpr *ase) {
                if (auto *index = dyn_cast<IntegerLiteral>(
                        ase->getIdx()->IgnoreParenImpCasts())) {
                    int indexValue = index->getValue().getZExtValue();
                    if (indexValue < start || indexValue > end) return 2;
                    return 1;
                }
                if (auto *dre = dyn_cast<DeclRefExpr>(
                        ase->getIdx()->IgnoreParenImpCasts())) {
                    if (dre->getDecl() == iterator_variable) {

                        return 1;
                    }
                }
                parallelizable = false;
                Check.diag(ase->getBeginLoc(),
                           "Pointer has invalid subscript");
                return 0;
            }

            bool IntegerForLoopExplorer::addToReadArraySubscriptList(ArraySubscriptExpr *ase,
                                                                     ASTContext *context) {

                if (isValidArraySubscript(ase) == 2) return false;
                Expr *base = ase->getBase();
                Expr *index = ase->getIdx();
                for (ArraySubscriptExpr *array : writeArraySubscriptList) {
                    if (Functions::areSameExpr(context, array->getBase(), base)) {
                        if (!Functions::areSameExpr(context, array->getIdx(), index)) {
                            Check.diag(ase->getBeginLoc(),
                                       "Inconsistent array subscription makes for loop "
                                       "parallelization unsafe");
                            parallelizable = false;
                            return false;
                        } else {
                            // read is actually write
                            if (ase->getSourceRange() == array->getSourceRange()) {
                                return false;
                            }
                        }
                    }
                }
                readArraySubscriptList.push_back(ase);
                return true;
            }

            bool IntegerForLoopExplorer::addToWriteArraySubscriptList(ArraySubscriptExpr *ase,
                                                                      ASTContext *context) {
                if (isValidArraySubscript(ase) == 2) return false;
                Expr *base = ase->getBase();
                Expr *index = ase->getIdx();

                for (ArraySubscriptExpr *array : readArraySubscriptList) {
                    if (Functions::areSameExpr(context, array->getBase(), base)) {
                        if (!Functions::areSameExpr(context, array->getIdx(), index)) {
                            Check.diag(ase->getBeginLoc(),
                                       "Inconsistent array subscription makes for loop "
                                       "parallelization unsafe");
                            parallelizable = false;
                            return false;
                        }
                    }
                }

                for (ArraySubscriptExpr *array : writeArraySubscriptList) {
                    if (Functions::areSameExpr(context, array->getBase(), base)) {
                        if (!Functions::areSameExpr(context, array->getIdx(), index)) {
                            Check.diag(ase->getBeginLoc(),
                                       "Inconsistent array subscription makes for loop "
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

                writeArraySubscriptList.push_back(ase);
                return true;
            }

            bool IntegerForLoopExplorer::isMapAssignment(Expr *write) {
                if (auto *BO_LHS = dyn_cast<ArraySubscriptExpr>(write)) {
                    // if not a local subscript to local array, make sure it is a valid write
                    const DeclRefExpr *ArraySubscriptPointer = getPointer(BO_LHS);
                    if (ArraySubscriptPointer == nullptr) {
                        Check.diag(write->getBeginLoc(),
                                   "Write to pointer subscript too complex to analyze makes "
                                   "loop parallelization unsafe");
                        parallelizable = false;
                        return false;
                    } else if (!isLocalVariable(
                            ArraySubscriptPointer->getDecl()->getDeclName())) {
                        return addToWriteArraySubscriptList(BO_LHS, Context);
                    }
                    return true;
                }
                return false;
            }

            //ContainerForLoop
            bool ContainerForLoopExplorer::VisitCXXOperatorCallExpr(CXXOperatorCallExpr *OO) {
                DeclRefExpr *DRE = isValidDereference(OO);
                if (DRE != nullptr) {
                    if (!hasElement(writeList, DRE)) {
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
                               "Invalid dereference");
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
                return nullptr;
            }

            bool ContainerForLoopExplorer::isMapAssignment(Expr *write) {
                DeclRefExpr *elem = isValidDereference(write);
                if (elem != nullptr) {
                    writeList.push_back(elem);
                    return true;
                }
                return false;
            }

            const Expr *ContainerForLoopExplorer::getOutput(Expr *write) {
                return Output;
            }

            //Range For Loop
            bool RangeForLoopExplorer::VisitDeclRefExpr(DeclRefExpr *DRE) {
                bool continueExploring = LoopExplorer::VisitDeclRefExpr(DRE);
                if (DRE->getFoundDecl() == iterator_variable) {
                    if (!hasElement(writeList, DRE)) {
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
                        return dre;
                    }
                }
                return nullptr;
            }

            bool RangeForLoopExplorer::isMapAssignment(Expr *write) {
                DeclRefExpr *elem = isElemDeclRefExpr(write);
                if (elem != nullptr) {
                    writeList.push_back(elem);
                    return true;
                }
                return false;
            }

            const Expr *RangeForLoopExplorer::getOutput(Expr *write) {
                return Output;
            }

//
// Matcher
//
            const auto getForRestrictions = [] {
                return hasBody(unless(anyOf(hasDescendant(gotoStmt()),
                                            hasDescendant(breakStmt()),
                                            hasDescendant(returnStmt()),
                                            hasDescendant(unaryOperator(hasOperatorName("*"),
                                                                        hasDescendant(binaryOperator())))
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
                // FIXME: Add matchers.

                Finder->addMatcher(
                        forStmt(
                                hasLoopInit(
                                        declStmt(hasSingleDecl(
                                                varDecl(hasInitializer(integerLiteral().bind("start"))).bind(
                                                        "initVar")))
                                                .bind("initFor")),
                                hasCondition(binaryOperator(hasOperatorName("<"),
                                                            hasLHS(ignoringParenImpCasts(declRefExpr(
                                                                    to(varDecl(hasType(isInteger())).bind(
                                                                            "condVar"))))),
                                                            hasRHS(expr(integerLiteral().bind("end"))))
                                                     .bind("condFor")),
                                hasIncrement(unaryOperator(hasOperatorName("++"),
                                                           hasUnaryOperand(declRefExpr(
                                                                   to(varDecl(hasType(isInteger())).bind("incVar")))))
                                                     .bind("incFor")),
                                getForRestrictions())
                                .bind("forLoop"),
                        this);

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
                Finder->addMatcher(cxxForRangeStmt(getForRestrictions()).bind("rangeForLoop"),
                                   this);
                /*Finder->addMatcher(
                        forStmt().bind("dummy"),
                        this);*/
            }

            void MapReduceCheck::ProcessIntegerForLoop(const ForStmt *IntegerForLoop,
                                                       const MatchFinder::MatchResult &Result) {

                std::cout << "Processing Integer loop" << std::endl;
                const auto *IncVar = Result.Nodes.getNodeAs<VarDecl>("incVar");
                const auto *CondVar = Result.Nodes.getNodeAs<VarDecl>("condVar");
                const auto *InitVar = Result.Nodes.getNodeAs<VarDecl>("initVar");
                const auto *start = Result.Nodes.getNodeAs<IntegerLiteral>("start");
                int startValue = start->getValue().getZExtValue();
                const auto *end = Result.Nodes.getNodeAs<IntegerLiteral>("end");
                int endValue = end->getValue().getZExtValue() - 1;

                if (!Functions::isSameVariable(IncVar, CondVar) || !Functions::isSameVariable(IncVar, InitVar))
                    return;

                if (Functions::alreadyExploredForLoop(IntegerForLoop))
                    return;

                forLoopList.push_back(IntegerForLoop);

                IntegerForLoop->dump();

                IntegerForLoopExplorer currentMap(Result.Context, *this,
                                                  std::vector<const Stmt *>(), IntegerForLoop->getBody(), startValue,
                                                  endValue,
                                                  InitVar);
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
                iteratorForLoop->dump();
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
                }

                const auto *const iteratorForLoop = Result.Nodes.getNodeAs<ForStmt>("iteratorForLoop");
                if (iteratorForLoop != nullptr) {
                    ProcessIteratorForLoop(iteratorForLoop, Result);
                }
                const auto *const rangeForLoop = Result.Nodes.getNodeAs<CXXForRangeStmt>("rangeForLoop");
                if (rangeForLoop != nullptr) {
                    ProcessRangeForLoop(rangeForLoop, Result);
                }


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
            }

        } // namespace misc
    } // namespace tidy
} // namespace clang


