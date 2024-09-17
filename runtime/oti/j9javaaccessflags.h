/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/* This file contains J9-specific things which have been moved from builder.
 * DO NOT include it directly - it is automatically included at the end of j9generated.h
 * (which also should not be directly included).  Include j9.h to get this file.
 */

#ifndef J9JAVAACCESSFLAGS_H
#define J9JAVAACCESSFLAGS_H

/* @ddr_namespace: map_to_type=J9JavaAccessFlags */

/*
 * Modifiers of ROM classes, fields or methods.
 * When a given modifier is defined by the JVM specification,
 * the value is taken from that specification.
 *
 * See map in ROMClassWriter::writeMethods to confirm available slots for methods.
 */
#define J9AccPublic                        0x00000001 /* class method field */
#define J9AccPrivate                       0x00000002 /* class(nested) method field */
#define J9AccProtected                     0x00000004 /* class(nested) method field */
#define J9AccStatic                        0x00000008 /* class(nested) method field */
#define J9AccFinal                         0x00000010 /* class method field */
/* Project Valhalla repurposes 0x00000020 as ACC_IDENTITY since ACC_SUPER has no effect after Java 8. */
#define J9AccSuper                         0x00000020 /* class */
#define J9AccClassHasIdentity              0x00000020 /* class */
#define J9AccSynchronized                  0x00000020 /* method */
#define J9AccBridge                        0x00000040 /* method */
#define J9AccVolatile                      0x00000040 /* field */
#define J9AccVarArgs                       0x00000080 /* method */
#define J9AccTransient                     0x00000080 /* field */
#define J9AccNative                        0x00000100 /* method */
#define J9AccInterface                     0x00000200 /* class */
#define J9AccGetterMethod                  0x00000200 /* method */
#define J9AccAbstract                      0x00000400 /* class method */
#define J9AccStrict                        0x00000800 /* method */
#define J9AccSynthetic                     0x00001000 /* class method field */
#define J9AccAnnotation                    0x00002000 /* class */
#define J9AccMethodUnused0x2000            0x00002000 /* method */
#define J9AccEnum                          0x00004000 /* class field */
#define J9AccEmptyMethod                   0x00004000 /* method */
/* ACC_MODULE reserves 0x00008000 for classes */
#define J9AccClassArray                    0x00010000 /* class */
#define J9AccMethodVTable                  0x00010000 /* method */
#define J9AccClassInternalPrimitiveType    0x00020000 /* class */
#define J9AccMethodHasExceptionInfo        0x00020000 /* method */
#define J9AccMethodHasDebugInfo            0x00040000 /* method */
#define J9AccMethodFrameIteratorSkip       0x00080000 /* method */
#define J9AccMethodCallerSensitive         0x00100000 /* method */
#define J9AccMethodHasBackwardBranches     0x00200000 /* method */
#define J9AccMethodObjectConstructor       0x00400000 /* method */
#define J9AccMethodHasMethodParameters     0x00800000 /* method */
#define J9AccMethodAllowFinalFieldWrites   0x01000000 /* method */
#define J9AccMethodHasGenericSignature     0x02000000 /* method */
#define J9AccMethodHasExtendedModifiers    0x04000000 /* method */
#define J9AccMethodHasMethodHandleInvokes  0x08000000 /* method */
#define J9AccMethodHasStackMap             0x10000000 /* method */
#define J9AccMethodHasMethodAnnotations    0x20000000 /* method */
#define J9AccMethodHasParameterAnnotations 0x40000000 /* method */
#define J9AccMethodHasDefaultAnnotation    0x80000000 /* method */
/* Masks and helpers */
#define J9AccClassCompatibilityMask 0x7FFF
#define J9AccMethodReturnMask 0x1C0000
#define J9AccMethodReturnShift 0x12
/* Used in HookedByTheJit.cpp */
#define J9AccMethodReturn0 0x0
#define J9AccMethodReturn1 0x40000
#define J9AccMethodReturn2 0x80000
#define J9AccMethodReturnA 0x140000
#define J9AccMethodReturnD 0x100000
#define J9AccMethodReturnF 0xC0000
/* DDR flags */
#define J9AccMethodHasTypeAnnotations 0x4000000

