<!--
Copyright (c) 2018, 2019 IBM Corp. and others

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
exist for non-array classes. As such, when an array class is first
encountered, it is impossible to generate a class chain validation for
the array class itself. Instead, the SVM finds the leaf component class,
generates records relating the array class to each of its (transitive)
component classes until the leaf component class is reached, and then
generates a class chain validation for the leaf component class.

Some class records (ClassByName, SystemClassByName, and ProfiledClass)
use a class chain offset in order to identify the class by name. Since
array ROM classes are not in the shared class cache, these records are
unable to name array classes directly. If an array class is found in any
of these ways, the SVM finds the leaf component class, then creates a
record for the leaf component instead of the array, followed by records
relating each (transitive) component type to its corresponding array
type, until the original array class is reached.

### Primitive Classes

Primitive Classes create a different problem for validation. As it turns
out, the ROM Classes for primitive classes aren't stored in the SCC.
This is an issue when storing Class By Name records, since the name
portion of the validation is derived from the ROM Class. In order to
deal with this problem, the Profiled Class and Class By Name Validations
are skipped entirely when the resulting class is primitive. This is
correct because Primitive Classes are guaranteed to always be the
same regardless of how they are acquired, and because the "name" (i.e.
single-character code such as I, F, etc., occurring in a signature) is
unambiguous.

### Guaranteed IDs

Because Primitive Classes (and their associated array classes) are always
guaranteed to exist, and because the Root Class and method from root class
have already been validated prior to even attempting to perfom the AOT
load, these symbols don't need to be validation records stored in 
the SCC. Therefore, the SVM, in its constructor, initializes the maps
with these IDs and the associated symbols.

### Well-known Classes

Some system classes are loaded very early and are referred to by most
JIT compilations. The SVM has a list of such classes, termed "well-known
classes." It will store a single copy of their class chain offsets in
the SCC. For each AOT-compiled method using the SVM, the emitted
relocation data needs just one offset into the SCC in order to refer to
the shared data. This not only deduplicates the offsets; it also allows
many otherwise necessary records to be eliminated.

In particular, well-known classes never need SystemClassByName,
ClassByName, or ClassFromCP records. Because these latter two have
beholders, eliminating them is a bit more difficult than eliminating
SystemClassByName. In order to make them unnecessary, the SVM checks for
every class it encounters that the class can see every well-known class,
failing otherwise. Furthermore, the well-known classes must have names
that only the bootstrap loader is allowed to define. Then any
ClassByName or ClassFromCP record naming a well-known class is
definitely redundant: its beholder can see a class with the given name,
and that class must have been defined by the bootstrap loader, so it
must be the same class whose class chain was validated at the beginning
of the AOT load.

In early compilations, it's possible that not all of the well-known
classes are available. In order to avoid spurious compilation failures,
the SVM permits some of the classes to be missing, in which case the
array of class chain offsets represents only a subset of the possible
well-known classes. The key under which it is stored specifies the
subset using (the hexadecimal expansion of) a bit-set. Any classes that
were missing at the beginning of a compilation are not considered
well-known for the purpose of that compilation. When loading, the SVM
finds the well-known classes in the particular subset used for the
method being loaded.

Each well-known class that is found is assigned the next available
sequential ID after the guaranteed IDs at the beginning of compilation,
as long as the well-known class is new (i.e. different from the root
class). When loading, the well-known classes are assigned sequential IDs
in the same way, to match the IDs assigned during compilation.

## Benefits

1. Makes explicit the provenance of every symbol acquired by the compiler
2. Enables code paths previously disabled under AOT
3. Allows relocations of classes/methods associated with cpIndex=-1
4. Facilitates the enablement of Known Object Table under AOT
5. Facilitates the enablement of JSR292 under AOT

## When to create a new Validation Record

Any time a new front end query is created, or an existing query is 
modified, the appropriate validation record should be created, or 
modified, respectively<sup>1</sup>. "Front end query" means, a query that the 
compiler (the "back end") makes of the runtime (the "front end") in order
to get information about the environment such that execution of the
compiled method will be functionally correct. The SVM uses the 
validation record to redo the query in order to perform the validation. 
Therefore, the validation record needs to contain all the information
necessary to redo the query in a different JVM instance. Each unique
front end query will have its own associated validation record; this is
a fundamental aspect of the SVM.

Note the validation is required where the query result is used in a way 
that could affect **functional correctness** - queries used only for 
heuristic purposes do not necessarily _need_ to be validated since, 
by nature, they do not affect the correctness of the program. 
All omissions on the basis of heuristic usage should be documented as 
such in the code to make the omission clearly intentional and the basis 
for that omission clear. The `enterHeuristicRegion`/`exitHeuristicRegion`
APIs are used to facilitate making frontend queries without generating
validation records.

<hr/>

1. We can have "compound" queries that simply combine other 
existing queries for convenience (e.g. `getMethodFromName`). These 
queries don't necessarily need their own validation records.
