<!--
Copyright (c) 2020, 2021 IBM Corp. and others

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

AOT compilations support inlining of methods.
However, ensuring validation and relocation is a non-trivial process with
many subtleties. The relocation records related to the inlined methods are 
different from the other records in that they do many things - they perform 
validation, relocation of the inlined table entry in the J9JITExceptionTable
metadata structure, as well as relocation of guards. 

This document will explain how an inlined site is validated, as well what 
what all is necessary as part of the relocation. Note, inlined method 
relocations still follow the high level process described by the
[Intro to Ahead Of Time Compilation](https://blog.openj9.org/2018/10/10/intro-to-ahead-of-time-compilation/)
blog series, which this doc assumes the reader has already read.

# Introduction

There are four general kinds of inlined method relocations:

* Inlined Methods
* Inlined Methods with NOP Guards
* Profiled Inlined Methods
* Profiled Inlined Methods with Guards

All relocations are used to validate the inlined method. However, the
relocations for methods with guards are used to relocate the associated
inlined table entry in the metadata as well as the guards for the 
inlined sites, whereas the rest are only used to relocate the associated
inlined table entry. Profiled Inlined Methods are methods that were 
inlined that were not named by the caller. For example,
consider the following scenario:

```
class base {
	void foo();
}
class child_one extends base {
	void foo();
}
child class_two extends base {
	void foo();
}

...

	void some_method(base b) {
		b.foo();
	}
```
Because `b` is of type `base`, the `foo` that gets executed could either
be the implementation in `base` or `child_one` or `child_two`. If the
inliner decided to pick `base.foo()` because that is what was named in
`some_method`, then the relocation would be the Inlined Method variant.
However, if the inliner decided to inline `child_two.foo()` based on 
profiling information, the relocation would be the Profiled Inlined
Method variant.

# Generating Guards & External Relocations

During an AOT compilation, i.e. when the code is first generated, external
relocation are generated at various points. Once all of these external 
relocations are added to a list, the inlined method external relocations
are then added in reverse order; because entries are added to the
list at the front, an external relocation associated with the last inlined
site index is added first, followed by the second last, continuing to the
first. This ensures that the first inlined method external relocation in
the list is associated with the first inlined site.

The reason for ensuring this is because validating/materializing values for
subsequent inlined methods may depend on the previous entry. For example,
if `foo()` calls `bar()` calls `baz()`, and `bar` and `baz` are inlined (first
`bar` would be inlined, then `baz`), `baz` is named in `bar`'s constant
pool, and `bar` is named in `foo`'s constant pool. Therefore, in order to
materialize the `J9Method` of `baz`, one first needs the `J9Method` of `bar`,
which first requires the `J9Method` of `foo`.

If the inlined method has a guard assocated with it, then the Inlined Method
with NOP Guard / Profiled Inlined Method with Guard relocations are 
generated. If not, then the Inlined Method / Profiled Inlined Method
relocations are generated. Additionally, unless the 
[SVM](https://github.com/eclipse/openj9/blob/master/doc/compiler/aot/SymbolValidationManager.md)
is enabled, the inlined method external relocations are the
last entries added to the list; if the SVM is enabled, the SVM validation
records are added next - part of the validation that would've been done by
the inlined method validation procedure is delegated to the SVM.

# Validating Inlined Sites 

The way an inlined method is validated depends on whether it was
profiled or not.

## Inlined Methods with or without NOP Guards

The process starts in `TR_RelocationRecordInlinedMethod::preparePrivateData`.
First `TR_RelocationRecordInlinedMethod::inlinedSiteValid` is called, which

1. Gets the J9Method of the caller. For the very first inlined site, this will be the method being compiled; for the rest it will be from one of the previously validated/relocated inlined sites.
2. Checks if the method has been unloaded.
3. Materializes the J9Method of the inlined method:
    * If the SVM is enabled the method ID in the relocation record is used.
    * If the SVM is disabled, `getMethodFromCP` is called which is a virtual method that has different implementations depending on the kind of inlined method (special, static, virtual, interface).
4. Checks if the inlined site can be activated because of debug or other JVM restrictions.
5. Checks if J9ROMClass of the J9Method from step 3 matches the J9ROMClass in the SCC based on the offset stored in the relocation record.

If everything succeeds, `inlinedSiteValid` returns true. The success or
failure is cached in `reloPrivateData->_failValidation`.

## Profiled Inlined Methods with or without Guards

The process occurs in `TR_RelocationRecordProfiledInlinedMethod::preparePrivateData`
which

1. Materializes the class of the inlined method:
    * If the SVM is enabled, the class ID in the relocation record is used.
    * If the SVM is disabled, the process described [here](https://github.com/eclipse/openj9/blob/master/doc/compiler/aot/ClassChains.md#finding-a-candidate-j9class) is used, specifically the section about using the class chain of the first class loaded by the classloader that loaded the class of the inlined method.
2. Validates the [Class Chain](https://github.com/eclipse/openj9/blob/master/doc/compiler/aot/ClassChains.md)  of the class from 1.
3. Materializes the inlined method from the class from 1 using the method index stored in the relocation record.
4. Checks if the inlined site can be activated because of debug or other JVM restrictions.

The success or failure is then cached in `reloPrivateData->_failValidation`.

# Relocating the Inlining Table in the J9JITExceptionTable

In both variants of validating the inlined sites, the final task if everything
succeeded is to relocate the entry in the inlining table in the 
J9JITExceptionTable. `TR_RelocationRecordInlinedMethod::fixInlinedSiteInfo` 
is called, which relocates the entry in the table, and registers an assumption 
to patch the entry in the table should the class of the method get unloaded.

# Relocating (Activating / Invalidating)  Guards

The way a guard is relocated depends on whether it is a NOP guard or not.
Inlined Profiled Methods do not use NOP guards; Inlined Methods _do_ use NOP
guards.

## Inlined Methods with NOP Guards

If `reloPrivateData->_failValidation` is true, then if the SVM is enabled
the AOT load is aborted; if the SVM is disabled then 
`TR_RelocationRecordNopGuard::invalidateGuard` is called, which patches
the guard to branch to the slow path. If `reloPrivateData->_failValidation`
is false, then `TR_RelocationRecordNopGuard::activateGuard` is called, which

1. Calls `createAssumptions`, which will add assumptions appropriate for the kind of the inlined method (virtual, interface).
2. Registers a Class Redefinition assumption against the class of the inlined method.

## Profiled Inlined Methods with Guards

If `reloPrivateData->_failValidation` is true, then if the SVM is enabled
the AOT load is aborted. If false, nothing is done here. Because these 
guards are **not** NOP guards, but explicit tests, the materialization of 
the pointer which is put into a register to be used in the comparison 
instruction is relocated in a separate relocation record (either a 
`TR_RelocationRecordClassPointer` or 
`TR_RelocationRecordMethodPointer` relocation record).

# Subtleties

This section goes through some subtleties that were either glossed over
above, or are not immediately obvious.

### Inlined Method Validations/Relocations have to happen first

If the SVM is enabled, then the SVM validations are performed first.
However, regardless of whether the SVM is enabled or not, the inlined 
method validations/relocations have to happen before the other
relocations. This is because many relocation records will store a CP
index and an inlined site index; this means that in order to materialize
the value, the relocation infrastructure needs to look in the constant 
pool of the class of the method inlined at the specified inlined site. 
Therefore, the inlining table in the J9JITExceptionTable must hold 
valid entries by this point.

### Aborting compilation with SVM enabled for an invalid inlined site

If the SVM is enabled, if all SVM validations pass then it is (almost)
not possible for any of the various tests for both Inlined Method and 
Profiled Inlined Method to fail; the exception is if 
`inlinedSiteCanBeActivated` returns false which can happen because of
debug or other JVM restrictions.

Because the SVM enables far more optimization, it is not immediately
obvious whether it is safe to execute the code. Without validating
an inlined method, it is not clear if some other part of the code's
functional correctness depends on the inlined method being the
same in the second run. As such, it is better to just take the
conservative approach of failing the AOT load rather than execute a 
potentially incorrect body.

### Class Initialization

There isn't a need to verify whether the class of an inlined method
has been initialized. The reason is because for most inlined methods,
in order to execute it, the receiver object must exist;
instantiating the receiver initializes the class including its
superclass(es) and interface class(es) it implements (if the interface
has default/static methods, static fields, or when reflection is
used). Therefore, even if the class isn't initialized at relocation
time, the class will be initialized at runtime.

Note, this logic does not apply static methods. However, it does not
matter as all of the VM APIs used to materialize the J9Method (in
the case of Inlined Method relocations) or the J9Class (in the case
of Profiled Inlined Method relocations) do a compile time resolve
and also check if the class is initialized. Therefore, there is no
need to explicitly check for initialization; if the API returns a
non-NULL result, the class must be initialized.

### Class Redefinition Assumptions for J9JITExceptionTable Inlining Table Entries

There is no need to register Class Redefinitions assumptions against
entries in the inlining table. This is because although a redefined
class will have the same J9Class pointer, the J9Method pointers of
the redefined class will be different. As Class Redefinition
semantics require that new invocations result in execution of the
redefined method, the inlining table entry must remain unchanged in
case there are threads that are still executing the old inlined bodies
(perhaps because they haven't reached a point where they can
transition to the new code).
