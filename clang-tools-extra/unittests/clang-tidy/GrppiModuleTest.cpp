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

			TEST(MapCheckTest, IntegerLoopArrayOutput) {
				std::string Expected = "grppi::map(grppi::dynamic_execution(), array, 10, array, [=](auto grppi_array){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("MapIntegerLoopArrayOutput.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopPointerOutput) {
				std::string Expected ="grppi::map(grppi::dynamic_execution(), pointer, 10, pointer, [=](auto grppi_pointer){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("MapIntegerLoopPointerOutput.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			TEST(MapCheckTest, IntegerLoopContainerOutput) {
				std::string Expected = "grppi::map(grppi::dynamic_execution(), std::begin(a), 10, std::begin(a), [=](auto grppi_a){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("MapIntegerLoopContainerOutput.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputContainerInput) {

				std::string Expected = "grppi::map(grppi::dynamic_execution(), std::begin(b), 10, std::begin(a), [=](auto grppi_b){return  grppi_b;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("MapIntegerLoopContainerOutputContainerInput.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputMultipleInput) {

				std::string Expected =
								"	grppi::map(grppi::dynamic_execution(), std::make_tuple( std::begin(b), pointer, array), 10, std::begin(a), "
								"[=](auto grppi_b, auto grppi_pointer, auto grppi_array){return  grppi_b + grppi_pointer + grppi_array;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("MapIntegerLoopContainerOutputMultipleInput.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputInputModifiedStartAndEnd) {

				std::string Expected =
								"grppi::map(grppi::dynamic_execution(), std::make_tuple( std::begin(b) + 5, pointer + 5, array + 5),"
								" 3, std::begin(a) + 5, [=](auto grppi_b, auto grppi_pointer, auto grppi_array){"
								"return  grppi_b + grppi_pointer + grppi_array;});\n";
				std::string Code = runCheckOnFile<MapReduceCheck>("MapIntegerLoopContainerOutputInputModifiedStartAndEnd.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, ContainerLoopNoInputBegin) {

				std::string Expected ="grppi::map(grppi::dynamic_execution(), std::begin(a), std::end(a), std::begin(a), [=](auto grppi_a){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("MapContainerLoopNoInputBegin.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, ContainerLoopNoInputSTDBegin) {

				std::string Expected ="grppi::map(grppi::dynamic_execution(), std::begin(a), std::end(a), std::begin(a), [=](auto grppi_a){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("MapContainerLoopNoInputSTDBegin.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, RangeLoopArrayOutput) {

				std::string Expected = "grppi::map(grppi::dynamic_execution(), array, array, [=](auto grppi_array){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("MapRangeLoopArrayOutput.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, RangeLoopContainerOutput) {

				std::string Expected ="	grppi::map(grppi::dynamic_execution(), a, a, [=](auto grppi_a){return  0;});\n";

				std::string Code = runCheckOnFile<MapReduceCheck>("MapRangeLoopContainerOutput.cpp", test_path);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}



		} // namespace test
	} // namespace tidy
} // namespace clang


