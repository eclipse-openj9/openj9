/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagObject;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldTypeBoolean;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldTypeByte;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldTypeDouble;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldTypeFloat;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldTypeInt;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldTypeLong;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldTypeShort;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_GC_MINIMUM_OBJECT_SIZE;
import static com.ibm.j9ddr.vm29.j9.ObjectFieldInfo.fj9object_t_SizeOf;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.TreeMap;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Table;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.ObjectFieldInfo;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectHeapIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

public class ObjectSizeInfo extends Command 
{
	private static final String nl = System.getProperty("line.separator");
	private static final String DATA_SIZE = "Data size";
	private static final String TOTAL_SIZE = "Total size";
	private static final String SPACE_USED = "Space used";

	private String className = null;
	TreeMap<String, ClassFieldInfo> fieldStats;
	private boolean includeArrays;

	public ObjectSizeInfo()
	{
		addCommand("objectsizeinfo", "[-a] [class name]", "Print information about object fields and size of the objects, including unused space");
		includeArrays = true;
	}
	
	private void printHelp(PrintStream out) {
		CommandUtils.dbgPrint(out, "!objectsizeinfo [-a] [classname]   -- Dump the information about objects of type <classname>.\n"
				+ "If no name is given, all classes are displayed. \n"
				+ TOTAL_SIZE + " includes the header, all instance and hidden fields, including unused space for sub-32-bit fields\n"
						+ DATA_SIZE+" includes only the header plus instance and hidden fields\n"
						+ SPACE_USED + " is the total space used for objects including padding for minimum size and alignment\n"
				+"'*' indicates that the size is less than the GC minimum object size.\n"
				+ "-a: suppress inclusion of array classes\n");
	}
	
