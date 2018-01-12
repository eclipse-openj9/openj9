/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "ROMClassCreationContext.hpp"

extern "C" void
romVerboseRecordPhaseStart(void *verboseContext, UDATA phase)
{
	ROMClassCreationContext *context = (ROMClassCreationContext*)verboseContext;
	context->recordPhaseStart(ROMClassCreationPhase(phase));
}

extern "C" void
romVerboseRecordPhaseEnd(void *verboseContext, UDATA phase)
{
	ROMClassCreationContext *context = (ROMClassCreationContext*)verboseContext;
	context->recordPhaseEnd(ROMClassCreationPhase(phase), OK);
}

void
ROMClassCreationContext::verbosePrintPhase(ROMClassCreationPhase phase, bool *printedPhases, UDATA indent)
{
	static const char *verbosePhaseName[] = { /* Ensure this matches ROMClassCreationPhase */
		"ROMClassCreation",
		"ROMClassTranslation",
		"CompareHashtableROMClass",
		"CompareSharedROMClass",
		"CreateSharedClass",
		"WalkUTF8sAndMarkInterns",
		"MarkAndCountUTF8s",
		"VisitUTF8Block",
		"WriteUTF8s",
		"PrepareUTF8sAfterInternsMarked",
		"ComputeExtraModifiers",
		"ComputeOptionalFlags",
		"ROMClassPrepareAndLayDown",
		"PrepareROMClass",
		"LayDownROMClass",
		"ClassFileAnalysis",
		"ClassFileHeaderAnalysis",
		"ClassFileFieldsAnalysis",
		"ClassFileAttributesAnalysis",
		"ClassFileInterfacesAnalysis",
		"ClassFileMethodsAnalysis",
		"ClassFileMethodAttributesAnalysis",
		"ClassFileAnnotationsAnalysis",
		"ClassFileAnnotationElementAnalysis",
		"ComputeSendSlotCount",
		"ClassFileMethodThrownExceptionsAnalysis",
		"ClassFileMethodCodeAttributeAnalysis",
		"ClassFileMethodCodeAttributeAttributesAnalysis",
		"ClassFileMethodMethodParametersAttributeAnalysis",
		"CompressLineNumbers",
		"ClassFileMethodCodeAttributeCaughtExceptionsAnalysis",
		"ClassFileMethodCodeAttributeCodeAnalysis",
		"CheckForLoop",
		"ClassFileStackMapSlotsAnalysis",
		"MethodIsFinalize",
		"MethodIsEmpty",
		"MethodIsForwarder",
		"MethodIsGetter",
		"MethodIsVirtual",
		"MethodIsObjectConstructor",
		"ShouldConvertInvokeVirtualToInvokeSpecial",
		"ParseClassFile",
		"ParseClassFileConstantPool",
		"ParseClassFileFields",
		"ParseClassFileMethods",
		"ParseClassFileAttributes",
		"ParseClassFileCheckClass",
		"ParseClassFileVerifyClass",
		"ParseClassFileInlineJSRs",
		"ConstantPoolMapping",
		"SRPOffsetTableCreation"
	};

	if (!printedPhases[phase]) {
		printedPhases[phase] = true;
		if ((0 != _verboseRecords[phase].accumulatedTime) || (0 != _verboseRecords[phase].failureTime) || (OK != _verboseRecords[phase].buildResult)) {
			bool printCompactElement = (0 == _verboseRecords[phase].failureTime) && (OK == _verboseRecords[phase].buildResult);
			ROMClassCreationPhase nested = ROMClassCreationPhase(phase + 1);
			/*
			 * Look for a nested phase.
			 * Postcondition: printCompactElement || (phase != _verboseRecords[i].parentPhase where i < nested && i > phase)
			 */
			while (printCompactElement && (nested < ROMClassCreationPhaseCount)) {
				if (phase == _verboseRecords[nested].parentPhase) {
					printCompactElement = false;
				} else {
					++nested;
				}
			}

			PORT_ACCESS_FROM_PORT(_portLibrary);
			if (printCompactElement) {
				j9tty_printf(_portLibrary, "% *c<phase name=\"%s\" totalusec=\"%i\" />\n", indent, ' ', verbosePhaseName[phase], _verboseRecords[phase].accumulatedTime);
			} else {
				j9tty_printf(_portLibrary, "% *c<phase name=\"%s\" totalusec=\"%i\">\n", indent, ' ', verbosePhaseName[phase], _verboseRecords[phase].accumulatedTime);
				if (0 != _verboseRecords[phase].failureTime) {
					j9tty_printf(_portLibrary, "% *c<failures totalusec=\"%i\" />\n", indent + 2, ' ', _verboseRecords[phase].failureTime);
				}
				if (OK != _verboseRecords[phase].buildResult) {
					j9tty_printf(_portLibrary, "% *c<result value=\"%i\" />\n", indent + 2, ' ', buildResultString(_verboseRecords[phase].buildResult));
				}

				/*
				 * Print nested phases.
				 * Precondition: phase != _verboseRecords[i].parentPhase where i < nested && i > phase
				 */
				while (nested < ROMClassCreationPhaseCount) {
					if (phase == _verboseRecords[nested].parentPhase) {
						verbosePrintPhase(nested, printedPhases, indent + 2);
					}
					++nested;
				}

				j9tty_printf(_portLibrary, "% *c</phase>\n", indent, ' ');
			}
		}
	}
}

