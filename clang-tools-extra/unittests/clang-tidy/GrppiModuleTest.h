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
			StringRef Vector = "namespace std{\n"
					  	 	   "template< class C >\n"
							   "auto begin( C& c ) -> decltype(c.begin());\n"
					  		   "template< class C >\n"
							   "auto end( C& c ) -> decltype(c.end());\n"
		  					   "template <class T>\n"
							   "class  vector\n"
							   "{\n"
							   "public:\n"
							   "\n"
							   "    typedef T * iterator;\n"
							   "\n"
							   "    vector();\n"
							   "    vector(unsigned int size);\n"
							   "    vector(unsigned int size, const T & initial);\n"
							   "    vector(const vector<T> & v);      \n"
							   "    ~vector();\n"
							   "\n"
							   "    unsigned int capacity() const;\n"
							   "    unsigned int size() const;\n"
							   "    bool empty() const;\n"
							   "    iterator begin();\n"
							   "    iterator end();\n"
							   "    T & front();\n"
							   "    T & back();\n"
							   "    void push_back(const T & value); \n"
							   "    void pop_back();  \n"
							   "\n"
							   "    void reserve(unsigned int capacity);   \n"
							   "    void resize(unsigned int size);   \n"
							   "\n"
							   "    T & operator[](unsigned int index);  \n"
							   "    T & operator*();  \n"
							   "    vector<T> & operator=(const vector<T> &);\n"
							   "    void clear();\n"
							   "private:\n"
							   "    unsigned int my_size;\n"
							   "    unsigned int my_capacity;\n"
							   "    T * buffer;\n"
							   "};\n"
							   "\n"
							   "template<class T>\n"
							   "vector<T>::vector()\n"
							   "{\n"
							   "    my_capacity = 0;\n"
							   "    my_size = 0;\n"
							   "    buffer = 0;\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "vector<T>::vector(const vector<T> & v)\n"
							   "{\n"
							   "    my_size = v.my_size;\n"
							   "    my_capacity = v.my_capacity;\n"
							   "    buffer = new T[my_size];  \n"
							   "    for (unsigned int i = 0; i < my_size; i++)\n"
							   "        buffer[i] = v.buffer[i];  \n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "vector<T>::vector(unsigned int size)\n"
							   "{\n"
							   "    my_capacity = size;\n"
							   "    my_size = size;\n"
							   "    buffer = new T[size];\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "vector<T>::vector(unsigned int size, const T & initial)\n"
							   "{\n"
							   "    my_size = size;\n"
							   "    my_capacity = size;\n"
							   "    buffer = new T [size];\n"
							   "    for (unsigned int i = 0; i < size; i++)\n"
							   "        buffer[i] = initial;\n"
							   "    //T();\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "vector<T> & vector<T>::operator = (const vector<T> & v)\n"
							   "{\n"
							   "    delete[ ] buffer;\n"
							   "    my_size = v.my_size;\n"
							   "    my_capacity = v.my_capacity;\n"
							   "    buffer = new T [my_size];\n"
							   "    for (unsigned int i = 0; i < my_size; i++)\n"
							   "        buffer[i] = v.buffer[i];\n"
							   "    return *this;\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "typename vector<T>::iterator vector<T>::begin()\n"
							   "{\n"
							   "    return buffer;\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "typename vector<T>::iterator vector<T>::end()\n"
							   "{\n"
							   "    return buffer + size();\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "T& vector<T>::front()\n"
							   "{\n"
							   "    return buffer[0];\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "T& vector<T>::back()\n"
							   "{\n"
							   "    return buffer[my_size - 1];\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "void vector<T>::push_back(const T & v)\n"
							   "{\n"
							   "    if (my_size >= my_capacity)\n"
							   "        reserve(my_capacity +5);\n"
							   "    buffer [my_size++] = v;\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "void vector<T>::pop_back()\n"
							   "{\n"
							   "    my_size--;\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "void vector<T>::reserve(unsigned int capacity)\n"
							   "{\n"
							   "    if(buffer == 0)\n"
							   "    {\n"
							   "        my_size = 0;\n"
							   "        my_capacity = 0;\n"
							   "    }    \n"
							   "    T * Newbuffer = new T [capacity];\n"
							   "    //assert(Newbuffer);\n"
							   "    unsigned int l_Size = capacity < my_size ? capacity : my_size;\n"
							   "    //copy (buffer, buffer + l_Size, Newbuffer);\n"
							   "\n"
							   "    for (unsigned int i = 0; i < l_Size; i++)\n"
							   "        Newbuffer[i] = buffer[i];\n"
							   "\n"
							   "    my_capacity = capacity;\n"
							   "    delete[] buffer;\n"
							   "    buffer = Newbuffer;\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "unsigned int vector<T>::size()const//\n"
							   "{\n"
							   "    return my_size;\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "void vector<T>::resize(unsigned int size)\n"
							   "{\n"
							   "    reserve(size);\n"
							   "    my_size = size;\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "T& vector<T>::operator[](unsigned int index)\n"
							   "{\n"
							   "    return buffer[index];\n"
							   "}  \n"
							   "\n"
							   "template<class T>\n"
							   "T& vector<T>::operator*()\n"
							   "{\n"
							   "    return buffer[0];\n"
							   "}  \n"
							   "\n"
							   "template<class T>\n"
							   "unsigned int vector<T>::capacity()const\n"
							   "{\n"
							   "    return my_capacity;\n"
							   "}\n"
							   "\n"
							   "template<class T>\n"
							   "vector<T>::~vector()\n"
							   "{\n"
							   "    delete[ ] buffer;\n"
							   "}\n"
							   "template <class T>\n"
							   "void vector<T>::clear()\n"
							   "{\n"
							   "    my_capacity = 0;\n"
							   "    my_size = 0;\n"
							   "    buffer = 0;\n"
							   "} }";

			template<typename... CheckTypes>
			std::string
			runCheckOnFile(StringRef Code, std::map<StringRef, StringRef> PathsToContent =
			std::map<StringRef, StringRef>(), const ClangTidyOptions &ExtraOptions = ClangTidyOptions(),
						   ArrayRef <std::string> ExtraArgs = None, const Twine &Filename = "input.cc") {
				static_assert(sizeof...(CheckTypes) > 0, "No checks specified");
				ClangTidyOptions Options = ExtraOptions;
				Options.Checks = "*";
				ClangTidyContext Context(std::make_unique<DefaultOptionsProvider>(
						ClangTidyGlobalOptions(), Options));
				ClangTidyDiagnosticConsumer DiagConsumer(Context);
				DiagnosticsEngine DE(new DiagnosticIDs(), new DiagnosticOptions,
									 &DiagConsumer, false);
				Context.setDiagnosticsEngine(&DE);

				std::vector<std::string> Args(1, "clang-tidy");
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
				Args.push_back("-Iinclude");
				Args.insert(Args.end(), ExtraArgs.begin(), ExtraArgs.end());
				Args.push_back(Filename.str());

				ast_matchers::MatchFinder Finder;
				llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> InMemoryFileSystem(
						new llvm::vfs::InMemoryFileSystem);
				llvm::IntrusiveRefCntPtr<FileManager> Files(
						new FileManager(FileSystemOptions(), InMemoryFileSystem));

				SmallVector<std::unique_ptr<ClangTidyCheck>, sizeof...(CheckTypes)> Checks;
				tooling::ToolInvocation Invocation(
						Args,
						std::make_unique<TestClangTidyAction<CheckTypes...>>(Checks, Finder,
																			 Context),
						Files.get());
				InMemoryFileSystem->addFile(Filename, 0,
											llvm::MemoryBuffer::getMemBuffer(Code));
				for (const auto &FileContent : PathsToContent) {
					InMemoryFileSystem->addFile(
							Twine("include/") + FileContent.first, 0,
							llvm::MemoryBuffer::getMemBuffer(FileContent.second));
				}

				Invocation.setDiagnosticConsumer(&DiagConsumer);
				if (!Invocation.run()) {
					std::string ErrorText;
					for (const auto &Error : DiagConsumer.take()) {
						ErrorText += Error.Message.Message + "\n";
					}
					llvm::report_fatal_error(ErrorText);
				}

				tooling::Replacements Fixes;
				std::vector<ClangTidyError> Diags = DiagConsumer.take();
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

			}
		}
	}
}


#endif //LLVM_GRPPIMODULETEST_H
