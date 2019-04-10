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

#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"

J9::ARM64::AheadOfTimeCompile::AheadOfTimeCompile(TR::CodeGenerator *cg) :
         J9::AheadOfTimeCompile(_relocationTargetTypeToHeaderSizeMap, cg->comp()),
         _cg(cg)
   {
   }

void J9::ARM64::AheadOfTimeCompile::processRelocations()
   {
   TR_UNIMPLEMENTED();
   }

uint8_t *J9::ARM64::AheadOfTimeCompile::initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

uint32_t J9::ARM64::AheadOfTimeCompile::_relocationTargetTypeToHeaderSizeMap[TR_NumExternalRelocationKinds] =
   {
   // TODO: Fill this table
   0,                                        // TR_ConstantPool                        = 0
   0,                                        // TR_HelperAddress                       = 1
   0,                                        // TR_RelativeMethodAddress               = 2
   0,                                        // TR_AbsoluteMethodAddress               = 3
   0,                                        // TR_DataAddress                         = 4
   0,                                        // TR_ClassObject                         = 5
   0,                                        // TR_MethodObject                        = 6
   0,                                        // TR_InterfaceObject                     = 7
   0,                                        // TR_AbsoluteHelperAddress               = 8
   0,                                        // TR_FixedSequenceAddress                = 9
   0,                                        // TR_FixedSequenceAddress2               = 10
   0,                                        // TR_JNIVirtualTargetAddress             = 11
   0,                                        // TR_JNIStaticTargetAddress              = 12
   0,                                        // TR_ArrayCopyHelper                     = 13
   0,                                        // TR_ArrayCopyToc                        = 14
   0,                                        // TR_BodyInfoAddress                     = 15
   0,                                        // TR_Thunks                              = 16
   0,                                        // TR_StaticRamMethodConst                = 17
   0,                                        // TR_Trampolines                         = 18
   0,                                        // TR_PicTrampolines                      = 19
   0,                                        // TR_CheckMethodEnter                    = 20
   0,                                        // TR_RamMethod                           = 21
   0,                                        // TR_RamMethodSequence                   = 22
   0,                                        // TR_RamMethodSequenceReg                = 23
   0,                                        // TR_VerifyClassObjectForAlloc           = 24
   0,                                        // TR_ConstantPoolOrderedPair             = 25
   0,                                        // TR_AbsoluteMethodAddressOrderedPair    = 26
   0,                                        // TR_VerifyRefArrayForAlloc              = 27
   0,                                        // TR_J2IThunks                           = 28
   0,                                        // TR_GlobalValue                         = 29
   0,                                        // TR_BodyInfoAddressLoad                 = 30
   0,                                        // TR_ValidateInstanceField               = 31
   0,                                        // TR_InlinedStaticMethodWithNopGuard     = 32
   0,                                        // TR_InlinedSpecialMethodWithNopGuard    = 33
   0,                                        // TR_InlinedVirtualMethodWithNopGuard    = 34
   0,                                        // TR_InlinedInterfaceMethodWithNopGuard  = 35
   0,                                        // TR_SpecialRamMethodConst               = 36
   0,                                        // TR_InlinedHCRMethod                    = 37
   0,                                        // TR_ValidateStaticField                 = 38
   0,                                        // TR_ValidateClass                       = 39
   0,                                        // TR_ClassAddress                        = 40
   0,                                        // TR_HCR                                 = 41
   0,                                        // TR_ProfiledMethodGuardRelocation       = 42
   0,                                        // TR_ProfiledClassGuardRelocation        = 43
   0,                                        // TR_HierarchyGuardRelocation            = 44
   0,                                        // TR_AbstractGuardRelocation             = 45
   0,                                        // TR_ProfiledInlinedMethodRelocation     = 46
   0,                                        // TR_MethodPointer                       = 47
   0,                                        // TR_ClassPointer                        = 48
   0,                                        // TR_CheckMethodExit                     = 49
   0,                                        // TR_ValidateArbitraryClass              = 50
   0,                                        // TR_EmitClass                           = 51
   0,                                        // TR_JNISpecialTargetAddress             = 52
   0,                                        // TR_VirtualRamMethodConst               = 53
   0,                                        // TR_InlinedInterfaceMethod              = 54
   0,                                        // TR_InlinedVirtualMethod                = 55
   0,                                        // TR_NativeMethodAbsolute                = 56
   0,                                        // TR_NativeMethodRelative                = 57
   0,                                        // TR_ArbitraryClassAddress               = 58
   0,                                        // TR_DebugCounter                        = 59
   0,                                        // TR_ClassUnloadAssumption               = 60
   0,                                        // TR_J2IVirtualThunkPointer              = 61
   0,                                        // TR_InlinedAbstractMethodWithNopGuard   = 62
   0,                                        // TR_ValidateRootClass                   = 63
   0,                                        // TR_ValidateClassByName                 = 64
   0,                                        // TR_ValidateProfiledClass               = 65
   0,                                        // TR_ValidateClassFromCP                 = 66
   0,                                        // TR_ValidateDefiningClassFromCP         = 67
   0,                                        // TR_ValidateStaticClassFromCP           = 68
   0,                                        // TR_ValidateClassFromMethod             = 69
   0,                                        // TR_ValidateComponentClassFromArrayClass= 70
   0,                                        // TR_ValidateArrayClassFromComponentClass= 71
   0,                                        // TR_ValidateSuperClassFromClass         = 72
   0,                                        // TR_ValidateClassInstanceOfClass        = 73
   0,                                        // TR_ValidateSystemClassByName           = 74
   0,                                        // TR_ValidateClassFromITableIndexCP      = 75
   0,                                        // TR_ValidateDeclaringClassFromFieldOrStatic = 76
   0,                                        // TR_ValidateClassClass                  = 77
   0,                                        // TR_ValidateConcreteSubClassFromClass   = 78
   0,                                        // TR_ValidateClassChain                  = 79
   0,                                        // TR_ValidateRomClass                    = 80
   0,                                        // TR_ValidatePrimitiveClass              = 81
   0,                                        // TR_ValidateMethodFromInlinedSite       = 82
   0,                                        // TR_ValidateMethodByName                = 83
   0,                                        // TR_ValidateMethodFromClass             = 84
   0,                                        // TR_ValidateStaticMethodFromCP          = 85
   0,                                        // TR_ValidateSpecialMethodFromCP         = 86
   0,                                        // TR_ValidateVirtualMethodFromCP         = 87
   0,                                        // TR_ValidateVirtualMethodFromOffset     = 88
   0,                                        // TR_ValidateInterfaceMethodFromCP       = 89
   0,                                        // TR_ValidateMethodFromClassAndSig       = 90
   0,                                        // TR_ValidateStackWalkerMaySkipFramesRecord = 91
   0,                                        // TR_ValidateArrayClassFromJavaVM        = 92
   0,                                        // TR_ValidateClassInfoIsInitialized      = 93
   0,                                        // TR_ValidateMethodFromSingleImplementer = 94
   0,                                        // TR_ValidateMethodFromSingleInterfaceImplementer = 95
   0,                                        // TR_ValidateMethodFromSingleAbstractImplementer = 96
   0,                                        // TR_ValidateImproperInterfaceMethodFromCP = 97
   0,                                        // TR_SymbolFromManager                   = 98
   0,                                        // TR_MethodCallAddress                   = 99
   0,                                        // TR_DiscontiguousSymbolFromManager      = 100
   0                                         // TR_ResolvedTrampolines                 = 101
   };