const char *
ROMClassCreationContext::buildResultString(BuildResult result)
{
	switch (result) {
	case OK: return "OK";
	case GenericError: return "GenericError";
	case OutOfROM: return "OutOfROM";
	case ClassRead: return "ClassRead";
	case BytecodeTranslationFailed: return "BytecodeTranslationFailed";
	case StackMapFailed: return "StackMapFailed";
	case InvalidBytecode: return "InvalidBytecode";
	case OutOfMemory: return "OutOfMemory";
	case VerifyErrorInlining: return "VerifyErrorInlining";
	case NeedWideBranches: return "NeedWideBranches";
	case UnknownAnnotation: return "UnknownAnnotation";
	case ClassNameMismatch: return "ClassNameMismatch";
	case InvalidAnnotation: return "InvalidAnnotation";
	default: return "Unknown";
	}
}

void
ROMClassCreationContext::reportVerboseStatistics()
{
	bool printedPhases[ROMClassCreationPhaseCount] = { false, }; // TODO check correct initialization syntax

	PORT_ACCESS_FROM_PORT(_portLibrary);
	j9tty_printf(_portLibrary, "<romclass name=\"%.*s\" result=\"%s\">\n", _classNameLength, _className, buildResultString(_buildResult));
	for (ROMClassCreationPhase phase = ROMClassCreation; phase != ROMClassCreationPhaseCount; ++phase) {
		verbosePrintPhase(phase, printedPhases, 2);
	}
	if (0 != _verboseOutOfMemoryCount) {
		j9tty_printf(_portLibrary, "  <oom count=\"%i\" lastBufferSizeExceeded=\"%i\" />\n", _verboseOutOfMemoryCount, _verboseLastBufferSizeExceeded);

	}
	j9tty_printf(_portLibrary, "</romclass>\n");
}

J9ROMMethod *
ROMClassCreationContext::romMethodFromOffset(IDATA offset)
{
	J9ROMMethod *returnMethod = NULL;
	if (NULL != _romClass) {
		UDATA romclassaddr = ((UDATA)_romClass) + offset;
		J9ROMMethod *currentMethod = J9ROMCLASS_ROMMETHODS(_romClass);

		for (UDATA i = 0; i < _romClass->romMethodCount; i++) {
			/* There's no easy way to get the exact end of a ROMMethod, so iterate
			 * thru the methods until we get to the last one.
			 *
			 * TODO:
			 * - If a bad & large address is being looked for the last method
			 *   will be returned.
			 * - If a bad & small address is being looked for then null will be returned.
			 * - A function similar to nextROMMethod should maybe written to replace this logic,
			 *   except have it return the end addr of the method.
			 */
			if (romclassaddr >= (UDATA)currentMethod) {
				returnMethod = currentMethod;
				currentMethod = nextROMMethod(currentMethod);
				continue;
			} else {
				break;
			}
		}
	}
	return returnMethod;
}
