# The LLVM Compiler Infrastructure-clang tidy check

This directory and its sub-directories contain source code for LLVM,
a toolkit for the construction of highly optimized compilers,
optimizers, and run-time environments. Added to it is the source code for a mapreduce check.

The README briefly describes how to get started with building clang-tidy.

This project is still being worked on and so will have some bugs.

## Clang-tidy check
The clang-tidy check created is called MapReduce, and will look for loops that can be parallelized. Once found, the check will suggest transforming the source code to its parallel equivalent, using GrPPI(Generic Reusable Parallel Pattern Interface).


More information on GrPPI can be found here:
* https://github.com/arcosuc3m/grppi


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
