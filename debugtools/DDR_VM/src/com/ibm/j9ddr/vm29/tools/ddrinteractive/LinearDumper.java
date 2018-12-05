/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Stack;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.I16Pointer;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.I8Pointer;
import com.ibm.j9ddr.vm29.pointer.IDATAPointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U64Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.WideSelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMNameAndSignaturePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.structure.J9UTF8;
import com.ibm.j9ddr.vm29.types.UDATA;

public class LinearDumper implements IClassWalkCallbacks {
	/**
	 * Private class used to store the regions.
	 *
	 * @author jeanpb
	 */
	public static class J9ClassRegion implements Comparable<J9ClassRegion> {
		public J9ClassRegion(AbstractPointer slotPtr, SlotType type, String name,
				String additionalInfo, long length, long offset, boolean computePadding) {
			this.slotPtr = slotPtr;
			this.type = type;
			this.name = name;
			this.additionalInfo = additionalInfo;
			this.length = length;
			this.offset = offset;
			this.computePadding = computePadding;
		}

		private final AbstractPointer slotPtr;
		private final SlotType type;
		private final String name;
		final String additionalInfo;
		private final long length;
		final long offset;
		private final boolean computePadding;

		@Override
		public int compareTo(final J9ClassRegion region2) {
			final J9ClassRegion region1 = this;
			int delta;

			{
				/* compare slot pointers: lower addresses go first, and corrupt regions at the end */
				boolean corrupt = false;
				long slot1 = 0;
				long slot2 = 0;

				try {
					slot1 = region1.getSlotPtr().longValue();
				} catch (CorruptDataException e) {
					corrupt = true;
				}

				try {
					slot2 = region2.getSlotPtr().longValue();
					if (corrupt) {
						/* region1 is corrupt, but region2 is not */
						return +1;
					}
				} catch (CorruptDataException e) {
					if (corrupt) {
						/* both regions are corrupt */
						return 0;
					} else {
						/* region2 is corrupt, but region1 is not */
						return -1;
					}
				}

				delta = Long.compare(slot1, slot2);
			}

			if (0 == delta) {
				SlotType type1 = region1.getType();
				SlotType type2 = region2.getType();

				/* compare types: one with a lower ordinal goes first */
				delta = Integer.compare(type2.ordinal(), type1.ordinal());

				if (0 == delta) {
					switch (type1) {
					case J9_SECTION_START:
					case J9_SECTION_END:
						/* compare section lengths: a longer section goes first */
						delta = Long.compare(region2.getLength(), region1.getLength());

						if (0 == delta) {
							/* compare section names: a longer name goes first */
							delta = Integer.compare(region2.getName().length(), region1.getName().length());
						}

						if (type1 == SlotType.J9_SECTION_END) {
							/* reverse ordering for section ends */
							delta = -delta;
						}
						break;

					default:
						break;
					}
				}
			}

			return delta;
		}

		public String getName() {
			return name;
		}

		public long getLength() {
			return length;
		}

		public SlotType getType() {
			return type;
		}

		public AbstractPointer getSlotPtr() {
			return slotPtr;
		}

		public boolean getComputePadding() {
			return computePadding;
		}
	}

	public static class J9ClassRegionNode implements Comparable<J9ClassRegionNode> {
		private J9ClassRegion nodeValue;
		private LinkedList<J9ClassRegionNode> children;

		public J9ClassRegionNode(J9ClassRegion nodeValue) {
			this.nodeValue = nodeValue;
			this.children = new LinkedList<J9ClassRegionNode>();
		}

		public J9ClassRegion getNodeValue() {
			return nodeValue;
		}

		public List<J9ClassRegionNode> getChildren() {
			return children;
		}

		public void addChild(J9ClassRegionNode child) {
			this.children.addLast(child);
		}

