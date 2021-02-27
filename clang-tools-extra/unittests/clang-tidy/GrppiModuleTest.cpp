#include "ClangTidyTest.h"
#include "readability/NamespaceCommentCheck.h"
#include "grppi/MapReduceCheck.h"
#include "gtest/gtest.h"

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
	remove(s, '\n');
	remove(s, '\t');
	remove(s2, '\n');
	remove(s2, '\t');
}

static const std::string getVectorString();

std::string InsertLoopIntoDeclarations(const std::string &loop) {
	return getVectorString() +
		   "void f(){\n"
		   "	int array[10];\n"
		   "	int *pointer = new int[10];\n"
		   "	std::vector<int> a(10);\n"
		   "	std::vector<int> b(10);\n"
		   + loop
		   + "}\n";
}


namespace clang {
	namespace tidy {
		namespace test {

			TEST(MapCheckTest, IntegerLoopArrayOutput) {
				const std::string Input = InsertLoopIntoDeclarations("	for(int i = 0; i < 10; i++){\n"
																	 "		array[i] = 0;\n"
																	 "	}\n");

				std::string Expected = InsertLoopIntoDeclarations(
						"	grppi::map(grppi::dynamic_execution(), (std::vector<int>::iterator) array, 10, array, [=](auto grppi_array){return  0;});\n");

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopPointerOutput) {
				const std::string Input = InsertLoopIntoDeclarations("	for(int i = 0; i < 10; i++){\n"
																	 "		pointer[i] = 0;\n"
																	 "	}\n");

				std::string Expected = InsertLoopIntoDeclarations(
						"	grppi::map(grppi::dynamic_execution(), (std::vector<int>::iterator) pointer, 10, pointer, [=](auto grppi_pointer){return  0;});\n");

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


			TEST(MapCheckTest, IntegerLoopContainerOutput) {
				const std::string Input = InsertLoopIntoDeclarations("	for(int i = 0; i < 10; i++){\n"
																	 "		a[i] = 0;\n"
																	 "	}\n");
				std::string Expected = InsertLoopIntoDeclarations(
						"	grppi::map(grppi::dynamic_execution(), std::begin(a), 10, std::begin(a), [=](auto grppi_a){return  0;});\n");

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputContainerInput) {
				const std::string Input = InsertLoopIntoDeclarations("	for(int i = 0; i < 10; i++){\n"
																	 "		a[i] = b[i];\n"
																	 "	}\n");
				std::string Expected =
						InsertLoopIntoDeclarations(
								"	grppi::map(grppi::dynamic_execution(), std::begin(b), 10, std::begin(a), [=](auto grppi_b){return  grppi_b;});\n");

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputMultipleInput) {
				const std::string Input = InsertLoopIntoDeclarations("	for(int i = 0; i < 10; i++){\n"
																	 "		a[i] = b[i] + pointer[i] + array[i];\n"
																	 "	}\n");
				std::string Expected =
						InsertLoopIntoDeclarations(
								"	grppi::map(grppi::dynamic_execution(), std::make_tuple( std::begin(b), "
		"(std::vector<int>::iterator) pointer, (std::vector<int>::iterator) array), 10, std::begin(a), "
  "[=](auto grppi_b, auto grppi_pointer, auto grppi_array){return  grppi_b + grppi_pointer + grppi_array;});\n");

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputInputModifiedStartAndEnd) {
				const std::string Input = InsertLoopIntoDeclarations("	for(int i = 5; i < 8; i++){\n"
																	 "		a[i] = b[i] + pointer[i] + array[i];\n"
																	 "	}\n");
				std::string Expected =
						InsertLoopIntoDeclarations(
								"	grppi::map(grppi::dynamic_execution(), std::make_tuple( std::begin(b) + 5,"
								" (std::vector<int>::iterator) pointer + 5, (std::vector<int>::iterator) array + 5), 3, std::begin(a) + 5, "
								"[=](auto grppi_b, auto grppi_pointer, auto grppi_array){return  grppi_b + grppi_pointer + grppi_array;});\n");
			}
			TEST(MapCheckTest, ContainerLoopNoInputBegin) {
					const std::string Input = InsertLoopIntoDeclarations("	for(auto i = a.begin(); i != a.end(); i++){\n"
																		 "		*i = 0;\n"
																		 "	}\n");
					std::string Expected =
							InsertLoopIntoDeclarations(
									"	grppi::map(grppi::dynamic_execution(), std::begin(a), std::end(a), std::begin(a), [=](auto grppi_a){return  0;});\n");

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			/*TEST(MapCheckTest, ContainerLoopNoInputSTDBegin) {
				const std::string Input = InsertLoopIntoDeclarations("	for(auto i = std::begin(a); i != std::end(a); i++){\n"
																	 "			*i = 0;\n"
																	 "	}\n");
				std::string Expected =
						InsertLoopIntoDeclarations(
								"	grppi::map(grppi::dynamic_execution(), std::begin(a), std::end(a), std::begin(a), [=](auto grppi_a){return  0;});\n");

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}*/

			TEST(MapCheckTest, RangeLoopArrayOutput) {
				const std::string Input = InsertLoopIntoDeclarations("	for(auto &elem: array){\n"
																	 "			elem = 0;\n"
																	 "	}\n");
				std::string Expected =
						InsertLoopIntoDeclarations(
								"	grppi::map(grppi::dynamic_execution(), array, array, [=](auto grppi_array){return  0;});\n");

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}

			TEST(MapCheckTest, RangeLoopContainerOutput) {
				const std::string Input = InsertLoopIntoDeclarations("	for(auto &elem: a){\n"
																	 "			elem = 0;\n"
																	 "	}\n");
				std::string Expected =
						InsertLoopIntoDeclarations(
								"	grppi::map(grppi::dynamic_execution(), a, a, [=](auto grppi_a){return  0;});\n");

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code, Expected);
				EXPECT_EQ(Code, Expected);
			}


		} // namespace test
	} // namespace tidy
} // namespace clang

//Vector API from std
static const std::string getVectorString() {
	return "namespace std{\n"
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
		   "    vector<T> & operator=(const vector<T> &);\n"
		   "    void clear();\n"
		   "private:\n"
		   "    unsigned int my_size;\n"
		   "    unsigned int my_capacity;\n"
		   "    T * buffer;\n"
		   "};\n"
		   "\n"
		   "// Your code goes here ...\n"
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
		   "}\n"
		   "} //end std\n";
}