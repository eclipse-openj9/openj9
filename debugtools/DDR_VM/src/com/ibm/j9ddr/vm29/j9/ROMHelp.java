/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionHandlerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodDebugInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodParameterPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodParametersDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMMethodHelper;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.structure.J9ExceptionHandler;
import com.ibm.j9ddr.vm29.structure.J9ExceptionInfo;
import com.ibm.j9ddr.vm29.structure.J9MethodParameter;
import com.ibm.j9ddr.vm29.structure.J9ROMMethod;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;

import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.*;

/**
 * Static helper functions. Equivalent to romhelp.c / rommeth.h
 * 
 * @author andhall
 * 
 */
public class ROMHelp {

	private static interface IROMHelpImpl extends IAlgorithm {
		public J9ROMMethodPointer getOriginalROMMethod(J9MethodPointer method) throws CorruptDataException;

		public J9ROMMethodPointer nextROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException;
		
		public U32 getExtendedModifiersDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException;
		
		public U32Pointer getMethodAnnotationsDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public U32Pointer getParameterAnnotationsDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public U32Pointer getDefaultAnnotationDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException;
		
		public U32Pointer getMethodTypeAnnotationDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public U32Pointer getCodeTypeAnnotationDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public J9MethodDebugInfoPointer getMethodDebugInfoFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public SelfRelativePointer J9EXCEPTIONINFO_THROWNAMES(J9ExceptionInfoPointer info) throws CorruptDataException;

		public J9ExceptionHandlerPointer J9EXCEPTIONINFO_HANDLERS(J9ExceptionInfoPointer info) throws CorruptDataException;

		public J9ExceptionInfoPointer J9_EXCEPTION_DATA_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public  U32Pointer J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public U32Pointer J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public UDATA J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public UDATA J9_BYTECODE_SIZE_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException;

		public U8Pointer J9_BYTECODE_START_FROM_ROM_METHOD(J9ROMMethodPointer method) throws CorruptDataException;

		public J9ROMMethodPointer J9_ROM_METHOD_FROM_RAM_METHOD(J9MethodPointer method) throws CorruptDataException;

		public J9ClassPointer J9_CLASS_FROM_METHOD(J9MethodPointer method) throws CorruptDataException;

		public U8 J9_ARG_COUNT_FROM_ROM_METHOD(J9ROMMethodPointer method) throws CorruptDataException;

		public U16 J9_TEMP_COUNT_FROM_ROM_METHOD(J9ROMMethodPointer method) throws CorruptDataException;

		public J9UTF8Pointer J9ROMMETHOD_SIGNATURE(J9ROMMethodPointer method) throws CorruptDataException;
		
		public U32Pointer getStackMapFromROMMethod(J9ROMMethodPointer method) throws CorruptDataException;

		public J9MethodParametersDataPointer getMethodParametersFromROMMethod(J9ROMMethodPointer method) throws CorruptDataException;
		
		public long J9_METHOD_PARAMS_SIZE_FROM_NUMBER_OF_PARAMS(long numberOfParams) throws CorruptDataException;
	}

	private static final AlgorithmPicker<IROMHelpImpl> picker = new AlgorithmPicker<IROMHelpImpl>(AlgorithmVersion.ROM_HELP_VERSION) {

		@Override
		protected Iterable<? extends IROMHelpImpl> allAlgorithms() {
			List<IROMHelpImpl> list = new LinkedList<IROMHelpImpl>();

			list.add(new ROMHelp_29_V0());
			list.add(new ROMHelp_29_V1());

			return list;
		}
	};

	private static IROMHelpImpl impl;

	private static IROMHelpImpl getImpl() {
		if (impl == null) {
			impl = picker.pickAlgorithm();
		}

		return impl;
	}

	public static J9ROMMethodPointer getOriginalROMMethod(J9MethodPointer method) throws CorruptDataException {
		return getImpl().getOriginalROMMethod(method);
	}

