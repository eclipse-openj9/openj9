/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef _J9MODIFIERS_API_H
#define _J9MODIFIERS_API_H

/* @ddr_namespace: default */
#include "j9cfg.h"

#define _J9ROMCLASS_J9MODIFIER_IS_SET(romClass,j9Modifiers) \
				J9_ARE_ALL_BITS_SET((romClass)->extraModifiers, j9Modifiers)
#define _J9ROMCLASS_J9MODIFIER_IS_ANY_SET(romClass,j9Modifiers) \
				J9_ARE_ANY_BITS_SET((romClass)->extraModifiers, j9Modifiers)
#define _J9ROMMETHOD_J9MODIFIER_IS_SET(romMethod,j9Modifiers) \
				J9_ARE_ALL_BITS_SET((romMethod)->modifiers, j9Modifiers)
#define _J9ROMMETHOD_J9MODIFIER_IS_ANY_SET(romMethod,j9Modifiers) \
				J9_ARE_ANY_BITS_SET((romMethod)->modifiers, j9Modifiers)
#define _J9ROMCLASS_SUNMODIFIER_IS_SET(romClass,sunModifiers) \
				J9_ARE_ALL_BITS_SET((romClass)->modifiers, sunModifiers)
#define _J9ROMCLASS_SUNMODIFIER_IS_ANY_SET(romClass,sunModifiers) \
				J9_ARE_ANY_BITS_SET((romClass)->modifiers, sunModifiers)


/* Macros that always operate directly a romClass copy, be it in process or out of process given a romClass
 * that has already been read in */
#define J9ROMCLASS_IS_PUBLIC(romClass)          _J9ROMCLASS_SUNMODIFIER_IS_SET((romClass), J9AccPublic)
#define J9ROMCLASS_IS_FINAL(romClass)           _J9ROMCLASS_SUNMODIFIER_IS_SET((romClass), J9AccFinal)
#define J9ROMCLASS_IS_SUPER(romClass)           _J9ROMCLASS_SUNMODIFIER_IS_SET((romClass), J9AccSuper)
#define J9ROMCLASS_IS_INTERFACE(romClass)       _J9ROMCLASS_SUNMODIFIER_IS_SET((romClass), J9AccInterface)
#define J9ROMCLASS_IS_ABSTRACT(romClass)        _J9ROMCLASS_SUNMODIFIER_IS_SET((romClass), J9AccAbstract)

#define J9ROMCLASS_IS_SYNTHETIC(romClass)		_J9ROMCLASS_SUNMODIFIER_IS_SET((romClass), J9AccSynthetic)
#define J9ROMCLASS_IS_ARRAY(romClass)			_J9ROMCLASS_SUNMODIFIER_IS_SET((romClass), J9AccClassArray)
#define J9ROMCLASS_IS_PRIMITIVE_TYPE(romClass)	_J9ROMCLASS_SUNMODIFIER_IS_SET((romClass), J9AccClassInternalPrimitiveType)
#define J9ROMCLASS_IS_INTERMEDIATE_DATA_A_CLASSFILE(romClass)		_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassIntermediateDataIsClassfile)
#define J9ROMCLASS_IS_UNSAFE(romClass)			_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassUnsafe)
#define J9ROMCLASS_HAS_VERIFY_DATA(romClass)	_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassHasVerifyData)
#define J9ROMCLASS_HAS_MODIFIED_BYTECODES(romClass)	_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassBytecodesModified)
#define J9ROMCLASS_HAS_EMPTY_FINALIZE(romClass)	_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassHasEmptyFinalize)
#define J9ROMCLASS_HAS_FINAL_FIELDS(romClass)	_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassHasFinalFields)
#define J9ROMCLASS_HAS_CLINIT(romClass)			_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassHasClinit)
#define J9ROMCLASS_REFERENCE_WEAK(romClass)		_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassReferenceWeak)
#define J9ROMCLASS_REFERENCE_SOFT(romClass)		_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassReferenceSoft)
#define J9ROMCLASS_REFERENCE_PHANTOM(romClass)	_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassReferencePhantom)
#define J9ROMCLASS_FINALIZE_NEEDED(romClass)	_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassFinalizeNeeded)
#define J9ROMCLASS_IS_CLONEABLE(romClass)		_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassCloneable)
#define J9ROMCLASS_ANNOTATION_REFERS_DOUBLE_SLOT_ENTRY(romClass)	_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassAnnnotionRefersDoubleSlotEntry)
#define J9ROMCLASS_IS_UNMODIFIABLE(romClass)	_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassIsUnmodifiable)

