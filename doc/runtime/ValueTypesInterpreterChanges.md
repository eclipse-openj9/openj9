<!--
Copyright (c) 2019, 2019 IBM Corp. and others

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

This document contains a description of the existing modifications made to the Interpreter in order to support ValueTypes.
This document is not a specification for ValueTypes and only reflects the current implementation. Anything stated here is 
subject to change.

Background:
-----------
ValueTypes (VTs) are immutable and identityless types which are subclasses of java/lang/Object. Just like normal Classes VTs can 
declare methods and fields, however, they cannot be subclassed and must be a direct descendant of a root type. VTs come in two 
flavours, nullable and non-nullable. When reading a non-nullable VT, it is not possible to observe a null, meaning that the VT 
must be initialized at all times. As a consequence, non-nullable VTs are allowed to be stored as flattened fields or scalarized 
when used as method parameters. On the other hand, a nullable VT may be assigned null, and is therefore not flattenable. 

ValueTypes
-----------
- Contains `ACC_VALUE_TYPE` in class modifiers
	- `J9AccValueType` in romClass->modifiers
	- `J9ClassIsValueType` in ramClazz->classFlags
- SuperClass is j.l.Object, no subclassing
- All instance fields are final (explicitly in field modifiers)
- A nested VT field with an L signature is nullable
- A nested VT field with a Q signature is flattenable and not nullable
	- note: flattenable means the field is allowed to be flattened, but does not guarantee that the field is flattened
	- if a nested VT field is flattened, the `J9FieldFlagFlattened` flag will be set in the ramFieldRef cp flags
	- There is currently no support for static Q type fields 
- The JVM will only flatten a nested VT if it contains the `J9ClassIsFlattened` flag in its ramClass Flags
	- If the size of the class is larger than the `valueFlatteningThreshold` (set by ) flattening will be disallowed
	- If the class has the `@contended` annotation flattening will be disallowed
	
New Bytecodes
-------------
DefaultValue: 
- param: classref
- stack effect: (--> NewObjectRef)
- resolves classref, throws ICCE if class is not a valuetype
- if the classref is already resolved, throw ICCE if class is not a valuetype
- initialize class, if not already initialized
- allocate a new instance, throws OOM if allocation fails
- all fields initialized to the default values (0 for primitives, Null for references ie. LObjects + LValues)
- pushes NewObjectRef on the stack

WithField:
- param: fieldref
- stack effect: (ObjectRef, value --> NewObjectRef)
- resolves instance fieldref, withfield is allowed to set final fields (similar to a constructor)
- throws ICCE if objectRef is not a VT
- allocates a new instances (NewObjectRef), throws OOM if allocation fails
- copies all values from the ObjectRef into NewObjectRef 
- writes field specified by fieldref with the value in NewObjectRef
- pops value and ObjectRef from the stack, pushes NewObjectRef onto the stack

Existing bytecode changes:
--------------------------
New:
- Resolves classref, throws ICCE if classref is a valueType
- If classref is resolved, throws ICCE if classref is a valueType

PutField:
- If class specified in fieldref is a valueType, putfield throws IAE

GetField:

Things to come:
----------------------
- array support for flattened value types
- static field support for value types
- declaration site flattenability
- uninitialized behaviour for non-flattened flattenable types
	- i.e. cannot observe null
	