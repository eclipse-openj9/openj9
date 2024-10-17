/*
 * Copyright IBM Corp. and others 2009
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
 */
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_BYTECODE_SIZE_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_ROM_METHOD_FROM_RAM_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_CLASS_ANNOTATION_INFO;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_ENCLOSING_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_GENERIC_SIGNATURE;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_IMPLICITCREATION_ATTRIBUTE;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_LOADABLEDESCRIPTORS_ATTRIBUTE;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_PERMITTEDSUBCLASSES_ATTRIBUTE;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_SIMPLE_NAME;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_SOURCE_DEBUG_EXTENSION;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_SOURCE_FILE_NAME;
import static com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_TYPE_ANNOTATION_INFO;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.walkers.LineNumber;
import com.ibm.j9ddr.vm29.j9.walkers.LineNumberIterator;
import com.ibm.j9ddr.vm29.j9.walkers.LocalVariableTableIterator;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9EnclosingObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodDebugInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SourceDebugExtensionPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodDebugInfoHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9MethodDebugInfo;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Analogue to util/optinfo.c
 * 
 * @author andhall
 * 
 */
public class OptInfo {
	private static IOptInfoImpl impl;

	private static final AlgorithmPicker<IOptInfoImpl> picker = new AlgorithmPicker<IOptInfoImpl>(AlgorithmVersion.OPT_INFO_VERSION) {

		@Override
		protected Iterable<? extends IOptInfoImpl> allAlgorithms() {
			List<IOptInfoImpl> list = new LinkedList<IOptInfoImpl>();

			list.add(new OptInfo_29_V0());

			return list;
		}
	};

	private static IOptInfoImpl getImpl() {
		if (impl == null) {
			impl = picker.pickAlgorithm();
		}

		return impl;
	}

	private static interface IOptInfoImpl extends IAlgorithm {
		public int getLineNumberForROMClass(J9MethodPointer method, UDATA relativePC) throws CorruptDataException;
	}

	private static class OptInfo_29_V0 extends BaseAlgorithm implements IOptInfoImpl {

		protected OptInfo_29_V0() {
			super(90, 0);
		}

		public int getLineNumberForROMClass(J9MethodPointer method, UDATA relativePC) throws CorruptDataException {
			UDATA bytecodeSize = J9_BYTECODE_SIZE_FROM_ROM_METHOD(J9_ROM_METHOD_FROM_RAM_METHOD(method));
			int number = -1;

			/*
			 * bug 113600 - bytecodeSize == 0 means that the method has been
			 * AOTd and the bytecodes have been stripped. So we can allow a
			 * relativePC that is >= bytecodeSize in that case
			 */
			if ((relativePC.lt(bytecodeSize)) || (bytecodeSize.eq(0))) {
				J9MethodDebugInfoPointer methodInfo = getMethodDebugInfoForROMClass(method);

				if (methodInfo.notNull()) {
					Iterator<LineNumber> lineNumberIterator = LineNumberIterator.lineNumberIteratorFor(methodInfo);
					LineNumber lineNumber;
					
					while (lineNumberIterator.hasNext()) {
						lineNumber = lineNumberIterator.next();
						if (relativePC.lt(lineNumber.getLocation())) {
							break;
						}
						number = lineNumber.getLineNumber().intValue();
					}
				}
			}

			return number;
		}
	}

	// Differing logic
	public static int getLineNumberForROMClass(J9MethodPointer method, UDATA relativePC) throws CorruptDataException {
		return getImpl().getLineNumberForROMClass(method, relativePC);
	}

	// Common logic

	public static J9MethodDebugInfoPointer getMethodDebugInfoForROMClass(J9MethodPointer method) throws CorruptDataException {
		return ROMHelp.getMethodDebugInfoFromROMMethod(ROMHelp.getOriginalROMMethod(method));
	}
	
	private static SelfRelativePointer getSRPPtr(U32Pointer ptr, UDATA flags, long option) {
		if ((!(flags.anyBitsIn(option))) || (ptr.isNull())) {
			return SelfRelativePointer.NULL;
		}

		return SelfRelativePointer.cast(ptr.add(countBits(COUNT_MASK(new U32(flags), option)) - 1));
	}