		public int compareTo(J9ClassRegionNode region2) {
			AbstractPointer slot1 = this.nodeValue.getSlotPtr();
			AbstractPointer slot2 = region2.nodeValue.getSlotPtr();

			if (slot1.lt(slot2)) {
				return -1;
			} else if (slot1.gt(slot2)) {
				return 1;
			} else {
				return 0;
			}
		}
	}

	private List<J9ClassRegion> classRegions = new LinkedList<J9ClassRegion>();
	private long firstJ9_ROM_UTF8 = Long.MAX_VALUE;
	private long lastJ9_ROM_UTF8 = Long.MIN_VALUE;

	public void addSlot(StructurePointer clazz, SlotType type,
			AbstractPointer slotPtr, String slotName)
			throws CorruptDataException {
		addSlot(clazz, type, slotPtr, slotName, "");
	}

	public void addSlot(StructurePointer clazz, SlotType type,
			AbstractPointer slotPtr, String slotName, String additionalInfo)
			throws CorruptDataException {
		try {
			J9ROMNameAndSignaturePointer nas;
			long offset;
			/* The slots of the type J9_ROM_UTF8 are changed to have 2 slots:
			 * -J9_SRP_TO_STRING
			 * -J9_ROM_UTF8
			 * This is done because we want to print the SRP field and also print
			 * the UTF8 it is pointing to */
			switch (type) {
				case J9_ROM_UTF8:
					offset = slotPtr.getAddress() - clazz.getAddress();
					classRegions.add(new J9ClassRegion(slotPtr, SlotType.J9_SRP_TO_STRING, slotName,
							additionalInfo, type.getSize(), offset, true));
		
					VoidPointer srp = SelfRelativePointer.cast(slotPtr).get();
					
					addUTF8Region(clazz, slotName, additionalInfo, srp);
					break;
				case J9_UTF8:
					addUTF8Region(clazz, slotName, additionalInfo, slotPtr);
					break;
				/* The fields of the type J9_SRPNAS or J9_SRP are changed to have 2 J9_ROM_UTF8
				 * fields for their name and signature separated. */
				case J9_SRPNAS:
					nas = J9ROMNameAndSignaturePointer.cast(SelfRelativePointer.cast(slotPtr).get());
					if (nas.notNull()) {
						addSlot(clazz, SlotType.J9_ROM_UTF8, nas.nameEA(), "name");
						addSlot(clazz, SlotType.J9_ROM_UTF8, nas.signatureEA(), "signature");
					}
					/* Since it is a SRP to a NAS, also print the SRP field. */
					addSlot(clazz, SlotType.J9_SRP, slotPtr, "cpFieldNAS");
					break;
				case J9_NAS:
					nas = J9ROMNameAndSignaturePointer.cast(slotPtr);
					addSlot(clazz, SlotType.J9_ROM_UTF8, nas.nameEA(), "name");
					addSlot(clazz, SlotType.J9_ROM_UTF8, nas.signatureEA(), "signature");
					break;
				case J9_IntermediateClassData:
					offset = slotPtr.getAddress() - clazz.getAddress();
					classRegions.add(new J9ClassRegion(slotPtr, type, slotName,
							additionalInfo, ((J9ROMClassPointer)clazz).intermediateClassDataLength().longValue(), offset, true));
					break;
				default:
					offset = slotPtr.getAddress() - clazz.getAddress();
					classRegions.add(new J9ClassRegion(slotPtr, type, slotName,
							additionalInfo, type.getSize(), offset, true));
					break;
			}
		} catch (Exception e) {
			
		}
	}

	private void addUTF8Region(StructurePointer clazz, String slotName,
			String additionalInfo, AbstractPointer utf8String)
			throws CorruptDataException {
		long offset = utf8String.getAddress() - clazz.getAddress();
		/* We do not want to print UTF8 outside of the ROM class. */
		long clazzSize = ((J9ROMClassPointer) clazz).romSize().longValue();
		if ((offset > 0) && (offset < clazzSize)) {
			if (utf8String.notNull()) {
				long UTF8Length = getUTF8Length(J9UTF8Pointer.cast(utf8String));
				if (utf8String.getAddress() < firstJ9_ROM_UTF8) {
					firstJ9_ROM_UTF8 = utf8String.getAddress();
				}
				if ((utf8String.getAddress() + UTF8Length) > lastJ9_ROM_UTF8) {
					lastJ9_ROM_UTF8 = utf8String.getAddress() + UTF8Length;
				}
				classRegions.add(new J9ClassRegion(utf8String,
						SlotType.J9_ROM_UTF8, slotName, additionalInfo,
						UTF8Length, offset, true));
			}
		}
	}

