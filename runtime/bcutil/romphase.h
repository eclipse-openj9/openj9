/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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


#ifndef romphase_h
#define romphase_h

/* @ddr_namespace: default */
/*
 * ROMClassCreationPhase is an enumeration of phases in the ROM class creation process.
 * Any changes to this enum should be reflected in the strings in ROMClassCreationContext::printVerbosePhase.
 * A basic assumption is that nested phases are strictly greater than their parent phase.
 */
enum ROMClassCreationPhase {
	ROMClassCreation = 0,
	ROMClassTranslation,
	CompareHashtableROMClass,
	CompareSharedROMClass,
	CreateSharedClass,
	WalkUTF8sAndMarkInterns,
	MarkAndCountUTF8s,
	VisitUTF8Block,
	WriteUTF8s,
	PrepareUTF8sAfterInternsMarked,
	ComputeExtraModifiers,
	ComputeOptionalFlags,
	ROMClassPrepareAndLayDown,
	PrepareROMClass,
	LayDownROMClass,
	ClassFileAnalysis,
	ClassFileHeaderAnalysis,
	ClassFileFieldsAnalysis,
	ClassFileAttributesAnalysis,
	ClassFileInterfacesAnalysis,
	ClassFileMethodsAnalysis,
	ClassFileMethodAttributesAnalysis,
	ClassFileAnnotationsAnalysis,
	ClassFileAnnotationElementAnalysis,
	ComputeSendSlotCount,
	ClassFileMethodThrownExceptionsAnalysis,
	ClassFileMethodCodeAttributeAnalysis,
	ClassFileMethodCodeAttributeAttributesAnalysis,
	ClassFileMethodMethodParametersAttributeAnalysis,
	CompressLineNumbers,
	ClassFileMethodCodeAttributeCaughtExceptionsAnalysis,
	ClassFileMethodCodeAttributeCodeAnalysis,
	CheckForLoop,
	ClassFileStackMapSlotsAnalysis,
	MethodIsFinalize,
	MethodIsEmpty,
	MethodIsForwarder,
	MethodIsGetter,
	MethodIsVirtual,
	MethodIsNonStaticNonAbstract,
	MethodIsObjectConstructor,
	ShouldConvertInvokeVirtualToInvokeSpecial,
	ParseClassFile,
	ParseClassFileConstantPool,
	ParseClassFileFields,
	ParseClassFileMethods,
	ParseClassFileAttributes,
	ParseClassFileCheckClass,
	ParseClassFileVerifyClass,
	ParseClassFileInlineJSRs,
	ConstantPoolMapping,
	SRPOffsetTableCreation,
	ROMClassCreationPhaseCount
};

#endif /* romphase_h */
