# The LLVM Compiler Infrastructure-clang tidy check


This directory and its sub-directories contain source code for LLVM,
a toolkit for the construction of highly optimized compilers,
optimizers, and run-time environments. Added to it is the source code for a mapreduce check.

The README briefly describes how to get started with building clang-tidy.

## Building clang-tidy

### Getting the Source Code and Building LLVM

The LLVM Getting Started documentation may be out of date.  The [Clang
Getting Started](http://clang.llvm.org/get_started.html) page might have more
accurate information.

This is an example work-flow and configuration to get and build the LLVM source:

1. Get the modified LLVM source code:

     * ``git clone https://github.com/danielmartinezdavies/llvm-project-clang-tidy-check.git``
`
2. Configure and build LLVM and Clang-tidy:

     * ``cd llvm-project``

     * ``mkdir build``

     * ``cd build``

     * ``cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;" ../llvm``
     * ``make install-clang-tidy -j8``