	public void addRegion(StructurePointer clazz, SlotType type,
			AbstractPointer slotPtr, String slotName, long length, boolean computePadding)
			throws CorruptDataException {
		classRegions.add(new J9ClassRegion(slotPtr, type, slotName, "", length,
				slotPtr.getAddress() - clazz.getAddress(), computePadding));
	}
	
	public void addSection(StructurePointer clazz, AbstractPointer address,
			long length, String name, boolean computePadding) throws CorruptDataException {
		if (0 != length) {
			addRegion(clazz, SlotType.J9_SECTION_START, address, name, length, computePadding);
			addRegion(clazz, SlotType.J9_SECTION_END, address.addOffset(length), name, length, computePadding);
		}
	}
	
	/**
	 * 
	 * Returns a tree of regions and slots. Each slot is under a region. The
	 * root element is always null.
	 * @param classWalker 
	 * 
	 * @return J9ClassRegionNode tree of J9ClassRegion
	 * @throws CorruptDataException
	 */
	public J9ClassRegionNode getAllRegions(ClassWalker classWalker) throws CorruptDataException {
		classWalker.allSlotsInObjectDo(this);
		final StructurePointer clazz = classWalker.getClazz();
		
		/* Add the UTF8 region */
		if (firstJ9_ROM_UTF8 != Long.MAX_VALUE) {
			addSection(clazz, PointerPointer.cast(firstJ9_ROM_UTF8), lastJ9_ROM_UTF8 - firstJ9_ROM_UTF8, "UTF8", true);
		}

		groupSectionByName(clazz, "methodDebugInfo", false);
		groupSectionByName(clazz, "variableInfo", false);
		
		/*
		 * the offset is a pointer which points at the end of the current
		 * region, in the case of a region which have no real size, it points at
		 * the beginning of the region
		 */
		AbstractPointer offset = PointerPointer.NULL;
		
		J9ClassRegionNode currentNode = new J9ClassRegionNode(null);
		Stack<J9ClassRegionNode> parentStack = new Stack<J9ClassRegionNode>();
		J9ClassRegion previousRegion = null;
		
		Collections.sort(classRegions);
		for (J9ClassRegion region : classRegions) {
			if (isSameRegion(previousRegion, region)) {
				previousRegion = region;
				continue;
			}
			previousRegion = region;
			if (SlotType.J9_SECTION_START == region.getType()) {
				if (region.getComputePadding() && offset.notNull() && !offset.eq(region.getSlotPtr())) {
					currentNode.addChild(new J9ClassRegionNode(new J9ClassRegion(offset, SlotType.J9_Padding, "Padding", "", region.getSlotPtr().getAddress() - offset.getAddress(), 0, true)));
				}
				if (region.getComputePadding()) {
					offset = region.getSlotPtr();
				}
				
				parentStack.push(currentNode);
				J9ClassRegionNode newChild = new J9ClassRegionNode(region);
				currentNode.addChild(newChild);
				currentNode = newChild;
			} else if (SlotType.J9_SECTION_END == region.getType()) {
				if (region.getComputePadding()) {
					long paddingSize = (region.getSlotPtr().getAddress() - offset.getAddress());
					if (paddingSize != 0) {
						currentNode.addChild(new J9ClassRegionNode(new J9ClassRegion(offset, SlotType.J9_Padding, "Padding", "", paddingSize , 0, true)));
					}
					offset = region.getSlotPtr();
				}
				currentNode = parentStack.pop();
			} else {
				boolean computePadding = false;
				if (currentNode.getNodeValue()!=null){
					computePadding = currentNode.getNodeValue().getComputePadding();
				}
				if (computePadding && offset.notNull() && !offset.eq(region.getSlotPtr())) {
					currentNode.addChild(new J9ClassRegionNode(new J9ClassRegion(offset, SlotType.J9_Padding, "Padding", "", region.getSlotPtr().getAddress() - offset.getAddress(), 0, true)));
				}
				if (computePadding) {
					offset = region.getSlotPtr().addOffset(region.length);
				}
				currentNode.addChild(new J9ClassRegionNode(region));
			}
		}
		// Padding after the class and inside the romSize.
		if (clazz instanceof J9ROMClassPointer) {
			long size = J9ROMClassPointer.cast(clazz).romSize().longValue();
			long paddingSize = (clazz.longValue() + size) - offset.longValue();
			if (paddingSize != 0) {
				currentNode.addChild(new J9ClassRegionNode(new J9ClassRegion(offset, SlotType.J9_Padding, "Padding", "", paddingSize, 0, true)));
				// The class padding might be inserted out of order
				Collections.sort(currentNode.getChildren());
			}
		}

		return currentNode;
	}

