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

# Symbol Validation Manager

In order for the OpenJ9 Compiler to compile Ahead Of Time (AOT),
or more accurately, Relocatable, code, it needs to be able to store
information for the AOT load infrastructure to validate all the 
assumptions made during the compile run, since the AOT load is 
performed in a different JVM environment than the one the compilation
was performed in. For these reasons, the Compiler and the Shared Class
Cache (SCC) has infrastructure to facilate these validations.

Without the Symbol Validation Manager, the validations involves either
creating a relocation record to validate an Arbitrary Class, a Class
from a Constant Pool, an Instance Field, or a Static Field. However,
when getting a Class By Name, the validation stored would be an 
Arbitrary Class Validation, which is technically incorrect when the
Name is retrieved from the signature of a method (since the answer
depends on what the Class of that method "sees"). In order to deal
with this, the compiler needs to know where the "beholder" Class comes
from. The Symbol Validation Manager fixes this problem by making the
answer to the question "where does this _whatever_ come from?" explicit
and unambigious.

## How it works

The Symbol Validation Manager (SVM) is very much similar to a symbol table
that a linker might use. The idea is to store information about where
a particular symbol (eg `J9Class`, `J9Method`, etc.) came from. For example,
if the compiler got the `J9Class` of `java/lang/String` by name from the
parameter list of method `C.foo(Ljava/lang/String)V`, then the SVM will
store a record stating that the compiler got `J9Class` `java/lang/String`
By Name as seen by `J9Class` `C`. Then in the load run, the load
infrastructure can validate these answers the compiler got by redoing the
same queries *in the same order as during the AOT compilation*. In this
manner, if the AOT load succeeds validations, the compiled body is 100%
guaranteed to be compatible in the current environment.

However, since `J9Class`es are artifacts of the current environment, how
does the SVM enable a new environment to validate it? The core idea is to
associate unique IDs to each new symbol the compiler gets a hold of. Thus,
when a symbol is acquired by asking a query using a different symbol, the
validation record that gets generated refers to these two symbols by their
IDs, which are environment agnostic. In the load run, the AOT load
infrastructure uses these IDs to build up its table of symbols that are
valid in _its_ environment.

To give a concrete example:

The compiler decides to compile `A.foo()`. As part of the compilation, it
gets class `B` from `A`'s constant pool, gets class `C` from `A`'s 
constant pool, and finally gets `B` by name as seen by `C`. Thus, the 
validation records that get generated would be:

1. Root Class Record for `A`; `A` gets ID 1
2. Method From Class Record for `foo`; `foo` gets ID 2
3. Class From CP of `A` for `B`; `B` gets ID 3
4. Class From CP of `A` for `C`; `C` gets ID 4
5. Class By Name as seen by `C`

When writing out the records, the information would be written out in the
following manner:

1. Root Class Record: ID=1
2. Method From Class Record: ID=2, classID=1, index=x
3. Class From CP Record: ID=3, beholderID=1, cpIndex=y
4. Class From CP Record: ID=4, beholderID=1, cpIndex=z
5. Class By Name Record: ID=3, beholder=4

Thus the information written in the validation records are entirely
environment agnostic. In the load run, the AOT infrastructure will
go through these validations and build up the symbols that are valid
in _its_ environment:

1. Map ID=1 to the class of the method being compiled
2. Get the method at index=x from the class associated with ID=1 and
map it to ID=2
3. Get the class from cpIndex=y from the class associated with ID=1
and map it to ID=3
4. Get the class from cpIndex=z from the class associated with ID=1
and map it to ID=4
5. Get the class by name as seen by the class associated with ID=4
and verify that it is the same class associated with ID=3

If any of the validations fail, the load is aborted. Thus the general 
approach in the load run is:

1. Find the symbol as specified by the validation record
2. Map the ID from the validation record to the symbol
3. If the map already contains a symbol for that ID, validate that
the symbol in the map is the same as the symbol currently being
validated; fail otherwise


## Subtleties

### Class Chains

Class Chains are a core component of AOT validation; they ensure that
the shape of a class is the same as that during the compile run. For
technical details about Class Chains, see 
[AOT Class Chains](https://github.com/eclipse/openj9/blob/master/doc/compiler/aot/AOTClassChains.md).
Thus, for every validation record involving classes, a Class Chain
Validation Record is also created (unless a record for that class
already exists).

### Array Classes

Array Classes create a problem for validation. Class Chains can only
exist for non-array classes. However, if the compiler acquired an 
array class by name or via profiling, in order to use it during an AOT
compilation, it still needs to ensure that the shape of that class will
be valid in the load run. Therefore, for every validation record 
involving classes, what actually happens is the SVM will get the
leaf component class, store the record as if the compiler acquired the
leaf component class from the query, and then store as many 
Array From Component Class Validation Records as there are array
dimensions. Therefore, on the load run, when performing a validation, 
the SVM also has ensure to get the leaf component class in order for
the validation to be consistent with the compile run. The actual array
class gets validated via the Array From Component Class records.

### Primitive Classes

Primitive Classes create a different problem for validation. As it turns
out, the ROM Classes for primitive classes aren't stored in the SCC.
This is an issue when storing Class By Name records, since the name
portion of the validation is derived from the ROM Class. In order to
deal with this problem, the Profiled Class and Class By Name 
Validations also store a char value representing the primitive type;
if the char value is '\0', then the class being validated isn't
primitive and the rest of the validation process continues. If the
char value is not '\0', then class is primitive, and the Profiled 
Class or Class By Name record returns validaion succeeded, which is
correct because Primitive Classes are guaranteed to always be the
same regardless of how they are acquired.

### Guaranteed IDs

Because Primitive Classes (and their associated array classes) are always
guaranteed to exist, and because the Root Class and method from root class
have already been validated prior to even attempting to perfom the AOT
load, these symbols don't need to be validation records stored in 
the SCC. Therefore, the SVM, in its constructor, initializes the maps
with these IDs and the associated symbols.

## Benefits

1. Makes explicit the provenance of every symbol acquired by the compiler
2. Enables code paths previously disabled under AOT
3. Allows relocations of classes/methods associated with cpIndex=-1
4. Facilitates the enablement of Known Object Table under AOT
5. Facilitates the enablement of JSR292 under AOT
