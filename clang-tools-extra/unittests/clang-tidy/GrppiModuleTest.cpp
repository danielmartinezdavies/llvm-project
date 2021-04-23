#include "ClangTidyTest.h"
#include "readability/NamespaceCommentCheck.h"
#include "grppi/MapReduceCheck.h"
#include "gtest/gtest.h"

#include "GrppiModuleTest.h"

using namespace clang::tidy::grppi;

static void remove(std::string &s, char c) {
	s.erase(std::remove(s.begin(), s.end(), c),
			s.end());
}

static void removeSpaces(std::string &s) {
	remove(s, '\n');
	remove(s, '\t');
}

static void removeSpaces(std::string &s, std::string &s2) {
	removeSpaces(s);
	removeSpaces(s2);
}

const clang::Twine test_path = "../../../../../../../clang-tools-extra/unittests/clang-tidy/grppi/";

namespace clang {
	namespace tidy {
		namespace test {

			std::map<StringRef, StringRef> PathToVector ={{"vector", Vector}};
			//
			//Map
			//
			//Integer for loop

			// Write to variable declared outside the for loop.
			TEST(MapCheckTest, IntegerLoopWriteGlobalVariable) {
				std::string Expected = "";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint l1 = 0;\n"
																  "\tint l2 = 5;\n"
																  "\n"
																  "\tstd::vector<int> a(10);\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\tl2 = 1;\n"
																  "\t\ta[i] = 0;\n"
																  "\t}\n"
																  "}" , PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopArrayOutput) {
				std::string Expected = "grppi::map(grppi::dynamic_execution(), array, 10, array, [=](auto grppi_array){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("int main() {\n"
																  "\tint array[10];\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\tarray[i] = 0;\n"
																  "\t}\n"
																  "}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopPointerOutput) {
				std::string Expected = "grppi::map(grppi::dynamic_execution(), pointer, 10, pointer, [=](auto grppi_pointer){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint *pointer = new int[10];\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\tpointer[i] = 0;\n"
																  "\t}\n"
																  "}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			TEST(MapCheckTest, IntegerLoopContainerOutput) {
				std::string Expected = "grppi::map(grppi::dynamic_execution(), std::begin(a), 10, std::begin(a), [=](auto grppi_a){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tstd::vector<int> a(10);\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\ta[i] = 0;\n"
																  "\t}\n"
																  "}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputContainerInput) {

				std::string Expected = "grppi::map(grppi::dynamic_execution(), std::begin(b), 10, std::begin(a), [=](auto grppi_b){return  grppi_b;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tstd::vector<int> b(10);\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\ta[i] = b[i];\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputMultipleInput) {

				std::string Expected =
						"	grppi::map(grppi::dynamic_execution(), std::make_tuple( std::begin(b), pointer, array), 10, std::begin(a), "
						"[=](auto grppi_b, auto grppi_pointer, auto grppi_array){return  grppi_b + grppi_pointer + grppi_array;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint array[10];\n"
																  "\tint *pointer = new int[10];\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tstd::vector<int> b(10);\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\ta[i] = b[i] + pointer[i] + array[i];\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputInputModifiedStartAndEnd) {

				std::string Expected =
						"grppi::map(grppi::dynamic_execution(), std::make_tuple( std::begin(b) + 5, pointer + 5, array + 5),"
						" 3, std::begin(a) + 5, [=](auto grppi_b, auto grppi_pointer, auto grppi_array){"
						"return  grppi_b + grppi_pointer + grppi_array;});\n";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint array[10];\n"
						"\tint *pointer = new int[10];\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\n"
						"\t\tfor(int i = 5; i < 8; i++){\n"
						"\t\t\ta[i] = b[i] + pointer[i] + array[i];\n"
						"\t\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			TEST(MapCheckTest, IntegerLoopSizeMinParallelization1) {
				ClangTidyOptions Options;
				Options.CheckOptions["test-check-0.IntegerForLoopSizeMin"] = "5";

				std::string Expected = "grppi::map(grppi::dynamic_execution(), std::begin(a), 10, std::begin(a), [=](auto grppi_a){return  0;});\n";
				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tstd::vector<int> a(10);\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\ta[i] = 0;\n"
																  "\t}\n"
																  "}", PathToVector, Options);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopSizeMinParallelization2) {
				ClangTidyOptions Options;
				Options.CheckOptions["test-check-0.IntegerForLoopSizeMin"] = "10";

				std::string Expected = "grppi::map(grppi::dynamic_execution(), std::begin(a), 10, std::begin(a), [=](auto grppi_a){return  0;});\n";
				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tstd::vector<int> a(10);\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\ta[i] = 0;\n"
																  "\t}\n"
																  "}", PathToVector, Options);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			//Minimum size is too big (loop will not iterate that many times)
			TEST(MapCheckTest, IntegerLoopSizeMinNoParallelization) {
				ClangTidyOptions Options;
				Options.CheckOptions["test-check-0.IntegerForLoopSizeMin"] = "11";

				std::string Expected = "";
				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tstd::vector<int> a(10);\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\ta[i] = 0;\n"
																  "\t}\n"
																  "}", PathToVector, Options);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopSizeMinUndecidable) {
				ClangTidyOptions Options;
				Options.CheckOptions["test-check-0.IntegerForLoopSizeMin"] = "100000";

				std::string Expected =
						"grppi::map(grppi::dynamic_execution(), std::begin(a), l2, std::begin(a), [=](auto grppi_a){return  0;});\n"
						"grppi::map(grppi::dynamic_execution(), std::begin(a) + l1, 5 - l1, std::begin(a) + l1, [=](auto grppi_a){return  0;});\n"
						"grppi::map(grppi::dynamic_execution(), std::begin(a) + l1, l2 - l1, std::begin(a) + l1, [=](auto grppi_a){return  0;});";
				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint l1 = 0;\n"
																  "\tint l2 = 5;\n"
																  "\tstd::vector<int> a(10);\n"
																  "\n"
																  "\tfor(int i = 0; i < l2; i++){\n"
																  "\t\ta[i] = 0;\n"
																  "\t}\n"
																  "\n"
																  "\tfor(int i = l1; i < 5; i++){\n"
																  "\t\ta[i] = 0;\n"
																  "\t}\n"
																  "\n"
																  "\tfor(int i = l1; i < l2; i++){\n"
																  "\t\ta[i] = 0;\n"
																  "\t}\n"
																  "}", PathToVector,
																  Options);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}