/* Extra modifiers of ROM classes.
 * When a given modifier is defined by the JVM specification,
 * the value is taken from that specification.
 * See map in ROMClassBuilder::computeExtraModifiers for
 * available slots.
 */
#define J9AccClassIsShared 0x20
#define J9AccClassIsValueBased 0x40
#define J9AccClassHiddenOptionNestmate 0x80
#define J9AccClassHiddenOptionStrong 0x100
#define J9AccSealed 0x200
#define J9AccRecord 0x400
#define J9AccClassAnonClass 0x800
#define J9AccClassIsInjectedInvoker 0x1000
#define J9AccClassUseBisectionSearch 0x2000
#define J9AccClassInnerClass 0x4000
#define J9AccClassHidden 0x8000
#define J9AccClassNeedsStaticConstantInit 0x10000
#define J9AccClassIntermediateDataIsClassfile 0x20000
#define J9AccClassUnsafe 0x40000
#define J9AccClassAnnnotionRefersDoubleSlotEntry 0x80000
#define J9AccClassBytecodesModified 0x100000
#define J9AccClassHasEmptyFinalize 0x200000
#define J9AccClassIsUnmodifiable 0x400000
#define J9AccClassHasVerifyData 0x800000
#define J9AccClassIsContended 0x1000000
#define J9AccClassHasFinalFields 0x2000000
#define J9AccClassHasClinit 0x4000000
#define J9AccClassHasNonStaticNonAbstractMethods 0x8000000
#define J9AccClassReferenceWeak 0x10000000
#define J9AccClassReferenceSoft 0x20000000
#define J9AccClassReferencePhantom 0x30000000
#define J9AccClassFinalizeNeeded 0x40000000
#define J9AccClassCloneable 0x80000000

/* RAM class classDepthAndFlags.
 * See map in createramclass.cpp:internalCreateRAMClassFromROMClassImpl
 * for available slots.
 */
#define J9AccClassRAMArray 0x10000
#define J9AccClassHasBeenOverridden 0x100000
#define J9AccClassOwnableSynchronizer 0x200000
#define J9AccClassHasJDBCNatives 0x400000
#define J9AccClassGCSpecial 0x800000
#define J9AccClassContinuation 0x1000000
#define J9AccClassHotSwappedOut 0x4000000
#define J9AccClassDying 0x8000000
/* Masks and helpers */
#define J9AccClassDepthMask 0xFFFF
#define J9AccClassReferenceMask 0x30000000
#define J9AccClassRomToRamMask 0xF3000000
#define J9AccClassRAMShapeShift 0x10
#define J9_JAVA_CLASS_FINALIZER_CHECK_MASK (J9AccClassFinalizeNeeded | J9AccClassOwnableSynchronizer | J9AccClassContinuation)
#define J9_JAVA_MODIFIERS_SPECIAL_OBJECT (J9AccClassFinalizeNeeded | J9AccClassReferenceMask)

/* Inform DDR that the static field ref constants support proper narrowing  */
#define J9PutfieldNarrowing 1

/* static field flags (low 8 bits of flagsAndClass) - low 3 bits are the field type for primitibve types */
#define J9StaticFieldRefFlagBits 0xFF
#define J9StaticFieldRefTypeMask 0x7
#define J9StaticFieldRefTypeObject 0x0
#define J9StaticFieldRefTypeBoolean 0x1
#define J9StaticFieldRefTypeByte 0x2
#define J9StaticFieldRefTypeChar 0x3
#define J9StaticFieldRefTypeShort 0x4
#define J9StaticFieldRefTypeIntFloat 0x5
#define J9StaticFieldRefTypeLongDouble 0x6
#define J9StaticFieldRefTypeUnused_0x7 0x7
#define J9StaticFieldRefVolatile 0x8
#define J9StaticFieldRefPutResolved 0x10
#define J9StaticFieldRefFinal 0x20
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
#define J9StaticFieldIsNullRestricted 0x40
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
#define J9StaticFieldRefUnused_0x80 0x80

/* ImplicitCreation attribute flags */
#define J9AccImplicitCreateHasDefaultValue 0x1
#define J9AccImplicitCreateNonAtomic 0x2

#endif /*J9JAVAACCESSFLAGS_H */
