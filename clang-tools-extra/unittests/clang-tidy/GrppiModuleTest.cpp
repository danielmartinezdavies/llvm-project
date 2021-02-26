#include "ClangTidyTest.h"
#include "readability/NamespaceCommentCheck.h"
#include "grppi/MapReduceCheck.h"
#include "grppi/TestCheck.h"
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
static std::string getVectorString();

namespace clang {
	namespace tidy {
		namespace test {

			TEST(MapCheckTest, IntegerLoopArrayOutput) {
				const std::string Input =
						"void f(){\n"
						"	int p[10];\n"
						"	for(int i = 0; i < 10; i++){\n"
						"		p[i] = 0;\n"
						"	}\n"
						"}\n";

				std::string Expected =
						"void f(){\n"
						"	int p[10];\n"
						"	grppi::map(grppi::dynamic_execution(), (std::vector<int>::iterator) p, 10, p, [=](auto grppi_p){return  0;});\n"
						"}\n";

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code);
				removeSpaces(Expected);
				EXPECT_EQ(
						Code,
						Expected);
			}

			TEST(MapCheckTest, IntegerLoopPointerOutput) {
				const std::string Input =
						"void f(){\n"
						"	int * p2 = new int[10];\n"
						"	for(int i = 0; i < 10; i++){\n"
						"		p2[i] = 0;\n"
						"	}\n"
						"}\n";


				std::string Expected =
						"void f(){\n"
						"	int * p2 = new int[10];\n"
						"	grppi::map(grppi::dynamic_execution(), (std::vector<int>::iterator) p2, 10, p2, [=](auto grppi_p2){return  0;});\n"
						"}\n";
				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code);
				removeSpaces(Expected);
				EXPECT_EQ(
						Code,
						Expected);
			}


			TEST(MapCheckTest, IntegerLoopContainerOutput) {
				const std::string Input =
						getVectorString() +
						"void f(){\n"
						"	Vector<int> a(10);\n"
						"	for(int i = 0; i < 10; i++){\n"
						"		a[i] = 0;\n"
						"	}\n;"
						"}\n";


				std::string Expected =
						getVectorString() +
						"void f(){\n"
						"	Vector<int> a(10);\n"
						"	grppi::map(grppi::dynamic_execution(), std::begin(a), 10, std::begin(a), [=](auto grppi_a){return  0;});\n;"
						"}\n";

				std::string Code = runCheckOnCode<MapReduceCheck>(Input);
				removeSpaces(Code);
				removeSpaces(Expected);
				EXPECT_EQ(
						Code,
						Expected);
			}

			TEST(MapCheckTest, IntegerLoopContainerOutputContainerInput) {
				const std::string Input =
						getVectorString() +
						"void f(){\n"
						"	Vector<int> a(10);\n"
	  					"	Vector<int> b(10);\n"
						"	for(int i = 0; i < 10; i++){\n"
						"		a[i] = b[i];\n"
						"	}\n;"
						"}\n";


				std::string Expected =
						getVectorString() +
						"void f(){\n"
						"	Vector<int> a(10);\n"
						"	Vector<int> b(10);\n"
						"	grppi::map(grppi::dynamic_execution(), std::begin(b), 10, std::begin(a), [=](auto grppi_b){return  grppi_b;});\n;"
						"}\n";

				std::string Code = runCheckOnCode<MapReduceCheck>(Input, nullptr, "input.cc",None, ClangTidyOptions(), {{"vector", "\n"}});
				removeSpaces(Code);
				removeSpaces(Expected);
				EXPECT_EQ(
						Code,
						Expected);
			}


		} // namespace test
	} // namespace tidy
} // namespace clang

static std::string getVectorString(){ return "template <class T>\n"
								  "class  Vector\n"
								  "{\n"
								  "public:\n"
								  "\n"
								  "    typedef T * iterator;\n"
								  "\n"
								  "    Vector();\n"
								  "    Vector(unsigned int size);\n"
								  "    Vector(unsigned int size, const T & initial);\n"
								  "    Vector(const Vector<T> & v);      \n"
								  "    ~Vector();\n"
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
								  "    Vector<T> & operator=(const Vector<T> &);\n"
								  "    void clear();\n"
								  "private:\n"
								  "    unsigned int my_size;\n"
								  "    unsigned int my_capacity;\n"
								  "    T * buffer;\n"
								  "};\n"
								  "\n"
								  "// Your code goes here ...\n"
								  "template<class T>\n"
								  "Vector<T>::Vector()\n"
								  "{\n"
								  "    my_capacity = 0;\n"
								  "    my_size = 0;\n"
								  "    buffer = 0;\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "Vector<T>::Vector(const Vector<T> & v)\n"
								  "{\n"
								  "    my_size = v.my_size;\n"
								  "    my_capacity = v.my_capacity;\n"
								  "    buffer = new T[my_size];  \n"
								  "    for (unsigned int i = 0; i < my_size; i++)\n"
								  "        buffer[i] = v.buffer[i];  \n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "Vector<T>::Vector(unsigned int size)\n"
								  "{\n"
								  "    my_capacity = size;\n"
								  "    my_size = size;\n"
								  "    buffer = new T[size];\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "Vector<T>::Vector(unsigned int size, const T & initial)\n"
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
								  "Vector<T> & Vector<T>::operator = (const Vector<T> & v)\n"
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
								  "typename Vector<T>::iterator Vector<T>::begin()\n"
								  "{\n"
								  "    return buffer;\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "typename Vector<T>::iterator Vector<T>::end()\n"
								  "{\n"
								  "    return buffer + size();\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "T& Vector<T>::front()\n"
								  "{\n"
								  "    return buffer[0];\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "T& Vector<T>::back()\n"
								  "{\n"
								  "    return buffer[my_size - 1];\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "void Vector<T>::push_back(const T & v)\n"
								  "{\n"
								  "    if (my_size >= my_capacity)\n"
								  "        reserve(my_capacity +5);\n"
								  "    buffer [my_size++] = v;\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "void Vector<T>::pop_back()\n"
								  "{\n"
								  "    my_size--;\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "void Vector<T>::reserve(unsigned int capacity)\n"
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
								  "unsigned int Vector<T>::size()const//\n"
								  "{\n"
								  "    return my_size;\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "void Vector<T>::resize(unsigned int size)\n"
								  "{\n"
								  "    reserve(size);\n"
								  "    my_size = size;\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "T& Vector<T>::operator[](unsigned int index)\n"
								  "{\n"
								  "    return buffer[index];\n"
								  "}  \n"
								  "\n"
								  "template<class T>\n"
								  "unsigned int Vector<T>::capacity()const\n"
								  "{\n"
								  "    return my_capacity;\n"
								  "}\n"
								  "\n"
								  "template<class T>\n"
								  "Vector<T>::~Vector()\n"
								  "{\n"
								  "    delete[ ] buffer;\n"
								  "}\n"
								  "template <class T>\n"
								  "void Vector<T>::clear()\n"
								  "{\n"
								  "    my_capacity = 0;\n"
								  "    my_size = 0;\n"
								  "    buffer = 0;\n"
								  "} ";}