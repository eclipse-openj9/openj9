<!--
Copyright (c) 2018, 2018 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# Exception Handling
The Compiler in OpenJ9 uses C++ Exceptions to signal an error during
the compilation of a method. However, because errors can occur even
before a compilation can occur, there are multiple nested try/catch
blocks in the compiler runtime.

The nested blocks can be visualized as follows:
```
try 
   {
   // Entry Processing

   try
      {
      // Compiler Infrastructure Initialization      

      try
         {
         // Compilation Initialization
         }

      try
         {
         // Compilation
         }

      }

   }
```

## Entry Processing
As described in the Memory Manager documentation, the Compiler uses the 
`J9::SegmentCache` to preallocate and cache memory to speed up memory 
allocation across multiple compilations. This first try/catch block is used 
to guard this initialization; it can be found in 
`TR::CompilationInfoPerThread::processEntries`. This outer most try/catch
block only exists when doing compilations on Compilation Threads; if the
compilation occurs on an Application Thread, then the `J9::SegmentCache` is 
not created.

The catch block here only expects to catch `std::bad_alloc`.

## Compilation Infrastructure Initialization
When the compiler decides to compile a method, the
`TR::CompilationInfoPerThreadBase::compile` (with three arguments) is invoked.
This method is used to initialize the scratch memory infrastructure (which
provides Compilation Threads with memory needed during the compilation) as 
well as other compilation infrastructure related initialization. Since an
exception can be thrown during this initialization, and because between the
beginning of Entry Processing and Compilation Infrastructure Initialization
there are actions performed (such as acquiring VM Access) that need to be 
undone on an error condition, a (nested) try/catch block is added.

The catch block exists primarily to catch `std::bad_alloc`. However, it also
catches `J9::JITShutdown` and `std::exception` in case in the future there is
an unguarded call to `acquireVMAccessIfNeeded` which can throw.

## Compilation Initialization
After the Compilation Infrastructure is initialized, `wrappedCompile` is
invoked; this method is a static method that is protected by `j9sig_protect`
and is used to wrap the compilation with a JVM defined signal handler.
`wrappedCompile` first initializes the compilation before actually starting
the compilation. As there can be exceptions thrown during initialization,
the code is guarded with a try/catch.

The catch block expects to catch `J9::JITShutdown`, `std::bad_alloc`, or
`std::exception`.

## Compilation
If everything is successful until this point, `wrappedCompile` then invokes
`TR::CompilationInfoPerThreadBase::compile` (with six arguments). The 
try/catch found in here is the one that guards the actual compilation, and
is used to catch errors found during the compilation of a method.

The catch block excepts to catch everything. It then re-throws and re-catches
the exception in `TR::CompilationInfoPerThreadBase::processException` to 
determine the type of exception throw, and what action needs to be taken
accordingly.

