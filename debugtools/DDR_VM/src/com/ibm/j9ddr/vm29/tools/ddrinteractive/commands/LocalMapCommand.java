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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.j9.stackmap.LocalMap;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9JavaVMHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;

public class LocalMapCommand extends Command 
{

	public LocalMapCommand()
	{
		addCommand("localmap", "<pc>", "calculate the local slot map for the specified PC");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try{
		if(args.length != 1) {
			CommandUtils.dbgPrint(out, "bad or missing PC\n");
			return;			
		}
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		
		long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
		
		U8Pointer pc = U8Pointer.cast(address);
		CommandUtils.dbgPrint(out, "Searching for PC=%d in VM=%s...\n", pc.longValue(), vm.getHexAddress());

		J9MethodPointer localMethod = J9JavaVMHelper.getMethodFromPC(vm, pc);
		if (localMethod.notNull()) {
			int[] localMap = new int[65536 / 32];
			
		
			CommandUtils.dbgPrint(out, "Found method %s !j9method %s\n", J9MethodHelper.getName(localMethod), localMethod.getHexAddress());

			UDATA offsetPC = new UDATA(pc.sub(U8Pointer.cast(localMethod.bytecodes())));
			CommandUtils.dbgPrint(out, "Relative PC = %d\n", offsetPC.longValue());

			J9ClassPointer localClass = J9_CLASS_FROM_CP(localMethod.constantPool());
			long methodIndex = new UDATA(localMethod.sub(localClass.ramMethods())).longValue();			
			CommandUtils.dbgPrint(out, "Method index is %d\n", methodIndex);
			
			J9ROMMethodPointer localROMMethod = J9ROMCLASS_ROMMETHODS(localClass.romClass());
			while (methodIndex != 0) {
				localROMMethod = ROMHelp.nextROMMethod(localROMMethod);
				--methodIndex;
			}
			CommandUtils.dbgPrint(out, "Using ROM method %s\n", localROMMethod.getHexAddress());
			
			U16 tempCount = localROMMethod.tempCount();
			U8 argCount = localROMMethod.argCount();
			long localCount = tempCount.add(argCount).longValue();

			if (localCount > 0) {
				int errorCode = LocalMap.j9localmap_LocalBitsForPC(localROMMethod, offsetPC, localMap);
				if (errorCode != 0) {
					CommandUtils.dbgPrint(out, "Local map failed, error code = %d\n", errorCode);
				} else {
					int currentDescription = localMap[(int)((localCount + 31) / 32)];					
					long descriptionLong = 0;

					CommandUtils.dbgPrint(out, "Local map (%d slots mapped): local %d --> ", localCount, localCount - 1);
					long bitsRemaining = localCount % 32;
					if (bitsRemaining != 0) {
						descriptionLong = currentDescription << (32 - bitsRemaining);
						currentDescription--;
					}
					while(localCount != 0) {
						if (bitsRemaining == 0) {
							descriptionLong = currentDescription;
							currentDescription--;
							bitsRemaining = 32;
						}
						CommandUtils.dbgPrint(out, "%d", (descriptionLong & (1 << 32)) != 0 ? 1 : 0 );
						descriptionLong = descriptionLong << 1;
						--bitsRemaining;
						--localCount;
					}
					CommandUtils.dbgPrint(out, " <-- local 0\n");
				}
			} else {
				CommandUtils.dbgPrint(out, "No locals to map\n");
			}
		} else {
			CommandUtils.dbgPrint(out, "Not found\n");
		}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}		
	}

	// #define J9ROMCLASS_ROMMETHODS(base) NNSRP_GET((base)->romMethods, struct J9ROMMethod*)
	private J9ROMMethodPointer J9ROMCLASS_ROMMETHODS(J9ROMClassPointer base) throws CorruptDataException 
	{
		return base.romMethods();		
	}

