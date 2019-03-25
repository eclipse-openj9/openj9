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

# AOT Class Chains
AOT Class Chains are a means to allow the AOT infrastructure to determine
if a class in the current JVM instance is the same as a class in a 
previous JVM instance. This is needed to ensure that a method that gets 
AOT compiled in one JVM instance can be AOT loaded in a different JVM 
instance.

## Background
In OpenJ9, there exist `J9Class`es and `J9ROMClass`es; the former are 
known as "ramClass"es and the latter are known as "romClass"es. The 
ramclass is a hierarchy of romclasses. The romclass corresponds roughly 
to the contents of a `.class` file; it is not a complete class because 
it only references its superclass and implemented interfaces by name.

The Shared Class Cache (SCC) only stores romclasses. However, when 
performing an AOT load, the AOT infrastructure needs to ensure that a 
relevant `J9Class` in one instance of a JVM is the same `J9Class` from 
a different JVM instance. This is because the `J9Class` provides a Java 
Class' entire hierarchy, starting from itself going up to 
`java/lang/Object`. The way OpenJ9 validates two ramclasses from
different JVM instances is by using Class Chains.

## Class Chains
Consider the following:

```
class B extends A {
    ...
    void foo() {
        ....
    }
}

class C extends B {
    ...
    void foo() {
        D d = ...;
        d.bar();
    }
}
```

In one JVM instance, the classes `A'`, `B'`, and `C'` might be loaded, 
where `C'` extends `B'` extends `A'`. In another JVM instance, the 
classes `A''`, `B''`, and `C''` might be loaded, where `C''` extends 
`B''` extends `A''`. The SCC can provide information about whether, 
`romclass(C'') == romclass(C')`, `romclass(B'') == romclass(B')`, 
and `romclass(A'') == romclass(A')`. Ignorning interfaces for the moment, 
if all of these relationships are true, then `J9Class(C'') == J9Class(C')`. 
This means that objects of `C''` or `C'` have the same shape, and any 
relationships known between `J9Class(C')` and `J9Class(B')` are also 
true between `J9Class(C'')` and `J9Class(B'')`.

Class Chains are this set of romclass relationships, and they are stored 
in the SCC. They are a list of romclasses; one first walks up the super 
class chain up to `java/lang/Object`, and then walks the class' iTable 
to get the full list of interface classes that are implemented. During 
a compilation, the JIT creates the list in the order described above 
and stores it into the SCC. Each romclass is encoded as an offset from 
the beginning of the SCC. The offsets of various class chains in the SCC 
are referenced in the various Relocation Records, which describe the list 
of `J9Class`es an AOT compilation has asked questions about. 

When AOT code, generated in one JVM instance, is loaded into a different 
JVM instance, the AOT infrastructure processes the Relocation Records; 
it finds a candidate `J9Class` pointer in the current JVM and walks its 
superclasses and interfaces, checking each one's romclass against the one 
in the Class Chain stored in the SCC. Any mismatch implies the classes 
might be different, in which case the AOT load fails. In the case of 
inlined methods however, the AOT load doesn't necessarily fail, rather, 
the AOT infrastructure disables a particular inlined call site.

## Finding a candidate J9Class
For classes that are directly referenced in a method's code, there is a 
Constant Pool entry in the romclass. For example, in `C.foo()` there is a
call to `D.bar()`; if `D.bar()` is a static call, there will be an entry
in `romclass(C)`'s constant pool containing a reference to `romclass(D)`.
The VM can directly be asked what `J9Class` that constant pool entry 
would resolve to. If that class has already been loaded and is visible 
to class `C`, the VM will return the `J9Class` for class `D` directly, 
and this can be validated by a constant pool index. Strictly speaking, 
this does not require checking the class chains, but in AOT w/ SVM, this
is done anyway for consistency.

On the other hand, finding a candidate class that was, during compilation,
acquired from profiling, is non-trivial. In the example above, `d`, 
a reference of type `D`, may be a reference to an object of type `D`, or 
it may be a reference to some subclass of `D` (eg. `E` extends `D`) that 
isn't directly referenced by `C`. In this case, due to information from 
the IProfiler, it might be known that `E` is the primary class of the `D` 
that's returned. In that case, the JIT will insert a Profiled Inlined 
Guard that ensures that `d.class == E` before executing the inlined code. 
 
Because Java uses Class Loaders to load classes, two class loaders can 
load the same "C extends B extends A" class twice, and this will result 
in two different `J9Class` pointers; the difference comes from the fact 
the two class loaders are different. Frameworks such as OSGI use different 
class loaders for different modules, and Java applications can use several 
OSGI modules. Even if there weren't multiple instances of a particular 
class in the current JVM instance, having to go through every possible 
class loader to check if it already has that particular class loaded would 
be extremely slow. If there were multiple instances, then the AOT 
infrastructure would additionally need to validate every single one. All 
this would effectively defeat the purpose of AOT code, which is to 
facilitate fast application startup.

Finding a particular `J9Class` in the current JVM instance requires 
a Class Loader and a Class Name. The Class Name can be obtained from 
the top romclass in the class chain. However, finding the Class Loader 
is not as simple since they are just Java Objects created by a Java 
application. OpenJ9 solves this problem by building hash tables that 
map a) Class Loaders to the first Class that loader loaded, and b) the 
first class that loader loaded to the Class Loader Object. Regarding the
"first class that loader loaded", what actually gets stored is an offset 
to the class chain for that class. These tables are not persisted; they 
are built up via the Class Load Hook in the JIT in the current JVM 
instance. When referencing a particular `J9Class` in a Relocation Record, 
the JIT stores 1) the offset to the class chain for the current `J9Class` 
itself, as well as 2) the offset to the class chain for the first class
loaded by the loader that loaded the current `J9Class`.

During an AOT load, the AOT infrastructure takes 1) and adds to the 
current address of the SCC in the current JVM instance to find the class 
chain to be validated. This class chain provides the name of the class. 
The AOT infrastructure then uses 2) to get the Class Loader Object in 
the current JVM instance, which will likely give the correct Class Loader. 
It then asks the Class Loader to provide the `J9Class` pointer 
corresponding to the Class Name. It is worth noting that this `J9Class`
may not yet be loaded. The validation is then performed using this `J9Class`
pointer, if it exists.