	public static int countBits(U32 word) {
		return Long.bitCount(word.longValue());
	}

	/**
	 * This method should be used when VM_LOCAL_VARIABLE_TABLE_VERSION >= 1
	 */
	public static U8Pointer	getV1VariableTableForMethodDebugInfo(J9MethodDebugInfoPointer methodInfo) throws CorruptDataException {
		LocalVariableTableIterator.checkVariableTableVersion();
		return U8Pointer.cast(OptInfo._getVariableTableForMethodDebugInfo(methodInfo));
	}
	private static VoidPointer _getVariableTableForMethodDebugInfo(J9MethodDebugInfoPointer methodInfo) throws CorruptDataException {
		if (!methodInfo.varInfoCount().eq(0)) {
			/* check for low tag */
			U32 taggedSizeOrSRP = U32Pointer.cast(methodInfo).at(0);
			if ( taggedSizeOrSRP.allBitsIn(1) ) {
				/* 
				 * low tag indicates that debug information is in line 
				 * skip over J9MethodDebugInfo header and the J9LineNumber table
				 */
				U32 lineNumberTableSize = J9MethodDebugInfoHelper.getLineNumberCompressedSize(methodInfo);
				return VoidPointer.cast(((U8Pointer.cast(methodInfo).addOffset(J9MethodDebugInfo.SIZEOF).addOffset(lineNumberTableSize))));
			} else {
				/* 
				 * debug information is out of line, this slot is an SRP to the
				 * J9VariableInfo table
				 */
				return VoidPointer.cast(methodInfo.srpToVarInfo());
			}
		}
		return VoidPointer.NULL;
	}

	public static U32 COUNT_MASK(U32 value, long mask) {
		return value.bitAnd((mask << 1) - 1);
	}

	public static String getSourceFileNameForROMClass(J9ROMClassPointer romClass) throws CorruptDataException {
		return getOption(romClass, J9_ROMCLASS_OPTINFO_SOURCE_FILE_NAME);
	}

	public static String getSimpleNameForROMClass(J9ROMClassPointer romClass) throws CorruptDataException {
		return getOption(romClass, J9_ROMCLASS_OPTINFO_SIMPLE_NAME);
	}

	public static U32Pointer getClassAnnotationsDataForROMClass(J9ROMClassPointer romClass) throws CorruptDataException {
		VoidPointer structure = getStructure(romClass, J9_ROMCLASS_OPTINFO_CLASS_ANNOTATION_INFO);
		return U32Pointer.cast(structure);
	}
	
	public static U32Pointer getClassTypeAnnotationsDataForROMClass(J9ROMClassPointer romClass) throws CorruptDataException {
		VoidPointer structure = getStructure(romClass, J9_ROMCLASS_OPTINFO_TYPE_ANNOTATION_INFO);
		return U32Pointer.cast(structure);
	}

	public static String getGenericSignatureForROMClass(J9ROMClassPointer romClass) throws CorruptDataException {
		return getOption(romClass, J9_ROMCLASS_OPTINFO_GENERIC_SIGNATURE);
	}

	public static J9EnclosingObjectPointer getEnclosingMethodForROMClass(J9ROMClassPointer romClass) throws CorruptDataException {
		VoidPointer structure = getStructure(romClass, J9_ROMCLASS_OPTINFO_ENCLOSING_METHOD);
		if (!structure.isNull()) {
			return J9EnclosingObjectPointer.cast(structure);
		} else {
			return J9EnclosingObjectPointer.NULL;
		}
	}

	private static String getOption(J9ROMClassPointer romClass, long option) throws CorruptDataException {
		VoidPointer structure = getStructure(romClass, option);
		if (structure != VoidPointer.NULL) {
			return J9UTF8Helper.stringValue(J9UTF8Pointer.cast(structure));
		} 
		return null;
	}

	private static VoidPointer getStructure(J9ROMClassPointer romClass, long option) throws CorruptDataException {
		SelfRelativePointer ptr = getSRPPtr(J9ROMClassHelper.optionalInfo(romClass), romClass.optionalFlags(), option);
		if (!ptr.isNull()) {
			return VoidPointer.cast(ptr.get());
		} 
		return VoidPointer.NULL;
	}