	private J9ClassPointer J9_CLASS_FROM_CP(J9ConstantPoolPointer constantPool) throws CorruptDataException 
	{
		return constantPool.ramClass();
	}
	
//	/*
//	 * Returns the first ROM method following the argument.
//	 * If this is called on the last ROM method in a ROM class
//	 * it will return an undefined value.
//	 */
//	J9ROMMethodPointer nextROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException
//	{
//		J9ROMMethodPointer result;
//		U32Pointer stackMap = stackMapFromROMMethod(romMethod);
//
//		/* skip the stack map if there is one */
//		if ( J9ROMMETHOD_HAS_STACK_MAP(romMethod) ) {
//			U32 stackMapSize = stackMap.at(0);
//			result = J9ROMMethodPointer.cast(U8Pointer.cast(stackMap).add(stackMapSize));
//		} else {
//			result = J9ROMMethodPointer.cast(stackMap);
//		}
//		return result;
//	}

//	private boolean J9ROMMETHOD_HAS_STACK_MAP(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return romMethod.hasStackMap();
//	}

//	private U32Pointer stackMapFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException
//	{
//		
//		U32Pointer result;
//
//		J9MethodDebugInfoPointer debugInfo = methodDebugInfoFromROMMethod(romMethod);
//
//		/* skip over the debug information if it exists*/
//		if (J9ROMMETHOD_HAS_DEBUG_INFO(romMethod)) {
//			U32 debugInfoSize = U32Pointer.cast(debugInfo).at(0);
//			if ((debugInfoSize.bitAnd(1).eq(1))) {
//				/* low tagged, indicates debug information is stored 
//				 * inline in the rom method and this is the size
//				 * mask out the low tag */
//				result = U32Pointer.cast(U8Pointer.cast(debugInfo).add(debugInfoSize.bitAnd(~1)));
//			} else {
//				/* not low tag, debug info is stored out of line
//				 * skip over the SRP to that data */
//				result = U32Pointer.cast(U8Pointer.cast(debugInfo).add(SelfRelativePointer.SIZEOF));
//			}
//		} else {
//			result = U32Pointer.cast(debugInfo);
//		}
//
//		return result;
//	}
	
//	private boolean J9ROMMETHOD_HAS_DEBUG_INFO(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return romMethod.hasDebugInfo();
//	}

//	private J9MethodDebugInfoPointer methodDebugInfoFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException
//	{
//		J9MethodDebugInfoPointer result;
//
//		U32Pointer annotation = defaultAnnotationFromROMMethod(romMethod);
//
//		/* skip the default annotation information if there is some */
//		if (J9ROMMETHOD_HAS_DEFAULT_ANNOTATION(romMethod)) {
//			result = J9MethodDebugInfoPointer.cast(SKIP_OVER_LENGTH_DATA_AND_PADDING(annotation));
//		} else {
//			result = J9MethodDebugInfoPointer.cast(annotation);
//		}
//		return result;
//	}

//	private boolean J9ROMMETHOD_HAS_DEFAULT_ANNOTATION(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return romMethod.hasDefaultAnnotation();
//	}

//	private U32Pointer defaultAnnotationFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException
//	{
//		
//		U32Pointer result;
//
//		U32Pointer annotation = parameterAnnotationsFromROMMethod(romMethod);
//
//		/* skip the exception table if there is one */
//		if (J9ROMMETHOD_HAS_PARAMETER_ANNOTATIONS(romMethod)) {
//			result = SKIP_OVER_LENGTH_DATA_AND_PADDING(annotation);
//		} else {
//			result = annotation;
//		}
//		return result;
//	}

//	private boolean J9ROMMETHOD_HAS_PARAMETER_ANNOTATIONS(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return romMethod.hasParameterAnnotations();
//	}
//
//	private U32Pointer parameterAnnotationsFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException
//	{		
//		U32Pointer result;
//
//		U32Pointer annotation = methodAnnotationsDataFromROMMethod(romMethod);
//
//		/* skip the exception table if there is one */
//		if (J9ROMMETHOD_HAS_ANNOTATIONS_DATA(romMethod)) {
//			result = SKIP_OVER_LENGTH_DATA_AND_PADDING(annotation);
//		} else {
//			result = annotation;
//		}
//		return result;
//	}
//	private U32Pointer SKIP_OVER_LENGTH_DATA_AND_PADDING(U32Pointer annotation) throws CorruptDataException {
//		U32Pointer result;
//		do { 
//			long bytesToPad = U32.SIZEOF - annotation.at(0).longValue()%U32.SIZEOF;
//			if (U32.SIZEOF==bytesToPad) {
//				bytesToPad = 0;
//			} 
//			/* Add the size of the annotation data (*annotation) and the size of the size of the annotation (4) and a pad to U_32*/
//			result = U32Pointer.cast(((UDATA.cast(annotation).add(annotation.at(0).add(4).add((int) bytesToPad)))));
//		} while (false);
//
//		return result;
//	}
//	private U32Pointer	methodAnnotationsDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException
//	{
//		
//		U32Pointer result;
//
//		J9ExceptionInfoPointer exceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
//
//		/* skip the exception table if there is one */
//		if (J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)) {
//			result = U32Pointer.cast(J9EXCEPTIONINFO_THROWNAMES(exceptionInfo).add(exceptionInfo.throwCount()));
//		} else {
//			result = U32Pointer.cast(exceptionInfo);
//		}
//		return result;
//	}

