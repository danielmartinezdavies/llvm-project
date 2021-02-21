//===--- MiscTidyModule.cpp - clang-tidy ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "MapReduceCheck.h"

namespace clang {
namespace tidy {
namespace grppi {

class GrppiModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<MapReduceCheck>(
        "grppi-map-reduce");
   
  }
};

} // namespace grppi

// Register the GrppiTidyModule using this statically initialized variable.
static ClangTidyModuleRegistry::Add<grppi::GrppiModule>
    X("grppi-module", "Adds grppi-specific lint checks.");

// This anchor is used to force the linker to link in the generated object file
// and thus register the GrppiModule.
volatile int GrppiModuleAnchorSource = 0;

} // namespace tidy
} // namespace clang