			//Container Loops
			TEST(MapCheckTest, ContainerLoopNoInputBegin) {

				std::string Expected = "grppi::map(grppi::dynamic_execution(), std::begin(a), std::end(a), std::begin(a), [=](auto grppi_a){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\n"
																  "\tstd::vector<int> a(10);\n"
																  "\n"
																  "\tfor(auto i = a.begin(); i != a.end(); i++){\n"
																  "\t\t*i = 0;\n"
																  "\t}\n"
																  "}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, ContainerLoopNoInputSTDBegin) {

				std::string Expected = "grppi::map(grppi::dynamic_execution(), std::begin(a), std::end(a), std::begin(a), [=](auto grppi_a){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\n"
																  "\tstd::vector<int> a(10);\n"
																  "\n"
																  "\tfor(auto i = std::begin(a); i != std::end(a); i++){\n"
																  "\t\t\t\t*i = 0;\n"
																  "\t}\n"
																  "}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			//Range for loop
			TEST(MapCheckTest, RangeLoopArrayOutput) {

				std::string Expected = "grppi::map(grppi::dynamic_execution(), array, array, [=](auto grppi_array){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("int main() {\n"
																  "\n"
																  "\tint array[10];\n"
																  "\n"
																  "\tfor (auto &elem: array) {\n"
																  "\t\telem = 0;\n"
																  "\t}\n"
																  "}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, RangeLoopContainerOutput) {

				std::string Expected = "grppi::map(grppi::dynamic_execution(), a, a, [=](auto grppi_a){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\n"
																  "\tstd::vector<int> a(10);\n"
																  "\n"
																  "\tfor (auto &elem: a) {\n"
																  "\t\telem = 0;\n"
																  "\t}\n"
																  "}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, RangeLoopNoReferenceVariable) {
				//No transformation should be applied
				std::string Expected = "";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tfor (auto elem: a) {\n"
																  "\t\telem = 0;\n"
																  "\t}\n"
																  "}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			TEST(MapCheckTest, RangeLoopRValueReferenceVariable) {

				std::string Expected = "grppi::map(grppi::dynamic_execution(), a, a, [=](auto grppi_a){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tfor (auto &&elem: a) {\n"
																  "\t\telem = 0;\n"
																  "\t}\n"
																  "}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			//
			//Reduce
			//
			TEST(ReduceCheckTest, IntegerLoopArrayInputCompoundAddition) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), array, 10, 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("int main() {\n"
																  "\tint k = 0;\n"
																  "\tint array[10];\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\tk += array[i];\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, IntegerLoopArrayInputAdditionLeft) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), array, 10, 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("int main() {\n"
																  "\tint k = 0;\n"
																  "\tint array[10];\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\tk = k + array[i];\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, IntegerLoopArrayInputAdditionRight) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), array, 10, 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("int main() {\n"
																  "\tint k = 0;\n"
																  "\tint array[10];\n"
																  "\n"
																  "\tfor(int i = 0; i < 10; i++){\n"
																  "\t\tk = array[i] + k;\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, IntegerLoopArrayInputCompoundAdditionFloat) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), array, 10, 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"int main() {\n"
						"\tfloat k = 0.f;\n"
						"\tint array[10];\n"
						"\n"
						"\tfor(int i = 0; i < 10; i++){\n"
						"\t\tk += array[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, IntegerLoopArrayInputCompoundMultiplication) {
				std::string Expected = "k *= grppi::reduce(grppi::dynamic_execution(), array, 10, 1L, [=](auto grppi_x, auto grppi_y){return grppi_x*grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tint array[10];\n"
						"\n"
						"\tfor(int i = 0; i < 10; i++){\n"
						"\t\tk *= array[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			//Container
			TEST(ReduceCheckTest, ContainerLoopVectorInputCompoundAddition) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint k = 0;\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tfor(auto i = a.begin(); i != a.end(); i++){\n"
																  "\t\tk += *i;\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, ContainerLoopVectorInputAdditionLeft) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint k = 0;\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tfor(auto i = a.begin(); i != a.end(); i++){\n"
																  "\t\tk = k + *i;\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, ContainerLoopVectorInputAdditionRight) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint k = 0;\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tfor(auto i = a.begin(); i != a.end(); i++){\n"
																  "\t\tk = *i + k;\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, ContainerLoopVectorInputCompoundAdditionFloat) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tfloat k = 0.f;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor(auto i = a.begin(); i != a.end(); i++){\n"
						"\t\tk += *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, ContainerLoopVectorInputCompoundMultiplication) {
				std::string Expected = "k *= grppi::reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 1L, [=](auto grppi_x, auto grppi_y){return grppi_x*grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor(auto i = a.begin(); i != a.end(); i++){\n"
						"\t\tk *= *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			//Range
			TEST(ReduceCheckTest, RangeLoopVectorInputCompoundAddition) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint k = 0;\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tfor(auto &elem : a){\n"
																  "\t\tk += elem;\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, RangeLoopVectorInputAdditionLeft) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint k = 0;\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tfor(auto &elem : a){\n"
																  "\t\tk = k + elem;\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, RangeLoopVectorInputAdditionRight) {
				std::string Expected = "k += grppi::reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("#include <vector>\n"
																  "\n"
																  "int main() {\n"
																  "\tint k = 0;\n"
																  "\tstd::vector<int> a(10);\n"
																  "\tfor(auto &elem : a){\n"
																  "\t\tk = elem + k;\n"
																  "\t}\n"
																  "}",
																  PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, RangeLoopVectorInputCompoundAdditionFloat) {
				std::string Expected = "";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tfloat k = 0.f;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor(auto &elem : a){\n"
						"\t\tk += k;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(ReduceCheckTest, RangeLoopVectorInputCompoundMultiplication) {
				std::string Expected = "k *= grppi::reduce(grppi::dynamic_execution(), a, 1L, [=](auto grppi_x, auto grppi_y){return grppi_x*grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor(auto &elem : a){\n"
						"\t\tk *= elem;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			/*
			 * Map Reduce
			 *
			 * */
			//Map and Reduce in same for loop
			TEST(MapReduceCheckTest, IntegerLoopVectorInputVectorOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(b), 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t\tk += a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, IntegerLoopArrayInputArrayOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), b, 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tint a[10];\n"
						"\tint b[10];\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t\tk += a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, IntegerLoopPointerInputPointerOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), b, 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tint *a = new int[10];\n"
						"\tint *b = new int[10];\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t\tk += a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, IntegerLoopVectorInputVectorOutputAdditionLeft) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(b), 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t\tk = k + a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, IntegerLoopVectorInputVectorOutputAdditionRight) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(b), 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t\tk =  a[i] + k;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, IntegerLoopVectorInputVectorOutputCompundMultiplication) {
				std::string Expected = "k *= grppi::map_reduce(grppi::dynamic_execution(), std::begin(b), 10, 1L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x*grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t\tk *=  a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, IntegerLoopMultipleMapReduce) {
				std::string Expected = "";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t\tk +=  a[i];\n"
						"\t\t\n"
						"\t\tb[i] = b[i]*2;\n"
						"\t\tk += b[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			//Container for loop
			TEST(MapReduceCheckTest, ContainerLoopVectorInputVectorOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto i = a.begin(); i != a.end();i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t\tk += *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			//TODO
			//Need to include in PathToVector std::begin for array
			/*
			TEST(MapReduceCheckTest, ContainerLoopArrayInputArrayOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, std::end(a), 0L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tint a[10];\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t\tk += *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}*/


			//Container for loop cannot iterate over a pointer
			/*
			TEST(MapReduceCheckTest, ContainerLoopPointerInputPointerOutputCompoundAddition) {
				std::string Expected = "";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}*/

			TEST(MapReduceCheckTest, ContainerLoopVectorInputVectorOutputAdditionLeft) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t\tk = k + *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, ContainerLoopVectorInputVectorOutputAdditionRight) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t\tk =  *i + k;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, ContainerLoopVectorInputVectorOutputCompundMultiplication) {
				std::string Expected = "k *= grppi::map_reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 1L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x*grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t\tk *=  *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, ContainerLoopMultipleMapReduce) {
				std::string Expected = "";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t\tk *=  *i;\n"
						"\n"
						"\t\t*i = *i * *i;\n"
						"\t\tk *=  *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			//Range for loop
			TEST(MapReduceCheckTest, RangeLoopVectorInputVectorOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto &e :a) {\n"
						"\t\te = e * 2;\n"
						"\t\tk += e;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, RangeLoopArrayInputArrayOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tint a[10];\n"
						"\tfor (auto &e : a) {\n"
						"\t\te = e * 2;\n"
						"\t\tk += e;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}



			TEST(MapReduceCheckTest, RangeLoopVectorInputVectorOutputAdditionLeft) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto &e : a) {\n"
						"\t\te = e * 2;\n"
						"\t\tk = k + e;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, RangeLoopVectorInputVectorOutputAdditionRight) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto &e : a) {\n"
						"\t\te = e * 2;\n"
						"\t\tk =  e + k;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, RangeLoopVectorInputVectorOutputCompundMultiplication) {
				std::string Expected = "k *= grppi::map_reduce(grppi::dynamic_execution(), a, 1L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x*grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto &e : a) {\n"
						"\t\te = e * 2;\n"
						"\t\tk *=  e;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, RangeLoopMultipleMapReduce) {
				std::string Expected = "";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto &e : a) {\n"
						"\t\te = e * 2;\n"
						"\t\tk +=  e;\n"
						"\t\t\n"
						"\t\te = e + e;\n"
						"\t\tk = e + k;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			//Map and Reduce in different for loops
			TEST(MapReduceCheckTest, MultipleIntegerLoopVectorInputVectorOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(b), 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t}\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\tk += a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleIntegerLoopArrayInputArrayOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), b, 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tint a[10];\n"
						"\tint b[10];\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t}\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\tk += a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleIntegerLoopPointerInputPointerOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), b, 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tint *a = new int[10];\n"
						"\tint *b = new int[10];\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t}\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\tk += a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleIntegerLoopVectorInputVectorOutputAdditionLeft) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(b), 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t}\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\tk = k + a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleIntegerLoopVectorInputVectorOutputAdditionRight) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(b), 10, 0L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t}\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\tk =  a[i] + k;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleIntegerLoopVectorInputVectorOutputCompundMultiplication) {
				std::string Expected = "k *= grppi::map_reduce(grppi::dynamic_execution(), std::begin(b), 10, 1L, [=](auto grppi_b){return  grppi_b;}, [=](auto grppi_x, auto grppi_y){return grppi_x*grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tstd::vector<int> b(10);\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\ta[i] = b[i];\n"
						"\t}\n"
						"\tfor (int i = 0; i < 10; i++) {\n"
						"\t\tk *=  a[i];\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			//Container for loop
			TEST(MapReduceCheckTest, MultipleContainerLoopVectorInputVectorOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto i = a.begin(); i != a.end();i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t}\n"
						"\tfor (auto i = a.begin(); i != a.end();i++) {\n"
						"\t\tk += *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			//TODO:
			//Need to include in PathToVector std::begin for array
			/*
			TEST(MapReduceCheckTest, ContainerLoopArrayInputArrayOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, std::end(a), 0L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tint a[10];\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t\tk += *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}*/


			//Container for loop cannot iterate over a pointer
			/*
			TEST(MapReduceCheckTest, ContainerLoopPointerInputPointerOutputCompoundAddition) {
				std::string Expected = "";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}*/

			TEST(MapReduceCheckTest, MultipleContainerLoopVectorInputVectorOutputAdditionLeft) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t}\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\tk = k + *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleContainerLoopVectorInputVectorOutputAdditionRight) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 0L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t}\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\tk =  *i + k;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleContainerLoopVectorInputVectorOutputCompundMultiplication) {
				std::string Expected = "k *= grppi::map_reduce(grppi::dynamic_execution(), std::begin(a), std::end(a), 1L, [=](auto grppi_a){return  grppi_a*2;}, [=](auto grppi_x, auto grppi_y){return grppi_x*grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\t*i = *i*2;\n"
						"\t}\n"
						"\tfor (auto i = std::begin(a); i != std::end(a);i++) {\n"
						"\t\tk *=  *i;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			//Range for loop
			TEST(MapReduceCheckTest, MultipleRangeLoopVectorInputVectorOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto &e :a) {\n"
						"\t\te = e * 2;\n"
						"\t}\n"
						"\tfor (auto &e :a) {\n"
						"\t\tk += e;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleRangeLoopArrayInputArrayOutputCompoundAddition) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";
				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tint a[10];\n"
						"\tfor (auto &e : a) {\n"
						"\t\te = e * 2;\n"
						"\t}\n"
						"\tfor (auto &e : a) {\n"
						"\t\tk += e;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}



			TEST(MapReduceCheckTest, MultipleRangeLoopVectorInputVectorOutputAdditionLeft) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto &e : a) {\n"
						"\t\te = e * 2;\n"
						"\t}\n"
						"\tfor (auto &e : a) {\n"
						"\t\tk = k + e;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleRangeLoopVectorInputVectorOutputAdditionRight) {
				std::string Expected = "k += grppi::map_reduce(grppi::dynamic_execution(), a, 0L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x+grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto &e : a) {\n"
						"\t\te = e * 2;\n"
						"\t}\n"
						"\tfor (auto &e : a) {\n"
						"\t\tk =  e + k;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapReduceCheckTest, MultipleRangeLoopVectorInputVectorOutputCompundMultiplication) {
				std::string Expected = "k *= grppi::map_reduce(grppi::dynamic_execution(), a, 1L, [=](auto grppi_a){return  grppi_a * 2;}, [=](auto grppi_x, auto grppi_y){return grppi_x*grppi_y;});";

				std::string Code = runCheckOnFile<MapReduceCheck>(
						"#include <vector>\n"
						"\n"
						"int main() {\n"
						"\tint k = 0;\n"
						"\tstd::vector<int> a(10);\n"
						"\tfor (auto &e : a) {\n"
						"\t\te = e * 2;\n"
						"\t}\n"
						"\tfor (auto &e : a) {\n"
						"\t\tk *=  e;\n"
						"\t}\n"
						"}", PathToVector);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


		} // namespace test
	} // namespace tidy
} // namespace clang


