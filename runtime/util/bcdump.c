/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cfreader.h"
#include "j9.h"

#include "pcstack.h"
#include "jbcmap.h"
#include "bcsizes.h"
#include "bcnames.h"
#include "j9port.h"
#include "j9protos.h"
#include "rommeth.h"
#include "util_internal.h"
#include "bcdump.h"
#include "stackmap_api.h"

#define CF_ALLOC_INCREMENT 4096
#define OPCODE_RELATIVE_BRANCHES 1

typedef void (* DISASSEMBLY_PRINT_FN)(void *userData, char *format, ...);

static void cfdumpBytecodePrintFunction (void *userData, char *format, ...);


IDATA j9bcutil_dumpBytecodes(J9PortLibrary * portLib, J9ROMClass * romClass,
							 U_8 * bytecodes, UDATA walkStartPC, UDATA walkEndPC,
							 UDATA flags, void *printFunction, void *userData, char *indent)
{
	PORT_ACCESS_FROM_PORT(portLib);
	DISASSEMBLY_PRINT_FN outputFunction = (DISASSEMBLY_PRINT_FN) printFunction;

#define _NEXT_LE_U16(value, index) ((value = (U_16)index[0] | ((U_16)index[1] << 8)), index += 2, value)
#define _NEXT_LE_U32(value, index) ((value = (U_32)index[0] | ((U_32)index[1] << 8) | ((U_32)index[2] << 16) | ((U_32)index[3] << 24)), index += 4, value)
#define _NEXT_BE_U16(value, index) ((value = ((U_16)index[0] << 8) | index[1]), index += 2, value)
#define _NEXT_BE_U32(value, index) ((value = ((U_32)index[0] << 24) | ((U_32)index[1] << 16) | ((U_32)index[2] << 8) | (U_32)index[3]), index += 4, value)

#define _GETNEXT_U8(value, index) (value = *(index++))

#define _GETNEXT_U16(value, index) \
	if (bigEndian) { \
		_NEXT_BE_U16(value, index); \
	} else { \
		_NEXT_LE_U16(value, index); \
	} \

#define _GETNEXT_U32(value, index) \
	if (bigEndian) { \
		_NEXT_BE_U32(value, index); \
	} else { \
		_NEXT_LE_U32(value, index); \
	} \

#define UTF_MAX_LENGTH 256
#define DUMP_UTF(val) \
		outputFunction(userData, \
			((J9UTF8_LENGTH(val) > UTF_MAX_LENGTH) ? "%.*s..." : "%.*s"), \
			((J9UTF8_LENGTH(val) > UTF_MAX_LENGTH) ? UTF_MAX_LENGTH : J9UTF8_LENGTH(val)), \
			(((char *)(val))+2))

#define LOCAL_MAP 0
#define DEBUG_MAP 1
#define STACK_MAP 2
#define MAP_COUNT 3

	I_32 i, j;
	U_8 *bcIndex;
	UDATA pc, index, target, start;
	I_32 low, high;
	U_32 npairs;
	U_8 bc;
	J9ROMMethod * romMethod = (J9ROMMethod *) (bytecodes - sizeof(J9ROMMethod));
	J9ROMConstantPoolItem *info;
	J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
	J9ROMNameAndSignature *nameAndSig;
	J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
	UDATA bigEndian = flags & BCT_BigEndianOutput;
	IDATA result;
	U_32 resultArray[8192];
	U_32 localsCount = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
	char environment[128] = "\0";
	BOOLEAN envVarDefined = FALSE;

	if (0 == j9sysinfo_get_env("j9bcutil_dumpBytecodes", environment, sizeof(environment))) {
		envVarDefined = TRUE; 
	}
	
	pc = walkStartPC;

	bcIndex = &bytecodes[pc];
	while ((U_32) pc <= walkEndPC) {
		if (flags & BCT_DumpMaps) {
			for(j = LOCAL_MAP; j < MAP_COUNT; j++) {
				UDATA wrapLine = 0;
				UDATA outputCount;
				U_8 mapChar;
				
				if (envVarDefined && (pc != atoi(environment))) {
					continue;
				}
				
				switch (j) {
				case LOCAL_MAP:
					result = j9localmap_LocalBitsForPC(portLib, romClass, romMethod, pc, resultArray,
					                NULL, NULL, NULL);
					mapChar = 'l';
					outputCount = localsCount;
					break;
				case DEBUG_MAP:
					result = j9localmap_DebugLocalBitsForPC(portLib, romClass, romMethod, pc, resultArray,
					                NULL, NULL, NULL);
					mapChar = 'd';
					outputCount = localsCount;
					break;
				case STACK_MAP:
					/* First call is to get the stack depth */
					result = j9stackmap_StackBitsForPC(portLib, pc, romClass, romMethod, NULL, 0,
					                NULL, NULL, NULL);
					result = j9stackmap_StackBitsForPC(portLib, pc, romClass, romMethod, resultArray, result,
					                NULL, NULL, NULL);
					mapChar = 's';
					outputCount = result;
					break;
				}
				
				if (outputCount == 0) {
					outputFunction(userData, "               %cmap [empty]", mapChar);
				} else {
					outputFunction(userData, "               %cmap [%5d] =", mapChar, outputCount);
				}
				for (i = 0; i < (I_32) outputCount; i++) {
					UDATA x = i / 32;
					if ((i % 8) == 0) {
						outputFunction(userData, " ");
						if (wrapLine) {
							outputFunction(userData, "\n                              ");
						}
					}
					outputFunction(userData, "%d ", resultArray[x] & 1);
					resultArray[x] >>= 1;
					wrapLine = (((i + 1) % 32) == 0);
				}
				outputFunction(userData, "\n");
			}
		}
		_GETNEXT_U8(bc, bcIndex);
		outputFunction(userData, "%s%5i %s ", indent, pc, JavaBCNames[bc]);
		start = pc;
		pc++;
		switch (bc) {
		case JBbipush:
			_GETNEXT_U8(index, bcIndex);
			outputFunction(userData, "%i\n", (I_8) index);
			pc++;
			break;

		case JBsipush:
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i\n", (I_16) index);
			pc += 2;
			break;

		case JBldc:
		case JBldcw:
			if (bc == JBldc) {
				_GETNEXT_U8(index, bcIndex);
				pc++;
			} else {
				_GETNEXT_U16(index, bcIndex);
				pc += 2;
			}
			outputFunction(userData, "%i ", index);
			info = &constantPool[index];

			if (((J9ROMSingleSlotConstantRef *) info)->cpType != BCT_J9DescriptionCpTypeScalar) {
				/* this is a string or class constant or methodhandle or methodtype */
				U_32 cpType = ((J9ROMSingleSlotConstantRef *) info)->cpType & BCT_J9DescriptionCpTypeMask;
				switch (cpType) {
				case BCT_J9DescriptionCpTypeClass:
					/* ClassRef */
					outputFunction(userData, "(java.lang.Class) ");
					DUMP_UTF(J9ROMCLASSREF_NAME((J9ROMClassRef *) info));
					break;
				case BCT_J9DescriptionCpTypeObject:
					/* StringRef */
					outputFunction(userData, "(java.lang.String) \"");
					DUMP_UTF(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info));
					break;
				case BCT_J9DescriptionCpTypeMethodType:
					/* MethodTypeRef */
					outputFunction(userData, "(java.dyn.MethodType) \"");
					DUMP_UTF(J9ROMMETHODTYPEREF_SIGNATURE((J9ROMMethodTypeRef *) info));
					break;
				case BCT_J9DescriptionCpTypeMethodHandle:
					/* MethodHandleRef */
					outputFunction(userData, "(java.dyn.MethodHandle) {0x%X, 0x%X}\"", ((J9ROMMethodHandleRef *) info)->methodOrFieldRefIndex, ((J9ROMMethodHandleRef *) info)->handleTypeAndCpType);
					break;
				}
				outputFunction(userData, "\n");
			} else {
				/* this is a float/int constant */
				outputFunction(userData, "(int/float) 0x%08X\n", ((J9ROMSingleSlotConstantRef *) info)->data);
			}
			break;

		case JBldc2lw:
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i ", index);
			info = &constantPool[index];
			outputFunction(userData, "(long) 0x%08X%08X\n",
				bigEndian ? info->slot1 : info->slot2,
				bigEndian ? info->slot2 : info->slot1);
			pc += 2;
			break;

		case JBldc2dw:
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i ", index);
			info = &constantPool[index];
			/* this will print incorrectly on Linux ARM! FIX ME! */
			outputFunction(userData, "(double) 0x%08X%08X\n",
				bigEndian ? info->slot1 : info->slot2,
				bigEndian ? info->slot2 : info->slot1);
			pc += 2;
			break;

		case JBiload:
		case JBlload:
		case JBfload:
		case JBdload:
		case JBaload:
		case JBistore:
		case JBlstore:
		case JBfstore:
		case JBdstore:
		case JBastore:
			_GETNEXT_U8(index, bcIndex);
			pc++;
			outputFunction(userData, "%i\n", index);
			break;

		case JBiloadw:
		case JBlloadw:
		case JBfloadw:
		case JBdloadw:
		case JBaloadw:
		case JBistorew:
		case JBlstorew:
		case JBfstorew:
		case JBdstorew:
		case JBastorew:
			_GETNEXT_U16(index, bcIndex);
			bcIndex++;
			pc += 3;
			outputFunction(userData, "%i\n", index);
			break;

		case JBiinc:
			_GETNEXT_U8(index, bcIndex);
			outputFunction(userData, "%i ", index);
			_GETNEXT_U8(target, bcIndex);
			outputFunction(userData, "%i\n", (I_8) target);
			pc += 2;
			break;

		case JBiincw:
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i ", index);
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i\n", (I_16) index);
			bcIndex++;
			pc += 5;
			break;

		case JBifeq:
		case JBifne:
		case JBiflt:
		case JBifge:
		case JBifgt:
		case JBifle:
		case JBificmpeq:
		case JBificmpne:
		case JBificmplt:
		case JBificmpge:
		case JBificmpgt:
		case JBificmple:
		case JBifacmpeq:
		case JBifacmpne:
		case JBgoto:
		case JBifnull:
		case JBifnonnull:
			_GETNEXT_U16(index, bcIndex);
			pc += 2;
#if OPCODE_RELATIVE_BRANCHES
			target = start + (I_16) index;
#else
			target = pc + (I_16) index;
#endif
			outputFunction(userData, "%i\n", target);
			break;

		case JBtableswitch:
			switch (start % 4) {
			case 0:
				bcIndex++;
				pc++;
			case 1:
				bcIndex++;
				pc++;
			case 2:
				bcIndex++;
				pc++;
			case 3:
				break;
			}
			_GETNEXT_U32(index, bcIndex);
			target = start + index;
			_GETNEXT_U32(index, bcIndex);
			low = (I_32) index;
			_GETNEXT_U32(index, bcIndex);
			high = (I_32) index;
			pc += 12;
			outputFunction(userData, "low %i high %i\n", low, high);
			outputFunction(userData, "        default %10i\n", target);
			npairs = high - low + 1;
			for (i = 0; i < (I_32) npairs; i++) {
				_GETNEXT_U32(index, bcIndex);
				target = start + index;
				outputFunction(userData, "     %10i %10i\n", i + low, target);
				pc += 4;
			}
			break;

		case JBlookupswitch:
			switch (start % 4) {
			case 0:
				bcIndex++;
				pc++;
			case 1:
				bcIndex++;
				pc++;
			case 2:
				bcIndex++;
				pc++;
			case 3:
				break;
			}
			_GETNEXT_U32(index, bcIndex);
			target = start + index;
			_GETNEXT_U32(npairs, bcIndex);
			outputFunction(userData, "pairs %i\n", npairs);
			outputFunction(userData, "        default %10i\n", target);
			pc += 8;
			for (i = 0; i < (I_32) npairs; i++) {
				_GETNEXT_U32(index, bcIndex);
				outputFunction(userData, "     %10i", index);
				_GETNEXT_U32(index, bcIndex);
				target = start + index;
				outputFunction(userData, " %10i\n", target);
				pc += 8;
			}
			break;

		case JBgetstatic:
		case JBputstatic:
		case JBgetfield:
		case JBputfield:
			_GETNEXT_U16(index, bcIndex);
			info = &constantPool[index];
			outputFunction(userData, "%i ", index);
			DUMP_UTF(J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMFieldRef *) info)->classRefCPIndex]));	/* dump declaringClassName */
			nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) info);
			outputFunction(userData, ".");
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig));	/* dump name */
			outputFunction(userData, " ");
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig));	/* dump signature */
			outputFunction(userData, "\n");
			pc += 2;
			break;

		case JBinvokeinterface2:
			bcIndex++;
			pc++;
			outputFunction(userData, "\n");
			break;

		case JBinvokedynamic:
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i ", index);
			nameAndSig = SRP_PTR_GET(callSiteData + index, J9ROMNameAndSignature*);
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig));	/* dump name */
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig));	/* dump signature */
			{
				U_16 *bsmIndices = (U_16 *) (callSiteData + romClass->callSiteCount);
				U_16 *bsmData = bsmIndices + romClass->callSiteCount;
				U_16 bsmIndex = bsmIndices[index];

				while (bsmIndex != 0) {
					bsmData = bsmData + 2 + bsmData[1];
					bsmIndex--;
				}

				info = &constantPool[bsmData[0]]; /* bootstrap method handle ref */
				info = &constantPool[((J9ROMMethodHandleRef *) info)->methodOrFieldRefIndex];
				outputFunction(userData, " bsm %i ", bsmIndices[index]);
				/* info will be either a field or a method ref - they both have the same shape so we can pretend it is always a methodref */
				DUMP_UTF(J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMMethodRef *) info)->classRefCPIndex]));	/* dump declaringClassName */
				nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info);
				outputFunction(userData, ".");
				DUMP_UTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig));	/* dump name */
				DUMP_UTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig));	/* dump signature */
			}
			outputFunction(userData, "\n");
			pc += 2;
			break;

		case JBinvokevirtual:
		case JBinvokespecial:
		case JBinvokestatic:
		case JBinvokeinterface:
		case JBinvokehandle:
		case JBinvokehandlegeneric:
		case JBinvokestaticsplit:
		case JBinvokespecialsplit:
			_GETNEXT_U16(index, bcIndex);
			if (JBinvokestaticsplit == bc) {
				U_16 *splitTable = J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass);
				index = splitTable[index];
			} else if (JBinvokespecialsplit == bc) {
				U_16 *splitTable = J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass);
				index = splitTable[index];
			}
			info = &constantPool[index];
			outputFunction(userData, "%i ", index);
			DUMP_UTF(J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMMethodRef *) info)->classRefCPIndex]));	/* dump declaringClassName */
			nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info);
			outputFunction(userData, ".");
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig));	/* dump name */
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig));	/* dump signature */
			outputFunction(userData, "\n");
			pc += 2;
			break;

		case JBnew:
		case JBnewdup:
		case JBanewarray:
		case JBcheckcast:
		case JBinstanceof:
			_GETNEXT_U16(index, bcIndex);
			info = &constantPool[index];
			outputFunction(userData, "%i ", index);
			DUMP_UTF(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info));	/* dump name */
			outputFunction(userData, "\n");
			pc += 2;
			break;

		case JBnewarray:
			_GETNEXT_U8(index, bcIndex);
			switch (index) {
			case /* T_BOOLEAN */ 4:
				outputFunction(userData, "boolean\n");
				break;
			case /* T_CHAR */ 5:
				outputFunction(userData, "char\n");
				break;
			case /* T_FLOAT */ 6:
				outputFunction(userData, "float\n");
				break;
			case /* T_DOUBLE */ 7:
				outputFunction(userData, "double\n");
				break;
			case /* T_BYTE */ 8:
				outputFunction(userData, "byte\n");
				break;
			case /* T_SHORT */ 9:
				outputFunction(userData, "short\n");
				break;
			case /* T_INT */ 10:
				outputFunction(userData, "int\n");
				break;
			case /* T_LONG */ 11:
				outputFunction(userData, "long\n");
				break;
			default:
				outputFunction(userData, "(unknown type %i)\n", index);
				break;
			}
			pc++;
			break;

		case JBmultianewarray:
			_GETNEXT_U16(index, bcIndex);
			info = &constantPool[index];
			outputFunction(userData, "%i ", index);
			_GETNEXT_U8(index, bcIndex);
			outputFunction(userData, "dims %i ", index);
			DUMP_UTF(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info));	/* dump name */
			outputFunction(userData, "\n");
			pc += 3;
			break;

		case JBgotow:
			_GETNEXT_U32(index, bcIndex);
			pc += 4;
