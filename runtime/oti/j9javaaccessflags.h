/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

/* This file contains J9-specific things which have been moved from builder.
 * DO NOT include it directly - it is automatically included at the end of j9generated.h
 * (which also should not be directly included).  Include j9.h to get this file.
 */

#ifndef J9JAVAACCESSFLAGS_H
#define J9JAVAACCESSFLAGS_H

/* @ddr_namespace: map_to_type=J9JavaAccessFlags */

/* Constants from J9JavaAccessFlags */
#define J9AccAbstract 0x400
#define J9AccAnnotation 0x2000
#define J9AccBridge 0x40
#define J9AccClassAnnnotionRefersDoubleSlotEntry 0x80000
#define J9AccClassAnonClass 0x800
#define J9AccClassArray 0x10000
#define J9AccClassBytecodesModified 0x100000
#define J9AccClassCloneable 0x80000000
#define J9AccClassCompatibilityMask 0x7FFF
#define J9AccClassDepthMask 0xFFFF
#define J9AccClassDying 0x8000000
#define J9AccClassFinalizeNeeded 0x40000000
#define J9AccClassGCSpecial 0x800000
#define J9AccClassHasBeenOverridden 0x100000
#define J9AccClassHasClinit 0x4000000
#define J9AccClassHasEmptyFinalize 0x200000
#define J9AccClassIsUnmodifiable 0x400000
#define J9AccClassHasFinalFields 0x2000000
#define J9AccClassHasJDBCNatives 0x400000
#define J9AccClassHasNonStaticNonAbstractMethods 0x8000000
#define J9AccClassHasVerifyData 0x800000
#define J9AccClassHotSwappedOut 0x4000000
#define J9AccClassInnerClass 0x4000
#define J9AccClassNeedsStaticConstantInit 0x10000
#define J9AccClassIntermediateDataIsClassfile 0x20000
#define J9AccClassInternalPrimitiveType 0x20000
#define J9AccClassIsContended 0x1000000
#define J9AccClassOwnableSynchronizer 0x200000
#define J9AccClassUnused200 0x200
#define J9AccClassUnused400 0x400
#define J9AccClassRAMArray 0x10000
#define J9AccClassRAMShapeShift 0x10
#define J9AccClassReferenceMask 0x30000000
#define J9AccClassReferencePhantom 0x30000000
#define J9AccClassReferenceShift 0x1C
#define J9AccClassReferenceSoft 0x20000000
#define J9AccClassReferenceWeak 0x10000000
#define J9AccClassRomToRamMask 0xF3000000
#define J9AccClassUnsafe 0x40000
#define J9AccClassUseBisectionSearch 0x2000
#define J9AccEmptyMethod 0x4000
#define J9AccEnum 0x4000
#define J9AccFinal 0x10
#define J9AccForwarderMethod 0x2000
#define J9AccGetterMethod 0x200
#define J9AccInterface 0x200
#define J9AccMandated 0x8000
#define J9AccMethodCallerSensitive 0x100000
#define J9AccMethodUnused0x1000000 0x1000000
#define J9AccMethodFrameIteratorSkip 0x80000
#define J9AccMethodHasBackwardBranches 0x200000
#define J9AccMethodHasDebugInfo 0x40000
#define J9AccMethodHasDefaultAnnotation 0x80000000
#define J9AccMethodHasExceptionInfo 0x20000
#define J9AccMethodHasExtendedModifiers 0x4000000
#define J9AccMethodHasGenericSignature 0x2000000
#define J9AccMethodHasMethodAnnotations 0x20000000
#define J9AccMethodHasMethodHandleInvokes 0x8000000
#define J9AccMethodHasMethodParameters 0x800000
#define J9AccMethodHasParameterAnnotations 0x40000000
#define J9AccMethodHasStackMap 0x10000000
#define J9AccMethodHasTypeAnnotations 0x4000000
#define J9AccMethodObjectConstructor 0x400000
#define J9AccMethodReturn0 0x0
#define J9AccMethodReturn1 0x40000
#define J9AccMethodReturn2 0x80000
#define J9AccMethodReturnA 0x140000
#define J9AccMethodReturnD 0x100000
#define J9AccMethodReturnF 0xC0000
#define J9AccMethodReturnMask 0x1C0000
#define J9AccMethodReturnShift 0x12
#define J9AccMethodVTable 0x10000
#define J9AccNative 0x100
#define J9AccPrivate 0x2
#define J9AccProtected 0x4
#define J9AccPublic 0x1
#define J9AccStatic 0x8
#define J9AccStrict 0x800
#define J9AccSuper 0x20
#define J9AccSynchronized 0x20
#define J9AccSynthetic 0x1000
#define J9AccTransient 0x80
#define J9AccValueType 0x100
#define J9AccVarArgs 0x80
#define J9AccVolatile 0x40
#define J9StaticFieldRefBaseType 0x1
#define J9StaticFieldRefDouble 0x2
#define J9StaticFieldRefVolatile 0x4
#define J9StaticFieldRefBoolean 0x8
#define J9StaticFieldRefPutResolved 0x10
#define J9StaticFieldRefFinal 0x20
#define J9StaticFieldRefFlagBits 0x3F
#define J9_JAVA_CLASS_FINALIZER_CHECK_MASK (J9AccClassFinalizeNeeded | J9AccClassOwnableSynchronizer)
#define J9_JAVA_MODIFIERS_SPECIAL_OBJECT (J9AccClassFinalizeNeeded | J9AccClassReferenceMask)

#endif /*J9JAVAACCESSFLAGS_H */
