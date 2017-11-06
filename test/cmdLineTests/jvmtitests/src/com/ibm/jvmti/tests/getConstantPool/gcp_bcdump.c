/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#if 0

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
//#include "util_internal.h"
//#include "bcdump.h"


const char * const maciek_JavaBCNames[] = {
"JBnop" /* 0 */,
"JBaconstnull" /* 1 */,
"JBiconstm1" /* 2 */,
"JBiconst0" /* 3 */,
"JBiconst1" /* 4 */,
"JBiconst2" /* 5 */,
"JBiconst3" /* 6 */,
"JBiconst4" /* 7 */,
"JBiconst5" /* 8 */,
"JBlconst0" /* 9 */,
"JBlconst1" /* 10 */,
"JBfconst0" /* 11 */,
"JBfconst1" /* 12 */,
"JBfconst2" /* 13 */,
"JBdconst0" /* 14 */,
"JBdconst1" /* 15 */,
"JBbipush" /* 16 */,
"JBsipush" /* 17 */,
"JBldc" /* 18 */,
"JBldcw" /* 19 */,
"JBldc2lw" /* 20 */,
"JBiload" /* 21 */,
"JBlload" /* 22 */,
"JBfload" /* 23 */,
"JBdload" /* 24 */,
"JBaload" /* 25 */,
"JBiload0" /* 26 */,
"JBiload1" /* 27 */,
"JBiload2" /* 28 */,
"JBiload3" /* 29 */,
"JBlload0" /* 30 */,
"JBlload1" /* 31 */,
"JBlload2" /* 32 */,
"JBlload3" /* 33 */,
"JBfload0" /* 34 */,
"JBfload1" /* 35 */,
"JBfload2" /* 36 */,
"JBfload3" /* 37 */,
"JBdload0" /* 38 */,
"JBdload1" /* 39 */,
"JBdload2" /* 40 */,
"JBdload3" /* 41 */,
"JBaload0" /* 42 */,
"JBaload1" /* 43 */,
"JBaload2" /* 44 */,
"JBaload3" /* 45 */,
"JBiaload" /* 46 */,
"JBlaload" /* 47 */,
"JBfaload" /* 48 */,
"JBdaload" /* 49 */,
"JBaaload" /* 50 */,
"JBbaload" /* 51 */,
"JBcaload" /* 52 */,
"JBsaload" /* 53 */,
"JBistore" /* 54 */,
"JBlstore" /* 55 */,
"JBfstore" /* 56 */,
"JBdstore" /* 57 */,
"JBastore" /* 58 */,
"JBistore0" /* 59 */,
"JBistore1" /* 60 */,
"JBistore2" /* 61 */,
"JBistore3" /* 62 */,
"JBlstore0" /* 63 */,
"JBlstore1" /* 64 */,
"JBlstore2" /* 65 */,
"JBlstore3" /* 66 */,
"JBfstore0" /* 67 */,
"JBfstore1" /* 68 */,
"JBfstore2" /* 69 */,
"JBfstore3" /* 70 */,
"JBdstore0" /* 71 */,
"JBdstore1" /* 72 */,
"JBdstore2" /* 73 */,
"JBdstore3" /* 74 */,
"JBastore0" /* 75 */,
"JBastore1" /* 76 */,
"JBastore2" /* 77 */,
"JBastore3" /* 78 */,
"JBiastore" /* 79 */,
"JBlastore" /* 80 */,
"JBfastore" /* 81 */,
"JBdastore" /* 82 */,
"JBaastore" /* 83 */,
"JBbastore" /* 84 */,
"JBcastore" /* 85 */,
"JBsastore" /* 86 */,
"JBpop" /* 87 */,
"JBpop2" /* 88 */,
"JBdup" /* 89 */,
"JBdupx1" /* 90 */,
"JBdupx2" /* 91 */,
"JBdup2" /* 92 */,
"JBdup2x1" /* 93 */,
"JBdup2x2" /* 94 */,
"JBswap" /* 95 */,
"JBiadd" /* 96 */,
"JBladd" /* 97 */,
"JBfadd" /* 98 */,
"JBdadd" /* 99 */,
"JBisub" /* 100 */,
"JBlsub" /* 101 */,
"JBfsub" /* 102 */,
"JBdsub" /* 103 */,
"JBimul" /* 104 */,
"JBlmul" /* 105 */,
"JBfmul" /* 106 */,
"JBdmul" /* 107 */,
"JBidiv" /* 108 */,
"JBldiv" /* 109 */,
"JBfdiv" /* 110 */,
"JBddiv" /* 111 */,
"JBirem" /* 112 */,
"JBlrem" /* 113 */,
"JBfrem" /* 114 */,
"JBdrem" /* 115 */,
"JBineg" /* 116 */,
"JBlneg" /* 117 */,
"JBfneg" /* 118 */,
"JBdneg" /* 119 */,
"JBishl" /* 120 */,
"JBlshl" /* 121 */,
"JBishr" /* 122 */,
"JBlshr" /* 123 */,
"JBiushr" /* 124 */,
"JBlushr" /* 125 */,
"JBiand" /* 126 */,
"JBland" /* 127 */,
"JBior" /* 128 */,
"JBlor" /* 129 */,
"JBixor" /* 130 */,
"JBlxor" /* 131 */,
"JBiinc" /* 132 */,
"JBi2l" /* 133 */,
"JBi2f" /* 134 */,
"JBi2d" /* 135 */,
"JBl2i" /* 136 */,
"JBl2f" /* 137 */,
"JBl2d" /* 138 */,
"JBf2i" /* 139 */,
"JBf2l" /* 140 */,
"JBf2d" /* 141 */,
"JBd2i" /* 142 */,
"JBd2l" /* 143 */,
"JBd2f" /* 144 */,
"JBi2b" /* 145 */,
"JBi2c" /* 146 */,
"JBi2s" /* 147 */,
"JBlcmp" /* 148 */,
"JBfcmpl" /* 149 */,
"JBfcmpg" /* 150 */,
"JBdcmpl" /* 151 */,
"JBdcmpg" /* 152 */,
"JBifeq" /* 153 */,
"JBifne" /* 154 */,
"JBiflt" /* 155 */,
"JBifge" /* 156 */,
"JBifgt" /* 157 */,
"JBifle" /* 158 */,
"JBificmpeq" /* 159 */,
"JBificmpne" /* 160 */,
"JBificmplt" /* 161 */,
"JBificmpge" /* 162 */,
"JBificmpgt" /* 163 */,
"JBificmple" /* 164 */,
"JBifacmpeq" /* 165 */,
"JBifacmpne" /* 166 */,
"JBgoto" /* 167 */,
"JBunimplemented" /* 168 */,
"JBunimplemented" /* 169 */,
"JBtableswitch" /* 170 */,
"JBlookupswitch" /* 171 */,
"JBreturn0" /* 172 */,
"JBreturn1" /* 173 */,
"JBreturn2" /* 174 */,
"JBsyncReturn0" /* 175 */,
"JBsyncReturn1" /* 176 */,
"JBsyncReturn2" /* 177 */,
"JBgetstatic" /* 178 */,
"JBputstatic" /* 179 */,
"JBgetfield" /* 180 */,
"JBputfield" /* 181 */,
"JBinvokevirtual" /* 182 */,
"JBinvokespecial" /* 183 */,
"JBinvokestatic" /* 184 */,
"JBinvokeinterface" /* 185 */,
"JBunimplemented" /* 186 */,
"JBnew" /* 187 */,
"JBnewarray" /* 188 */,
"JBanewarray" /* 189 */,
"JBarraylength" /* 190 */,
"JBathrow" /* 191 */,
"JBcheckcast" /* 192 */,
"JBinstanceof" /* 193 */,
"JBmonitorenter" /* 194 */,
"JBmonitorexit" /* 195 */,
"JBunimplemented" /* 196 */,
"JBmultianewarray" /* 197 */,
"JBifnull" /* 198 */,
"JBifnonnull" /* 199 */,
"JBgotow" /* 200 */,
"JBunimplemented" /* 201 */,
"JBbreakpoint" /* 202 */,
"JBiloadw" /* 203 */,
"JBlloadw" /* 204 */,
"JBfloadw" /* 205 */,
"JBdloadw" /* 206 */,
"JBaloadw" /* 207 */,
"JBistorew" /* 208 */,
"JBlstorew" /* 209 */,
"JBfstorew" /* 210 */,
"JBdstorew" /* 211 */,
"JBastorew" /* 212 */,
"JBiincw" /* 213 */,
"JBunimplemented" /* 214 */,
"JBaload0getfield" /* 215 */,
"JBunimplemented" /* 216 */,
"JBunimplemented" /* 217 */,
"JBunimplemented" /* 218 */,
"JBunimplemented" /* 219 */,
"JBunimplemented" /* 220 */,
"JBunimplemented" /* 221 */,
"JBunimplemented" /* 222 */,
"JBunimplemented" /* 223 */,
"JBunimplemented" /* 224 */,
"JBunimplemented" /* 225 */,
"JBunimplemented" /* 226 */,
"JBunimplemented" /* 227 */,
"JBreturnFromConstructor" /* 228 */,
"JBgenericReturn" /* 229 */,
"JBunimplemented" /* 230 */,
"JBinvokeinterface2" /* 231 */,
"JBunimplemented" /* 232 */,
"JBunimplemented" /* 233 */,
"JBunimplemented" /* 234 */,
"JBunimplemented" /* 235 */,
"JBunimplemented" /* 236 */,
"JBunimplemented" /* 237 */,
"JBunimplemented" /* 238 */,
"JBunimplemented" /* 239 */,
"JBunimplemented" /* 240 */,
"JBunimplemented" /* 241 */,
"JBunimplemented" /* 242 */,
"JBreturnToMicroJIT" /* 243 */,
"JBretFromNative0" /* 244 */,
"JBretFromNative1" /* 245 */,
"JBretFromNativeF" /* 246 */,
"JBretFromNativeD" /* 247 */,
"JBretFromNativeJ" /* 248 */,
"JBldc2dw" /* 249 */,
"JBasyncCheck" /* 250 */,
"JBunimplemented" /* 251 */,
"JBunimplemented" /* 252 */,
"JBunimplemented" /* 253 */,
"JBimpdep1" /* 254 */,
"JBimpdep2" /* 255 */
};