	// #define J9EXCEPTIONINFO_THROWNAMES(info) ((J9SRP *) (((U_8 *) J9EXCEPTIONINFO_HANDLERS(info)) + ((info)->catchCount * sizeof(J9ExceptionHandler))))
//	private SelfRelativePointer J9EXCEPTIONINFO_THROWNAMES(J9ExceptionInfoPointer info) throws CorruptDataException {
//		U16 handleOffset = info.catchCount().mult((int) J9ExceptionHandler.SIZEOF);
//		return  SelfRelativePointer.cast(U8Pointer.cast(J9EXCEPTIONINFO_HANDLERS(info)).add(handleOffset));		
//	}

	// #define J9EXCEPTIONINFO_HANDLERS(info) ((J9ExceptionHandler *) (((U_8 *) (info)) + sizeof(J9ExceptionInfo)))
//	private J9ExceptionHandlerPointer J9EXCEPTIONINFO_HANDLERS(J9ExceptionInfoPointer info) {
//		return J9ExceptionHandlerPointer.cast(U8Pointer.cast(info).add(J9ExceptionInfo.SIZEOF));
//	}

//	private boolean J9ROMMETHOD_HAS_EXCEPTION_INFO(J9ROMMethodPointer romMethod) throws CorruptDataException {		
//		return romMethod.hasExceptionInfo();
//	}

	// #define J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod) \ 
	// ((J9ExceptionInfo *) (J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(romMethod) + (J9ROMMETHOD_HAS_GENERIC_SIGNATURE(romMethod) ? 1 : 0)))
//	private J9ExceptionInfoPointer J9_EXCEPTION_DATA_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return J9ExceptionInfoPointer.cast(J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(romMethod).add(J9ROMMETHOD_HAS_GENERIC_SIGNATURE(romMethod) ? 1 : 0));
//	}

//	private boolean J9ROMMETHOD_HAS_GENERIC_SIGNATURE(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return romMethod.hasGenericSignature();
//	}

	// #define J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(romMethod) \
	// (((U_32 *) (J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) + J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod))))
//	private AbstractPointer J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return U32Pointer.cast(J9_BYTECODE_START_FROM_ROM_METHOD(romMethod).add(J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod)));
//	}

	// #define J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) (((U_8 *) (romMethod)) + sizeof(J9ROMMethod))
//	private AbstractPointer J9_BYTECODE_START_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return romMethod.bytecodes();
//	}
	
	// #define J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod) ((J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod) + 3) & ~(UDATA)3)
//	private Scalar J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod).add(3).bitAnd(new UDATA(3).bitNot());
//	}

//	private UDATA J9_BYTECODE_SIZE_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return romMethod.bytecodeSize();
//	}
//	private boolean J9ROMMETHOD_HAS_ANNOTATIONS_DATA(J9ROMMethodPointer romMethod) throws CorruptDataException {
//		return romMethod.hasAnnotation();
//	}	
}