	/**
	 * Find the first and the last section and group them in another section with the same name ending with a "s".
	 * For example, it will group all of the methodDebugInfo in a section named methodDebugInfos
	 * It is important to note that the function does not check if the sections grouped are contiguous, it will
	 * group all sections between the first and the last section even if their names doesn't match.
	 * 
	 * @param clazz
	 * @param name name of sections to group
	 * @param computePadding
	 * @throws CorruptDataException
	 */
	private void groupSectionByName(final StructurePointer clazz, String name, boolean computePadding)
			throws CorruptDataException {
		AbstractPointer firstMethodDebugInfo = null;
		AbstractPointer lastMethodDebugInfo = null;
		for (J9ClassRegion region : classRegions) {
			if (SlotType.J9_SECTION_START == region.getType() && region.getName().equals(name)) {
				if (firstMethodDebugInfo == null || region.getSlotPtr().lt(firstMethodDebugInfo)) {
					firstMethodDebugInfo = region.getSlotPtr();
				}
				if (lastMethodDebugInfo == null || region.getSlotPtr().add(region.getLength()).gt(lastMethodDebugInfo)) {
					lastMethodDebugInfo = region.getSlotPtr().addOffset(region.getLength());
				}
			}
		}
		if (firstMethodDebugInfo != null && lastMethodDebugInfo != null) {
			UDATA length = UDATA.cast(lastMethodDebugInfo).sub(UDATA.cast(firstMethodDebugInfo));
			addSection(clazz, PointerPointer.cast(firstMethodDebugInfo), length.longValue(), name + "s", computePadding);
		}
	}
	
	
	
	/**
	 * Prints a class in a linear way to the PrintStream
	 * 
	 * @param out
	 * @param classWalker
	 *            an instance of a IClassWalker
	 * @param nestingThreshold
	 *            the level of detail. Level 1 prints the sections and the
	 *            sizes. Level 2 prints more details.
	 * @throws CorruptDataException
	 */
	public void gatherLayoutInfo(PrintStream out, ClassWalker classWalker,
			long nestingThreshold) throws CorruptDataException {

		J9ClassRegionNode allRegionsNode = getAllRegions(classWalker);
		printAllRegions(out, classWalker.getClazz(), nestingThreshold, allRegionsNode, 0);
	}
	public static void printAllRegions(PrintStream out, StructurePointer clazz,
			long nestingThreshold, J9ClassRegionNode regionNode, int nesting) throws CorruptDataException {
		final J9ClassRegion region = regionNode.getNodeValue();

		if (nesting == nestingThreshold || regionNode.getChildren().size() == 0) {
			printRegionLine(region, out);
			return;
		} else if (nesting > nestingThreshold) {
			return;
		}
		
		/* section start */
		if (region != null) {
			String str = String.format("Section Start: %s (%d bytes)", region.getName(), region.getLength());
			out.println();
			out.println(String.format("=== %-85s ===", str));
		}
		
		/* subsections */
		for (J9ClassRegionNode classRegionNode:regionNode.getChildren()) {
			printAllRegions(out, clazz, nestingThreshold, classRegionNode, nesting + 1);
		}
		
		/* section end */
		if (region != null) {
			String str = String.format("Section End: %s", region.getName());
			out.println();
			out.println(String.format("=== %-85s ===", str));
		}
	}