	private boolean parseArgs(PrintStream out, String[] args) throws DDRInteractiveCommandException {
		className = null;
		includeArrays = true;
		if (args.length > 2) {
			out.append("Invalid number of arguments" + nl);
			return false;
		}
		for (String a: args) {
			if (a.equals("help")) {
				printHelp(out);
				return false;
			} else if (a.equals("-a")){
				includeArrays = false;
			} else if (!a.startsWith("-")) {
				className = a;
			} else {
				out.append("Invalid argument: " + a);
			}
		}
		return true;
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		fieldStats = new TreeMap<String, ClassFieldInfo> ();
		Table summary = new Table("Object field size summary");
		summary.row(ClassFieldInfo.getTitleRow());
		HeapFieldInfo hfi = new HeapFieldInfo();
		
		if (args != null) {
			if (!parseArgs(out, args)) {
				return;
			}
		}
		
		try {
			scanHeap();
			for (FieldInfo cfi: fieldStats.values()) {
				summary.row(cfi.getResults());	
				hfi.addClass(cfi);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
		summary.row(FieldInfo.getTitleRow());
		try {
			summary.row(hfi.getResults());
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
		summary.render(out);
	}
	
	private void scanHeap() {
		try {
			GCHeapRegionIterator regions = GCHeapRegionIterator.from();
			while (regions.hasNext()) {
				GCHeapRegionDescriptor region = regions.next();
				scanObjects(region);
			}
		} catch (CorruptDataException e) {
			e.printStackTrace();
		}

	}
	
	private void scanObjects(GCHeapRegionDescriptor region) throws CorruptDataException	{
		GCObjectHeapIterator heapIterator = GCObjectHeapIterator.fromHeapRegionDescriptor(region, true, true);
		while (heapIterator.hasNext()) {
			J9ObjectPointer object = heapIterator.next();
			J9ClassPointer objClass = J9ObjectHelper.clazz(object);
			if (!objClass.isNull() &&
					(!J9ClassHelper.isArrayClass(objClass) || includeArrays)
					) {
				String objClassString = J9ClassHelper.getJavaName(objClass);
				if ((null == className) || className.equals(objClassString)) {
					ClassFieldInfo cfInfo = fieldStats.get(objClassString);
					if (null == cfInfo) {
						cfInfo = new ClassFieldInfo(objClass);
						fieldStats.put(objClassString, cfInfo);
					} 
					cfInfo.addInstance(object);
				}
			}
		}
	}
	
	static  abstract class FieldInfo {
		public static String[] getTitleRow() {
			return new String[] {"Class", TOTAL_SIZE, DATA_SIZE, SPACE_USED, "Instances", "char", "byte", "short", "int", "long", "float", "double", "boolean", "object", "hidden"};
		}

		protected int instanceCount;		
		protected int charCount;
		protected int byteCount;
		protected int shortCount;
		protected int intCount;
		protected int longCount;
		protected int floatCount;
		protected int doubleCount;
		protected int boolCount;
		protected int objectCount;
		protected int hiddenFields;
		protected long spaceUsed; /* sume of space used by each object of this class */
		protected long totalInstanceSize;
		protected String objectClassString;
		protected long bytesContainingData; /* exclude bytes for padding and alignment */
		protected boolean isArray;
		
		public String[] getResults() throws CorruptDataException {
				ArrayList<String> results = new ArrayList<String>(16);
				results.add(objectClassString);
				String totSizeString = "N/A";
				String minSizeString = "N/A";
				if (!isArray) {
					 totSizeString = Long.toString(totalInstanceSize);
					if (totalInstanceSize < J9_GC_MINIMUM_OBJECT_SIZE) {
						totSizeString += '*';
					}
					 minSizeString = Long.toString(bytesContainingData);
					if (bytesContainingData < J9_GC_MINIMUM_OBJECT_SIZE) {
						minSizeString += '*';
					}
				} 
				results.add(totSizeString);
				results.add(minSizeString);
				results.add(Long.toString(spaceUsed));
				results.add(Integer.toString(instanceCount));
				results.add(Integer.toString(charCount));
				results.add(Integer.toString(byteCount));
				results.add(Integer.toString(shortCount));
				results.add(Integer.toString(intCount));
				results.add(Integer.toString(longCount));
				results.add(Integer.toString(floatCount));
				results.add(Integer.toString(doubleCount));
				results.add(Integer.toString(boolCount));
				results.add(Integer.toString(objectCount));
				results.add(Integer.toString(hiddenFields));
				return results.toArray(new String[results.size()]);
		}

		public FieldInfo() {
			charCount = 0;
			byteCount = 0;
			shortCount = 0;
			intCount = 0;
			longCount = 0;
			floatCount = 0;
			doubleCount = 0;
			boolCount = 0;
			objectCount = 0;
			hiddenFields = 0;
			spaceUsed = 0;
		}

	}
	
	static class ClassFieldInfo extends FieldInfo {
		private J9ClassPointer objectClass;

		public int getInstanceCount() {
			return instanceCount;
		}

		public String getObjectClassString() {
			return objectClassString;
		}


		public ClassFieldInfo(J9ClassPointer objClass) throws CorruptDataException {
			super();
			this.objectClass = objClass;
			instanceCount = 0;
			objectClassString = J9ClassHelper.getJavaName(objClass);
			isArray = false;

			if (!J9ClassHelper.isArrayClass(objClass)) {
				U32 flags = new U32(J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN);
				J9ClassPointer superclass = J9ClassHelper.superclass(objClass);

				totalInstanceSize = objClass.totalInstanceSize().longValue() + fj9object_t_SizeOf;
				Iterator<J9ObjectFieldOffset> fieldIter = J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(objClass.romClass(), objClass, superclass, flags);
				while (fieldIter.hasNext()) {
					J9ObjectFieldOffset fo = fieldIter.next();
					if (!fo.isStatic()) {
						J9ROMFieldShapePointer f = fo.getField();
						UDATA mods = f.modifiers();
						if (mods.anyBitsIn(J9FieldTypeByte)) {
							byteCount += 1;
						} else if (mods.anyBitsIn(J9FieldTypeShort)) {
							shortCount += 1;
						} else if (mods.anyBitsIn(J9FieldTypeInt)) {
							intCount += 1;
						} else if (mods.anyBitsIn(J9FieldTypeLong)) {
							longCount += 1;
						} else if (mods.anyBitsIn(J9FieldTypeFloat)) {
							floatCount += 1;
						} else if (mods.anyBitsIn(J9FieldTypeDouble)) {
							doubleCount += 1;
						} else if (mods.anyBitsIn(J9FieldTypeBoolean)) {
							boolCount += 1;
						} else if (mods.anyBitsIn(J9FieldFlagObject)) {
							objectCount += 1;
						} else  {
							charCount += 1; /* by default, type is char */
						}
						if (fo.isHidden()) {
							hiddenFields += 1;
						}
					}
				}
				bytesContainingData = calculateBytesContainingData() + fj9object_t_SizeOf;
			} else {
				bytesContainingData = 0;
				isArray = true;
			}
		}

		private long calculateBytesContainingData() throws CorruptDataException {
			J9ClassPointer superclass = J9ClassHelper.superclass(objectClass);
			
			long accum = 0;
			if (!superclass.isNull()) {
				accum = superclass.totalInstanceSize().intValue();
				if (superclass.backfillOffset().gt(0)) {
					accum -= ObjectFieldInfo.BACKFILL_SIZE;
				}
			}
			accum += (charCount + byteCount + boolCount) * U8.SIZEOF;
			accum += shortCount * U16.SIZEOF;
			accum += (intCount + floatCount) * U32.SIZEOF;
			accum += (longCount + doubleCount) * U64.SIZEOF;
			accum += objectCount * fj9object_t_SizeOf;
			return accum;
		}

		public void addInstance(J9ObjectPointer object) throws CorruptDataException {
			++instanceCount;
			spaceUsed += ObjectModel.getConsumedSizeInBytesWithHeader(object).longValue();
		}
		
	}
	static class HeapFieldInfo extends FieldInfo {
		public HeapFieldInfo() {
			objectClassString = "Heap summary";
		}
		public void addClass(FieldInfo cfi) {
			instanceCount += cfi.instanceCount;    
			charCount += cfi.charCount * cfi.instanceCount;
			byteCount += cfi.byteCount * cfi.instanceCount;
			shortCount += cfi.shortCount * cfi.instanceCount;
			intCount += cfi.intCount * cfi.instanceCount;
			longCount += cfi.longCount * cfi.instanceCount;
			floatCount += cfi.floatCount * cfi.instanceCount;
			doubleCount += cfi.doubleCount * cfi.instanceCount;
			boolCount += cfi.boolCount * cfi.instanceCount;
			objectCount += cfi.objectCount * cfi.instanceCount;
			hiddenFields += cfi.hiddenFields * cfi.instanceCount;
			totalInstanceSize += cfi.totalInstanceSize * cfi.instanceCount;
			bytesContainingData += cfi.bytesContainingData * cfi.instanceCount;
			spaceUsed += cfi.spaceUsed;
		}
		
	}

}
