<!--
Copyright IBM Corp. and others 2021

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
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

The relocation infrastructure contains a lot of boilerplate; as such there
are several places one must be aware of when adding a new relocation.
This doc outlines all the various places in the code that need to be
addressed for a new relocation. The steps involved are:

1. [Add new relocation type](#add-new-relocation-type)
2. [Add SVM Record](#add-svm-record)
3. [Add or reuse a Binary Template](#add-or-reuse-a-binary-template)
4. [Extend some API Class](#extend-some-api-class)
5. [Initialize the Relocation Record Header](#initialize-the-relocation-record-header)
6. [Update Table of Sizes](#update-table-of-sizes)
7. [Add External Relocations](#add-external-relocations)


## Add new relocation type

Add a new type to the `TR_ExternalRelocationTargetKind` enum, as well as the
associated string to `TR::ExternalRelocation::_externalRelocationTargetKindNames`
in OMR.

## Add SVM Record

If the record is not a [Symbol Validation Manager](SymbolValidationManager.md)
(SVM) validation record, then skip this section.

If the validation record is for a class, then add a new struct and extend
`ClassValidationRecord`, for a method extend `MethodValidationRecord`, or
for anything else extend `SymbolValidationRecord`. Add implementations for
`isLessThanWithinKind` and `printFields`. Next, add both "add" and
"validate" methods to the `SymbolValidationManager` class; these methods
are the public API that will be used to add and validate the new record. 
In the implementation of the "add" method, use `addClassRecord` for classes,
`addMethodRecord` for methods, and `addVanillaRecord` for anything else.

## Add or reuse a Binary Template

The binary template is the data stucture that contains the data needed 
to perform the validation and/or materialize the value that is valid in
the current JVM instance. Depending on the data needed, it is possible
that an existing template may suffice either completely or partially.
If it sufficies partially then add a new struct extending the
relevant template; otherwise, the new struct should extend
`TR_RelocationRecordBinaryTemplate`. This is necessary because this base 
class contains fields common to all binary templates that the relocation
infrastructure uses to determine the type of relocation record as well
as the number of offsets the record applies to.

## Extend some API Class

`TR_RelocationRecord` and extending classes are the API classes. These
are used to read from / write to the buffer that contains the
relocation records that will be / were stored to the SCC. If in the previous
step, an existing binary template completely or partially sufficed,
then an existing API class will contain methods that can be reused for
the new relocation record. Unlike the binary template (which could be
reused entirely), a new API class should always be created. The reason
is because how the value is materialized and how the relocation has
to be applied is going to be different from existing relocation records;
otherwise, there wouldn't be a need for a new record.

The following will have be done for the new API class:
* Add the two constructors
* Add `isValidationRecord` (if needed) if this is for a validation record
* Add reader/writer APIs for each of the fields in the binary template (that is not already addressed some super class)
* Add the `name` method
* Add the `print` method if needed
* Add `preparePrivateData` and `applyRelocation` implementations

## Initialize the Relocation Record Header

The Relocation Record Header is described by the 
[Binary Template](#add-or-reuse-a-binary-template). The code that
initializes the header is invoked during an AOT compilation; it is
part of the process of generating the relocation records that get
stored into the SCC. The associated API class is used to write data
into the header. This data is acquired from the compiler via the
`TR::ExternalRelocation`s that are generated during compilation.

As most records are platform agnostic, the code should be added to
`initializeCommonAOTRelocationHeader`. However, if a record must
be platform specific, then the code should be added to the appropriate
platform's implementation of 
`initializePlatformSpecificAOTRelocationHeader`.

## Update Table of Sizes

`TR_RelocationRecord::_relocationRecordHeaderSizeTable`
should be updated with the `sizeof` of the binary template associated
with the new relocation record.

## Add External Relocations

Ensure the compiler generates the `TR::ExternalRelocation`s
at the necessary points during codegen.