	/**
	 * Prints a line which will contain the address and more information
	 * depending on the region'type<br>
	 * Outputs<br> 0x6B3C300-0x6B3C378 [    (SECTION) ramHeader                    ]   120 bytes <br>for example
	 * @param clazz 
	 * 
	 * @param region
	 * @param out 
	 */
	private static void printRegionLine(J9ClassRegion region, PrintStream out) {
		try {
			long start, end;
			if (region.getType() == SlotType.J9_SECTION_START) {
				start = region.getSlotPtr().getAddress();
				end = region.getSlotPtr().getAddress() + region.getLength();
			} else {
				start = region.getSlotPtr().getAddress();
				end = region.getSlotPtr().getAddress() + region.getLength();
			}
			String address;
			address = String.format("%s-%s [ %20s %-28s ] %s", 
					UDATAPointer.cast(start).getHexAddress(),
					UDATAPointer.cast(end).getHexAddress(),
					getRegionValueString(region), region.getName(),
					getRegionDetailString(region));
			out.println(address);
		} catch (CorruptDataException e) {
			out.println(e.getMessage());
			/*
			 * Nothing to be done, if a memory access fail, we do not print the
			 * field It serves the purpose of validateRangeCallback
			 */
		}
	}

	private static Object getRegionDetailString(J9ClassRegion region)
			throws CorruptDataException {
		VoidPointer srp;
		long slotAddress;
		String returnValue = "";
		
		switch (region.getType()) {
		case J9_SECTION_START:
			returnValue = String.format(" %5d bytes", region.getLength());
			break;
		case J9_RAM_UTF8:
			UDATAPointer slotPtr = UDATAPointer.cast(region.getSlotPtr());
			if (slotPtr.at(0).longValue() != 0) {
				returnValue = J9ObjectHelper.stringValue(J9ObjectPointer.cast(slotPtr.at(0)));
			}
			break;
		case J9_UTF8:
		case J9_ROM_UTF8:
			returnValue = J9UTF8Helper.stringValue(J9UTF8Pointer.cast(region.getSlotPtr()));
			break;
		case J9_iTableMethod:
			/*
			 * This case is special for the iTableMethod, because their pointers
			 * are related to the beginning of the class and are not absolute
			 */
			AbstractPointer classOffset = region.getSlotPtr().subOffset(region.offset);
			slotAddress = region.getSlotPtr().at(0).longValue();
			classOffset = classOffset.addOffset(slotAddress);
			returnValue = region.additionalInfo
				+ " "
				+ classOffset.getHexAddress();
			break;
		case J9_SRP:
			srp = SelfRelativePointer.cast(region.getSlotPtr()).get();
			if (srp.notNull()){
				returnValue = " -> " + srp.getHexAddress();
			}
			break;
		case J9_WSRP:
			srp = WideSelfRelativePointer.cast(region.getSlotPtr()).get();
			if (srp.notNull()){
				returnValue = " -> " + srp.getHexAddress();
			}
			break;
		case J9_SRP_TO_STRING:
			srp = SelfRelativePointer.cast(region.getSlotPtr()).get();
			if (srp.notNull()){
				returnValue = " -> " + srp.getHexAddress();
				returnValue += " " + J9UTF8Helper.stringValue(J9UTF8Pointer.cast(srp));
			}
			break;
		case J9_Padding:
			returnValue = region.length + " byte(s)";
			break;
		case J9_IntermediateClassData:
			returnValue = region.length + " byte(s) " + region.additionalInfo;
			break;
		default:
			String detail = "";
			slotAddress = UDATAPointer.cast(region.getSlotPtr()).at(0).longValue();
			long slotAddressOriginal = slotAddress;
			if ((null != region.additionalInfo) && (region.additionalInfo.length() != 0)) {
				if ("classAndFlags".equals(region.name) && (UDATA.MASK != slotAddress) ) {
					slotAddress = slotAddress & ~J9JavaAccessFlags.J9StaticFieldRefFlagBits;
				}
				detail += region.additionalInfo
						+ (String.format(" 0x%08X", slotAddress)) ;
				
				/* For special debug extension, more information is shown */
				if ((0 != slotAddress) && (region.additionalInfo.equals("!j9class"))) {
					try {
						detail += " - " + J9UTF8Helper.stringValue(J9ClassPointer.cast(slotAddress).romClass().className());
						
						if ("classAndFlags".equals(region.name)) {
							String flags = "";
							if (0 != (J9JavaAccessFlags.J9StaticFieldRefBaseType & slotAddressOriginal)) {
								flags += "J9StaticFieldRefBaseType, ";
							}
							
							if (0 != (J9JavaAccessFlags.J9StaticFieldRefDouble & slotAddressOriginal)) {
								flags += "J9StaticFieldRefDouble, ";
							}
							
							if (0 != (J9JavaAccessFlags.J9StaticFieldRefVolatile & slotAddressOriginal)) {
								flags += "J9StaticFieldRefVolatile, ";
							}
							
							/* Check there is any flag or not */
							if (0 < flags.length()) {
								/*remove last two chars = ", " */
								flags = flags.substring(0, flags.length() - 2);
								detail += "\t Flag(s) = " + flags;
							}

						}
					} catch (Exception e) {
						/* This is only for information purpose, it is ok if an exception was thrown */
					}
				}
			}
			returnValue = detail;
		}
		
		return returnValue;
	}

