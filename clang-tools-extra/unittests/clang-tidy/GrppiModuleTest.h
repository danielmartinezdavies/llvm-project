#ifndef LLVM_GRPPIMODULETEST_H
#define LLVM_GRPPIMODULETEST_H

#include "ClangTidyCheck.h"
#include "ClangTidyDiagnosticConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Core/Diagnostic.h"
#include "clang/Tooling/Core/Replacement.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Path.h"

#include <map>
#include <memory>



namespace clang {
	namespace tidy {
		namespace test {

			template<typename... CheckTypes>
			std::string
			runCheckOnFile(const Twine &File, const Twine &Path, std::vector <ClangTidyError> *Errors = nullptr,
						   ArrayRef <std::string> ExtraArgs = None,
						   const ClangTidyOptions &ExtraOptions = ClangTidyOptions()) {
				static_assert(sizeof...(CheckTypes) > 0, "No checks specified");
				const Twine &Filename = Path + File;
				ClangTidyOptions Options = ExtraOptions;
				Options.Checks = "*";
				ClangTidyContext Context(std::make_unique<DefaultOptionsProvider>(
						ClangTidyGlobalOptions(), Options));
				ClangTidyDiagnosticConsumer DiagConsumer(Context);
				DiagnosticsEngine DE(new DiagnosticIDs(), new DiagnosticOptions,
									 &DiagConsumer, false);
				Context.setDiagnosticsEngine(&DE);

				std::vector <std::string> Args(1, "../../../../../../bin/clang-tidy");
				Args.push_back("-fsyntax-only");
				Args.push_back("-fno-delayed-template-parsing");
				std::string extension(
						std::string(llvm::sys::path::extension(Filename.str())));
				if (extension == ".m" || extension == ".mm") {
					Args.push_back("-fobjc-abi-version=2");
					Args.push_back("-fobjc-arc");
				}
				if (extension == ".cc" || extension == ".cpp" || extension == ".mm") {
					Args.push_back("-std=c++11");
				}
				Args.insert(Args.end(), ExtraArgs.begin(), ExtraArgs.end());
				Args.push_back(Filename.str());

				ast_matchers::MatchFinder Finder;
				llvm::IntrusiveRefCntPtr <FileManager> Files(
						new FileManager(FileSystemOptions(), llvm::vfs::getRealFileSystem()));
				auto buffer = Files->getBufferForFile(Filename.str());
				if (std::error_code ec = buffer.getError()) {
					return ec.message();
				}

				SmallVector<std::unique_ptr<ClangTidyCheck>, sizeof...(CheckTypes)> Checks;
				tooling::ToolInvocation Invocation(
						Args,
				std::make_unique < TestClangTidyAction < CheckTypes...>>(Checks, Finder,
						Context),
						Files.get());

				Invocation.setDiagnosticConsumer(&DiagConsumer);
				if (!Invocation.run()) {
					std::string ErrorText;
					for (const auto &Error : DiagConsumer.take()) {
						ErrorText += Error.Message.Message + "\n";
					}
					llvm::report_fatal_error(ErrorText);
				}

				tooling::Replacements Fixes;
				std::vector <ClangTidyError> Diags = DiagConsumer.take();
				for (const ClangTidyError &Error : Diags) {
					if (const auto *ChosenFix = tooling::selectFirstFix(Error))
						for (const auto &FileAndFixes : *ChosenFix) {
							for (const auto &Fix : FileAndFixes.second) {
								auto Err = Fixes.add(Fix);
								// FIXME: better error handling. Keep the behavior for now.
								if (Err) {
									llvm::errs() << llvm::toString(std::move(Err)) << "\n";
									return "";
								}
							}
						}
				}

				std::string resulting_fixes = "";
				for (auto fix: Fixes) {
					resulting_fixes += fix.getReplacementText().str();
				}
				return resulting_fixes;

				/*if (Errors)
					*Errors = std::move(Diags);
				auto Result = tooling::applyAllReplacements(Code, Fixes);
				if (!Result) {
					// FIXME: propagate the error.
					llvm::consumeError(Result.takeError());
					return "";
				}
				return *Result;*/
			}
		}
	}
}



#endif //LLVM_GRPPIMODULETEST_H