	public static J9ROMMethodPointer nextROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().nextROMMethod(romMethod);
	}
	
	public static U32 getExtendedModifiersDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().getExtendedModifiersDataFromROMMethod(romMethod);
	}

	public static U32Pointer getMethodAnnotationsDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().getMethodAnnotationsDataFromROMMethod(romMethod);
	}

	public static U32Pointer getParameterAnnotationsDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().getParameterAnnotationsDataFromROMMethod(romMethod);
	}

	public static U32Pointer getDefaultAnnotationDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().getDefaultAnnotationDataFromROMMethod(romMethod);
	}

	public static U32Pointer getMethodTypeAnnotationDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().getMethodTypeAnnotationDataFromROMMethod(romMethod);
	}

	public static U32Pointer getCodeTypeAnnotationDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().getCodeTypeAnnotationDataFromROMMethod(romMethod);
	}

	public static J9MethodDebugInfoPointer getMethodDebugInfoFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().getMethodDebugInfoFromROMMethod(romMethod);
	}

	public static SelfRelativePointer J9EXCEPTIONINFO_THROWNAMES(J9ExceptionInfoPointer info) throws CorruptDataException {
		return getImpl().J9EXCEPTIONINFO_THROWNAMES(info);
	}

	public static J9ExceptionHandlerPointer J9EXCEPTIONINFO_HANDLERS(J9ExceptionInfoPointer info) throws CorruptDataException {
		return getImpl().J9EXCEPTIONINFO_HANDLERS(info);
	}

	public static J9ExceptionInfoPointer J9_EXCEPTION_DATA_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
	}

	public static U32Pointer J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(romMethod);
	}

	public static U32Pointer J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(romMethod);
	}

	public static UDATA J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	}

	public static UDATA J9_BYTECODE_SIZE_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	}

	public static U8Pointer J9_BYTECODE_START_FROM_ROM_METHOD(J9ROMMethodPointer method) throws CorruptDataException {
		return getImpl().J9_BYTECODE_START_FROM_ROM_METHOD(method);
	}

	public static J9ROMMethodPointer J9_ROM_METHOD_FROM_RAM_METHOD(J9MethodPointer method) throws CorruptDataException {
		return getImpl().J9_ROM_METHOD_FROM_RAM_METHOD(method);
	}

	public static J9ClassPointer J9_CLASS_FROM_METHOD(J9MethodPointer method) throws CorruptDataException {
		return getImpl().J9_CLASS_FROM_METHOD(method);
	}

	public static U8 J9_ARG_COUNT_FROM_ROM_METHOD(J9ROMMethodPointer method) throws CorruptDataException {
		return getImpl().J9_ARG_COUNT_FROM_ROM_METHOD(method);
	}

	public static U16 J9_TEMP_COUNT_FROM_ROM_METHOD(J9ROMMethodPointer method) throws CorruptDataException {
		return getImpl().J9_TEMP_COUNT_FROM_ROM_METHOD(method);
	}

	public static J9UTF8Pointer J9ROMMETHOD_SIGNATURE(J9ROMMethodPointer method) throws CorruptDataException {
		return getImpl().J9ROMMETHOD_SIGNATURE(method);
	}
	
	public static U32Pointer getStackMapFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().getStackMapFromROMMethod(romMethod);
	}
	
	public static J9MethodParametersDataPointer getMethodParametersFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
		return getImpl().getMethodParametersFromROMMethod(romMethod);
	}

	public static long J9_METHOD_PARAMS_SIZE_FROM_NUMBER_OF_PARAMS(long numberOfParams) throws CorruptDataException {
		return getImpl().J9_METHOD_PARAMS_SIZE_FROM_NUMBER_OF_PARAMS(numberOfParams);
	}
	
	private static class ROMHelp_29_V0 extends BaseAlgorithm implements IROMHelpImpl {

		protected ROMHelp_29_V0() {
			super(90, 0);
		}

		public ROMHelp_29_V0(int vmMinorVersion, int algorithmVersion) {
			super(vmMinorVersion, algorithmVersion);
		}

		public J9ROMMethodPointer getOriginalROMMethod(J9MethodPointer method) throws CorruptDataException {
			J9ClassPointer methodClass = J9_CLASS_FROM_METHOD(method);
			J9ROMClassPointer romClass = methodClass.romClass();
			J9ROMMethodPointer romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			U8Pointer bytecodes = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);

			if (bytecodes.lt(romClass) || (bytecodes.gte(romClass.addOffset(romClass.romSize())))) {
				long methodIndex = UDATA.cast(method).sub(UDATA.cast(methodClass.ramMethods())).longValue();

				romMethod = romClass.romMethods();
				while (methodIndex > 0) {
					romMethod = nextROMMethod(romMethod);
					methodIndex--;
				}
			}

			return romMethod;
		}
		

		private J9ROMMethodPointer skipOverLengthDataAndPadding(J9ROMMethodPointer annotation) throws CorruptDataException {
			J9ROMMethodPointer result = annotation;
			UDATA size = U32.roundToSizeofU32(new UDATA(U32Pointer.cast(annotation).at(0)));
			/* Add the size of the annotation data and the size of the size of the annotation (4) and a pad to U_32*/
			result = result.addOffset(U32.SIZEOF);
			result = result.addOffset(size);
			return result;
		}

		public J9ROMMethodPointer nextROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			J9ROMMethodPointer result;

			boolean hasExceptionInfo = J9ROMMethodHelper.hasExceptionInfo(romMethod);
			boolean hasMethodAnnotations = J9ROMMethodHelper.hasMethodAnnotations(romMethod);
			boolean hasParameterAnnotations = J9ROMMethodHelper.hasParameterAnnotations(romMethod);
			boolean hasDefaultAnnotation = J9ROMMethodHelper.hasDefaultAnnotation(romMethod);
			boolean hasDebugInfo = J9ROMMethodHelper.hasDebugInfo(romMethod);
			boolean hasStackMap = J9ROMMethodHelper.hasStackMap(romMethod);
			boolean hasMethodParameters = J9ROMMethodHelper.hasMethodParameters(romMethod);
			
			J9ExceptionInfoPointer exceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);

			/* skip the exception table if there is one */
			if (hasExceptionInfo) {
				result = J9ROMMethodPointer.cast(J9EXCEPTIONINFO_THROWNAMES(exceptionInfo).add(exceptionInfo.throwCount()));
			} else {
				result = J9ROMMethodPointer.cast(exceptionInfo);
			}
			
			/* skip method annotations if there are some */ 
			if (hasMethodAnnotations) {
				result = skipOverLengthDataAndPadding(result);
			}
			
			/* skip parameter annotations if there are some */
			if (hasParameterAnnotations) {
				result = skipOverLengthDataAndPadding(result);
			}
			
			/* skip default annotation if there is one */
			if (hasDefaultAnnotation) {
				result = skipOverLengthDataAndPadding(result);
			}
			
			/* skip type annotations if any */
			if (J9ROMMethodHelper.hasMethodTypeAnnotations(romMethod)) {
				result = skipOverLengthDataAndPadding(result);
			}
			
			if (J9ROMMethodHelper.hasCodeTypeAnnotations(romMethod)) {
				result = skipOverLengthDataAndPadding(result);
			}
			
			/* skip debug information if there is some */
			if (hasDebugInfo) {
				/* check for low tag */
				U32 taggedSizeOrSRP = U32Pointer.cast(result).at(0);
				if ( taggedSizeOrSRP.allBitsIn(1) ) {
					/* low tag indicates that debug data is inline in the ROMMethod,
					 * skip over the debug info by adding the size of debug info (masking out the low tag) 
					 * would be nice to have a 'tag'/'untag'/'bitMask' to do this, currently depending on ~1 
					 * yielding the correct result.*/
					result = result.addOffset(taggedSizeOrSRP.bitAnd(~1));
				} else {
					/* no low tag, means data is out of line and this is an SRP, skip over it */
					result = result.addOffset(SelfRelativePointer.SIZEOF);
				}
			}

			/* skip the stack map if there is one */
			if (hasStackMap) {
				result = result.addOffset(U32Pointer.cast(result).at(0));
			}
			
			if (hasMethodParameters) {
				J9MethodParametersDataPointer methodParametersData = J9MethodParametersDataPointer.cast(result);
				J9MethodParameterPointer parameters = methodParametersData.parameters();
				long methodParametersSize = ROMHelp.J9_METHOD_PARAMS_SIZE_FROM_NUMBER_OF_PARAMS(methodParametersData.parameterCount().longValue());
				long padding = U32.SIZEOF - (methodParametersSize % U32.SIZEOF);
				long size = 0;
				
				if (padding == U32.SIZEOF) {
					padding = 0;
				}
				size = methodParametersSize + padding;
				result = result.addOffset(size);
			}

			return result;
		}
		
		private U32Pointer methodAnnotationsDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			boolean hasExceptionInfo = J9ROMMethodHelper.hasExceptionInfo(romMethod);
			J9ROMMethodPointer result;
			J9ExceptionInfoPointer exceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);

			/* skip the exception table if there is one */
			if (hasExceptionInfo) {
				result = J9ROMMethodPointer.cast(J9EXCEPTIONINFO_THROWNAMES(exceptionInfo).add(exceptionInfo.throwCount()));
			} else {
				result = J9ROMMethodPointer.cast(exceptionInfo);
			}
			return U32Pointer.cast(result);
		}

		public U32Pointer getMethodAnnotationsDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			boolean hasMethodAnnotations = J9ROMMethodHelper.hasMethodAnnotations(romMethod);
			if (hasMethodAnnotations) {
				return methodAnnotationsDataFromROMMethod(romMethod);
			}
			return U32Pointer.NULL;
		}
 
		
		private U32Pointer parameterAnnotationsFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			J9ROMMethodPointer result = J9ROMMethodPointer.cast(methodAnnotationsDataFromROMMethod(romMethod));
			boolean hasMethodAnnotations = J9ROMMethodHelper.hasMethodAnnotations(romMethod);

			/* skip the exception table if there is one */
			if (hasMethodAnnotations) {
				result = skipOverLengthDataAndPadding(result);
			} 
			return U32Pointer.cast(result);
		}

		public U32Pointer getParameterAnnotationsDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			boolean hasParameterAnnotations = J9ROMMethodHelper.hasParameterAnnotations(romMethod);
			if (hasParameterAnnotations) {
				return parameterAnnotationsFromROMMethod(romMethod);
			}
			return U32Pointer.NULL;
		}
 
		
		private U32Pointer defaultAnnotationFromROMMethod(J9ROMMethodPointer romMethod)  throws CorruptDataException{
			J9ROMMethodPointer result = J9ROMMethodPointer.cast(parameterAnnotationsFromROMMethod(romMethod));
			boolean hasParameterAnnotations = J9ROMMethodHelper.hasParameterAnnotations(romMethod);

			/* skip the exception table if there is one */
			if (hasParameterAnnotations) {
				result = skipOverLengthDataAndPadding(result);
			} 
			return U32Pointer.cast(result);
		}

		public U32Pointer getDefaultAnnotationDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			if (J9ROMMethodHelper.hasDefaultAnnotation(romMethod)) {
				return defaultAnnotationFromROMMethod(romMethod);
			} else {
				return U32Pointer.NULL;
			}
		}
 
		public U32 getExtendedModifiersDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			if (J9ROMMethodHelper.hasExtendedModifiers(romMethod)) {
				return J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(romMethod).at(0);
			} else {
				return new U32(0);
			}
		}
 
		public U32Pointer getMethodTypeAnnotationDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			U32Pointer result = U32Pointer.NULL;
			if (J9ROMMethodHelper.hasMethodTypeAnnotations(romMethod)) {
				result = methodTypeAnnotationFromROMMethod(romMethod);
			}
			return result;
		}
		
		public U32Pointer getCodeTypeAnnotationDataFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			U32Pointer result = U32Pointer.NULL;
			if (J9ROMMethodHelper.hasCodeTypeAnnotations(romMethod)) {
				result = codeTypeAnnotationFromROMMethod(romMethod);
			}
			return result;
		}
		
		private J9MethodDebugInfoPointer methodDebugInfoFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			J9ROMMethodPointer result = J9ROMMethodPointer.cast(codeTypeAnnotationFromROMMethod(romMethod));
			if (J9ROMMethodHelper.hasCodeTypeAnnotations(romMethod)) {
				result = skipOverLengthDataAndPadding(result);
			}
			
			return J9MethodDebugInfoPointer.cast(result);
		}

		private U32Pointer methodTypeAnnotationFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			J9ROMMethodPointer result = J9ROMMethodPointer.cast(defaultAnnotationFromROMMethod(romMethod));
			if (J9ROMMethodHelper.hasDefaultAnnotation(romMethod)) {
				result = skipOverLengthDataAndPadding(result);
			}
			
			return U32Pointer.cast(result);
		}

		private U32Pointer codeTypeAnnotationFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			J9ROMMethodPointer result = J9ROMMethodPointer.cast(methodTypeAnnotationFromROMMethod(romMethod));
			if (J9ROMMethodHelper.hasMethodTypeAnnotations(romMethod)) {
				result = skipOverLengthDataAndPadding(result);
			}
			
			return U32Pointer.cast(result);			
		}

		public J9MethodDebugInfoPointer getMethodDebugInfoFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			boolean hasDebugInfo = J9ROMMethodHelper.hasDebugInfo(romMethod);
			if (hasDebugInfo ) {
				J9MethodDebugInfoPointer result = methodDebugInfoFromROMMethod(romMethod);
				/* check for low tag */
				U32 taggedSizeOrSRP = U32Pointer.cast(result).at(0);
				if ( taggedSizeOrSRP.allBitsIn(1) ) {
					/* low tag indicates that debug data is inline in the ROMMethod,
					 * skip over the debug info by adding the size of debug info (masking out the low tag) 
					 * would be nice to have a 'tag'/'untag'/'bitMask' to do this, currently depending on ~1 
					 * yielding the correct result.*/
					return result;
				} else {
					/* no low tag, means data is out of line and this is an SRP, skip over it */
					return J9MethodDebugInfoPointer.cast(SelfRelativePointer.cast(result).get());
				}
			}
			return J9MethodDebugInfoPointer.NULL;
		}
		
		public U32Pointer stackMapFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			boolean hasDebugInfo = J9ROMMethodHelper.hasDebugInfo(romMethod);
			J9MethodDebugInfoPointer debugInfo = methodDebugInfoFromROMMethod(romMethod);
			U32Pointer result = U32Pointer.cast(debugInfo);
			if (hasDebugInfo) {
				/* check for low tag */
				U32 taggedSizeOrSRP = U32Pointer.cast(debugInfo).at(0);
				if (taggedSizeOrSRP.allBitsIn(1)) {
					/* low tag indicates that debug data is inline in the ROMMethod,
					 * skip over the debug info by adding the size of debug info (masking out the low tag) 
					 * would be nice to have a 'tag'/'untag'/'bitMask' to do this, currently depending on ~1 
					 * yielding the correct result.*/
					result = result.addOffset(taggedSizeOrSRP.bitAnd(~1));
				} else {
					/* no low tag, means data is out of line and this is an SRP, skip over it */
					result = result.addOffset(SelfRelativePointer.SIZEOF);
				}
			}
			return result;
		}
		
		public U32Pointer getStackMapFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			if (J9ROMMethodHelper.hasStackMap(romMethod)) {
				return stackMapFromROMMethod(romMethod);	
			}
			return U32Pointer.NULL;
		}
		
		public J9MethodParametersDataPointer methodParametersFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			
			U32Pointer stackMap = stackMapFromROMMethod(romMethod);
			J9MethodParametersDataPointer result = J9MethodParametersDataPointer.cast(stackMap);
			
			if (J9ROMMethodHelper.hasStackMap(romMethod)) {
				U32 stackMapSize = stackMap.at(0);
				result = result.addOffset(stackMapSize);				
			}
			return result;
		}
		
		public J9MethodParametersDataPointer getMethodParametersFromROMMethod(J9ROMMethodPointer romMethod) throws CorruptDataException {
			if (J9ROMMethodHelper.hasMethodParameters(romMethod)) {
				return methodParametersFromROMMethod(romMethod);	
			}
			return J9MethodParametersDataPointer.NULL;
		}
		
		
		public SelfRelativePointer J9EXCEPTIONINFO_THROWNAMES(J9ExceptionInfoPointer info) throws CorruptDataException {
			return SelfRelativePointer.cast(J9EXCEPTIONINFO_HANDLERS(info).addOffset(new U32(info.catchCount()).mult((int) J9ExceptionHandler.SIZEOF)));
		}

		public J9ExceptionHandlerPointer J9EXCEPTIONINFO_HANDLERS(J9ExceptionInfoPointer info) throws CorruptDataException {
			return J9ExceptionHandlerPointer.cast(info.addOffset(J9ExceptionInfo.SIZEOF));
		}

		public J9ExceptionInfoPointer J9_EXCEPTION_DATA_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
			U32Pointer sigAddr = J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(romMethod);

			if (romMethod.modifiers().anyBitsIn(J9AccMethodHasGenericSignature)) {
				sigAddr = sigAddr.add(1);
			}

			return J9ExceptionInfoPointer.cast(sigAddr);
		}

		public U32Pointer J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
			return U32Pointer.cast(J9_BYTECODE_START_FROM_ROM_METHOD(romMethod).addOffset(J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod)));
		}

		public U32Pointer J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
			U32Pointer genericSigsAddr = J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(romMethod);
			if (J9ROMMethodHelper.hasExtendedModifiers(romMethod)) {
				genericSigsAddr = genericSigsAddr.add(1);
			}
			return genericSigsAddr;
		}

		public UDATA J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
			return J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod).add(3).bitAnd(new UDATA(3).bitNot());
		}

		public UDATA J9_BYTECODE_SIZE_FROM_ROM_METHOD(J9ROMMethodPointer romMethod) throws CorruptDataException {
//			#define J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod) \
//				(((UDATA) (romMethod)->bytecodeSizeLow) + (((UDATA) (romMethod)->bytecodeSizeHigh) << 16))
			return new UDATA(romMethod.bytecodeSizeLow()).add(new UDATA(romMethod.bytecodeSizeHigh()).leftShift(16));
		}

		public U8Pointer J9_BYTECODE_START_FROM_ROM_METHOD(J9ROMMethodPointer method) throws CorruptDataException {
			return U8Pointer.cast(method).add(J9ROMMethod.SIZEOF);
		}

		public J9ROMMethodPointer J9_ROM_METHOD_FROM_RAM_METHOD(J9MethodPointer method) throws CorruptDataException {
			return J9ROMMethodPointer.cast(method.bytecodes().sub(J9ROMMethod.SIZEOF));
		}

		public J9ClassPointer J9_CLASS_FROM_METHOD(J9MethodPointer method) throws CorruptDataException {
			J9ConstantPoolPointer cp = method.constantPool().untag(J9Consts.J9_STARTPC_STATUS);

			return cp.ramClass();
		}

		public U8 J9_ARG_COUNT_FROM_ROM_METHOD(J9ROMMethodPointer method) throws CorruptDataException {
			return method.argCount();
		}

		public U16 J9_TEMP_COUNT_FROM_ROM_METHOD(J9ROMMethodPointer method) throws CorruptDataException {
			return method.tempCount();
		}

		public J9UTF8Pointer J9ROMMETHOD_SIGNATURE(J9ROMMethodPointer method) throws CorruptDataException {
			return method.nameAndSignature().signature();
		}

		public long J9_METHOD_PARAMS_SIZE_FROM_NUMBER_OF_PARAMS(long numberOfParams) throws CorruptDataException {
			long size = U8.SIZEOF + (numberOfParams * J9MethodParameter.SIZEOF);
			return size;
		}
	}
	
	private static class ROMHelp_29_V1 extends ROMHelp_29_V0 {
		protected ROMHelp_29_V1() {
			super(90, 1);
		}

	}
}
