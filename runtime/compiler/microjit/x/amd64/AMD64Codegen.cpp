/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
#include <string.h>
#include <cstdio>
#include <unordered_map>
#include "j9comp.h"
#include "j9.h"
#include "env/VMJ9.h"
#include "il/Block.hpp"
#include "infra/Cfg.hpp"
#include "compile/Compilation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "control/Recompilation.hpp"
#include "microjit/MethodBytecodeMetaData.hpp"
#include "microjit/x/amd64/AMD64Codegen.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "env/PersistentInfo.hpp"
#include "runtime/J9Runtime.hpp"

#define MJIT_JITTED_BODY_INFO_PTR_SIZE 8
#define MJIT_SAVE_AREA_SIZE 2
#define MJIT_LINKAGE_INFO_SIZE 4
#define GENERATE_SWITCH_TO_INTERP_PREPROLOGUE 1

#define BIT_MASK_32 0xffffffff
#define BIT_MASK_64 0xffffffffffffffff

// Debug Params, comment/uncomment undef to get omit/enable debugging info
// #define MJIT_DEBUG 1
#undef MJIT_DEBUG

#ifdef MJIT_DEBUG
#define MJIT_DEBUG_MAP_PARAMS 1
#else
#undef MJIT_DEBUG_MAP_PARAMS
#endif
#ifdef MJIT_DEBUG
#define MJIT_DEBUG_BC_WALKING 1
#define MJIT_DEBUG_BC_LOG(logFile, text) trfprintf(logFile, text)
#else
#undef MJIT_DEBUG_BC_WALKING
#define MJIT_DEBUG_BC_LOG(logFile, text) do {} while(0)
#endif

#define STACKCHECKBUFFER 512

extern J9_CDATA char * const JavaBCNames[];