	private static String getRegionValueString(J9ClassRegion region)
			throws CorruptDataException {
		switch (region.getType()) {
		case J9_SECTION_START:
			return "(SECTION)";
		case J9_UTF8:
		case J9_ROM_UTF8:
		case J9_RAM_UTF8:
			return "UTF8";
		case J9_I8:
			return I8Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_U8:
			return U8Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_I16:
			return I16Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_U16:
			return U16Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_I32:
			return I32Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_U32:
			return U32Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_I64:
			return I64Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_U64:
			return U64Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_SRP:
			return I32Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_WSRP:
			return IDATAPointer.cast(region.getSlotPtr()).getHexValue();
		case J9_SRP_TO_STRING:
			return I32Pointer.cast(region.getSlotPtr()).getHexValue();
		case J9_Padding:
		case J9_IntermediateClassData:
			return "";
		default:
			return region.getSlotPtr().hexAt(0);
		}
	}
	private int getUTF8Length(J9UTF8Pointer j9utf8Pointer) throws CorruptDataException {

		UDATA length = new UDATA(j9utf8Pointer.length().longValue() + J9UTF8.SIZEOF - U8.SIZEOF * 2 /*TODO sizeof(utf8->data)*/);
		if (length.anyBitsIn(1)) {
			length = length.add(1);
		}
		return length.intValue();
	}
	
	/* Skip duplicates (e.g. if there were two SRPs to the same thing). */
	private boolean isSameRegion(J9ClassRegion region1, J9ClassRegion region2)
	{
		if (region1 == null || region2 == null) {
			return false;
		}
		boolean sameRegion =
			(region1.offset == region2.offset) &&
			(region1.getLength() == region2.getLength());

		/* Compare section names to not discard nested sections of same size. */
		if (sameRegion && ((SlotType.J9_SECTION_START == region1.getType()) || (SlotType.J9_SECTION_END == region1.getType()))) {
			sameRegion = false;
		}
		return sameRegion;
	}
}