/* 
 * Note that resolvefield ignores this flag if the cache line size cannot be determined.
 * Use ObjectFieldInfo.isContendedClasslayout() to determine if the class is layed out
 * per JEP-142.
 */
#define J9ROMCLASS_IS_CONTENDED(romClass)	_J9ROMCLASS_J9MODIFIER_IS_SET((romClass), J9AccClassIsContended)

#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
/* Will need to modify this if ValObject/RefObject proposal goes through */
#define J9ROMCLASS_IS_VALUE(romClass)	_J9ROMCLASS_SUNMODIFIER_IS_SET((romClass), J9AccValueType)
#endif/* #ifdef J9VM_OPT_VALHALLA_VALUE_TYPES */

#define J9ROMMETHOD_IS_GETTER(romMethod)				_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccGetterMethod)
#define J9ROMMETHOD_IS_FORWARDER(romMethod)				_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccForwarderMethod)
#define J9ROMMETHOD_IS_EMPTY(romMethod)					_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccEmptyMethod)
#define J9ROMMETHOD_HAS_VTABLE(romMethod)				_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodVTable)
#define J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)		_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasExceptionInfo)
#define J9ROMMETHOD_HAS_DEBUG_INFO(romMethod)			_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasDebugInfo)
#define J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod)	_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasBackwardBranches)
#define J9ROMMETHOD_HAS_GENERIC_SIGNATURE(romMethod)	_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasGenericSignature)
#define J9ROMMETHOD_HAS_STACK_MAP(romMethod)			_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasStackMap)
#define J9ROMMETHOD_HAS_ANNOTATIONS_DATA(romMethod)		_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasMethodAnnotations)
#define J9ROMMETHOD_HAS_PARAMETER_ANNOTATIONS(romMethod)	_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasParameterAnnotations)
#define J9ROMMETHOD_HAS_METHOD_PARAMETERS(romMethod)	_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasMethodParameters)
#define J9ROMMETHOD_HAS_CODE_TYPE_ANNOTATIONS(extendedModifiers)	J9_ARE_ALL_BITS_SET(extendedModifiers, CFR_METHOD_EXT_HAS_CODE_TYPE_ANNOTATIONS)
#define J9ROMMETHOD_HAS_METHOD_TYPE_ANNOTATIONS(extendedModifiers)	J9_ARE_ALL_BITS_SET(extendedModifiers, CFR_METHOD_EXT_HAS_METHOD_TYPE_ANNOTATIONS)
#define J9ROMMETHOD_HAS_DEFAULT_ANNOTATION(romMethod)	_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasDefaultAnnotation)
#define J9ROMMETHOD_HAS_EXTENDED_MODIFIERS(romMethod)	_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodHasExtendedModifiers)
#define J9ROMMETHOD_IS_OBJECT_CONSTRUCTOR(romMethod)	_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodObjectConstructor)
#define J9ROMMETHOD_IS_CALLER_SENSITIVE(romMethod)	_J9ROMMETHOD_J9MODIFIER_IS_SET((romMethod), J9AccMethodCallerSensitive)

#define J9ROMFIELD_IS_CONTENDED(romField)	J9_ARE_ALL_BITS_SET((romField)->modifiers, J9FieldFlagIsContended)


/* Composite Flag checks */

#define J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass) _J9ROMCLASS_SUNMODIFIER_IS_ANY_SET((romClass), J9AccClassArray | J9AccClassInternalPrimitiveType)
#define J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod) \
	((((romMethod)->modifiers) & (J9AccMethodObjectConstructor | J9AccEmptyMethod)) == J9AccMethodObjectConstructor)

/* Class instances are allocated via the new bytecode */
#define J9ROMCLASS_ALLOCATES_VIA_NEW(romClass) \
	J9_ARE_NO_BITS_SET((romClass)->modifiers, J9AccAbstract | J9AccInterface | J9AccClassArray | J9AccValueType)

/* Class instances are allocated via J9RAMClass->totalInstanceSize */
#define J9ROMCLASS_ALLOCATE_USES_TOTALINSTANCESIZE(romClass) \
	J9_ARE_NO_BITS_SET((romClass)->modifiers, J9AccAbstract | J9AccInterface | J9AccClassArray)

/* Only public vTable methods go into the iTable */
#define J9ROMMETHOD_IN_ITABLE(romMethod) \
	J9_ARE_ALL_BITS_SET((romMethod)->modifiers, J9AccMethodVTable | J9AccPublic)

#endif	/* _J9MODIFIERS_API_H */