#define COPY_TEMPLATE(buffer, INSTRUCTION, size) \
   do { \
         memcpy(buffer, INSTRUCTION, INSTRUCTION##Size); \
         buffer=(buffer+INSTRUCTION##Size); \
         size += INSTRUCTION##Size; \
   } while (0)

#define PATCH_RELATIVE_ADDR_8_BIT(buffer, absAddress) \
   do { \
         intptr_t relativeAddress = (intptr_t)(buffer); \
         intptr_t minAddress = (relativeAddress < (intptr_t)(absAddress)) ? relativeAddress : (intptr_t)(absAddress); \
         intptr_t maxAddress = (relativeAddress > (intptr_t)(absAddress)) ? relativeAddress : (intptr_t)(absAddress); \
         intptr_t absDistance = maxAddress - minAddress; \
         MJIT_ASSERT_NO_MSG(absDistance < (intptr_t)0x00000000000000ff); \
         relativeAddress = (relativeAddress < (intptr_t)(absAddress)) ? absDistance : (-1*(intptr_t)(absDistance)); \
         patchImm1(buffer, (U_32)(relativeAddress & 0x00000000000000ff)); \
   } while (0)

// TODO: Find out how to jump to trampolines for far calls
#define PATCH_RELATIVE_ADDR_32_BIT(buffer, absAddress) \
   do { \
         intptr_t relativeAddress = (intptr_t)(buffer); \
         intptr_t minAddress = (relativeAddress < (intptr_t)(absAddress)) ? relativeAddress : (intptr_t)(absAddress); \
         intptr_t maxAddress = (relativeAddress > (intptr_t)(absAddress)) ? relativeAddress : (intptr_t)(absAddress); \
         intptr_t absDistance = maxAddress - minAddress; \
         MJIT_ASSERT_NO_MSG(absDistance < (intptr_t)0x00000000ffffffff); \
         relativeAddress = (relativeAddress < (intptr_t)(absAddress)) ? absDistance : (-1*(intptr_t)(absDistance)); \
         patchImm4(buffer, (U_32)(relativeAddress & 0x00000000ffffffff)); \
   } while (0)

// Using char[] to signify that we are treating this
// like data, even though they are instructions.
#define EXTERN_TEMPLATE(TEMPLATE_NAME) extern char TEMPLATE_NAME[]
#define EXTERN_TEMPLATE_SIZE(TEMPLATE_NAME) extern unsigned short TEMPLATE_NAME##Size
#define DECLARE_TEMPLATE(TEMPLATE_NAME) EXTERN_TEMPLATE(TEMPLATE_NAME); \
   EXTERN_TEMPLATE_SIZE(TEMPLATE_NAME)

// Labels linked from templates.
DECLARE_TEMPLATE(movRSPR10);
DECLARE_TEMPLATE(movR10R14);
DECLARE_TEMPLATE(subR10Imm4);
DECLARE_TEMPLATE(addR10Imm4);
DECLARE_TEMPLATE(addR14Imm4);
DECLARE_TEMPLATE(subR14Imm4);
DECLARE_TEMPLATE(subRSPImm8);
DECLARE_TEMPLATE(subRSPImm4);
DECLARE_TEMPLATE(subRSPImm2);
DECLARE_TEMPLATE(subRSPImm1);
DECLARE_TEMPLATE(addRSPImm8);
DECLARE_TEMPLATE(addRSPImm4);
DECLARE_TEMPLATE(addRSPImm2);
DECLARE_TEMPLATE(addRSPImm1);
DECLARE_TEMPLATE(jbe4ByteRel);
DECLARE_TEMPLATE(cmpRspRbpDerefOffset);
DECLARE_TEMPLATE(loadEBPOffset);
DECLARE_TEMPLATE(loadESPOffset);
DECLARE_TEMPLATE(loadEAXOffset);
DECLARE_TEMPLATE(loadESIOffset);
DECLARE_TEMPLATE(loadEDXOffset);
DECLARE_TEMPLATE(loadECXOffset);
DECLARE_TEMPLATE(loadEBXOffset);
DECLARE_TEMPLATE(loadEBPOffset);
DECLARE_TEMPLATE(loadESPOffset);
DECLARE_TEMPLATE(loadRAXOffset);
DECLARE_TEMPLATE(loadRSIOffset);
DECLARE_TEMPLATE(loadRDXOffset);
DECLARE_TEMPLATE(loadRCXOffset);
DECLARE_TEMPLATE(loadRBXOffset);
DECLARE_TEMPLATE(loadRBPOffset);
DECLARE_TEMPLATE(loadRSPOffset);
DECLARE_TEMPLATE(loadR9Offset);
DECLARE_TEMPLATE(loadR10Offset);
DECLARE_TEMPLATE(loadR11Offset);
DECLARE_TEMPLATE(loadR12Offset);
DECLARE_TEMPLATE(loadR13Offset);
DECLARE_TEMPLATE(loadR14Offset);
DECLARE_TEMPLATE(loadR15Offset);
DECLARE_TEMPLATE(loadXMM0Offset);
DECLARE_TEMPLATE(loadXMM1Offset);
DECLARE_TEMPLATE(loadXMM2Offset);
DECLARE_TEMPLATE(loadXMM3Offset);
DECLARE_TEMPLATE(loadXMM4Offset);
DECLARE_TEMPLATE(loadXMM5Offset);
DECLARE_TEMPLATE(loadXMM6Offset);
DECLARE_TEMPLATE(loadXMM7Offset);
DECLARE_TEMPLATE(saveEAXOffset);
DECLARE_TEMPLATE(saveESIOffset);
DECLARE_TEMPLATE(saveEDXOffset);
DECLARE_TEMPLATE(saveECXOffset);
DECLARE_TEMPLATE(saveEBXOffset);
DECLARE_TEMPLATE(saveEBPOffset);
DECLARE_TEMPLATE(saveESPOffset);
DECLARE_TEMPLATE(saveRAXOffset);
DECLARE_TEMPLATE(saveRSIOffset);
DECLARE_TEMPLATE(saveRDXOffset);
DECLARE_TEMPLATE(saveRCXOffset);
DECLARE_TEMPLATE(saveRBXOffset);
DECLARE_TEMPLATE(saveRBPOffset);
DECLARE_TEMPLATE(saveRSPOffset);
DECLARE_TEMPLATE(saveR9Offset);
DECLARE_TEMPLATE(saveR10Offset);
DECLARE_TEMPLATE(saveR11Offset);
DECLARE_TEMPLATE(saveR12Offset);
DECLARE_TEMPLATE(saveR13Offset);
DECLARE_TEMPLATE(saveR14Offset);
DECLARE_TEMPLATE(saveR15Offset);
DECLARE_TEMPLATE(saveXMM0Offset);
DECLARE_TEMPLATE(saveXMM1Offset);
DECLARE_TEMPLATE(saveXMM2Offset);
DECLARE_TEMPLATE(saveXMM3Offset);
DECLARE_TEMPLATE(saveXMM4Offset);
DECLARE_TEMPLATE(saveXMM5Offset);
DECLARE_TEMPLATE(saveXMM6Offset);
DECLARE_TEMPLATE(saveXMM7Offset);
DECLARE_TEMPLATE(saveXMM0Local);
DECLARE_TEMPLATE(saveXMM1Local);
DECLARE_TEMPLATE(saveXMM2Local);
DECLARE_TEMPLATE(saveXMM3Local);
DECLARE_TEMPLATE(saveXMM4Local);
DECLARE_TEMPLATE(saveXMM5Local);
DECLARE_TEMPLATE(saveXMM6Local);
DECLARE_TEMPLATE(saveXMM7Local);
DECLARE_TEMPLATE(saveRAXLocal);
DECLARE_TEMPLATE(saveRSILocal);
DECLARE_TEMPLATE(saveRDXLocal);
DECLARE_TEMPLATE(saveRCXLocal);
DECLARE_TEMPLATE(saveR11Local);
DECLARE_TEMPLATE(callByteRel);
DECLARE_TEMPLATE(call4ByteRel);
DECLARE_TEMPLATE(jump4ByteRel);
DECLARE_TEMPLATE(jumpByteRel);
DECLARE_TEMPLATE(nopInstruction);
DECLARE_TEMPLATE(movRDIImm64);
DECLARE_TEMPLATE(movEDIImm32);
DECLARE_TEMPLATE(movRAXImm64);
DECLARE_TEMPLATE(movEAXImm32);
DECLARE_TEMPLATE(movRSPOffsetR11);
DECLARE_TEMPLATE(jumpRDI);
DECLARE_TEMPLATE(jumpRAX);
DECLARE_TEMPLATE(paintRegister);
DECLARE_TEMPLATE(paintLocal);
DECLARE_TEMPLATE(moveCountAndRecompile);
DECLARE_TEMPLATE(checkCountAndRecompile);
DECLARE_TEMPLATE(loadCounter);
DECLARE_TEMPLATE(decrementCounter);
DECLARE_TEMPLATE(incrementCounter);
DECLARE_TEMPLATE(jgCount);
DECLARE_TEMPLATE(callRetranslateArg1);
DECLARE_TEMPLATE(callRetranslateArg2);
DECLARE_TEMPLATE(callRetranslate);
DECLARE_TEMPLATE(setCounter);
DECLARE_TEMPLATE(jmpToBody);

// bytecodes
DECLARE_TEMPLATE(debugBreakpoint);
DECLARE_TEMPLATE(aloadTemplatePrologue);
DECLARE_TEMPLATE(iloadTemplatePrologue);
DECLARE_TEMPLATE(lloadTemplatePrologue);
DECLARE_TEMPLATE(loadTemplate);
DECLARE_TEMPLATE(astoreTemplate);
DECLARE_TEMPLATE(istoreTemplate);
DECLARE_TEMPLATE(lstoreTemplate);
DECLARE_TEMPLATE(popTemplate);
DECLARE_TEMPLATE(pop2Template);
DECLARE_TEMPLATE(swapTemplate);
DECLARE_TEMPLATE(dupTemplate);
DECLARE_TEMPLATE(dupx1Template);
DECLARE_TEMPLATE(dupx2Template);
DECLARE_TEMPLATE(dup2Template);
DECLARE_TEMPLATE(dup2x1Template);
DECLARE_TEMPLATE(dup2x2Template);
DECLARE_TEMPLATE(getFieldTemplatePrologue);
DECLARE_TEMPLATE(intGetFieldTemplate);
DECLARE_TEMPLATE(longGetFieldTemplate);
DECLARE_TEMPLATE(floatGetFieldTemplate);
DECLARE_TEMPLATE(doubleGetFieldTemplate);
DECLARE_TEMPLATE(intPutFieldTemplatePrologue);
DECLARE_TEMPLATE(addrPutFieldTemplatePrologue);
DECLARE_TEMPLATE(longPutFieldTemplatePrologue);
DECLARE_TEMPLATE(floatPutFieldTemplatePrologue);
DECLARE_TEMPLATE(doublePutFieldTemplatePrologue);
DECLARE_TEMPLATE(intPutFieldTemplate);
DECLARE_TEMPLATE(longPutFieldTemplate);
DECLARE_TEMPLATE(floatPutFieldTemplate);
DECLARE_TEMPLATE(doublePutFieldTemplate);
DECLARE_TEMPLATE(invokeStaticTemplate);
DECLARE_TEMPLATE(staticTemplatePrologue);
DECLARE_TEMPLATE(intGetStaticTemplate);
DECLARE_TEMPLATE(addrGetStaticTemplate);
DECLARE_TEMPLATE(addrGetStaticTemplatePrologue);
DECLARE_TEMPLATE(longGetStaticTemplate);
DECLARE_TEMPLATE(floatGetStaticTemplate);
DECLARE_TEMPLATE(doubleGetStaticTemplate);
DECLARE_TEMPLATE(intPutStaticTemplate);
DECLARE_TEMPLATE(addrPutStaticTemplatePrologue);
DECLARE_TEMPLATE(addrPutStaticTemplate);
DECLARE_TEMPLATE(longPutStaticTemplate);
DECLARE_TEMPLATE(floatPutStaticTemplate);
DECLARE_TEMPLATE(doublePutStaticTemplate);
DECLARE_TEMPLATE(iAddTemplate);
DECLARE_TEMPLATE(iSubTemplate);
DECLARE_TEMPLATE(iMulTemplate);
DECLARE_TEMPLATE(iDivTemplate);
DECLARE_TEMPLATE(iRemTemplate);
DECLARE_TEMPLATE(iNegTemplate);
DECLARE_TEMPLATE(iShlTemplate);
DECLARE_TEMPLATE(iShrTemplate);
DECLARE_TEMPLATE(iUshrTemplate);
DECLARE_TEMPLATE(iAndTemplate);
DECLARE_TEMPLATE(iOrTemplate);
DECLARE_TEMPLATE(iXorTemplate);
DECLARE_TEMPLATE(i2lTemplate);
DECLARE_TEMPLATE(l2iTemplate);
DECLARE_TEMPLATE(i2bTemplate);
DECLARE_TEMPLATE(i2sTemplate);
DECLARE_TEMPLATE(i2cTemplate);
DECLARE_TEMPLATE(i2dTemplate);
DECLARE_TEMPLATE(l2dTemplate);
DECLARE_TEMPLATE(d2iTemplate);
DECLARE_TEMPLATE(d2lTemplate);
DECLARE_TEMPLATE(iconstm1Template);
DECLARE_TEMPLATE(iconst0Template);
DECLARE_TEMPLATE(iconst1Template);
DECLARE_TEMPLATE(iconst2Template);
DECLARE_TEMPLATE(iconst3Template);
DECLARE_TEMPLATE(iconst4Template);
DECLARE_TEMPLATE(iconst5Template);
DECLARE_TEMPLATE(bipushTemplate);
DECLARE_TEMPLATE(sipushTemplatePrologue);
DECLARE_TEMPLATE(sipushTemplate);
DECLARE_TEMPLATE(iIncTemplate_01_load);
DECLARE_TEMPLATE(iIncTemplate_02_add);
DECLARE_TEMPLATE(iIncTemplate_03_store);
DECLARE_TEMPLATE(lAddTemplate);
DECLARE_TEMPLATE(lSubTemplate);
DECLARE_TEMPLATE(lMulTemplate);
DECLARE_TEMPLATE(lDivTemplate);
DECLARE_TEMPLATE(lRemTemplate);
DECLARE_TEMPLATE(lNegTemplate);
DECLARE_TEMPLATE(lShlTemplate);
DECLARE_TEMPLATE(lShrTemplate);
DECLARE_TEMPLATE(lUshrTemplate);
DECLARE_TEMPLATE(lAndTemplate);
DECLARE_TEMPLATE(lOrTemplate);
DECLARE_TEMPLATE(lXorTemplate);
DECLARE_TEMPLATE(lconst0Template);
DECLARE_TEMPLATE(lconst1Template);
DECLARE_TEMPLATE(fAddTemplate);
DECLARE_TEMPLATE(fSubTemplate);
DECLARE_TEMPLATE(fMulTemplate);
DECLARE_TEMPLATE(fDivTemplate);
DECLARE_TEMPLATE(fRemTemplate);
DECLARE_TEMPLATE(fNegTemplate);
DECLARE_TEMPLATE(fconst0Template);
DECLARE_TEMPLATE(fconst1Template);
DECLARE_TEMPLATE(fconst2Template);
DECLARE_TEMPLATE(dAddTemplate);
DECLARE_TEMPLATE(dSubTemplate);
DECLARE_TEMPLATE(dMulTemplate);
DECLARE_TEMPLATE(dDivTemplate);
DECLARE_TEMPLATE(dRemTemplate);
DECLARE_TEMPLATE(dNegTemplate);
DECLARE_TEMPLATE(dconst0Template);
DECLARE_TEMPLATE(dconst1Template);
DECLARE_TEMPLATE(i2fTemplate);
DECLARE_TEMPLATE(f2iTemplate);
DECLARE_TEMPLATE(l2fTemplate);
DECLARE_TEMPLATE(f2lTemplate);
DECLARE_TEMPLATE(d2fTemplate);
DECLARE_TEMPLATE(f2dTemplate);
DECLARE_TEMPLATE(eaxReturnTemplate);
DECLARE_TEMPLATE(raxReturnTemplate);
DECLARE_TEMPLATE(xmm0ReturnTemplate);
DECLARE_TEMPLATE(retTemplate_add);
DECLARE_TEMPLATE(vReturnTemplate);
DECLARE_TEMPLATE(moveeaxForCall);
DECLARE_TEMPLATE(moveesiForCall);
DECLARE_TEMPLATE(moveedxForCall);
DECLARE_TEMPLATE(moveecxForCall);
DECLARE_TEMPLATE(moveraxRefForCall);
DECLARE_TEMPLATE(moversiRefForCall);
DECLARE_TEMPLATE(moverdxRefForCall);
DECLARE_TEMPLATE(movercxRefForCall);
DECLARE_TEMPLATE(moveraxForCall);
DECLARE_TEMPLATE(moversiForCall);
DECLARE_TEMPLATE(moverdxForCall);
DECLARE_TEMPLATE(movercxForCall);
DECLARE_TEMPLATE(movexmm0ForCall);
DECLARE_TEMPLATE(movexmm1ForCall);
DECLARE_TEMPLATE(movexmm2ForCall);
DECLARE_TEMPLATE(movexmm3ForCall);
DECLARE_TEMPLATE(moveDxmm0ForCall);
DECLARE_TEMPLATE(moveDxmm1ForCall);
DECLARE_TEMPLATE(moveDxmm2ForCall);
DECLARE_TEMPLATE(moveDxmm3ForCall);
DECLARE_TEMPLATE(retTemplate_sub);
DECLARE_TEMPLATE(loadeaxReturn);
DECLARE_TEMPLATE(loadraxReturn);
DECLARE_TEMPLATE(loadxmm0Return);
DECLARE_TEMPLATE(loadDxmm0Return);
DECLARE_TEMPLATE(lcmpTemplate);
DECLARE_TEMPLATE(fcmplTemplate);
DECLARE_TEMPLATE(fcmpgTemplate);
DECLARE_TEMPLATE(dcmplTemplate);
DECLARE_TEMPLATE(dcmpgTemplate);
DECLARE_TEMPLATE(gotoTemplate);
DECLARE_TEMPLATE(ifneTemplate);
DECLARE_TEMPLATE(ifeqTemplate);
DECLARE_TEMPLATE(ifltTemplate);
DECLARE_TEMPLATE(ifleTemplate);
DECLARE_TEMPLATE(ifgtTemplate);
DECLARE_TEMPLATE(ifgeTemplate);
DECLARE_TEMPLATE(ificmpeqTemplate);
DECLARE_TEMPLATE(ificmpneTemplate);
DECLARE_TEMPLATE(ificmpltTemplate);
DECLARE_TEMPLATE(ificmpleTemplate);
DECLARE_TEMPLATE(ificmpgeTemplate);
DECLARE_TEMPLATE(ificmpgtTemplate);
DECLARE_TEMPLATE(ifacmpeqTemplate);
DECLARE_TEMPLATE(ifacmpneTemplate);
DECLARE_TEMPLATE(ifnullTemplate);
DECLARE_TEMPLATE(ifnonnullTemplate);
DECLARE_TEMPLATE(decompressReferenceTemplate);
DECLARE_TEMPLATE(compressReferenceTemplate);
DECLARE_TEMPLATE(decompressReference1Template);
DECLARE_TEMPLATE(compressReference1Template);
DECLARE_TEMPLATE(addrGetFieldTemplatePrologue);
DECLARE_TEMPLATE(addrGetFieldTemplate);



static void
debug_print_hex(char *buffer, unsigned long long size)
   {
   printf("Start of dump:\n");
   while (size--)
      printf("%x ", 0xc0 & *(buffer++));
   printf("\nEnd of dump:");
   }

inline uint32_t
align(uint32_t number, uint32_t requirement, TR::FilePointer *fp)
   {
   MJIT_ASSERT(fp, requirement && ((requirement & (requirement -1)) == 0), "INCORRECT ALIGNMENT");
   return (number + requirement - 1) & ~(requirement - 1);
   }

static bool
getRequiredAlignment(uintptr_t cursor, uintptr_t boundary, uintptr_t margin, uintptr_t *alignment)
   {
   if ((boundary & (boundary-1)) != 0)
      return true;
   *alignment = (-cursor - margin) & (boundary-1);
   return false;
   }

static intptr_t
getHelperOrTrampolineAddress(TR_RuntimeHelper h, uintptr_t callsite)
   {
   uintptr_t helperAddress = (uintptr_t)runtimeHelperValue(h);
   uintptr_t minAddress = (callsite < (uintptr_t)(helperAddress)) ? callsite : (uintptr_t)(helperAddress);
   uintptr_t maxAddress = (callsite > (uintptr_t)(helperAddress)) ? callsite : (uintptr_t)(helperAddress);
   uintptr_t distance = maxAddress - minAddress;
   if (distance > 0x00000000ffffffff)
      helperAddress = (uintptr_t)TR::CodeCacheManager::instance()->findHelperTrampoline(h, (void *)callsite);
   return helperAddress;
   }

bool
MJIT::nativeSignature(J9Method *method, char *resultBuffer)
   {
   J9UTF8 *methodSig;
   UDATA arg;
   U_16 i, ch;
   BOOLEAN parsingReturnType = FALSE, processingBracket = FALSE;
   char nextType = '\0';

   methodSig = J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(method));
   i = 0;
   arg = 3; /* skip the return type slot and JNI standard slots, they will be filled in later. */

   while (i < J9UTF8_LENGTH(methodSig))
      {
      ch = J9UTF8_DATA(methodSig)[i++];
      switch (ch)
         {
         case '(':                                        /* start of signature -- skip */
            continue;
         case ')':                                        /* End of signature -- done args, find return type */
            parsingReturnType = TRUE;
            continue;
         case MJIT::CLASSNAME_TYPE_CHARACTER:
            nextType = MJIT::CLASSNAME_TYPE_CHARACTER;
            while(J9UTF8_DATA(methodSig)[i++] != ';') {}        /* a type string - loop scanning for ';' to end it - i points past ';' when done loop */
            break;
         case MJIT::BOOLEAN_TYPE_CHARACTER:
            nextType = MJIT::BOOLEAN_TYPE_CHARACTER;
            break;
         case MJIT::BYTE_TYPE_CHARACTER:
            nextType = MJIT::BYTE_TYPE_CHARACTER;
            break;
         case MJIT::CHAR_TYPE_CHARACTER:
            nextType = MJIT::CHAR_TYPE_CHARACTER;
            break;
         case MJIT::SHORT_TYPE_CHARACTER:
            nextType = MJIT::SHORT_TYPE_CHARACTER;
            break;
         case MJIT::INT_TYPE_CHARACTER:
            nextType = MJIT::INT_TYPE_CHARACTER;
            break;
         case MJIT::LONG_TYPE_CHARACTER:
            nextType = MJIT::LONG_TYPE_CHARACTER;
            break;
         case MJIT::FLOAT_TYPE_CHARACTER:
            nextType = MJIT::FLOAT_TYPE_CHARACTER;
            break;
         case MJIT::DOUBLE_TYPE_CHARACTER:
            nextType = MJIT::DOUBLE_TYPE_CHARACTER;
            break;
         case '[':
            processingBracket = TRUE;
            continue;                /* go back to top of loop for next char */
         case MJIT::VOID_TYPE_CHARACTER:
            if (!parsingReturnType)
               return true;
            nextType = MJIT::VOID_TYPE_CHARACTER;
            break;
         default:
            nextType = '\0';
            return true;
         }
      if (processingBracket)
         {
         if (parsingReturnType)
            {
            resultBuffer[0] = MJIT::CLASSNAME_TYPE_CHARACTER;
            break;            /* from the while loop */
            }
         else
            {
            resultBuffer[arg] = MJIT::CLASSNAME_TYPE_CHARACTER;
            arg++;
            processingBracket = FALSE;
            }
         }
      else if (parsingReturnType)
         {
         resultBuffer[0] = nextType;
         break;            /* from the while loop */
         }
      else
         {
         resultBuffer[arg] = nextType;
         arg++;
         }
      }

   resultBuffer[1] = MJIT::CLASSNAME_TYPE_CHARACTER;     /* the JNIEnv */
   resultBuffer[2] = MJIT::CLASSNAME_TYPE_CHARACTER;    /* the jobject or jclass */
   resultBuffer[arg] = '\0';
   return false;
   }

/**
 * Assuming that 'buffer' is the end of the immediate value location,
 * this function replaces the value supplied value 'imm'
 *
 * Assumes 64-bit value
 */
static void
patchImm8(char *buffer, U_64 imm)
   {
   *(((unsigned long long int*)buffer)-1) = imm;
   }

/**
 * Assuming that 'buffer' is the end of the immediate value location,
 * this function replaces the value supplied value 'imm'
 *
 * Assumes 32-bit value
 */
static void
patchImm4(char *buffer, U_32 imm)
   {
   *(((unsigned int*)buffer)-1) = imm;
   }

/**
 * Assuming that 'buffer' is the end of the immediate value location,
 * this function replaces the value supplied value 'imm'
 *
 * Assumes 16-bit value
 */
static void
patchImm2(char *buffer, U_16 imm)
   {
   *(((unsigned short int*)buffer)-1) = imm;
   }

/**
 * Assuming that 'buffer' is the end of the immediate value location,
 * this function replaces the value supplied value 'imm'
 *
 * Assumes 8-bit value
 */
static void
patchImm1(char *buffer, U_8 imm)
   {
   *(unsigned char*)(buffer-1) = imm;
   }

/*
| Architecture | Endian | Return address | vTable index         |
| x86-64       | Little | Stack          | R8 (receiver in RAX) |

| Integer Return value registers | Integer Preserved registers | Integer Argument registers |
| EAX (32-bit) RAX (64-bit)      | RBX R9 RSP                  | RAX RSI RDX RCX            |

| Long Return value registers    | Long Preserved registers    | Long Argument registers    |
| RAX (64-bit)                   | RBX R9 RSP                  | RAX RSI RDX RCX            |

| Float Return value registers   | Float Preserved registers   | Float Argument registers   |
| XMM0                           | XMM8-XMM15                  | XMM0-XMM7                  |

| Double Return value registers  | Double Preserved registers  | Double Argument registers  |
| XMM0                           | XMM8-XMM15                  | XMM0-XMM7                  |
*/

inline buffer_size_t
savePreserveRegisters(char *buffer, int32_t offset)
   {
   buffer_size_t size = 0;
   COPY_TEMPLATE(buffer, saveRBXOffset, size);
   patchImm4(buffer, offset);
   COPY_TEMPLATE(buffer, saveR9Offset, size);
   patchImm4(buffer, offset-8);
   COPY_TEMPLATE(buffer, saveRSPOffset, size);
   patchImm4(buffer, offset-16);
   return size;
   }

inline buffer_size_t
loadPreserveRegisters(char *buffer, int32_t offset)
   {
   buffer_size_t size = 0;
   COPY_TEMPLATE(buffer, loadRBXOffset, size);
   patchImm4(buffer, offset);
   COPY_TEMPLATE(buffer, loadR9Offset, size);
   patchImm4(buffer, offset-8);
   COPY_TEMPLATE(buffer, loadRSPOffset, size);
   patchImm4(buffer, offset-16);
   return size;
   }

#define COPY_IMM1_TEMPLATE(buffer, size, value) \
   do { \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         patchImm1(buffer, (U_8)(value)); \
   } while (0)

#define COPY_IMM2_TEMPLATE(buffer, size, value) \
   do { \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         patchImm2(buffer, (U_16)(value)); \
   } while (0)

#define COPY_IMM4_TEMPLATE(buffer, size, value) \
   do { \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         patchImm4(buffer, (U_32)(value)); \
   } while (0)

#define COPY_IMM8_TEMPLATE(buffer, size, value) \
   do { \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         COPY_TEMPLATE(buffer, nopInstruction, size); \
         patchImm8(buffer, (U_64)(value)); \
   } while (0)

MJIT::RegisterStack
MJIT::mapIncomingParams(
      char *typeString,
      U_16 maxLength,
      int *error_code,
      ParamTableEntry *paramTable,
      U_16 actualParamCount,
      TR::FilePointer *logFileFP)
   {
   MJIT::RegisterStack stack;
   initParamStack(&stack);

   uint16_t index = 0;
   intptr_t offset = 0;

   for (int i = 0; i<actualParamCount; i++)
      {
      // 3rd index of typeString is first param.
      char typeChar = typeString[i+3];
      U_16 currentOffset = calculateOffset(&stack);
      U_8 size = MJIT::typeSignatureSize(typeChar);
      U_8 slots = size/8;
      if (i == 0 && (typeChar == MJIT::LONG_TYPE_CHARACTER || typeChar == MJIT::DOUBLE_TYPE_CHARACTER))
         {
         // TR requires that if first argument is of type long or double, it uses 8 bytes on the c stack
         size = 8;
         }
      int regNo = TR::RealRegister::NoReg;
      bool isRef = false;
      switch (typeChar)
         {
         case MJIT::CLASSNAME_TYPE_CHARACTER:
            isRef = true;
         case MJIT::BYTE_TYPE_CHARACTER:
         case MJIT::CHAR_TYPE_CHARACTER:
         case MJIT::INT_TYPE_CHARACTER:
         case MJIT::SHORT_TYPE_CHARACTER:
         case MJIT::BOOLEAN_TYPE_CHARACTER:
         case MJIT::LONG_TYPE_CHARACTER:
            regNo = addParamIntToStack(&stack, size);
            goto makeParamTableEntry;
         case MJIT::DOUBLE_TYPE_CHARACTER:
         case MJIT::FLOAT_TYPE_CHARACTER:
            regNo = addParamFloatToStack(&stack, size);
            goto makeParamTableEntry;
         makeParamTableEntry:
            paramTable[index] = (regNo != TR::RealRegister::NoReg) ? // If this is a register
               makeRegisterEntry(regNo, currentOffset, size, slots, isRef) : // Add a register entry
               makeStackEntry(currentOffset, size, slots, isRef); // else it's a stack entry
               // first stack entry (1 used) should be at offset 0.
               index += slots;
            break;
         default:
            *error_code = 2;
            return stack;
         }
      }

   return stack;
   }

int
MJIT::getParamCount(char *typeString, U_16 maxLength)
   {
   U_16 paramCount = 0;
   for (int i = 3; i<maxLength; i++)
      {
      switch (typeString[i])
         {
         case MJIT::BYTE_TYPE_CHARACTER:
         case MJIT::CHAR_TYPE_CHARACTER:
         case MJIT::INT_TYPE_CHARACTER:
         case MJIT::CLASSNAME_TYPE_CHARACTER:
         case MJIT::SHORT_TYPE_CHARACTER:
         case MJIT::BOOLEAN_TYPE_CHARACTER:
         case MJIT::FLOAT_TYPE_CHARACTER:
            paramCount++;
            break;
         case MJIT::LONG_TYPE_CHARACTER:
         case MJIT::DOUBLE_TYPE_CHARACTER:
            paramCount+=2;
            break;
         default:
            return paramCount;
         }
      }
   return paramCount;
   }

int
MJIT::getActualParamCount(char *typeString, U_16 maxLength)
   {
   U_16 paramCount = 0;
   for (int i = 3; i<maxLength; i++)
      {
      switch (typeString[i])
         {
         case MJIT::BYTE_TYPE_CHARACTER:
         case MJIT::CHAR_TYPE_CHARACTER:
         case MJIT::INT_TYPE_CHARACTER:
         case MJIT::CLASSNAME_TYPE_CHARACTER:
         case MJIT::SHORT_TYPE_CHARACTER:
         case MJIT::BOOLEAN_TYPE_CHARACTER:
         case MJIT::FLOAT_TYPE_CHARACTER:
         case MJIT::LONG_TYPE_CHARACTER:
         case MJIT::DOUBLE_TYPE_CHARACTER:
            paramCount++;
            break;
         default:
            return paramCount;
         }
      }
   return paramCount;
   }

int
MJIT::getMJITStackSlotCount(char *typeString, U_16 maxLength)
   {
   U_16 slotCount = 0;
   for (int i = 3; i<maxLength; i++)
      {
      switch (typeString[i])
         {
         case MJIT::CLASSNAME_TYPE_CHARACTER:
         case MJIT::BYTE_TYPE_CHARACTER:
         case MJIT::CHAR_TYPE_CHARACTER:
         case MJIT::INT_TYPE_CHARACTER:
         case MJIT::SHORT_TYPE_CHARACTER:
         case MJIT::BOOLEAN_TYPE_CHARACTER:
         case MJIT::FLOAT_TYPE_CHARACTER:
            slotCount += 1;
            break;
         case MJIT::LONG_TYPE_CHARACTER:
         case MJIT::DOUBLE_TYPE_CHARACTER:
            // TR requires that if first argument is of type long or double, it uses 1 slot!
            slotCount += ((3 == i) ? 1 : 2);
            break;
         case '\0':
            return slotCount;
         default:
            return 0;
         }
      }
   return slotCount;
   }

inline uint32_t
gcd(uint32_t a, uint32_t b)
   {
   while (b != 0)
      {
      uint32_t t = b;
      b = a % b;
      a = t;
      }
   return a;
   }

inline uint32_t
lcm(uint32_t a, uint32_t b)
   {
   return a * b / gcd(a, b);
   }

MJIT::ParamTable::ParamTable(ParamTableEntry *tableEntries, U_16 paramCount, U_16 actualParamCount, RegisterStack *registerStack)
   : _tableEntries(tableEntries)
   , _paramCount(paramCount)
   , _actualParamCount(actualParamCount)
   , _registerStack(registerStack)
   {

   }

bool
MJIT::ParamTable::getEntry(U_16 paramIndex, MJIT::ParamTableEntry *entry)
   {
   bool success = ((-1 >= paramIndex) ? false : (paramIndex < _paramCount));
   if (success)
      *entry = _tableEntries[paramIndex];
   return success;
   }

bool
MJIT::ParamTable::setEntry(U_16 paramIndex, MJIT::ParamTableEntry *entry)
   {
   bool success = ((-1 >= paramIndex) ? false : (paramIndex < _paramCount));
   if (success)
      _tableEntries[paramIndex] = *entry;
   return success;
   }

U_16
MJIT::ParamTable::getTotalParamSize()
   {
   return calculateOffset(_registerStack);
   }

U_16
MJIT::ParamTable::getParamCount()
   {
   return _paramCount;
   }

U_16
MJIT::ParamTable::getActualParamCount()
   {
   return _actualParamCount;
   }

MJIT::LocalTable::LocalTable(LocalTableEntry *tableEntries, U_16 localCount)
   : _tableEntries(tableEntries)
   , _localCount(localCount)
   {

   }

bool
MJIT::LocalTable::getEntry(U_16 localIndex, MJIT::LocalTableEntry *entry)
   {
   bool success = ((-1 >= localIndex) ? false : (localIndex < _localCount));
   if (success)
      *entry = _tableEntries[localIndex];
   return success;
   }

U_16
MJIT::LocalTable::getTotalLocalSize()
   {
   U_16 localSize = 0;
   for (int i=0; i<_localCount; i++)
      localSize += _tableEntries[i].slots*8;
   return localSize;
   }

U_16
MJIT::LocalTable::getLocalCount()
   {
   return _localCount;
   }

buffer_size_t
MJIT::CodeGenerator::generateGCR(
      char *buffer,
      int32_t initialCount,
      J9Method *method,
      uintptr_t startPC)
   {
   // see TR_J9ByteCodeIlGenerator::prependGuardedCountForRecompilation(TR::Block * originalFirstBlock)
   // guardBlock:           if (!countForRecompile) goto originalFirstBlock;
   // bumpCounterBlock:     count--;
   //                       if (bodyInfo->count > 0) goto originalFirstBlock;
   // callRecompileBlock:   call jitRetranslateCallerWithPreparation(j9method, startPC);
   //                       bodyInfo->count=10000
   //                       goto originalFirstBlock
   // bumpCounter:          count
   // originalFirstBlock:   ...
   //
   buffer_size_t size = 0;

   // MicroJIT either needs its own guard, or it needs to not have a guard block
   // The following is kept here so that if a new guard it
   /*
   COPY_TEMPLATE(buffer, moveCountAndRecompile, size);
   char *guardBlock = buffer;
   COPY_TEMPLATE(buffer, checkCountAndRecompile, size);
   char *guardBlockJump = buffer;
   */

   // bumpCounterBlock
   COPY_TEMPLATE(buffer, loadCounter, size);
   char *counterLoader = buffer;
   COPY_TEMPLATE(buffer, decrementCounter, size);
   char *counterStore = buffer;
   COPY_TEMPLATE(buffer, jgCount, size);
   char *bumpCounterBlockJump = buffer;

   // callRecompileBlock
   COPY_TEMPLATE(buffer, callRetranslateArg1, size);
   char *retranslateArg1Patch = buffer;
   COPY_TEMPLATE(buffer, callRetranslateArg2, size);
   char *retranslateArg2Patch = buffer;
   COPY_TEMPLATE(buffer, callRetranslate, size);
   char *callSite = buffer;
   COPY_TEMPLATE(buffer, setCounter, size);
   char *counterSetter = buffer;
   COPY_TEMPLATE(buffer, jmpToBody, size);
   char *callRecompileBlockJump = buffer;

   // bumpCounter
   char *counterLocation = buffer;
   COPY_IMM4_TEMPLATE(buffer, size, initialCount);

   uintptr_t jitRetranslateCallerWithPrepHelperAddress = getHelperOrTrampolineAddress(TR_jitRetranslateCallerWithPrep, (uintptr_t)buffer);

   // Find the countForRecompile's address and patch for the guard block.
   patchImm8(retranslateArg1Patch, (uintptr_t)(method));
   patchImm8(retranslateArg2Patch, (uintptr_t)(startPC));
   // patchImm8(guardBlock, (uintptr_t)(&(_persistentInfo->_countForRecompile)));
   PATCH_RELATIVE_ADDR_32_BIT(callSite, jitRetranslateCallerWithPrepHelperAddress);
   patchImm8(counterLoader, (uintptr_t)counterLocation);
   patchImm8(counterStore, (uintptr_t)counterLocation);
   patchImm8(counterSetter, (uintptr_t)counterLocation);
   // PATCH_RELATIVE_ADDR_32_BIT(guardBlockJump, buffer);
   PATCH_RELATIVE_ADDR_32_BIT(bumpCounterBlockJump, buffer);
   PATCH_RELATIVE_ADDR_32_BIT(callRecompileBlockJump, buffer);

   return size;
   }

buffer_size_t
MJIT::CodeGenerator::saveArgsInLocalArray(
      char *buffer,
      buffer_size_t stack_alloc_space,
      char *typeString)
   {
   buffer_size_t saveSize = 0;
   U_16 slot = 0;
   TR::RealRegister::RegNum regNum = TR::RealRegister::NoReg;
   char typeChar;
   int index = 0;
   ParamTableEntry entry;
   while (index<_paramTable->getParamCount())
      {
      MJIT_ASSERT(_logFileFP, _paramTable->getEntry(index, &entry), "Bad index for table entry");
      if (entry.notInitialized)
         {
         index++;
         continue;
         }
      if (entry.onStack)
         break;
      int32_t regNum = entry.regNo;
      switch (regNum)
         {
         case TR::RealRegister::xmm0:
            COPY_TEMPLATE(buffer, saveXMM0Local, saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::xmm1:
            COPY_TEMPLATE(buffer, saveXMM1Local, saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::xmm2:
            COPY_TEMPLATE(buffer, saveXMM2Local, saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::xmm3:
            COPY_TEMPLATE(buffer, saveXMM3Local, saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::xmm4:
            COPY_TEMPLATE(buffer, saveXMM4Local, saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::xmm5:
            COPY_TEMPLATE(buffer, saveXMM5Local, saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::xmm6:
            COPY_TEMPLATE(buffer, saveXMM6Local, saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::xmm7:
            COPY_TEMPLATE(buffer, saveXMM7Local, saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::eax:
            COPY_TEMPLATE(buffer, saveRAXLocal,  saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::esi:
            COPY_TEMPLATE(buffer, saveRSILocal,  saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::edx:
            COPY_TEMPLATE(buffer, saveRDXLocal,  saveSize);
            goto PatchAndBreak;
         case TR::RealRegister::ecx:
            COPY_TEMPLATE(buffer, saveRCXLocal,  saveSize);
            goto PatchAndBreak;
         PatchAndBreak:
            patchImm4(buffer, (U_32)((slot*8) & BIT_MASK_32));
            break;
         case TR::RealRegister::NoReg:
            break;
         }
      slot += entry.slots;
      index += entry.slots;
      }

   while (index<_paramTable->getParamCount())
      {
      MJIT_ASSERT(_logFileFP, _paramTable->getEntry(index, &entry), "Bad index for table entry");
      if (entry.notInitialized)
         {
         index++;
         continue;
         }
      uintptr_t offset = entry.offset;
      // Our linkage templates for loading from the stack assume that the offset will always be less than 0xff.
      //  This is however not always true for valid code. If this becomes a problem in the future we will
      //  have to add support for larger offsets by creating new templates that use 2 byte offsets from rsp.
      //  Whether that will be a change to the current templates, or new templates with supporting code
      //  is a decision yet to be determined.
      if ((offset + stack_alloc_space) < uintptr_t(0xff))
         {
         if (_comp->getOption(TR_TraceCG))
            trfprintf(_logFileFP, "Offset too large, add support for larger offsets");
         return -1;
         }
      COPY_TEMPLATE(buffer, movRSPOffsetR11, saveSize);
      patchImm1(buffer, (U_8)(offset+stack_alloc_space));
      COPY_TEMPLATE(buffer, saveR11Local, saveSize);
      patchImm4(buffer, (U_32)((slot*8) & BIT_MASK_32));
      slot += entry.slots;
      index += entry.slots;
      }
   return saveSize;
   }

buffer_size_t
MJIT::CodeGenerator::saveArgsOnStack(
      char *buffer,
      buffer_size_t stack_alloc_space,
      ParamTable *paramTable)
   {
   buffer_size_t saveArgsSize = 0;
   U_16 offset = paramTable->getTotalParamSize();
   TR::RealRegister::RegNum regNum = TR::RealRegister::NoReg;
   ParamTableEntry entry;
   for (int i=0; i<paramTable->getParamCount();)
      {
      MJIT_ASSERT(_logFileFP, paramTable->getEntry(i, &entry), "Bad index for table entry");
      if (entry.notInitialized)
         {
         i++;
         continue;
         }
      if (entry.onStack)
         break;
      int32_t regNum = entry.regNo;
      if (i != 0)
         offset -= entry.size;
      switch (regNum)
         {
         case TR::RealRegister::xmm0:
            COPY_TEMPLATE(buffer, saveXMM0Offset, saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm1:
            COPY_TEMPLATE(buffer, saveXMM1Offset, saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm2:
            COPY_TEMPLATE(buffer, saveXMM2Offset, saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm3:
            COPY_TEMPLATE(buffer, saveXMM3Offset, saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm4:
            COPY_TEMPLATE(buffer, saveXMM4Offset, saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm5:
            COPY_TEMPLATE(buffer, saveXMM5Offset, saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm6:
            COPY_TEMPLATE(buffer, saveXMM6Offset, saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm7:
            COPY_TEMPLATE(buffer, saveXMM7Offset, saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::eax:
            COPY_TEMPLATE(buffer, saveRAXOffset,  saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::esi:
            COPY_TEMPLATE(buffer, saveRSIOffset,  saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::edx:
            COPY_TEMPLATE(buffer, saveRDXOffset,  saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::ecx:
            COPY_TEMPLATE(buffer, saveRCXOffset,  saveArgsSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::NoReg:
            break;
         }
      i += entry.slots;
      }
   return saveArgsSize;
   }

buffer_size_t
MJIT::CodeGenerator::loadArgsFromStack(
      char *buffer,
      buffer_size_t stack_alloc_space,
      ParamTable *paramTable)
   {
   U_16 offset = paramTable->getTotalParamSize();
   buffer_size_t argLoadSize = 0;
   TR::RealRegister::RegNum regNum = TR::RealRegister::NoReg;
   ParamTableEntry entry;
   for (int i=0; i<paramTable->getParamCount();)
      {
      MJIT_ASSERT(_logFileFP, paramTable->getEntry(i, &entry), "Bad index for table entry");
      if (entry.notInitialized)
         {
         i++;
         continue;
         }
      if (entry.onStack)
         break;
      int32_t regNum = entry.regNo;
      if (i != 0)
         offset -= entry.size;
      switch(regNum)
         {
         case TR::RealRegister::xmm0:
            COPY_TEMPLATE(buffer, loadXMM0Offset, argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm1:
            COPY_TEMPLATE(buffer, loadXMM1Offset, argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm2:
            COPY_TEMPLATE(buffer, loadXMM2Offset, argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm3:
            COPY_TEMPLATE(buffer, loadXMM3Offset, argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm4:
            COPY_TEMPLATE(buffer, loadXMM4Offset, argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm5:
            COPY_TEMPLATE(buffer, loadXMM5Offset, argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm6:
            COPY_TEMPLATE(buffer, loadXMM6Offset, argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::xmm7:
            COPY_TEMPLATE(buffer, loadXMM7Offset, argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::eax:
            COPY_TEMPLATE(buffer, loadRAXOffset,  argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::esi:
            COPY_TEMPLATE(buffer, loadRSIOffset,  argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::edx:
            COPY_TEMPLATE(buffer, loadRDXOffset,  argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::ecx:
            COPY_TEMPLATE(buffer, loadRCXOffset,  argLoadSize);
            patchImm4(buffer, (U_32)((offset+ stack_alloc_space) & 0xffffffff));
            break;
         case TR::RealRegister::NoReg:
            break;
         }
      i += entry.slots;
      }
   return argLoadSize;
   }

MJIT::CodeGenerator::CodeGenerator(
      struct J9JITConfig *config,
      J9VMThread *thread,
      TR::FilePointer *fp,
      TR_J9VMBase& vm,
      ParamTable *paramTable,
      TR::Compilation *comp,
      MJIT::CodeGenGC *mjitCGGC,
      TR::PersistentInfo *persistentInfo,
      TR_Memory *trMemory,
      TR_ResolvedMethod *compilee)
      :_linkage(Linkage(comp->cg()))
      ,_logFileFP(fp)
      ,_vm(vm)
      ,_codeCache(NULL)
      ,_stackPeakSize(0)
      ,_paramTable(paramTable)
      ,_comp(comp)
      ,_mjitCGGC(mjitCGGC)
      ,_atlas(NULL)
      ,_persistentInfo(persistentInfo)
      ,_trMemory(trMemory)
      ,_compilee(compilee)
      ,_maxCalleeArgsSize(0)
   {
   _linkage._properties.setOutgoingArgAlignment(lcm(16, _vm.getLocalObjectAlignmentInBytes()));
   }

TR::GCStackAtlas *
MJIT::CodeGenerator::getStackAtlas()
   {
   return _atlas;
   }

buffer_size_t
MJIT::CodeGenerator::generateSwitchToInterpPrePrologue(
      char *buffer,
      J9Method *method,
      buffer_size_t boundary,
      buffer_size_t margin,
      char *typeString,
      U_16 maxLength)
   {
   uintptr_t startOfMethod = (uintptr_t)buffer;
   buffer_size_t switchSize = 0;

   // Store the J9Method address in edi and store the label of this jitted method.
   uintptr_t j2iTransitionPtr = getHelperOrTrampolineAddress(TR_j2iTransition, (uintptr_t)buffer);
   uintptr_t methodPtr = (uintptr_t)method;
   COPY_TEMPLATE(buffer, movRDIImm64, switchSize);
   patchImm8(buffer, methodPtr);

   // if (comp->getOption(TR_EnableHCR))
   //    comp->getStaticHCRPICSites()->push_front(prev);

   // Pass args on stack, expects the method to run in RDI
   buffer_size_t saveArgSize = saveArgsOnStack(buffer, 0, _paramTable);
   buffer += saveArgSize;
   switchSize += saveArgSize;

   // Generate jump to the TR_i2jTransition function
   COPY_TEMPLATE(buffer, jump4ByteRel, switchSize);
   PATCH_RELATIVE_ADDR_32_BIT(buffer, j2iTransitionPtr);

   margin += 2;
   uintptr_t requiredAlignment = 0;
   if (getRequiredAlignment((uintptr_t)buffer, boundary, margin, &requiredAlignment))
      {
      buffer = (char*)startOfMethod;
      return 0;
      }
   for (U_8 i = 0; i<=requiredAlignment; i++)
      COPY_TEMPLATE(buffer, nopInstruction, switchSize);

   // Generate relative jump to start
   COPY_TEMPLATE(buffer, jumpByteRel, switchSize);
   PATCH_RELATIVE_ADDR_8_BIT(buffer, startOfMethod);

   return switchSize;
   }

buffer_size_t
MJIT::CodeGenerator::generatePrePrologue(
      char *buffer,
      J9Method *method,
      char** magicWordLocation,
      char** first2BytesPatchLocation,
      char** samplingRecompileCallLocation)
   {
   // Return size (in bytes) of pre-prologue on success

   buffer_size_t preprologueSize = 0;
   int alignmentMargin = 0;
   int alignmentBoundary = 8;
   uintptr_t endOfPreviousMethod = (uintptr_t)buffer;

   J9ROMClass *romClass = J9_CLASS_FROM_METHOD(method)->romClass;
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
   U_16 maxLength = J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod));
   char typeString[maxLength];
   if (nativeSignature(method, typeString)    // If this had no signature, we can't compile it.
       || !_comp->getRecompilationInfo())     // If this has no recompilation info we won't be able to recompile, so let TR compile it later.
      {
      return 0;
      }

   // Save area for the first two bytes of the method + jitted body info pointer + linkageinfo in margin.
   alignmentMargin += MJIT_SAVE_AREA_SIZE + MJIT_JITTED_BODY_INFO_PTR_SIZE + MJIT_LINKAGE_INFO_SIZE;

   // Make sure the startPC at least 4-byte aligned. This is important, since the VM
   // depends on the alignment (it uses the low order bits as tag bits).
   //
   // TODO: `mustGenerateSwitchToInterpreterPrePrologue` checks that the code generator's compilation is not:
   //         `usesPreexistence`,
   //         `getOption(TR_EnableHCR)`,
   //         `!fej9()->isAsyncCompilation`,
   //         `getOption(TR_FullSpeedDebug)`,
   //
   if (GENERATE_SWITCH_TO_INTERP_PREPROLOGUE)
      {
      // TODO: Replace with a check that performs the above checks.
      // generateSwitchToInterpPrePrologue will align data for use
      char *old_buff = buffer;
      preprologueSize += generateSwitchToInterpPrePrologue(buffer, method, alignmentBoundary, alignmentMargin, typeString, maxLength);
      buffer += preprologueSize;
#ifdef MJIT_DEBUG
      trfprintf(_logFileFP, "\ngenerateSwitchToInterpPrePrologue: %d\n", preprologueSize);
      for (int32_t i = 0; i < preprologueSize; i++)
         trfprintf(_logFileFP, "%02x\n", ((unsigned char)old_buff[i]) & (unsigned char)0xff);
#endif
      }
   else
      {
      uintptr_t requiredAlignment = 0;
      if (getRequiredAlignment((uintptr_t)buffer, alignmentBoundary, alignmentMargin, &requiredAlignment))
         {
         buffer = (char*)endOfPreviousMethod;
         return 0;
         }
      for (U_8 i = 0; i<requiredAlignment; i++)
         COPY_TEMPLATE(buffer, nopInstruction, preprologueSize);
      }

   // On 64 bit target platforms
   // Copy the first two bytes of the method, in case we need to un-patch them
   COPY_IMM2_TEMPLATE(buffer, preprologueSize, 0xcccc);
   *first2BytesPatchLocation = buffer;

   // if we are using sampling, generate the call instruction to patch the sampling mathod
   // Note: the displacement of this call gets patched, but thanks to the
   // alignment constraint on the startPC, it is automatically aligned to a
   // 4-byte boundary, which is more than enough to ensure it can be patched
   // automatically.
   *samplingRecompileCallLocation = buffer;
   COPY_TEMPLATE(buffer, call4ByteRel, preprologueSize);
   uintptr_t samplingRecompilehelperAddress = getHelperOrTrampolineAddress(TR_AMD64samplingRecompileMethod, (uintptr_t)buffer);
   PATCH_RELATIVE_ADDR_32_BIT(buffer, samplingRecompilehelperAddress);

   // The address of the persistent method info is inserted in the pre-prologue
   // information. If the method is not to be compiled again a null value is
   // inserted.
   TR_PersistentJittedBodyInfo *bodyInfo = _comp->getRecompilationInfo()->getJittedBodyInfo();
   bodyInfo->setUsesGCR();
   bodyInfo->setIsMJITCompiledMethod(true);
   COPY_IMM8_TEMPLATE(buffer, preprologueSize, bodyInfo);

   // Allow 4 bytes for private linkage return type info. Allocate the 4 bytes
   // even if the linkage is not private, so that all the offsets are
   // predictable.
   uint32_t magicWord = 0x00000010; // All MicroJIT compilations are set to recompile with GCR.
   switch (typeString[0])
      {
      case MJIT::VOID_TYPE_CHARACTER:
         COPY_IMM4_TEMPLATE(buffer, preprologueSize, magicWord | TR_VoidReturn);
         break;
      case MJIT::BYTE_TYPE_CHARACTER:
      case MJIT::SHORT_TYPE_CHARACTER:
      case MJIT::CHAR_TYPE_CHARACTER:
      case MJIT::BOOLEAN_TYPE_CHARACTER:
      case MJIT::INT_TYPE_CHARACTER:
         COPY_IMM4_TEMPLATE(buffer, preprologueSize, magicWord | TR_IntReturn);
         break;
      case MJIT::LONG_TYPE_CHARACTER:
         COPY_IMM4_TEMPLATE(buffer, preprologueSize, magicWord | TR_LongReturn);
         break;
      case MJIT::CLASSNAME_TYPE_CHARACTER:
         COPY_IMM4_TEMPLATE(buffer, preprologueSize, magicWord | TR_ObjectReturn);
         break;
      case MJIT::FLOAT_TYPE_CHARACTER:
         COPY_IMM4_TEMPLATE(buffer, preprologueSize, magicWord | TR_FloatXMMReturn);
         break;
      case MJIT::DOUBLE_TYPE_CHARACTER:
         COPY_IMM4_TEMPLATE(buffer, preprologueSize, magicWord | TR_DoubleXMMReturn);
         break;
      }
   *magicWordLocation = buffer;
   return preprologueSize;
   }

/*
 * Write the prologue to the buffer and return the size written to the buffer.
 */
buffer_size_t
MJIT::CodeGenerator::generatePrologue(
      char *buffer,
      J9Method *method,
      char  **jitStackOverflowJumpPatchLocation,
      char *magicWordLocation,
      char *first2BytesPatchLocation,
      char *samplingRecompileCallLocation,
      char **firstInstLocation,
      TR_J9ByteCodeIterator *bci)
   {
   buffer_size_t prologueSize = 0;
   uintptr_t prologueStart = (uintptr_t)buffer;

   J9ROMClass *romClass = J9_CLASS_FROM_METHOD(method)->romClass;
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
   U_16 maxLength = J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod));
   char typeString[maxLength];
   if (nativeSignature(method, typeString))
      return 0;
   uintptr_t startPC = (uintptr_t)buffer;
   // Same order as saving (See generateSwitchToInterpPrePrologue)
   buffer_size_t loadArgSize = loadArgsFromStack(buffer, 0, _paramTable);
   buffer += loadArgSize;
   prologueSize += loadArgSize;

   uint32_t magicWord = *(magicWordLocation-4); // We save the end of the location for patching, not the start.
   magicWord |= (((uintptr_t)buffer) - prologueStart) << 16;
   patchImm4(magicWordLocation, magicWord);

   // check for stack overflow
   *firstInstLocation = buffer;
   COPY_TEMPLATE(buffer, cmpRspRbpDerefOffset, prologueSize);
   patchImm4(buffer, (U_32)(0x50));
   patchImm2(first2BytesPatchLocation, *(uint16_t*)(*firstInstLocation));
   COPY_TEMPLATE(buffer, jbe4ByteRel, prologueSize);
   *jitStackOverflowJumpPatchLocation = buffer;

   // Compute frame size
   //
   // allocSize: bytes to be subtracted from the stack pointer when allocating the frame
   // peakSize:  maximum bytes of stack this method might consume before encountering another stack check
   //
   struct TR::X86LinkageProperties properties = _linkage._properties;
   // Max amount of data to be preserved. It is overkill, but as long as the prologue/epilogue save/load everything
   // it is a workable solution for now. Later try to determine what registers need to be preserved ahead of time.
   const int32_t pointerSize = properties.getPointerSize();
   U_32 preservedRegsSize = properties._numPreservedRegisters * pointerSize;
   const int32_t localSize = (romMethod->argCount + romMethod->tempCount)*pointerSize;
   MJIT_ASSERT(_logFileFP, localSize >= 0, "assertion failure");
   int32_t frameSize = localSize + preservedRegsSize + ( properties.getReservesOutgoingArgsInPrologue() ? pointerSize : 0 );
   uint32_t stackSize = frameSize + properties.getRetAddressWidth();
   uint32_t adjust = align(stackSize, properties.getOutgoingArgAlignment(), _logFileFP) - stackSize;
   auto allocSize = frameSize + adjust;

   const int32_t mjitRegisterSize = (6 * pointerSize);

   // return address is allocated by call instruction
   // Here we conservatively assume there is a call
   _stackPeakSize =
      allocSize + // space for stack frame
      mjitRegisterSize + // saved registers, conservatively expecting a call
      _stackPeakSize; // space for mjit value stack (set in CompilationInfoPerThreadBase::mjit)

   // Small: entire stack usage fits in STACKCHECKBUFFER, so if sp is within
   // the soft limit before buying the frame, then the whole frame will fit
   // within the hard limit.
   //
   // Medium: the additional stack required after bumping the sp fits in
   // STACKCHECKBUFFER, so if sp after the bump is within the soft limit, the
   // whole frame will fit within the hard limit.
   //
   // Large: No shortcuts. Calculate the maximum extent of stack needed and
   // compare that against the soft limit. (We have to use the soft limit here
   // if for no other reason than that's the one used for asyncchecks.)

   COPY_TEMPLATE(buffer, subRSPImm4, prologueSize);
   patchImm4(buffer, _stackPeakSize);

   buffer_size_t savePreserveSize = savePreserveRegisters(buffer, _stackPeakSize-8);
   buffer += savePreserveSize;
   prologueSize += savePreserveSize;

   buffer_size_t saveArgSize = saveArgsOnStack(buffer, _stackPeakSize, _paramTable);
   buffer += saveArgSize;
   prologueSize += saveArgSize;

   /*
   * MJIT stack frames must conform to TR stack frame shapes. However, so long as we are able to have our frames walked, we should be
   * free to allocate more things on the stack before the required data which is used for walking. The following is the general shape of the
   * initial MJIT stack frame:
   * +-----------------------------+
   * |             ...             |
   * | Paramaters passed by caller | <-- The space for saving params passed in registers is also here
   * |             ...             |
   * +-----------------------------+
   * |        Return Address       | <-- RSP points here before we sub the _stackPeakSize
   * +-----------------------------+
   * |       Alignment space       |
   * +-----------------------------+ <-- Caller/Callee stack boundary
   * |             ...             |
   * |  Callee Preserved Registers |
   * |             ...             |
   * +-----------------------------+
   * |             ...             |
   * |       Local Variables       |
   * |             ...             | <-- R10 points here at the end of the prologue, R14 always points here
   * +-----------------------------+
   * |             ...             |
   * |    MJIT Computation Stack   |
   * |             ...             |
   * +-----------------------------+
   * |             ...             |
   * |  Caller Preserved Registers |
   * |             ...             | <-- RSP points here after we sub _stackPeakSize the first time
   * +-----------------------------+
   * |             ...             |
   * | Paramaters passed to callee |
   * |             ...             | <-- RSP points here after we sub _stackPeakSize the second time
   * +-----------------------------+
   * |        Return Address       | <-- RSP points here after a callq instruction
   * +-----------------------------+ <-- End of stack frame
   */

   // Set up MJIT value stack
   COPY_TEMPLATE(buffer, movRSPR10, prologueSize);
   COPY_TEMPLATE(buffer, addR10Imm4, prologueSize);
   patchImm4(buffer, (U_32)(mjitRegisterSize));
   COPY_TEMPLATE(buffer, addR10Imm4, prologueSize);
   patchImm4(buffer, (U_32)(romMethod->maxStack*8));
   // TODO: Find a way to cleanly preserve this value before overriding it in the _stackAllocSize

   // Set up MJIT local array
   COPY_TEMPLATE(buffer, movR10R14, prologueSize);

   // Move parameters to where the method body will expect to find them
   buffer_size_t saveSize = saveArgsInLocalArray(buffer, _stackPeakSize, typeString);
   if ((int32_t)saveSize == -1)
      return 0;
   buffer += saveSize;
   prologueSize += saveSize;
   if (!saveSize && romMethod->argCount)
      return 0;

   int32_t firstLocalOffset = (preservedRegsSize+8);
   // This is the number of slots, not variables.
   U_16 localVariableCount = romMethod->argCount + romMethod->tempCount;
   MJIT::LocalTableEntry localTableEntries[localVariableCount];
   for (int i=0; i<localVariableCount; i++)
      localTableEntries[i] = initializeLocalTableEntry();
   bool resolvedAllCallees = true;
   InternalMetaData internalMetaData = createInternalMethodMetadata(bci, localTableEntries, localVariableCount, firstLocalOffset, _compilee, _comp, _paramTable, pointerSize, &resolvedAllCallees);
   if (!resolvedAllCallees)
      return 0;
   LocalTable *localTable = &internalMetaData._localTable;

   // Make space for args of methods called by this one.
   COPY_TEMPLATE(buffer, subRSPImm4, prologueSize);
   _maxCalleeArgsSize = internalMetaData._maxCalleeArgSize;
   patchImm4(buffer, _maxCalleeArgsSize);
   _stackPeakSize += _maxCalleeArgsSize;
   _comp->cg()->setStackFramePaddingSizeInBytes(adjust);
   _comp->cg()->setFrameSizeInBytes(_stackPeakSize);

   // The Parameters are now in the local array, so we update the entries in the ParamTable with their values from the LocalTable
   ParamTableEntry entry;
   for (int i=0; i<_paramTable->getParamCount(); i += ((entry.notInitialized) ? 1 : entry.slots))
      {
      // The indexes should be the same in both tables
      // because (as far as we know) the parameters
      // are indexed the same as locals and parameters
      localTable->getEntry(i, &entry);
      _paramTable->setEntry(i, &entry);
      }

   TR::GCStackAtlas *atlas = _mjitCGGC->createStackAtlas(_comp, _paramTable, localTable);
   if (!atlas)
      return 0;
   _atlas = atlas;

   // Support to paint allocated frame slots.
   if (( _comp->getOption(TR_PaintAllocatedFrameSlotsDead) || _comp->getOption(TR_PaintAllocatedFrameSlotsFauxObject) )  && allocSize!=0)
      {
      uint64_t paintValue = 0;

      // Paint the slots with deadf00d
      if (_comp->getOption(TR_PaintAllocatedFrameSlotsDead))
         paintValue = (uint64_t)CONSTANT64(0xdeadf00ddeadf00d);
      else //Paint stack slots with a arbitrary object aligned address.
         paintValue = ((uintptr_t) ((uintptr_t)_comp->getOptions()->getHeapBase() + (uintptr_t) 4096));

      COPY_TEMPLATE(buffer, paintRegister, prologueSize);
      patchImm8(buffer, paintValue);
      for (int32_t i=_paramTable->getParamCount(); i<localTable->getLocalCount();)
         {
         if (entry.notInitialized)
            {
            i++;
            continue;
            }
         localTable->getEntry(i, &entry);
         COPY_TEMPLATE(buffer, paintLocal, prologueSize);
         patchImm4(buffer, i * pointerSize);
         i += entry.slots;
         }
      }
   uint32_t count = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? TR_DEFAULT_INITIAL_BCOUNT : TR_DEFAULT_INITIAL_COUNT;
   prologueSize += generateGCR(buffer, count, method, startPC);

   return prologueSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateEpologue(char *buffer)
   {
   buffer_size_t epologueSize = loadPreserveRegisters(buffer, _stackPeakSize-8);
   buffer += epologueSize;
   COPY_TEMPLATE(buffer, addRSPImm4, epologueSize);
   patchImm4(buffer, _stackPeakSize - _maxCalleeArgsSize);
   return epologueSize;
   }

// Macros to clean up the switch case for generate body
#define loadCasePrologue(byteCodeCaseType) \
   case TR_J9ByteCode::byteCodeCaseType
#define loadCaseBody(byteCodeCaseType) \
   goto GenericLoadCall
#define storeCasePrologue(byteCodeCaseType) \
   case TR_J9ByteCode::byteCodeCaseType
#define storeCaseBody(byteCodeCaseType) \
   goto GenericStoreCall

// used for each conditional and jump except gotow (wide)
#define branchByteCode(byteCode, template) \
   case byteCode: \
      { \
      _targetIndex = bci->next2BytesSigned() + bci->currentByteCodeIndex(); \
      COPY_TEMPLATE(buffer, template, codeGenSize); \
      MJIT::JumpTableEntry _entry(_targetIndex, buffer); \
      _jumpTable[_targetCounter] = _entry; \
      _targetCounter++; \
      break; \
      }

buffer_size_t
MJIT::CodeGenerator::generateBody(char *buffer, TR_J9ByteCodeIterator *bci)
   {
   buffer_size_t codeGenSize = 0;
   buffer_size_t calledCGSize = 0;
   int32_t _currentByteCodeIndex = 0;
   int32_t _targetIndex = 0;
   int32_t _targetCounter = 0;

#ifdef MJIT_DEBUG_BC_WALKING
   std::string _bcMnemonic, _bcText;
   u_int8_t _bcOpcode;
#endif

   uintptr_t SSEfloatRemainder;
   uintptr_t SSEdoubleRemainder;

   // Map between ByteCode Index to generated JITed code address
   char *_byteCodeIndexToJitTable[bci->maxByteCodeIndex()];

   // Each time we encounter a branch or goto
   // add an entry. After codegen completes use
   // these entries to patch addresses using _byteCodeIndexToJitTable.
   MJIT::JumpTableEntry _jumpTable[bci->maxByteCodeIndex()];

#ifdef MJIT_DEBUG_BC_WALKING
   MJIT_DEBUG_BC_LOG(_logFileFP, "\nMicroJIT Bytecode Dump:\n");
   bci->printByteCodes();
#endif

   struct TR::X86LinkageProperties *properties = &_linkage._properties;
   TR::RealRegister::RegNum returnRegIndex;
   U_16 slotCountRem = 0;
   bool isDouble = false;
   for (TR_J9ByteCode bc = bci->first(); bc != J9BCunknown; bc = bci->next())
      {
#ifdef MJIT_DEBUG_BC_WALKING
      // In cases of conjunction of two bytecodes, the _bcMnemonic needs to be replaced by correct bytecodes,
      // i.e., JBaload0getfield needs to be replaced by JBaload0
      // and JBnewdup by JBnew
      if (!strcmp(bci->currentMnemonic(), "JBaload0getfield"))
         {
         const char * newMnemonic = "JBaload0";
         _bcMnemonic = std::string(newMnemonic);
         }
      else if (!strcmp(bci->currentMnemonic(), "JBnewdup"))
         {
         const char * newMnemonic = "JBnew";
         _bcMnemonic = std::string(newMnemonic);
         }
      else
         {
         _bcMnemonic = std::string(bci->currentMnemonic());
         }
      _bcOpcode = bci->currentOpcode();
      _bcText = _bcMnemonic + "\n";
      MJIT_DEBUG_BC_LOG(_logFileFP, _bcText.c_str());
#endif

      _currentByteCodeIndex = bci->currentByteCodeIndex();

      // record BCI to JIT address
      _byteCodeIndexToJitTable[_currentByteCodeIndex] = buffer;

      switch (bc)
         {
         loadCasePrologue(J9BCiload0):
            loadCaseBody(J9BCiload0);
         loadCasePrologue(J9BCiload1):
            loadCaseBody(J9BCiload1);
         loadCasePrologue(J9BCiload2):
            loadCaseBody(J9BCiload2);
         loadCasePrologue(J9BCiload3):
            loadCaseBody(J9BCiload3);
         loadCasePrologue(J9BCiload):
            loadCaseBody(J9BCiload);
         loadCasePrologue(J9BClload0):
            loadCaseBody(J9BClload0);
         loadCasePrologue(J9BClload1):
            loadCaseBody(J9BClload1);
         loadCasePrologue(J9BClload2):
            loadCaseBody(J9BClload2);
         loadCasePrologue(J9BClload3):
            loadCaseBody(J9BClload3);
         loadCasePrologue(J9BClload):
            loadCaseBody(J9BClload);
         loadCasePrologue(J9BCfload0):
            loadCaseBody(J9BCfload0);
         loadCasePrologue(J9BCfload1):
            loadCaseBody(J9BCfload1);
         loadCasePrologue(J9BCfload2):
            loadCaseBody(J9BCfload2);
         loadCasePrologue(J9BCfload3):
            loadCaseBody(J9BCfload3);
         loadCasePrologue(J9BCfload):
            loadCaseBody(J9BCfload);
         loadCasePrologue(J9BCdload0):
            loadCaseBody(J9BCdload0);
         loadCasePrologue(J9BCdload1):
            loadCaseBody(J9BCdload1);
         loadCasePrologue(J9BCdload2):
            loadCaseBody(J9BCdload2);
         loadCasePrologue(J9BCdload3):
            loadCaseBody(J9BCdload3);
         loadCasePrologue(J9BCdload):
            loadCaseBody(J9BCdload);
         loadCasePrologue(J9BCaload0):
            loadCaseBody(J9BCaload0);
         loadCasePrologue(J9BCaload1):
            loadCaseBody(J9BCaload1);
         loadCasePrologue(J9BCaload2):
            loadCaseBody(J9BCaload2);
         loadCasePrologue(J9BCaload3):
            loadCaseBody(J9BCaload3);
         loadCasePrologue(J9BCaload):
            loadCaseBody(J9BCaload);
         GenericLoadCall:
            if(calledCGSize = generateLoad(buffer, bc, bci))
               {
                  buffer += calledCGSize;
                  codeGenSize += calledCGSize;
               }
            else
               {
#ifdef MJIT_DEBUG_BC_WALKING
               trfprintf(_logFileFP, "Unsupported load bytecode - %s (%d)\n", _bcMnemonic.c_str(), _bcOpcode);
#endif
               return 0;
               }
            break;
         storeCasePrologue(J9BCistore0):
            storeCaseBody(J9BCistore0);
         storeCasePrologue(J9BCistore1):
            storeCaseBody(J9BCistore1);
         storeCasePrologue(J9BCistore2):
            storeCaseBody(J9BCistore2);
         storeCasePrologue(J9BCistore3):
            storeCaseBody(J9BCistore3);
         storeCasePrologue(J9BCistore):
            storeCaseBody(J9BCistore);
         storeCasePrologue(J9BClstore0):
            storeCaseBody(J9BClstore0);
         storeCasePrologue(J9BClstore1):
            storeCaseBody(J9BClstore1);
         storeCasePrologue(J9BClstore2):
            storeCaseBody(J9BClstore2);
         storeCasePrologue(J9BClstore3):
            storeCaseBody(J9BClstore3);
         storeCasePrologue(J9BClstore):
            storeCaseBody(J9BClstore);
         storeCasePrologue(J9BCfstore0):
            storeCaseBody(J9BCfstore0);
         storeCasePrologue(J9BCfstore1):
            storeCaseBody(J9BCfstore1);
         storeCasePrologue(J9BCfstore2):
            storeCaseBody(J9BCfstore2);
         storeCasePrologue(J9BCfstore3):
            storeCaseBody(J9BCfstore3);
         storeCasePrologue(J9BCfstore):
            storeCaseBody(J9BCfstore);
         storeCasePrologue(J9BCdstore0):
            storeCaseBody(J9BCdstore0);
         storeCasePrologue(J9BCdstore1):
            storeCaseBody(J9BCdstore1);
         storeCasePrologue(J9BCdstore2):
            storeCaseBody(J9BCdstore2);
         storeCasePrologue(J9BCdstore3):
            storeCaseBody(J9BCdstore3);
         storeCasePrologue(J9BCdstore):
            storeCaseBody(J9BCdstore);
         storeCasePrologue(J9BCastore0):
            storeCaseBody(J9BCastore0);
         storeCasePrologue(J9BCastore1):
            storeCaseBody(J9BCastore1);
         storeCasePrologue(J9BCastore2):
            storeCaseBody(J9BCastore2);
         storeCasePrologue(J9BCastore3):
            storeCaseBody(J9BCastore3);
         storeCasePrologue(J9BCastore):
            storeCaseBody(J9BCastore);
         GenericStoreCall:
            if (calledCGSize = generateStore(buffer, bc, bci))
               {
               buffer += calledCGSize;
               codeGenSize += calledCGSize;
               }
            else
               {
#ifdef MJIT_DEBUG_BC_WALKING
               trfprintf(_logFileFP, "Unsupported store bytecode - %s (%d)\n", _bcMnemonic.c_str(), _bcOpcode);
#endif
               return 0;
               }
            break;
         branchByteCode(TR_J9ByteCode::J9BCgoto,gotoTemplate);
         branchByteCode(TR_J9ByteCode::J9BCifne,ifneTemplate);
         branchByteCode(TR_J9ByteCode::J9BCifeq,ifeqTemplate);
         branchByteCode(TR_J9ByteCode::J9BCiflt,ifltTemplate);
         branchByteCode(TR_J9ByteCode::J9BCifle,ifleTemplate);
         branchByteCode(TR_J9ByteCode::J9BCifgt,ifgtTemplate);
         branchByteCode(TR_J9ByteCode::J9BCifge,ifgeTemplate);
         branchByteCode(TR_J9ByteCode::J9BCificmpeq,ificmpeqTemplate);
         branchByteCode(TR_J9ByteCode::J9BCificmpne,ificmpneTemplate);
         branchByteCode(TR_J9ByteCode::J9BCificmplt,ificmpltTemplate);
         branchByteCode(TR_J9ByteCode::J9BCificmple,ificmpleTemplate);
         branchByteCode(TR_J9ByteCode::J9BCificmpge,ificmpgeTemplate);
         branchByteCode(TR_J9ByteCode::J9BCificmpgt,ificmpgtTemplate);
         branchByteCode(TR_J9ByteCode::J9BCifacmpeq,ifacmpeqTemplate);
         branchByteCode(TR_J9ByteCode::J9BCifacmpne,ifacmpneTemplate);
         branchByteCode(TR_J9ByteCode::J9BCifnull,ifnullTemplate);
         branchByteCode(TR_J9ByteCode::J9BCifnonnull,ifnonnullTemplate);
         /* Commenting out as this is currently untested
         case TR_J9ByteCode::J9BCgotow:
            {
            _targetIndex = bci->next4BytesSigned() + bci->currentByteCodeIndex(); // Copied because of 4 byte jump
            COPY_TEMPLATE(buffer, gotoTemplate, codeGenSize);
            MJIT::JumpTableEntry _entry(_targetIndex, buffer);
            _jumpTable[_targetCounter] = _entry;
            _targetCounter++;
            break;
            }
         */
         case TR_J9ByteCode::J9BClcmp:
            COPY_TEMPLATE(buffer, lcmpTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfcmpl:
            COPY_TEMPLATE(buffer, fcmplTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfcmpg:
            COPY_TEMPLATE(buffer, fcmpgTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdcmpl:
            COPY_TEMPLATE(buffer, dcmplTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdcmpg:
            COPY_TEMPLATE(buffer, dcmpgTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCpop:
            COPY_TEMPLATE(buffer, popTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCpop2:
            COPY_TEMPLATE(buffer, pop2Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCswap:
            COPY_TEMPLATE(buffer, swapTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdup:
            COPY_TEMPLATE(buffer, dupTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdupx1:
            COPY_TEMPLATE(buffer, dupx1Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdupx2:
            COPY_TEMPLATE(buffer, dupx2Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdup2:
            COPY_TEMPLATE(buffer, dup2Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdup2x1:
            COPY_TEMPLATE(buffer, dup2x1Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdup2x2:
            COPY_TEMPLATE(buffer, dup2x2Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCgetfield:
            if (calledCGSize = generateGetField(buffer, bci))
               {
               buffer += calledCGSize;
               codeGenSize += calledCGSize;
               }
            else
               {
#ifdef MJIT_DEBUG_BC_WALKING
               trfprintf(_logFileFP, "Unsupported getField bytecode  - %s (%d)\n", _bcMnemonic.c_str(), _bcOpcode);
#endif
               return 0;
               }
            break;
         case TR_J9ByteCode::J9BCputfield:
            return 0;
            if (calledCGSize = generatePutField(buffer, bci))
               {
               buffer += calledCGSize;
               codeGenSize += calledCGSize;
               }
            else
               {
#ifdef MJIT_DEBUG_BC_WALKING
               trfprintf(_logFileFP, "Unsupported putField bytecode  - %s (%d)\n", _bcMnemonic.c_str(), _bcOpcode);
#endif
               return 0;
               }
            break;
         case TR_J9ByteCode::J9BCgetstatic:
            if (calledCGSize = generateGetStatic(buffer, bci))
               {
               buffer += calledCGSize;
               codeGenSize += calledCGSize;
               }
            else
               {
#ifdef MJIT_DEBUG_BC_WALKING
               trfprintf(_logFileFP, "Unsupported getstatic bytecode  - %s (%d)\n", _bcMnemonic.c_str(), _bcOpcode);
#endif
               return 0;
               }
            break;
         case TR_J9ByteCode::J9BCputstatic:
            return 0;
            if (calledCGSize = generatePutStatic(buffer, bci))
               {
               buffer += calledCGSize;
               codeGenSize += calledCGSize;
               }
            else
               {
#ifdef MJIT_DEBUG_BC_WALKING
               trfprintf(_logFileFP, "Unsupported putstatic bytecode - %s %d\n", _bcMnemonic.c_str(), _bcOpcode);
#endif
               return 0;
               }
            break;
         case TR_J9ByteCode::J9BCiadd:
            COPY_TEMPLATE(buffer, iAddTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCisub:
            COPY_TEMPLATE(buffer, iSubTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCimul:
            COPY_TEMPLATE(buffer, iMulTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCidiv:
            COPY_TEMPLATE(buffer, iDivTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCirem:
            COPY_TEMPLATE(buffer, iRemTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCineg:
            COPY_TEMPLATE(buffer, iNegTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCishl:
            COPY_TEMPLATE(buffer, iShlTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCishr:
            COPY_TEMPLATE(buffer, iShrTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiushr:
            COPY_TEMPLATE(buffer, iUshrTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiand:
            COPY_TEMPLATE(buffer, iAndTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCior:
            COPY_TEMPLATE(buffer, iOrTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCixor:
            COPY_TEMPLATE(buffer, iXorTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCi2l:
            COPY_TEMPLATE(buffer, i2lTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCl2i:
            COPY_TEMPLATE(buffer, l2iTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCi2b:
            COPY_TEMPLATE(buffer, i2bTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCi2s:
            COPY_TEMPLATE(buffer, i2sTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCi2c:
            COPY_TEMPLATE(buffer, i2cTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCi2d:
            COPY_TEMPLATE(buffer, i2dTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCl2d:
            COPY_TEMPLATE(buffer, l2dTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCd2i:
            COPY_TEMPLATE(buffer, d2iTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCd2l:
            COPY_TEMPLATE(buffer, d2lTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiconstm1:
            COPY_TEMPLATE(buffer, iconstm1Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiconst0:
            COPY_TEMPLATE(buffer, iconst0Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiconst1:
            COPY_TEMPLATE(buffer, iconst1Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiconst2:
            COPY_TEMPLATE(buffer, iconst2Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiconst3:
            COPY_TEMPLATE(buffer, iconst3Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiconst4:
            COPY_TEMPLATE(buffer, iconst4Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiconst5:
            COPY_TEMPLATE(buffer, iconst5Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCbipush:
            COPY_TEMPLATE(buffer, bipushTemplate, codeGenSize);
            patchImm4(buffer, (U_32)bci->nextByteSigned());
            break;
         case TR_J9ByteCode::J9BCsipush:
            COPY_TEMPLATE(buffer, sipushTemplatePrologue, codeGenSize);
            patchImm2(buffer, (U_32)bci->next2Bytes());
            COPY_TEMPLATE(buffer, sipushTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCiinc:
            {
            uint8_t index = bci->nextByte();
            int8_t value = bci->nextByteSigned(2);
            COPY_TEMPLATE(buffer, iIncTemplate_01_load, codeGenSize);
            patchImm4(buffer, (U_32)(index*8));
            COPY_TEMPLATE(buffer, iIncTemplate_02_add, codeGenSize);
            patchImm1(buffer, value);
            COPY_TEMPLATE(buffer, iIncTemplate_03_store, codeGenSize);
            patchImm4(buffer, (U_32)(index*8));
            break;
            }
         case TR_J9ByteCode::J9BCladd:
            COPY_TEMPLATE(buffer, lAddTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClsub:
            COPY_TEMPLATE(buffer, lSubTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClmul:
            COPY_TEMPLATE(buffer, lMulTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCldiv:
            COPY_TEMPLATE(buffer, lDivTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClrem:
            COPY_TEMPLATE(buffer, lRemTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClneg:
            COPY_TEMPLATE(buffer, lNegTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClshl:
            COPY_TEMPLATE(buffer, lShlTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClshr:
            COPY_TEMPLATE(buffer, lShrTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClushr:
            COPY_TEMPLATE(buffer, lUshrTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCland:
            COPY_TEMPLATE(buffer, lAndTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClor:
            COPY_TEMPLATE(buffer, lOrTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClxor:
            COPY_TEMPLATE(buffer, lXorTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClconst0:
            COPY_TEMPLATE(buffer, lconst0Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BClconst1:
            COPY_TEMPLATE(buffer, lconst1Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfadd:
            COPY_TEMPLATE(buffer, fAddTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfsub:
            COPY_TEMPLATE(buffer, fSubTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfmul:
            COPY_TEMPLATE(buffer, fMulTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfdiv:
            COPY_TEMPLATE(buffer, fDivTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfrem:
            COPY_TEMPLATE(buffer, fRemTemplate, codeGenSize);
            SSEfloatRemainder = getHelperOrTrampolineAddress(TR_AMD64floatRemainder, (uintptr_t)buffer);
            PATCH_RELATIVE_ADDR_32_BIT(buffer, (intptr_t)SSEfloatRemainder);
            slotCountRem = 1;
            goto genRem;
         case TR_J9ByteCode::J9BCdrem:
            COPY_TEMPLATE(buffer, dRemTemplate, codeGenSize);
            SSEdoubleRemainder = getHelperOrTrampolineAddress(TR_AMD64doubleRemainder, (uintptr_t)buffer);
            PATCH_RELATIVE_ADDR_32_BIT(buffer, (intptr_t)SSEdoubleRemainder);
            slotCountRem = 2;
            isDouble = true;
            goto genRem;
         genRem:
            returnRegIndex = properties->getFloatReturnRegister();
            COPY_TEMPLATE(buffer, retTemplate_sub, codeGenSize);
            patchImm1(buffer, slotCountRem*8);
            switch (returnRegIndex)
               {
               case TR::RealRegister::xmm0:
                  if (isDouble)
                     COPY_TEMPLATE(buffer, loadDxmm0Return, codeGenSize);
                  else
                     COPY_TEMPLATE(buffer, loadxmm0Return, codeGenSize);
                  break;
               }
            break;
         case TR_J9ByteCode::J9BCfneg:
            COPY_TEMPLATE(buffer, fNegTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfconst0:
            COPY_TEMPLATE(buffer, fconst0Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfconst1:
            COPY_TEMPLATE(buffer, fconst1Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCfconst2:
            COPY_TEMPLATE(buffer, fconst2Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdconst0:
            COPY_TEMPLATE(buffer, dconst0Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdconst1:
            COPY_TEMPLATE(buffer, dconst1Template, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdadd:
            COPY_TEMPLATE(buffer, dAddTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdsub:
            COPY_TEMPLATE(buffer, dSubTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdmul:
            COPY_TEMPLATE(buffer, dMulTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCddiv:
            COPY_TEMPLATE(buffer, dDivTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCdneg:
            COPY_TEMPLATE(buffer, dNegTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCi2f:
            COPY_TEMPLATE(buffer, i2fTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCf2i:
            COPY_TEMPLATE(buffer, f2iTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCl2f:
            COPY_TEMPLATE(buffer, l2fTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCf2l:
            COPY_TEMPLATE(buffer, f2lTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCd2f:
            COPY_TEMPLATE(buffer, d2fTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCf2d:
            COPY_TEMPLATE(buffer, f2dTemplate, codeGenSize);
            break;
         case TR_J9ByteCode::J9BCReturnB:    /* FALLTHROUGH */
         case TR_J9ByteCode::J9BCReturnC:    /* FALLTHROUGH */
         case TR_J9ByteCode::J9BCReturnS:    /* FALLTHROUGH */
         case TR_J9ByteCode::J9BCReturnZ:    /* FALLTHROUGH */
         case TR_J9ByteCode::J9BCgenericReturn:
            if (calledCGSize = generateReturn(buffer, _compilee->returnType(), properties))
               {
               buffer += calledCGSize;
               codeGenSize += calledCGSize;
               }
            else if (_compilee->returnType() == TR::Int64)
               {
               buffer_size_t epilogueSize = generateEpologue(buffer);
               buffer += epilogueSize;
               codeGenSize += epilogueSize;
               COPY_TEMPLATE(buffer, eaxReturnTemplate, codeGenSize);
               COPY_TEMPLATE(buffer, retTemplate_add, codeGenSize);
               patchImm1(buffer, 8);
               COPY_TEMPLATE(buffer, vReturnTemplate, codeGenSize);
               }
            else
               {
#ifdef MJIT_DEBUG_BC_WALKING
               trfprintf(_logFileFP, "Unknown Return type: %d\n", _compilee->returnType());
#endif
               return 0;
               }
            break;
         case TR_J9ByteCode::J9BCinvokestatic:
            if (calledCGSize = generateInvokeStatic(buffer, bci, properties))
               {
               buffer += calledCGSize;
               codeGenSize += calledCGSize;
               }
            else
               {
#ifdef MJIT_DEBUG_BC_WALKING
               trfprintf(_logFileFP, "Unsupported invokestatic bytecode %d\n", bc);
#endif
               return 0;
               }
            break;
         default:
#ifdef MJIT_DEBUG_BC_WALKING
            trfprintf(_logFileFP, "no match for bytecode - %s (%d)\n", _bcMnemonic.c_str(), _bcOpcode);
#endif
            return 0;
         }
      }

   // patch branches and goto addresses
   for (int i = 0; i < _targetCounter; i++)
      {
      MJIT::JumpTableEntry e = _jumpTable[i];
      char *_target = _byteCodeIndexToJitTable[e.byteCodeIndex];
      PATCH_RELATIVE_ADDR_32_BIT(e.codeCacheAddress, (uintptr_t)_target);
      }

   return codeGenSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateEdgeCounter(char *buffer, TR_J9ByteCodeIterator *bci)
   {
   buffer_size_t returnSize = 0;
   // TODO: Get the pointer to the profiling counter
   uint32_t *profilingCounterLocation = NULL;

   COPY_TEMPLATE(buffer, loadCounter, returnSize);
   patchImm8(buffer, (uint64_t)profilingCounterLocation);
   COPY_TEMPLATE(buffer, incrementCounter, returnSize);
   patchImm8(buffer, (uint64_t)profilingCounterLocation);

   return returnSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateReturn(char *buffer, TR::DataType dt, struct TR::X86LinkageProperties *properties)
   {
   buffer_size_t returnSize = 0;
   buffer_size_t calledCGSize = 0;
   TR::RealRegister::RegNum returnRegIndex;
   U_16 slotCount = 0;
   bool isAddressOrLong = false;
   switch (dt)
      {
      case TR::Address:
         slotCount = 1;
         isAddressOrLong = true;
         returnRegIndex = properties->getIntegerReturnRegister();
         goto genReturn;
      case TR::Int64:
         slotCount = 2;
         isAddressOrLong = true;
         returnRegIndex = properties->getIntegerReturnRegister();
         goto genReturn;
      case TR::Int8:                /* FALLTHROUGH */
      case TR::Int16:               /* FALLTHROUGH */
      case TR::Int32:               /* FALLTHROUGH */
         slotCount = 1;
         returnRegIndex = properties->getIntegerReturnRegister();
         goto genReturn;
      case TR::Float:
         slotCount = 1;
         returnRegIndex = properties->getFloatReturnRegister();
         goto genReturn;
      case TR::Double:
         slotCount = 2;
         returnRegIndex = properties->getFloatReturnRegister();
         goto genReturn;
      case TR::NoType:
         returnRegIndex = TR::RealRegister::NoReg;
         goto genReturn;
      genReturn:
         calledCGSize = generateEpologue(buffer);
         buffer += calledCGSize;
         returnSize += calledCGSize;
         switch (returnRegIndex)
            {
            case TR::RealRegister::eax:
               if (isAddressOrLong)
                  COPY_TEMPLATE(buffer, raxReturnTemplate, returnSize);
               else
                  COPY_TEMPLATE(buffer, eaxReturnTemplate, returnSize);
               goto genSlots;
            case TR::RealRegister::xmm0:
               COPY_TEMPLATE(buffer, xmm0ReturnTemplate, returnSize);
               goto genSlots;
            case TR::RealRegister::NoReg:
               goto genSlots;
            genSlots:
               if (slotCount)
                  {
                  COPY_TEMPLATE(buffer, retTemplate_add, returnSize);
                  patchImm1(buffer, slotCount*8);
                  }
               COPY_TEMPLATE(buffer, vReturnTemplate, returnSize);
               break;
            }
         break;
      default:
         trfprintf(_logFileFP, "Argument type %s is not supported\n", dt.toString());
         break;
      }
   return returnSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateLoad(char *buffer, TR_J9ByteCode bc, TR_J9ByteCodeIterator *bci)
   {
   char *signature = _compilee->signatureChars();
   int index = -1;
   buffer_size_t loadSize = 0;
   switch (bc)
      {
      GenerateATemplate:
         COPY_TEMPLATE(buffer, aloadTemplatePrologue, loadSize);
         patchImm4(buffer, (U_32)(index*8));
         COPY_TEMPLATE(buffer, loadTemplate, loadSize);
         break;
      GenerateITemplate:
         COPY_TEMPLATE(buffer, iloadTemplatePrologue, loadSize);
         patchImm4(buffer, (U_32)(index*8));
         COPY_TEMPLATE(buffer, loadTemplate, loadSize);
         break;
      GenerateLTemplate:
         COPY_TEMPLATE(buffer, lloadTemplatePrologue, loadSize);
         patchImm4(buffer, (U_32)(index*8));
         COPY_TEMPLATE(buffer, loadTemplate, loadSize);
         break;
      case TR_J9ByteCode::J9BClload0:
      case TR_J9ByteCode::J9BCdload0:
         index = 0;
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfload0:
      case TR_J9ByteCode::J9BCiload0:
         index = 0;
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCaload0:
         index = 0;
         goto GenerateATemplate;
      case TR_J9ByteCode::J9BClload1:
      case TR_J9ByteCode::J9BCdload1:
         index = 1;
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfload1:
      case TR_J9ByteCode::J9BCiload1:
         index = 1;
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCaload1:
         index = 1;
         goto GenerateATemplate;
      case TR_J9ByteCode::J9BClload2:
      case TR_J9ByteCode::J9BCdload2:
         index = 2;
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfload2:
      case TR_J9ByteCode::J9BCiload2:
         index = 2;
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCaload2:
         index = 2;
         goto GenerateATemplate;
      case TR_J9ByteCode::J9BClload3:
      case TR_J9ByteCode::J9BCdload3:
         index = 3;
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfload3:
      case TR_J9ByteCode::J9BCiload3:
         index = 3;
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCaload3:
         index = 3;
         goto GenerateATemplate;
      case TR_J9ByteCode::J9BClload:
      case TR_J9ByteCode::J9BCdload:
         index = bci->nextByte();
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfload:
      case TR_J9ByteCode::J9BCiload:
         index = bci->nextByte();
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCaload:
         index = bci->nextByte();
         goto GenerateATemplate;
      default:
         return 0;
      }
   return loadSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateStore(char *buffer, TR_J9ByteCode bc, TR_J9ByteCodeIterator *bci)
   {
   char *signature = _compilee->signatureChars();
   int index = -1;
   buffer_size_t storeSize = 0;
   switch (bc)
      {
      GenerateATemplate:
         COPY_TEMPLATE(buffer, astoreTemplate, storeSize);
         patchImm4(buffer, (U_32)(index*8));
         break;
      GenerateLTemplate:
         COPY_TEMPLATE(buffer, lstoreTemplate, storeSize);
         patchImm4(buffer, (U_32)(index*8));
         break;
      GenerateITemplate:
         COPY_TEMPLATE(buffer, istoreTemplate, storeSize);
         patchImm4(buffer, (U_32)(index*8));
         break;
      case TR_J9ByteCode::J9BClstore0:
      case TR_J9ByteCode::J9BCdstore0:
         index = 0;
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfstore0:
      case TR_J9ByteCode::J9BCistore0:
         index = 0;
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCastore0:
         index = 0;
         goto GenerateATemplate;
      case TR_J9ByteCode::J9BClstore1:
      case TR_J9ByteCode::J9BCdstore1:
         index = 1;
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfstore1:
      case TR_J9ByteCode::J9BCistore1:
         index = 1;
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCastore1:
         index = 1;
         goto GenerateATemplate;
      case TR_J9ByteCode::J9BClstore2:
      case TR_J9ByteCode::J9BCdstore2:
         index = 2;
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfstore2:
      case TR_J9ByteCode::J9BCistore2:
         index = 2;
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCastore2:
         index = 2;
         goto GenerateATemplate;
      case TR_J9ByteCode::J9BClstore3:
      case TR_J9ByteCode::J9BCdstore3:
         index = 3;
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfstore3:
      case TR_J9ByteCode::J9BCistore3:
         index = 3;
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCastore3:
         index = 3;
         goto GenerateATemplate;
      case TR_J9ByteCode::J9BClstore:
      case TR_J9ByteCode::J9BCdstore:
         index = bci->nextByte();
         goto GenerateLTemplate;
      case TR_J9ByteCode::J9BCfstore:
      case TR_J9ByteCode::J9BCistore:
         index = bci->nextByte();
         goto GenerateITemplate;
      case TR_J9ByteCode::J9BCastore:
         index = bci->nextByte();
         goto GenerateATemplate;
      default:
         return 0;
      }
   return storeSize;
   }

// TODO: see ::emitSnippitCode and redo this to use snippits.
buffer_size_t
MJIT::CodeGenerator::generateArgumentMoveForStaticMethod(
      char *buffer,
      TR_ResolvedMethod *staticMethod,
      char *typeString,
      U_16 typeStringLength,
      U_16 slotCount,
      struct TR::X86LinkageProperties *properties)
   {
   buffer_size_t argumentMoveSize = 0;

   // Pull out parameters and find their place for calling
   U_16 paramCount = MJIT::getParamCount(typeString, typeStringLength);
   U_16 actualParamCount = MJIT::getActualParamCount(typeString, typeStringLength);
   // TODO: Implement passing arguments on stack when surpassing register count.
   if (actualParamCount > 4)
      return 0;

   if (!paramCount)
      return -1;

   MJIT::ParamTableEntry paramTableEntries[paramCount];
   for (int i=0; i<paramCount; i++)
      paramTableEntries[i] = initializeParamTableEntry();

   int error_code = 0;
   MJIT::RegisterStack stack = MJIT::mapIncomingParams(typeString, typeStringLength, &error_code, paramTableEntries, actualParamCount, _logFileFP);
   if (error_code)
      return 0;

   // Move arguments from MJIT computation stack to registers and stack offsets for call
   MJIT::ParamTable paramTable(paramTableEntries, paramCount, actualParamCount, &stack);

   int16_t lastIndex = paramTable.getParamCount()-1;
   while (lastIndex > 0)
      {
      if (paramTableEntries[lastIndex].notInitialized)
         lastIndex--;
      else
         break;
      }
   bool isLongOrDouble;
   bool isReference;
   TR::RealRegister::RegNum argRegNum;
   MJIT::ParamTableEntry entry;
   while (actualParamCount > 0)
      {
      isLongOrDouble = false;
      isReference = false;
      argRegNum = TR::RealRegister::NoReg;
      switch (actualParamCount-1)
         {
         case 0:    /* FALLTHROUGH */
         case 1:    /* FALLTHROUGH */
         case 2:    /* FALLTHROUGH */
         case 3:
            {
            switch (typeString[(actualParamCount-1)+3])
               {
               case MJIT::BYTE_TYPE_CHARACTER:
               case MJIT::CHAR_TYPE_CHARACTER:
               case MJIT::INT_TYPE_CHARACTER:
               case MJIT::SHORT_TYPE_CHARACTER:
               case MJIT::BOOLEAN_TYPE_CHARACTER:
                  argRegNum = properties->getIntegerArgumentRegister(actualParamCount-1);
                  goto genArg;
               case MJIT::LONG_TYPE_CHARACTER:
                  isLongOrDouble = true;
                  argRegNum = properties->getIntegerArgumentRegister(actualParamCount-1);
                  goto genArg;
               case MJIT::CLASSNAME_TYPE_CHARACTER:
                  isReference = true;
                  argRegNum = properties->getIntegerArgumentRegister(actualParamCount-1);
                  goto genArg;
               case MJIT::DOUBLE_TYPE_CHARACTER:
                  isLongOrDouble = true;
                  argRegNum = properties->getFloatArgumentRegister(actualParamCount-1);
                  goto genArg;
               case MJIT::FLOAT_TYPE_CHARACTER:
                  argRegNum = properties->getFloatArgumentRegister(actualParamCount-1);
                  goto genArg;
               genArg:
                  switch (argRegNum)
                     {
                     case TR::RealRegister::eax:
                        if (isLongOrDouble)
                           COPY_TEMPLATE(buffer, moveraxForCall, argumentMoveSize);
                        else if (isReference)
                           COPY_TEMPLATE(buffer, moveraxRefForCall, argumentMoveSize);
                        else
                           COPY_TEMPLATE(buffer, moveeaxForCall, argumentMoveSize);
                        break;
                     case TR::RealRegister::esi:
                        if (isLongOrDouble)
                           COPY_TEMPLATE(buffer, moversiForCall, argumentMoveSize);
                        else if (isReference)
                           COPY_TEMPLATE(buffer, moversiRefForCall, argumentMoveSize);
                        else
                           COPY_TEMPLATE(buffer, moveesiForCall, argumentMoveSize);
                        break;
                     case TR::RealRegister::edx:
                        if (isLongOrDouble)
                           COPY_TEMPLATE(buffer, moverdxForCall, argumentMoveSize);
                        else if (isReference)
                           COPY_TEMPLATE(buffer, moverdxRefForCall, argumentMoveSize);
                        else
                           COPY_TEMPLATE(buffer, moveedxForCall, argumentMoveSize);
                        break;
                     case TR::RealRegister::ecx:
                        if (isLongOrDouble)
                           COPY_TEMPLATE(buffer, movercxForCall, argumentMoveSize);
                        else if (isReference)
                           COPY_TEMPLATE(buffer, movercxRefForCall, argumentMoveSize);
                        else
                           COPY_TEMPLATE(buffer, moveecxForCall, argumentMoveSize);
                        break;
                     case TR::RealRegister::xmm0:
                        if (isLongOrDouble)
                           COPY_TEMPLATE(buffer, moveDxmm0ForCall, argumentMoveSize);
                        else
                           COPY_TEMPLATE(buffer, movexmm0ForCall, argumentMoveSize);
                        break;
                     case TR::RealRegister::xmm1:
                        if (isLongOrDouble)
                           COPY_TEMPLATE(buffer, moveDxmm1ForCall, argumentMoveSize);
                        else
                           COPY_TEMPLATE(buffer, movexmm1ForCall, argumentMoveSize);
                        break;
                     case TR::RealRegister::xmm2:
                        if (isLongOrDouble)
                           COPY_TEMPLATE(buffer, moveDxmm2ForCall, argumentMoveSize);
                        else
                           COPY_TEMPLATE(buffer, movexmm2ForCall, argumentMoveSize);
                        break;
                     case TR::RealRegister::xmm3:
                        if (isLongOrDouble)
                           COPY_TEMPLATE(buffer, moveDxmm3ForCall, argumentMoveSize);
                        else
                           COPY_TEMPLATE(buffer, movexmm3ForCall, argumentMoveSize);
                        break;
                     }
                  break;
               }
            break;
            }
         default:
            // TODO: Add stack argument passing here.
            break;
         }
      actualParamCount--;
      }

   argumentMoveSize += saveArgsOnStack(buffer, -8, &paramTable);

   return argumentMoveSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateInvokeStatic(char *buffer, TR_J9ByteCodeIterator *bci, struct TR::X86LinkageProperties *properties)
   {
   buffer_size_t invokeStaticSize = 0;
   int32_t cpIndex = (int32_t)bci->next2Bytes();

   // Attempt to resolve the address at compile time. This can fail, and if it does we must fail the compilation for now.
   // In the future research project we could attempt runtime resolution. TR does this with self modifying code.
   // These are all from TR and likely have implications on how we use this address. However, we do not know that as of yet.
   bool isUnresolvedInCP;
   TR_ResolvedMethod *resolved = _compilee->getResolvedStaticMethod(_comp, cpIndex, &isUnresolvedInCP);
   // TODO: Split this into generateInvokeStaticJava and generateInvokeStaticNative
   // and call the correct one with required args. Do not turn this method into a mess.
   if (!resolved || resolved->isNative())
      return 0;

   // Get method signature
   J9Method *ramMethod = static_cast<TR_ResolvedJ9Method*>(resolved)->ramMethod();
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
   U_16 maxLength = J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod));
   char typeString[maxLength];
   if (MJIT::nativeSignature(ramMethod, typeString))
      return 0;

   // TODO: Find the maximum size that will need to be saved
   //       for calling a method and reserve it in the prologue.
   //       This could be done during the meta-data gathering phase.
   U_16 slotCount = MJIT::getMJITStackSlotCount(typeString, maxLength);

   // Generate template and instantiate immediate values
   buffer_size_t argMoveSize = generateArgumentMoveForStaticMethod(buffer, resolved, typeString, maxLength, slotCount, properties);

   if (!argMoveSize)
      return 0;
   else if(-1 == argMoveSize)
      argMoveSize = 0;
   buffer += argMoveSize;
   invokeStaticSize += argMoveSize;

   // Save Caller preserved registers
   COPY_TEMPLATE(buffer, saveR10Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x0 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, saveR11Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x8 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, saveR12Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x10 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, saveR13Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x18 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, saveR14Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x20 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, saveR15Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x28 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, invokeStaticTemplate, invokeStaticSize);
   patchImm8(buffer, (U_64)ramMethod);

   // Call the glue code
   COPY_TEMPLATE(buffer, call4ByteRel, invokeStaticSize);
   intptr_t interpreterStaticAndSpecialGlue = getHelperOrTrampolineAddress(TR_X86interpreterStaticAndSpecialGlue, (uintptr_t)buffer);
   PATCH_RELATIVE_ADDR_32_BIT(buffer, interpreterStaticAndSpecialGlue);

   // Load Caller preserved registers
   COPY_TEMPLATE(buffer, loadR10Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x00 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, loadR11Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x08 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, loadR12Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x10 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, loadR13Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x18 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, loadR14Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x20 + (slotCount * 8)));
   COPY_TEMPLATE(buffer, loadR15Offset, invokeStaticSize);
   patchImm4(buffer, (U_32)(0x28 + (slotCount * 8)));
   TR::RealRegister::RegNum returnRegIndex;
   slotCount = 0;
   bool isAddressOrLongorDouble = false;

   switch (typeString[0])
      {
      case MJIT::VOID_TYPE_CHARACTER:
         break;
      case MJIT::BOOLEAN_TYPE_CHARACTER:
      case MJIT::BYTE_TYPE_CHARACTER:
      case MJIT::CHAR_TYPE_CHARACTER:
      case MJIT::SHORT_TYPE_CHARACTER:
      case MJIT::INT_TYPE_CHARACTER:
         slotCount = 1;
         returnRegIndex = properties->getIntegerReturnRegister();
         goto genStatic;
      case MJIT::CLASSNAME_TYPE_CHARACTER:
         slotCount = 1;
         isAddressOrLongorDouble = true;
         returnRegIndex = properties->getIntegerReturnRegister();
         goto genStatic;
      case MJIT::LONG_TYPE_CHARACTER:
         slotCount = 2;
         isAddressOrLongorDouble = true;
         returnRegIndex = properties->getIntegerReturnRegister();
         goto genStatic;
      case MJIT::FLOAT_TYPE_CHARACTER:
         slotCount = 1;
         returnRegIndex = properties->getFloatReturnRegister();
         goto genStatic;
      case MJIT::DOUBLE_TYPE_CHARACTER:
         slotCount = 2;
         isAddressOrLongorDouble = true;
         returnRegIndex = properties->getFloatReturnRegister();
         goto genStatic;
      genStatic:
         if (slotCount)
            {
            COPY_TEMPLATE(buffer, retTemplate_sub, invokeStaticSize);
            patchImm1(buffer, slotCount*8);
            }
         switch (returnRegIndex)
            {
            case TR::RealRegister::eax:
               if (isAddressOrLongorDouble)
                  COPY_TEMPLATE(buffer, loadraxReturn, invokeStaticSize);
               else
                  COPY_TEMPLATE(buffer, loadeaxReturn, invokeStaticSize);
               break;
            case TR::RealRegister::xmm0:
               if (isAddressOrLongorDouble)
                  COPY_TEMPLATE(buffer, loadDxmm0Return, invokeStaticSize);
               else
                  COPY_TEMPLATE(buffer, loadxmm0Return, invokeStaticSize);
               break;
            }
         break;
      default:
         MJIT_ASSERT(_logFileFP, false, "Bad return type.");
      }
   return invokeStaticSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateGetField(char *buffer, TR_J9ByteCodeIterator *bci)
   {
   buffer_size_t getFieldSize = 0;
   int32_t cpIndex = (int32_t)bci->next2Bytes();

   // Attempt to resolve the offset at compile time. This can fail, and if it does we must fail the compilation for now.
   // In the future research project we could attempt runtime resolution. TR does this with self modifying code.
   // These are all from TR and likely have implications on how we use this offset. However, we do not know that as of yet.
   TR::DataType type = TR::NoType;
   U_32 fieldOffset = 0;
   bool isVolatile, isFinal, isPrivate, isUnresolvedInCP;
   bool resolved = _compilee->fieldAttributes(_comp, cpIndex, &fieldOffset, &type, &isVolatile, &isFinal, &isPrivate, false, &isUnresolvedInCP);
   if (!resolved)
      return 0;

   switch (type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, getFieldTemplatePrologue, getFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         COPY_TEMPLATE(buffer, intGetFieldTemplate, getFieldSize);
         break;
      case TR::Address:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, getFieldTemplatePrologue, getFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         COPY_TEMPLATE(buffer, addrGetFieldTemplatePrologue, getFieldSize);
         if (TR::Compiler->om.compressObjectReferences())
            {
            int32_t shift = TR::Compiler->om.compressedReferenceShift();
            COPY_TEMPLATE(buffer, decompressReferenceTemplate, getFieldSize);
            patchImm1(buffer, shift);
            }
         COPY_TEMPLATE(buffer, addrGetFieldTemplate, getFieldSize);
         break;
      case TR::Int64:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, getFieldTemplatePrologue, getFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         COPY_TEMPLATE(buffer, longGetFieldTemplate, getFieldSize);
         break;
      case TR::Float:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, getFieldTemplatePrologue, getFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         COPY_TEMPLATE(buffer, floatGetFieldTemplate, getFieldSize);
         break;
      case TR::Double:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, getFieldTemplatePrologue, getFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         COPY_TEMPLATE(buffer, doubleGetFieldTemplate, getFieldSize);
         break;
      default:
         trfprintf(_logFileFP, "Argument type %s is not supported\n", type.toString());
         break;
      }
   if (_comp->getOption(TR_TraceCG))
      trfprintf(_logFileFP, "Field Offset: %u\n", fieldOffset);

   return getFieldSize;
   }

buffer_size_t
MJIT::CodeGenerator::generatePutField(char *buffer, TR_J9ByteCodeIterator *bci)
   {
   buffer_size_t putFieldSize = 0;
   int32_t cpIndex = (int32_t)bci->next2Bytes();

   // Attempt to resolve the address at compile time. This can fail, and if it does we must fail the compilation for now.
   // In the future research project we could attempt runtime resolution. TR does this with self modifying code.
   // These are all from TR and likely have implications on how we use this address. However, we do not know that as of yet.
   U_32 fieldOffset = 0;
   TR::DataType type = TR::NoType;
   bool isVolatile, isFinal, isPrivate, isUnresolvedInCP;
   bool resolved = _compilee->fieldAttributes(_comp, cpIndex, &fieldOffset, &type, &isVolatile, &isFinal, &isPrivate, true, &isUnresolvedInCP);

   if (!resolved)
      return 0;

   switch (type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, intPutFieldTemplatePrologue, putFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         COPY_TEMPLATE(buffer, intPutFieldTemplate, putFieldSize);
         break;
      case TR::Address:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, addrPutFieldTemplatePrologue, putFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         if (TR::Compiler->om.compressObjectReferences())
            {
            int32_t shift = TR::Compiler->om.compressedReferenceShift();
            COPY_TEMPLATE(buffer, compressReferenceTemplate, putFieldSize);
            patchImm1(buffer, shift);
            }
         COPY_TEMPLATE(buffer, intPutFieldTemplate, putFieldSize);
         break;
      case TR::Int64:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, longPutFieldTemplatePrologue, putFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         COPY_TEMPLATE(buffer, longPutFieldTemplate, putFieldSize);
         break;
      case TR::Float:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, floatPutFieldTemplatePrologue, putFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         COPY_TEMPLATE(buffer, floatPutFieldTemplate, putFieldSize);
         break;
      case TR::Double:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, doublePutFieldTemplatePrologue, putFieldSize);
         patchImm4(buffer, (U_32)(fieldOffset));
         COPY_TEMPLATE(buffer, doublePutFieldTemplate, putFieldSize);
         break;
      default:
         trfprintf(_logFileFP, "Argument type %s is not supported\n", type.toString());
         break;
      }
   if (_comp->getOption(TR_TraceCG))
      trfprintf(_logFileFP, "Field Offset: %u\n", fieldOffset);

   return putFieldSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateGetStatic(char *buffer, TR_J9ByteCodeIterator *bci)
   {
   buffer_size_t getStaticSize = 0;
   int32_t cpIndex = (int32_t)bci->next2Bytes();

   // Attempt to resolve the address at compile time. This can fail, and if it does we must fail the compilation for now.
   // In the future research project we could attempt runtime resolution. TR does this with self modifying code.
   void *dataAddress;
   //These are all from TR and likely have implications on how we use this address. However, we do not know that as of yet.
   TR::DataType type = TR::NoType;
   bool isVolatile, isFinal, isPrivate, isUnresolvedInCP;
   bool resolved = _compilee->staticAttributes(_comp, cpIndex, &dataAddress, &type, &isVolatile, &isFinal, &isPrivate, false, &isUnresolvedInCP);
   if (!resolved)
      return 0;

   switch (type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, getStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, intGetStaticTemplate, getStaticSize);
         break;
      case TR::Address:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, getStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, addrGetStaticTemplatePrologue, getStaticSize);
         /*
         if (TR::Compiler->om.compressObjectReferences())
            {
            int32_t shift = TR::Compiler->om.compressedReferenceShift();
            COPY_TEMPLATE(buffer, decompressReferenceTemplate, getStaticSize);
            patchImm1(buffer, shift);
            }
         */
         COPY_TEMPLATE(buffer, addrGetStaticTemplate, getStaticSize);
         break;
      case TR::Int64:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, getStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, longGetStaticTemplate, getStaticSize);
         break;
      case TR::Float:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, getStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, floatGetStaticTemplate, getStaticSize);
         break;
      case TR::Double:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, getStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, doubleGetStaticTemplate, getStaticSize);
         break;
      default:
         trfprintf(_logFileFP, "Argument type %s is not supported\n", type.toString());
         break;
      }

   return getStaticSize;
   }

buffer_size_t
MJIT::CodeGenerator::generatePutStatic(char *buffer, TR_J9ByteCodeIterator *bci)
   {
   buffer_size_t putStaticSize = 0;
   int32_t cpIndex = (int32_t)bci->next2Bytes();

   // Attempt to resolve the address at compile time. This can fail, and if it does we must fail the compilation for now.
   // In the future research project we could attempt runtime resolution. TR does this with self modifying code.
   void *dataAddress;
   // These are all from TR and likely have implications on how we use this address. However, we do not know that as of yet.
   TR::DataType type = TR::NoType;
   bool isVolatile, isFinal, isPrivate, isUnresolvedInCP;
   bool resolved = _compilee->staticAttributes(_comp, cpIndex, &dataAddress, &type, &isVolatile, &isFinal, &isPrivate, true, &isUnresolvedInCP);
   if (!resolved)
      return 0;

   switch (type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, putStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, intPutStaticTemplate, putStaticSize);
         break;
      case TR::Address:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, putStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, addrPutStaticTemplatePrologue, putStaticSize);
         /*
         if (TR::Compiler->om.compressObjectReferences())
            {
            int32_t shift = TR::Compiler->om.compressedReferenceShift();
            COPY_TEMPLATE(buffer, compressReferenceTemplate, putStaticSize);
            patchImm1(buffer, shift);
            }
         */
         COPY_TEMPLATE(buffer, addrPutStaticTemplate, putStaticSize);
         break;
      case TR::Int64:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, putStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, longPutStaticTemplate, putStaticSize);
         break;
      case TR::Float:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, putStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, floatPutStaticTemplate, putStaticSize);
         break;
      case TR::Double:
         // Generate template and instantiate immediate values
         COPY_TEMPLATE(buffer, staticTemplatePrologue, putStaticSize);
         MJIT_ASSERT(_logFileFP, (U_64)dataAddress <= 0x00000000ffffffff, "data is above 4-byte boundary");
         patchImm4(buffer, (U_32)((U_64)dataAddress & 0x00000000ffffffff));
         COPY_TEMPLATE(buffer, doublePutStaticTemplate, putStaticSize);
         break;
      default:
         trfprintf(_logFileFP, "Argument type %s is not supported\n", type.toString());
         break;
      }

   return putStaticSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateColdArea(char *buffer, J9Method *method, char *jitStackOverflowJumpPatchLocation)
   {
   buffer_size_t coldAreaSize = 0;
   PATCH_RELATIVE_ADDR_32_BIT(jitStackOverflowJumpPatchLocation, (intptr_t)buffer);
   COPY_TEMPLATE(buffer, movEDIImm32, coldAreaSize);
   patchImm4(buffer, _stackPeakSize);

   COPY_TEMPLATE(buffer, call4ByteRel, coldAreaSize);
   uintptr_t jitStackOverflowHelperAddress = getHelperOrTrampolineAddress(TR_stackOverflow, (uintptr_t)buffer);
   PATCH_RELATIVE_ADDR_32_BIT(buffer, jitStackOverflowHelperAddress);

   COPY_TEMPLATE(buffer, jump4ByteRel, coldAreaSize);
   PATCH_RELATIVE_ADDR_32_BIT(buffer, jitStackOverflowJumpPatchLocation);

   return coldAreaSize;
   }

buffer_size_t
MJIT::CodeGenerator::generateDebugBreakpoint(char *buffer)
   {
   buffer_size_t codeGenSize = 0;
   COPY_TEMPLATE(buffer, debugBreakpoint, codeGenSize);
   return codeGenSize;
   }

TR::CodeCache *
MJIT::CodeGenerator::getCodeCache()
   {
   return _codeCache;
   }

U_8 *
MJIT::CodeGenerator::allocateCodeCache(int32_t length, TR_J9VMBase *vmBase, J9VMThread *vmThread)
   {
   TR::CodeCacheManager *manager = TR::CodeCacheManager::instance();
   int32_t compThreadID = vmBase->getCompThreadIDForVMThread(vmThread);
   int32_t numReserved;

   if (!_codeCache)
      {
      _codeCache = manager->reserveCodeCache(false, length, compThreadID, &numReserved);
      if (!_codeCache)
         return NULL;
      _comp->cg()->setCodeCache(_codeCache);
      }

   uint8_t *coldCode;
   U_8 *codeStart = manager->allocateCodeMemory(length, 0, &_codeCache, &coldCode, false);
   if (!codeStart)
      return NULL;

   return codeStart;
   }