#if OPCODE_RELATIVE_BRANCHES
			target = start + index;
#else
			target = pc + index;
#endif
			outputFunction(userData, "%i\n", target);
			break;

		default:
			outputFunction(userData, "\n");
			break;
		}
	}
	return BCT_ERR_NO_ERROR;

#undef _NEXT_U8
#undef _NEXT_LE_U16
#undef _NEXT_LE_U32
#undef _NEXT_BE_U16
#undef _NEXT_BE_U32
#undef _GETNEXT_U8
#undef _GETNEXT_U16
#undef _GETNEXT_U32
#undef DUMP_UTF8
#undef UTF_MAX_LENGTH
}



IDATA dumpBytecodes(J9PortLibrary * portLib, J9ROMClass * romClass, J9ROMMethod * romMethod, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA temp, length;

	j9tty_printf(PORTLIB, "  Argument Count: %d\n", J9_ARG_COUNT_FROM_ROM_METHOD(romMethod));
	temp = J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
	j9tty_printf(PORTLIB, "  Temp Count: %d\n", temp);
	j9tty_printf(PORTLIB, "\n");

	length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	if(length == 0) return BCT_ERR_NO_ERROR;		/* catch abstract methods */

	return j9bcutil_dumpBytecodes(portLib, romClass,
								  J9_BYTECODE_START_FROM_ROM_METHOD(romMethod), 0,
								  length - 1, flags,
								  (void *) cfdumpBytecodePrintFunction, portLib, "");
}



static void cfdumpBytecodePrintFunction(void *userData, char *format, ...)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary*)userData);
	va_list args;
	char outputBuffer[512];

	va_start(args, format);
	j9str_vprintf(outputBuffer, 512, format, args);
	va_end(args);

	j9tty_printf(PORTLIB, "%s", outputBuffer);
}