	public static J9SourceDebugExtensionPointer getSourceDebugExtensionForROMClass(J9ROMClassPointer romClass) throws CorruptDataException {
		SelfRelativePointer srpPtr = getSRPPtr(J9ROMClassHelper.optionalInfo(romClass), romClass.optionalFlags(), J9_ROMCLASS_OPTINFO_SOURCE_DEBUG_EXTENSION);
		if (!srpPtr.isNull()) {
			return J9SourceDebugExtensionPointer.cast(srpPtr.get());
		}
		return J9SourceDebugExtensionPointer.NULL;
	}

	private static U32Pointer getPermittedSubclassPointer(J9ROMClassPointer romClass) throws CorruptDataException {
		SelfRelativePointer srpPtr = getSRPPtr(J9ROMClassHelper.optionalInfo(romClass), romClass.optionalFlags(),
			J9_ROMCLASS_OPTINFO_PERMITTEDSUBCLASSES_ATTRIBUTE);
		if (srpPtr.notNull()) {
			return U32Pointer.cast(srpPtr.get());
		}
		return U32Pointer.NULL;
	}

	public static int getPermittedSubclassCount(J9ROMClassPointer romClass) throws CorruptDataException {
		U32Pointer permittedSubclassPointer = getPermittedSubclassPointer(romClass);
		if (permittedSubclassPointer.notNull()) {
			return permittedSubclassPointer.at(0).intValue();
		}
		return 0;
	}

	public static J9UTF8Pointer getPermittedSubclassNameAtIndex(J9ROMClassPointer romClass, int index) throws CorruptDataException {
		U32Pointer permittedSubclassPointer = getPermittedSubclassPointer(romClass);
		if (permittedSubclassPointer.notNull()) {
			/* extra 1 is to move past permitted subclass count. */
			permittedSubclassPointer = permittedSubclassPointer.add(index + 1);

			SelfRelativePointer nameSrp = SelfRelativePointer.cast(permittedSubclassPointer);
			return J9UTF8Pointer.cast(nameSrp.get());
		}
		return J9UTF8Pointer.NULL;
	}

	public static int getLoadableDescriptorsCount(J9ROMClassPointer romClass) throws CorruptDataException {
		U32Pointer loadableDescriptorsInfoPointer = getLoadableDescriptorsInfoPointer(romClass);
		if (loadableDescriptorsInfoPointer.notNull()) {
			return loadableDescriptorsInfoPointer.at(0).intValue();
		}
		return 0;
	}

	private static U32Pointer getLoadableDescriptorsInfoPointer(J9ROMClassPointer romClass) throws CorruptDataException {
		SelfRelativePointer srpPtr = getSRPPtr(J9ROMClassHelper.optionalInfo(romClass), romClass.optionalFlags(),
			J9_ROMCLASS_OPTINFO_LOADABLEDESCRIPTORS_ATTRIBUTE);
		if (srpPtr.notNull()) {
			return U32Pointer.cast(srpPtr.get());
		}
		return U32Pointer.NULL;
	}

	public static J9UTF8Pointer getLoadableDescriptorAtIndex(J9ROMClassPointer romClass, int index) throws CorruptDataException {
		U32Pointer loadableDescriptorsInfoPointer = getLoadableDescriptorsInfoPointer(romClass);
		if (loadableDescriptorsInfoPointer.notNull()) {
			/* extra 1 is to move past the descriptors count */
			SelfRelativePointer nameSrp = SelfRelativePointer.cast(loadableDescriptorsInfoPointer.add(index + 1));
			return J9UTF8Pointer.cast(nameSrp.get());
		}
		return J9UTF8Pointer.NULL;
	}

	public static U32 getImplicitCreationFlags(J9ROMClassPointer romClass) throws CorruptDataException {
		SelfRelativePointer srpPtr = getSRPPtr(J9ROMClassHelper.optionalInfo(romClass), romClass.optionalFlags(),
			J9_ROMCLASS_OPTINFO_IMPLICITCREATION_ATTRIBUTE);
		if (srpPtr.notNull()) {
			U32Pointer u32Ptr = U32Pointer.cast(srpPtr.get());
			return u32Ptr.at(0);
		}
		return U32.MIN;
	}
}
