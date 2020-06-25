<!--
Copyright (c) 2020, 2020 IBM Corp. and others

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

# Overview

Ahead Of Time (AOT) compilation is the act of compiling a 
programming language, or some intermediate representation such as 
bytecode, into native code so that input language can be natively
executed<sup>1</sup>. This is different from Just In Time (JIT) 
compilation, where a JIT compiler, usually a component of some 
Virtual Machine (VM), will produce native code from some input source 
code while the VM is interpreting the same input. Applying AOT to 
interpreted languages such as Java can be done using a static approach; 
the AOT compiler will generate code given some input set of classes. 
Therefore, this kind of AOT can be called Static AOT; OpenJ9, however, 
uses Dynamic AOT.

Dynamic AOT is essentially a hybrid between Static AOT and JIT. The 
compiler does not generate native code prior to executing Java code but 
rather _while_ the VM interprets Java code. Thus, the process of 
performing an AOT compilation is identical to a JIT compilation. 
However, when performing an AOT compilation, the compiler will also 
generate validation and relocation information. The code and data is 
then stored into the Shared Classes Cache (SCC). In a different JVM
instance, the code is also loaded on demand; when the time comes to
perform a JIT compilation, the compiler checks whether code for the 
method already exists in the SCC and loads that instead.

# Topics

1. [Feature Documentation](https://www.eclipse.org/openj9/docs/aot/)
2. [Introduction to AOT](https://blog.openj9.org/2018/10/10/intro-to-ahead-of-time-compilation/)
3. [AOT: Relocation](https://blog.openj9.org/2018/10/26/ahead-of-time-compilation-relocation/)
4. [AOT: Validation](https://blog.openj9.org/2018/11/08/ahead-of-time-compilation-validation/)
5. [Class Chains](https://github.com/eclipse/openj9/blob/master/doc/compiler/aot/ClassChains.md)
6. [Symbol Validation Manager](https://github.com/eclipse/openj9/blob/master/doc/compiler/aot/SymbolValidationManager.md)


<hr/>

1. https://en.wikipedia.org/wiki/Ahead-of-time_compilation 
