# The LLVM Compiler Infrastructure-clang tidy check

This directory and its sub-directories contain source code for LLVM,
a toolkit for the construction of highly optimized compilers,
optimizers, and run-time environments. Added to it is the source code 
for a mapreduce check in the grppi module.

The README briefly describes how to get started with building this forked
clang-tidy project.

This project is meant for educational purposes.

## Clang-tidy check
The fork adds a new clang-tidy check called MapReduce, which looks
for loops that can be parallelized. Once found, the check will suggest
transforming the source code to its parallel equivalent, 
using GrPPI(Generic Reusable Parallel Pattern Interface).

More information on GrPPI can be found here:
* https://github.com/arcosuc3m/grppi

For code analysis, a combination of Clang matchers and the RecursiveAST Visitor 
pattern is used. Matchers find candidate loops while the Visitor allows for in-depth
exploration.

The files of interest are located in clang-tools-extra/clang-tidy/grppi 
and clang-tools-extra/unittests/clang-tidy. 
### Types of loops identified
Three types of loops will be explored for possible parallelism:
#### IntegerForLoop
      for(int i = 0; i < a.size(); i++){
         a[i] = a[i]*2;
      }
#### ContainerForLoop
      for(auto i = a. begin(); i != a.end(); i++){
         *i = *i *2;
      }
#### RangeForLoop
      for(auto &elem : a){
         elem = elem *2;
      }
### Parallelism Patterns
#### Map
The following map pattern is identified:

      int array [10];
      for(int i = 0; i < 10; i++){
         array [i] = 0;
      }
And following grppi code will be suggested by the MapReduce check:

      grppi :: map( grppi :: dynamic_execution () , array , 10, array , 
      [=](auto grppi_array ){ return 0;});
#### Reduce
Original:

      # include <vector>
      int main () {
         int k = 0;
         std :: vector <int > a(10);
         for(auto i = a. begin (); i != a.end (); i++){
            k += *i;
         }
      }

Suggested:

      k += grppi :: reduce ( 
      grppi :: dynamic_execution () , std :: begin (a), std :: end(a), 0L, 
      [=]( auto grppi_x , auto grppi_y ){ return grppi_x + grppi_y ;});
#### MapReduce
Original:

      # include <vector>
      int main () {
         int k = 0;
         std :: vector <int > a(10);
         for (auto &e :a) {
            e = e * 2;
            k += e;
         }
      }
Suggested:

      k += grppi :: map_reduce ( grppi :: dynamic_execution () , a, 0L, 
      [=](auto grppi_a){ return grppi_a * 2;} , 
      [=](auto grppi_x , auto grppi_y){ return grppi_x + grppi_y ;});

### Demo
[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/eC1XCjiIqQg/maxresdefault.jpg)](https://youtu.be/eC1XCjiIqQg)


### Getting the Source Code and Building LLVM

The LLVM Getting Started documentation may be out of date.  The [Clang
Getting Started](http://clang.llvm.org/get_started.html) page might have more
accurate information.

This is an example work-flow and configuration to get and build the LLVM source:

1. Get the modified LLVM source code:

     * ``git clone https://github.com/danielmartinezdavies/llvm-project-clang-tidy-check.git``
     
2. Configure and build LLVM and Clang-tidy:

     * ``cd llvm-project-clang-tidy-check``

     * ``mkdir build``

     * ``cd build``

     * ``cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;" ../llvm``
     * ``make clang-tidy -j8``
     
 Alternatively: 
 * ``make install-clang-tidy -j8``

### Testing
Tests for the Map, Reduce and Map Reduce patterns where added to the 
ClangTidyTests Google Test target. Three suites were added: MapCheckTest, ReduceCheckTest
and MapReduceCheckTest.

