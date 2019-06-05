/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.j9.gc.GCClassHeapIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCSegmentIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadListIterator;
import com.ibm.j9ddr.vm29.j9.walkers.MemorySegmentIterator;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9DbgROMClassBuilderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9DbgStringInternTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9InternHashTableEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9TranslationBufferSetPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VTableHeaderPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMMethodHelper;
import com.ibm.j9ddr.vm29.structure.J9ClassInitFlags;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.structure.J9MemorySegment;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;

public class VmCheckCommand extends Command 
{
	private static final String nl = System.getProperty("line.separator");

	public VmCheckCommand()
	{
		addCommand("vmcheck", "", "Run VM state sanity checks");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			J9JavaVMPointer javaVM = J9RASHelper.getVM(DataType.getJ9RASPointer());
			try {
				checkJ9VMThreadSanity(javaVM, out);
			} catch (Exception e) {
				e.printStackTrace(out);
			}
			try {
				checkJ9ClassSanity(javaVM, out);
			} catch (Exception e) {
				e.printStackTrace(out);
			}
			try {
				checkJ9ROMClassSanity(javaVM, out);
			} catch (Exception e) {
				e.printStackTrace(out);
			}
			try {
				checkJ9MethodSanity(javaVM, out);
			} catch (Exception e) {
				e.printStackTrace(out);
			}
			try {
				checkLocalInternTableSanity(javaVM, out);
			} catch (Exception e) {
				e.printStackTrace(out);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	private void appendLine(PrintStream out, String format, Object... args) {
		String line = String.format(format, args);
		out.append(line);
		out.append(nl);
	}

	private void reportError(PrintStream out, String format, Object... args) {
		appendLine(out, "<vm check: FAILED - Error %s>", String.format(format, args));
	}
	
	private void reportMessage(PrintStream out, String format, Object... args) {
		appendLine(out, "<vm check: %s>", String.format(format, args));
	}

	/*
	 *  Based on runtime/vmchk/checkthreads.c
	 *
	 *	J9VMThread sanity:
	 *		Valid VM check:
	 *			J9VMThread->javaVM->javaVM == J9VMThread->javaVM.
	 *
	 *		Exclusive access check:
	 *			If any J9VMThread has exclusive access, ensure no other thread has exclusive or vm access.
	 */
	private void checkJ9VMThreadSanity(J9JavaVMPointer javaVM, PrintStream out) throws CorruptDataException {
		GCVMThreadListIterator gcvmThreadListIterator = GCVMThreadListIterator.from();

		int count = 0;
		int numberOfThreadsWithVMAccess = 0;
		boolean exclusiveVMAccess = javaVM.exclusiveAccessState().eq(J9Consts.J9_XACCESS_EXCLUSIVE);

		reportMessage(out, "Checking threads");
		
		while (gcvmThreadListIterator.hasNext()) {
			J9VMThreadPointer thread = gcvmThreadListIterator.next();

			verifyJ9VMThread(out, thread, javaVM);

			if (thread.inNative().eq(0)) {
				if (thread.publicFlags().allBitsIn(J9Consts.J9_PUBLIC_FLAGS_VM_ACCESS)) {
					numberOfThreadsWithVMAccess += 1;
				}
			}

			count += 1;
		}

		if (exclusiveVMAccess && numberOfThreadsWithVMAccess > 1) {
			reportError(out, "numberOfThreadsWithVMAccess (%d) > 1 with vm->exclusiveAccessState == J9_XACCESS_EXCLUSIVE",
				numberOfThreadsWithVMAccess);
		}
		reportMessage(out, "Checking %d threads done", count);
	}

	private void verifyJ9VMThread(PrintStream out, J9VMThreadPointer thread, J9JavaVMPointer javaVM) throws CorruptDataException {
		J9JavaVMPointer threadJavaVM = thread.javaVM();

		if (!threadJavaVM.eq(javaVM)) {
			reportError(out, "vm (0x%s) != thread->javaVM (0x%s) for thread=0x%s", Long.toHexString(javaVM.getAddress()), Long.toHexString(threadJavaVM.getAddress()), Long.toHexString(thread.getAddress()));
		} else {
			J9JavaVMPointer threadJavaVMJavaVM = threadJavaVM.javaVM();
			if (!threadJavaVMJavaVM.eq(javaVM)) {
				reportError(out, "thread->javaVM (0x%s) != thread->javaVM->javaVM (0x%s) for thread=0x%s", Long.toHexString(threadJavaVM.getAddress()), Long.toHexString(threadJavaVMJavaVM.getAddress()), Long.toHexString(thread.getAddress()));
			}
		}
	}
	
	/*
	 *  Based on vmchk/checkclasses.c r1.7
	 *
	 *	J9Class sanity:
	 *		Eyecatcher check:
	 *			Ensure J9Class->eyecatcher == 0x99669966.
	 *
	 *		Superclasses check:
	 *			Ensure J9Class->superclasses != null unless J9Class is Object.
	 *
	 *		ClassObject null check:
	 *			Ensure J9Class->classObject != null if (J9Class->initializeStatus == J9ClassInitSucceeded)
	 *
	 *		ClassLoader segment check:
	 *			Ensure J9Class->classLoader->classSegments contains J9Class.
	 *
	 *		ConstantPool check:
	 *			Ensure J9Class->ramConstantPool->ramClass is equal to the J9Class.
	 *
	 *		Subclass hierarchy check:
	 *			Ensure subclasses can be traversed per the J9Class classDepth.
	 *
	 *		Obsolete class check:
	 *			Ensure obsolete classes are found in the replacedClass linked list on the currentClass.
	 */
	private void checkJ9ClassSanity(J9JavaVMPointer javaVM, PrintStream out) throws CorruptDataException {

		reportMessage(out, "Checking classes");

		// Stolen from RootScanner.scanClasses()
		// GCClassLoaderIterator gcClassLoaderIterator =
		// GCClassLoaderIterator.from();
		GCSegmentIterator segmentIterator = GCSegmentIterator.fromJ9MemorySegmentList(javaVM.classMemorySegments(), J9MemorySegment.MEMORY_TYPE_RAM_CLASS);
		int count = 0;
		int obsoleteCount = 0;

		while (segmentIterator.hasNext()) {
			J9MemorySegmentPointer segment = segmentIterator.next();

			GCClassHeapIterator classHeapIterator = GCClassHeapIterator.fromJ9MemorySegment(segment);
			while (classHeapIterator.hasNext()) {
				J9ClassPointer clazz = classHeapIterator.next();
				
				if (!J9ClassHelper.isObsolete(clazz)) {
					verifyJ9Class(javaVM, out, clazz);
				} else {
					verifyObsoleteJ9Class(javaVM, out, clazz);
					obsoleteCount++;
				}

				count++;
			}
		}

		reportMessage(out, "Checking %d classes (%d obsolete) done", count, obsoleteCount);
	}

	private boolean verifyJ9Class(J9JavaVMPointer javaVM, PrintStream out, J9ClassPointer classPointer) throws CorruptDataException {
		boolean passed = verifyJ9ClassHeader(javaVM, out, classPointer);

		if (!classPointer.classLoader().isNull()) {
			J9MemorySegmentPointer segment = classPointer.classLoader().classSegments();
			segment = findSegmentInClassLoaderForAddress(classPointer, segment);

			if (segment.isNull()) {
				reportError(out, "class=0x%s not found in classLoader=0x%s", Long.toHexString(classPointer.getAddress()), Long.toHexString(classPointer.classLoader().getAddress()));
				passed = false;
			}
		}

		if (!verifyJ9ClassSubclassHierarchy(javaVM, out, classPointer)) {
			passed = false;
		}
		
		return passed;
	}

	private boolean verifyJ9ClassHeader(J9JavaVMPointer javaVM, PrintStream out, J9ClassPointer classPointer) throws CorruptDataException {
		boolean passed = true;
		J9ROMClassPointer romClass = classPointer.romClass();
		String rootClasses = "java.lang.Object";

		if (false == J9ClassHelper.hasValidEyeCatcher(classPointer)) {
			reportError(out, "0x99669966 != eyecatcher (0x%s) for class=0x%s",
				Long.toHexString(classPointer.eyecatcher().longValue()), Long.toHexString(classPointer.getAddress()));
			passed = false;
		}
		if (romClass.isNull()) {
			reportError(out, "NULL == romClass for class=0x%s", Long.toHexString(classPointer.getAddress()));
			passed = false;
		}
		if (classPointer.classLoader().isNull()) {
			reportError(out, "NULL == classLoader for class=0x%s", Long.toHexString(classPointer.getAddress()));
			passed = false;
		}
		
		long classDepth = J9ClassHelper.classDepth(classPointer).longValue();
		if (classDepth > 0) {
			if (classPointer.superclasses().isNull()) {
				reportError(out, "NULL == superclasses for non-" + rootClasses + " class=0x%s", Long.toHexString(classPointer.getAddress()));
				passed = false;
			} else {
				// Verify that all entries of superclasses[] are non-null.
				for (long i = 0; i < classDepth; i++) {
					if (classPointer.superclasses().at(i).isNull()) {
						reportError(out, "superclasses[%d] is NULL for class=0x%s", Long.toHexString(classPointer.getAddress()));
						passed = false;
						break;
					}
				}
			}
		} else {
			if (classPointer.superclasses().at(-1).notNull()) {
				reportError(out, "superclasses[-1] should be NULL for class=0x%s", Long.toHexString(classPointer.getAddress()));
				passed = false;
			}
		}

		if (classPointer.initializeStatus().eq(J9ClassInitFlags.J9ClassInitSucceeded)) {
			if (classPointer.classObject().isNull()) {
				reportError(out, "NULL == class->classObject for initialized class=0x%s", Long.toHexString(classPointer.getAddress()));
				passed = false;
			}
		}

		if (J9ClassHelper.isObsolete(classPointer)) {
			reportError(out, "clazz=0x%s is obsolete", Long.toHexString(classPointer.getAddress()));
			passed = false;
		}

		if (!romClass.isNull() && !romClass.romConstantPoolCount().eq(0)) {
			J9ConstantPoolPointer constantPool = J9ConstantPoolPointer.cast(classPointer.ramConstantPool());
			J9ClassPointer cpClass = constantPool.ramClass();
			
			if (!classPointer.eq(cpClass)) {
				reportError(out, "clazz=0x%s not equal clazz->ramConstantPool->ramClass=0x%s",
					Long.toHexString(classPointer.getAddress()), Long.toHexString(cpClass.getAddress()));
				passed = false;
			}

		}

		return passed;
	}
	
	private boolean verifyObsoleteJ9Class(J9JavaVMPointer javaVM, PrintStream out, J9ClassPointer classPointer) throws CorruptDataException {
		boolean passed = true;
		J9ClassPointer replacedClass;
		J9ClassPointer currentClass = J9ClassHelper.currentClass(classPointer);

		verifyJ9ClassHeader(javaVM, out, currentClass);

		replacedClass = currentClass.replacedClass();
		while (!replacedClass.isNull()) {
			if (replacedClass.eq(classPointer)) {
				/* Expected case: found classPointer in the list. */
				break;
			}
			replacedClass = replacedClass.replacedClass();
		}

		if (replacedClass.isNull()) {
			reportError(out, "obsolete class=0x%s is not in replaced list on currentClass=0x%s",
				Long.toHexString(classPointer.getAddress()), Long.toHexString(currentClass.getAddress()));
			passed = false;
		}

		return passed;
	}
	
	private boolean verifyJ9ClassSubclassHierarchy(J9JavaVMPointer javaVM, PrintStream out, J9ClassPointer classPointer) throws CorruptDataException {
		int index = 0;
		UDATA rootDepth = J9ClassHelper.classDepth(classPointer);
		J9ClassPointer currentClass = classPointer;
		boolean done = false;

		while (!done) {
			J9ClassPointer nextSubclass = currentClass.subclassTraversalLink();

			if (nextSubclass.isNull()) {
				reportError(out, "class=0x%s had NULL entry in subclassTraversalLink list at index=%d following class=0x%s",
					Long.toHexString(classPointer.getAddress()), index, Long.toHexString(currentClass.getAddress()));
				return false;
			}

			/* Sanity check the nextSubclass. */
			if (!verifyJ9ClassHeader(javaVM, out, nextSubclass)) {
				return false;
			}
			
			if (J9ClassHelper.classDepth(nextSubclass).lte(rootDepth)) {
				done = true;
			} else {
				currentClass = nextSubclass;
				index++;
			}
		}

		return true;
	}

	private J9MemorySegmentPointer findSegmentInClassLoaderForAddress(J9ClassPointer classPointer, J9MemorySegmentPointer segment) throws CorruptDataException {
		while (!segment.isNull()) {
			long address = classPointer.getAddress();
			if ((address >= segment.heapBase().longValue()) && (address < segment.heapAlloc().longValue())) {
				break;
			}
			segment = segment.nextSegment();
		}
		return segment;
	}

	/*
	 *  Based on vmchk/checkromclasses.c r1.5
	 * 
	 *	J9ROMClass sanity:
	 *		SRP check:
	 *			Ensure J9ROMClass->interfaces SRP is in the same segment if J9ROMClass->interfaceCount != 0.
	 *			Ensure J9ROMClass->romMethods SRP is in the same segment if J9ROMClass->romMethodCount != 0.
	 *			Ensure J9ROMClass->romFields SRP is in the same segment if J9ROMClass->romFieldCount != 0.
	 *			Ensure J9ROMClass->innerClasses SRP is in the same segment if J9ROMClass->innerClasseCount != 0.
	 *			Ensure cpShapeDescription in the same segment.
	 *			Ensure all SRPs are in range on 64 bit platforms (including className, superclassName, and outerClassName).
	 *
	 *		ConstantPool count check:
	 *			Ensure ramConstantPoolCount <= romConstantPoolCount
	 */
	private void checkJ9ROMClassSanity(J9JavaVMPointer vm, PrintStream out) throws CorruptDataException {

		reportMessage(out, "Checking ROM classes");

		// Stolen from RootScanner.scanClasses()
		// GCClassLoaderIterator gcClassLoaderIterator =
		// GCClassLoaderIterator.from();
		GCSegmentIterator segmentIterator = GCSegmentIterator.fromJ9MemorySegmentList(vm.classMemorySegments(), J9MemorySegment.MEMORY_TYPE_RAM_CLASS);
		int count = 0;
		while (segmentIterator.hasNext()) {
			J9MemorySegmentPointer segment = segmentIterator.next();

			GCClassHeapIterator classHeapIterator = GCClassHeapIterator.fromJ9MemorySegment(segment);
			while (classHeapIterator.hasNext()) {
				J9ClassPointer clazz = classHeapIterator.next();
				verifyJ9ROMClass(out, vm, clazz);
				count++;
			}
		}

		reportMessage(out, "Checking %d ROM classes done", count);

	}

	private void verifyJ9ROMClass(PrintStream out, J9JavaVMPointer vm, J9ClassPointer clazz) throws CorruptDataException {
		J9ROMClassPointer romClass = clazz.romClass();
		J9ClassLoaderPointer classLoader = clazz.classLoader();

		J9MemorySegmentPointer segment = findSegmentInClassLoaderForAddress(classLoader, romClass);
		if (!segment.isNull()) {
			long address;
			if (romClass.interfaceCount().longValue() != 0) {
				address = romClass.interfaces().getAddress();
				verifyAddressInSegment(out, vm, segment, address, "romClass->interfaces");
			}
			if (romClass.romMethodCount().longValue() != 0) {
				address = romClass.romMethods().longValue();
				verifyAddressInSegment(out, vm, segment, address, "romClass->romMethods");
			}

			if (romClass.romFieldCount().longValue() != 0) {
				address = romClass.romFields().longValue();
				verifyAddressInSegment(out, vm, segment, address, "romClass->romFields");
			}

			if (romClass.innerClassCount().longValue() != 0) {
				address = romClass.innerClasses().longValue();
				verifyAddressInSegment(out, vm, segment, address, "romClass->innerClasses");
			}

			U32Pointer cpShapeDescription = J9ROMClassHelper.cpShapeDescription(romClass);

			/* TODO: is !isNull() check required or not? */
			if (!cpShapeDescription.isNull()) {
				address = cpShapeDescription.getAddress();
				verifyAddressInSegment(out, vm, segment, address, "romClass->cpShapeDescription");
			}
		}

		{
			J9UTF8Pointer className = romClass.className();
			J9UTF8Pointer superclassName = romClass.superclassName();
			J9UTF8Pointer outerClassName = romClass.outerClassName();

			if (className.isNull() || !verifyUTF8(className)) {
				reportError(out, "invalid className=0x%s utf8 for romClass=0x%s", Long.toHexString(className.getAddress()), Long.toHexString(romClass.getAddress()));
			}

			if (!superclassName.isNull() && !verifyUTF8(superclassName)) {
				reportError(out, "invalid superclassName=0x%s utf8 for romClass=0x%s", Long.toHexString(superclassName.getAddress()), Long.toHexString(romClass.getAddress()));
			}

			if (!outerClassName.isNull() && !verifyUTF8(outerClassName)) {
				reportError(out, "invalid outerclassName=0x%s utf8 for romClass=0x%s", Long.toHexString(outerClassName.getAddress()), Long.toHexString(romClass.getAddress()));
			}
		}

		UDATA ramConstantPoolCount = romClass.ramConstantPoolCount();
		UDATA romConstantPoolCount = romClass.romConstantPoolCount();
		if (ramConstantPoolCount.gt(romConstantPoolCount)) {
			reportError(out, "ramConstantPoolCount=%d > romConstantPoolCount=%d for romClass=0x%s", ramConstantPoolCount.longValue(), romConstantPoolCount.longValue(), Long.toHexString(romClass.getAddress()));
		}

	}

	private boolean verifyUTF8(J9UTF8Pointer utf8) throws CorruptDataException {
		if (utf8.isNull()) {
			return false;
		}
		UDATA length = new UDATA(utf8.length());
		U8Pointer utf8Data = utf8.dataEA();
		while (length.longValue() > 0) {
			U16 temp = new U16(0); // not used

			U32 lengthRead = decodeUTF8CharN(utf8Data, temp, length);
			if (lengthRead.eq(0)) {
				return false;
			}
			length = length.sub(lengthRead);
			utf8Data = utf8Data.addOffset(lengthRead);
		}

		return true;
	}

	/**
	 * Decode the UTF8 character.
	 * 
	 * Decode the input UTF8 character and stores it into result.
	 * 
	 * @param[in] input The UTF8 character
	 * @param[out] result buffer for unicode characters
	 * @param[in] bytesRemaining number of bytes remaining in input
	 * 
	 * @return The number of UTF8 characters consumed (1,2,3) on success, 0 on
	 *         failure
	 * @throws CorruptDataException
	 * @note Don't read more than bytesRemaining characters.
	 * @note If morecharacters are required to fully decode the character,
	 *       return failure
	 */
	U32 decodeUTF8CharN(U8Pointer input, /** not used **/
	U16 result, UDATA bytesRemaining) throws CorruptDataException {
		U8 c;
		U8Pointer cursor = input;

		if (bytesRemaining.longValue() < 1) {
			return new U32(0);
		}

		c = cursor.at(0);
		cursor = cursor.add(1);

		if (c.eq(0x0)) {
			/* illegal NUL encoding */
			return new U32(0);
		} else if ((c.bitAnd(0x80)).eq(0x0)) {
			/* one byte encoding */
			// *result = (U_16)c;
			return new U32(1);
		} else if (c.bitAnd(0xE0).eq(0xC0)) {
			/* two byte encoding */
			U16 unicodeC;

			if (bytesRemaining.lt(2)) {
				return new U32(0);
			}
			unicodeC = new U16(c.bitAnd(0x1F).leftShift(6));

			c = cursor.at(0);
			cursor = cursor.add(1);

			unicodeC = unicodeC.add(new U16(unicodeC.add(c.bitAnd(0x3F))));
			if (!c.bitAnd(0xC0).eq(0x80)) {
				return new U32(0);
			}
			// *result = unicodeC;
			return new U32(2);
		} else if (c.bitAnd(0xF0).eq(0xE0)) {
			/* three byte encoding */
			U16 unicodeC;

			if (bytesRemaining.lt(3)) {
				return new U32(0);
			}
			unicodeC = new U16(c.bitAnd(0x0F).leftShift(12));

			c = cursor.at(0);
			cursor = cursor.add(1);

			unicodeC = unicodeC.add(new U16(c.bitAnd(0x3F).leftShift(6)));

			if (!c.bitAnd(0xC0).eq(0x80)) {
				return new U32(0);
			}

			c = cursor.at(0);
			cursor = cursor.add(1);

			unicodeC = unicodeC.add(new U16(c.bitAnd(0x3F)));
			if (!c.bitAnd(0xC0).eq(0x80)) {
				return new U32(0);
			}
			// *result = unicodeC;
			return new U32(3);
		} else {
			return new U32(0);
		}
	}

	private void verifyAddressInSegment(PrintStream out, J9JavaVMPointer vm, J9MemorySegmentPointer segment, long address, String description) throws CorruptDataException {
		U8Pointer heapBase = segment.heapBase();
		U8Pointer heapAlloc = segment.heapAlloc();

		if (address < heapBase.getAddress() || (address >= heapAlloc.getAddress())) {
			reportError(out, "address 0x%s (%s) not in segment [heapBase=0x%s, heapAlloc=0x%s]", address, description, Long.toHexString(heapBase.getAddress()), Long.toHexString(heapAlloc.getAddress()));
		}
	}

	/**
	 * Based on vmchk/checkclasses.c function: findSegmentInClassLoaderForAddress
	 * 
	 * This method searches classloader's segments to find out on which segment this ROMClass lays in.
	 * @param classLoader Classloader that romclass is being searched
	 * @param romClassPointer ROMClass that is searched in classloader segments
	 * @return Classloader segment which has this romclass, otherwise null. 
	 * @throws CorruptDataException
	 */
	public J9MemorySegmentPointer findSegmentInClassLoaderForAddress(J9ClassLoaderPointer classLoader, J9ROMClassPointer romClassPointer) throws CorruptDataException {
		MemorySegmentIterator memorySegmentIterator = new MemorySegmentIterator(classLoader.classSegments(), MemorySegmentIterator.MEMORY_ALL_TYPES, true);
		J9MemorySegmentPointer memorySegmentPointer = J9MemorySegmentPointer.NULL;
		J9MemorySegmentPointer foundMemorySegmentPointer = J9MemorySegmentPointer.NULL;
		
		while (memorySegmentIterator.hasNext()) {
			Object next2 = memorySegmentIterator.next();
			memorySegmentPointer = (J9MemorySegmentPointer) next2;
			if (romClassPointer.getAddress() >= memorySegmentPointer.heapBase().longValue() && romClassPointer.getAddress() < memorySegmentPointer.heapAlloc().longValue()) {
				foundMemorySegmentPointer = memorySegmentPointer;
				break;
			}
		}
		return foundMemorySegmentPointer;
	}

	/*
	 *  Based on vmchk/checkmethods.c r1.5
	 * 
	 *	J9Method sanity:
	 *		Bytecode check:
	 *			Ensure (bytecodes - sizeof(J9ROMMethod) is in the right "area" to be a ROM Method.
	 *			Use ROMClass to determine where ROM methods for that class begin.
	 *		Constant pool check:
	 *			Ensure method's constant pool is the same as that of its J9Class.
	 *			Useful to validate that HCR doesn't violate this invariant.
	 *		VTable check:
	 *			If method of a non-interface class has modifier J9AccMethodVTable,
	 *			ensure it exists in the VTable of its J9Class.
	 *			Useful to validate that HCR doesn't violate this invariant.
	 */
	private void checkJ9MethodSanity(J9JavaVMPointer vm, PrintStream out) throws CorruptDataException {
		reportMessage(out, "Checking methods");

		// Stolen from RootScanner.scanClasses()
		// GCClassLoaderIterator gcClassLoaderIterator =
		// GCClassLoaderIterator.from();
		GCSegmentIterator segmentIterator = GCSegmentIterator.fromJ9MemorySegmentList(vm.classMemorySegments(), J9MemorySegment.MEMORY_TYPE_RAM_CLASS);
		int count = 0;
		while (segmentIterator.hasNext()) {
			J9MemorySegmentPointer segment = segmentIterator.next();

			GCClassHeapIterator classHeapIterator = GCClassHeapIterator.fromJ9MemorySegment(segment);
			while (classHeapIterator.hasNext()) {
				J9ClassPointer clazz = classHeapIterator.next();
				if (!J9ClassHelper.isObsolete(clazz)) {
					count += verifyClassMethods(vm, out, clazz);
				}
			}
		}

		reportMessage(out, "Checking %d methods done", count);
	}

	private int verifyClassMethods(J9JavaVMPointer vm, PrintStream out, J9ClassPointer clazz) throws CorruptDataException {
		int count = 0;
		J9ROMClassPointer romClass = clazz.romClass();
		int methodCount = romClass.romMethodCount().intValue();
		J9MethodPointer methods = clazz.ramMethods();
		boolean isInterfaceClass = J9ROMClassHelper.isInterface(romClass);
		J9ConstantPoolPointer ramConstantPool = J9ConstantPoolPointer.cast(clazz.ramConstantPool());

		for (int i = 0; i < methodCount; i++) {
			J9MethodPointer method = methods.add(i);
			J9ROMMethodPointer romMethod = J9MethodHelper.romMethod(method);
			boolean methodInVTable = J9ROMMethodHelper.hasVTable(romMethod);

			if (!findROMMethodInClass(vm, romClass, romMethod, methodCount)) {
				reportError(out, "romMethod=0x%s (ramMethod=0x%s) not found in romClass=0x%s",
					Long.toHexString(romMethod.getAddress()), Long.toHexString(method.getAddress()), Long.toHexString(romClass.getAddress()));
			}

			if (!isInterfaceClass && methodInVTable) {
				if (!findMethodInVTable(method, clazz)) {
					reportError(out, "romMethod=0x%s (ramMethod=0x%s) not found in vTable of ramClass=0x%s",
						Long.toHexString(romMethod.getAddress()), Long.toHexString(method.getAddress()), Long.toHexString(clazz.getAddress()));
				}
			}

			if (!ramConstantPool.eq(ConstantPoolHelpers.J9_CP_FROM_METHOD(method))) {
				reportError(out, "ramConstantPool=0x%s on ramMethod=0x%s not equal to ramConstantPool=0x%s on ramClass=0x%s",
					Long.toHexString(ConstantPoolHelpers.J9_CP_FROM_METHOD(method).getAddress()), Long.toHexString(method.getAddress()),
					Long.toHexString(ramConstantPool.getAddress()), Long.toHexString(clazz.getAddress()));
			}
			
			count++;
		}
	
		return count;
	}
	
	private boolean findMethodInVTable(J9MethodPointer method, J9ClassPointer classPointer) throws CorruptDataException {
		UDATAPointer vTable;
		long vTableSize;
		long vTableIndex;

		if (AlgorithmVersion.getVersionOf(AlgorithmVersion.VTABLE_VERSION).getAlgorithmVersion() >= 1) {
			J9VTableHeaderPointer vTableHeader = J9ClassHelper.vTableHeader(classPointer);
			vTableSize = vTableHeader.size().longValue();
			vTable = J9ClassHelper.vTable(vTableHeader);
			vTableIndex = 0;
		} else {
			vTable = J9ClassHelper.oldVTable(classPointer);
			vTableSize = vTable.at(0).longValue() + 1;
			vTableIndex = 2;
		}
		

		for (; vTableIndex < vTableSize; vTableIndex++) {
			//System.out.printf("%d COMP 0x%s vs. 0x%s\n", (int)vTableIndex, Long.toHexString(method.getAddress()), Long.toHexString(vTable.at(vTableIndex).longValue()));
			if (method.eq(J9MethodPointer.cast(vTable.at(vTableIndex)))) {
				return true;
			}
		}

		return false;
	}

	private boolean findROMMethodInClass(J9JavaVMPointer vm, J9ROMClassPointer romClass, J9ROMMethodPointer romMethodToFind, int methodCount) throws CorruptDataException {
		J9ROMMethodPointer romMethod = romClass.romMethods();
		for (int i = 0; i < methodCount; i++) {
			if (i != 0) {
				romMethod = ROMHelp.nextROMMethod(romMethod);
			}
			if (romMethodToFind.eq(romMethod)) {
				return true;
			}
		}
		return false;
	}

	/*
	 *  Based on runtime/vmchk/checkinterntable.c
	 * 
	 *	J9LocalInternTableSanity sanity:
	 *		if (J9JavaVM->dynamicLoadBuffers != NULL) && (J9JavaVM->dynamicLoadBuffers->romClassBuilder != NULL)
	 *			invariantInternTree check:
	 *				For each J9InternHashTableEntry
	 *					Ensure J9InternHashTableEntry->utf8 is valid
	 *					Ensure J9InternHashTableEntry->classLoader is valid
	 */
	private void checkLocalInternTableSanity(J9JavaVMPointer vm, PrintStream out) throws CorruptDataException {
		int count = 0;

		reportMessage(out, "Checking ROM intern string nodes");

		J9TranslationBufferSetPointer dynamicLoadBuffers = vm.dynamicLoadBuffers();
		if (dynamicLoadBuffers.notNull()) {
			J9DbgROMClassBuilderPointer romClassBuilder = J9DbgROMClassBuilderPointer.cast(dynamicLoadBuffers.romClassBuilder());
			if (romClassBuilder.notNull()) {
				J9DbgStringInternTablePointer stringInternTable = romClassBuilder.stringInternTable();
				J9InternHashTableEntryPointer node = stringInternTable.headNode();
				while (node.notNull()) {
					J9UTF8Pointer utf8 = node.utf8();
					J9ClassLoaderPointer classLoader = node.classLoader();
					if (!verifyUTF8(utf8)) {
						reportError(out, "invalid utf8=0x%s for node=0x%s",
								Long.toHexString(utf8.getAddress()), Long.toHexString(node.getAddress()));
					}
					if (!verifyJ9ClassLoader(vm, classLoader)) {
						reportError(out, "invalid classLoader=0x%s for node=0x%s",
								Long.toHexString(classLoader.getAddress()), Long.toHexString(node.getAddress()));
					}
					count += 1;
					node = node.nextNode();
				}
			}
		}
		reportMessage(out, "Checking %d ROM intern string nodes done", count);
	}

	private boolean verifyJ9ClassLoader(J9JavaVMPointer vm, J9ClassLoaderPointer classLoader) throws CorruptDataException {

		GCClassLoaderIterator classLoaderIterator = GCClassLoaderIterator.from();
		while(classLoaderIterator.hasNext()) {
			J9ClassLoaderPointer classLoaderPointer = classLoaderIterator.next();
			if(classLoaderPointer.eq(classLoader)) {
				return true;
			}			
		}	
		return false;
	}
}