#define CF_ALLOC_INCREMENT 4096
#define OPCODE_RELATIVE_BRANCHES 1

typedef void (*DISASSEMBLY_PRINT_FN)(void *userData, char *format, ...);

static void maciek_cfdumpBytecodePrintFunction(void *userData, char *format, ...);



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

#define UTF_MAX_LENGTH 128
#define DUMP_UTF(val) \
		outputFunction(userData, \
			((((J9UTF8*)(val))->length > UTF_MAX_LENGTH) ? "%.*s..." : "%.*s"), \
			((((J9UTF8*)(val))->length > UTF_MAX_LENGTH) ? UTF_MAX_LENGTH : ((J9UTF8*)(val))->length), \
			(((char *)(val))+2))


IDATA
maciek_j9bcutil_dumpBytecodes(J9PortLibrary * portLib, U_8 * bytecodes, UDATA walkStartPC, UDATA walkEndPC,
			      UDATA flags, void *printFunction, void *userData, char *indent)
{
	PORT_ACCESS_FROM_PORT(portLib);
	DISASSEMBLY_PRINT_FN outputFunction = (DISASSEMBLY_PRINT_FN) printFunction;

	I_32 i;
	U_8 *bcIndex;
	I_32 pc, index, target, start;
	I_32 low, high;
	U_32 npairs;
	U_8 bc;
	U_32 bigEndian = flags & BCT_BigEndianOutput;

	pc = walkStartPC;

	
	bcIndex = &bytecodes[pc];
	while ((U_32) pc <= walkEndPC) {
		_GETNEXT_U8(bc, bcIndex);
		outputFunction(userData, "%s%5i %s ", indent, pc, maciek_JavaBCNames[bc]);
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
			outputFunction(userData, "%i \n", index);
			break;

		case JBldc2lw:
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i \n", index);
#if 0			
			info = &constantPool[index];
			outputFunction(userData, "(long) 0x%08X%08X\n",
				bigEndian ? info->slot1 : info->slot2,
				bigEndian ? info->slot2 : info->slot1);
#endif			
			pc += 2;
			break;

		case JBldc2dw:
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i \n", index);
#if 0			
			info = &constantPool[index];
			/* this will print incorrectly on Linux ARM! FIX ME! */
			outputFunction(userData, "(double) 0x%08X%08X\n", 
				bigEndian ? info->slot1 : info->slot2,
				bigEndian ? info->slot2 : info->slot1);
#endif			
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
			outputFunction(userData, "%i \n", index);
#if 0			
			info = &constantPool[index];
			DUMP_UTF(J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMFieldRef *) info)->classRefCPIndex]));	/* dump declaringClassName */
			nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) info);
			outputFunction(userData, ".");
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig));	/* dump name */
			outputFunction(userData, " ");
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig));	/* dump signature */
			outputFunction(userData, "\n");
#endif			
			pc += 2;
			break;

		case JBinvokevirtual:
		case JBinvokespecial:
		case JBinvokestatic:
		case JBinvokeinterface:
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i \n", index);
#if 0			
			info = &constantPool[index];           
			DUMP_UTF(J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMMethodRef *) info)->classRefCPIndex]));	/* dump declaringClassName */
			nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info);
			outputFunction(userData, ".");
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig));	/* dump name */
			DUMP_UTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig));	/* dump signature */
			outputFunction(userData, "\n");
#endif
			pc += 2;
			break;

		case JBnew:
		case JBanewarray:
		case JBcheckcast:
		case JBinstanceof:
			_GETNEXT_U16(index, bcIndex);
			outputFunction(userData, "%i \n", index);
#if 0			
			info = &constantPool[index];
			DUMP_UTF(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info));	/* dump name */
			outputFunction(userData, "\n");
#endif			
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
			outputFunction(userData, "%i ", index);
			_GETNEXT_U8(index, bcIndex);
			outputFunction(userData, "dims %i \n", index);
#if 0			
			info = &constantPool[index];
			DUMP_UTF(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info));	/* dump name */
			outputFunction(userData, "\n");
#endif			
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



I_32 maciek_dumpBytecodes(J9PortLibrary * portLib, U_8 * bytecodes, UDATA walkStartPC, UDATA walkEndPC, UDATA flags)
{
	return maciek_j9bcutil_dumpBytecodes(portLib, bytecodes, walkStartPC, walkEndPC, 
					     flags, (void *) maciek_cfdumpBytecodePrintFunction, portLib, "");
}



static void maciek_cfdumpBytecodePrintFunction(void *userData, char *format, ...)
{
	J9PortLibrary *portLib = (J9PortLibrary *) userData;
	va_list args;
	char outputBuffer[512];
	PORT_ACCESS_FROM_PORT(portLib);

	va_start(args, format);
	j9str_vprintf(PORTLIB, outputBuffer, 512, format, args);
	va_end(args);

	j9tty_printf(PORTLIB, "%s", outputBuffer);
}

#else

/* to avoid warnings */
void gcp_bcdump_stub(void) {
}

#endif
