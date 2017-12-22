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
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Table;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.LiveSetWalker.ObjectVisitor;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.j9.LiveSetWalker;
import com.ibm.j9ddr.vm29.j9.RootSet.RootSetType;
import com.ibm.j9ddr.vm29.j9.gc.GCClassIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCClassIteratorClassSlots;
import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionManager;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectIterator;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_AllocationContextPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_AllocationContextTarokPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorVLHGCPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionManagerPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

public class ACCommand extends Command {
	
	private GCHeapRegionManager heapRegionManager;

	public ACCommand() {
		addCommand("ac", "<address> [ xrefs | ownedRegions ]", "Dump allocation context details");
		addCommand("acforobject", "<address>", "Find allocation context which owns the specified object");
	}

	public void run(String command, String[] args, Context context, PrintStream out)
			throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
	
			if (command.equalsIgnoreCase("!acforobject")) {
				if (null == heapRegionManager) {
					MM_HeapRegionManagerPointer hrmPointer;
					hrmPointer = MM_GCExtensionsPointer.cast(vm.gcExtensions()).heapRegionManager();
					heapRegionManager = GCHeapRegionManager.fromHeapRegionManager(hrmPointer);
				}
				
				if (args.length < 1) {
					throw new DDRInteractiveCommandException("Invalid number of arguments specified.");
				}
			
				long addr = Long.decode(args[0]);
				J9ObjectPointer objectPointer = J9ObjectPointer.cast(addr);
				
				dumpACForObject(vm, objectPointer, out);
			} else {
				MM_GCExtensionsPointer gcExtensions = MM_GCExtensionsPointer.cast(vm.gcExtensions());
				
				if (args.length < 1) {
					throw new DDRInteractiveCommandException("Invalid number of arguments specified.");
				}
				
				boolean dumpACExternalReferences = false;
				boolean dumpOwnedRegions = false;
				
				for (int i=1; i < args.length; i++) {
					String arg = args[i];

					if (arg.equalsIgnoreCase("xrefs")) {
						dumpACExternalReferences = true;
 					} else if (arg.equalsIgnoreCase("ownedRegions")) {
						dumpOwnedRegions = true;
					} else {
						throw new DDRInteractiveCommandException("Unrecognized acforobject subcommand -->" + arg);
					}
				}

				long addr = Long.decode(args[0]);
				MM_AllocationContextPointer ac = MM_AllocationContextPointer.cast(addr);

				if (dumpACExternalReferences) {
					//TODO: handle other AC types
					if (GCExtensions.isVLHGC()) {
						context.execute("!mm_allocationcontexttarok", new String[]{args[0]}, out);
					}

					dumpLiveReferences(vm, ac, out);
				}
				
				if (dumpOwnedRegions) {
					dumpOwnedRegions(vm, ac, out);
				}
			}
		} catch (DDRInteractiveCommandException e) {
			throw e;
		} catch (Throwable e) {
			e.printStackTrace(out);
			throw new DDRInteractiveCommandException(e);
		}
	}

	private void dumpOwnedRegions(J9JavaVMPointer vm, MM_AllocationContextPointer allocationContext, PrintStream out) throws CorruptDataException 
	{
		if (GCExtensions.isVLHGC()) {
			Table table = new Table("Regions Owned by AC " + allocationContext.getHexAddress());
			table.row("Region", "containsObjects");

			GCHeapRegionIterator regionIterator = GCHeapRegionIterator.from();
			while (regionIterator.hasNext()) {
				GCHeapRegionDescriptor region = regionIterator.next();
				MM_HeapRegionDescriptorVLHGCPointer vlhgcRegion = MM_HeapRegionDescriptorVLHGCPointer.cast(region.getHeapRegionDescriptorPointer());
				MM_AllocationContextTarokPointer currentAllocationContextTarok = vlhgcRegion._allocateData()._owningContext();
				if (currentAllocationContextTarok.eq(allocationContext)) {									
					table.row("!mm_heapregiondescriptorvlhgc " + vlhgcRegion.getHexAddress()
							, Boolean.toString(region.containsObjects()));
				}
			}

			table.render(out);
		}
	}
	
	static class LiveReferenceVisitor implements ObjectVisitor {
		GCHeapRegionManager heapRegionManager;
		MM_AllocationContextTarokPointer allocationContext;
		Table table;
		int numExternalReferences = 0;
		
		public LiveReferenceVisitor(GCHeapRegionManager heapRegionManager, MM_AllocationContextTarokPointer allocationContext, Table table) {
			this.heapRegionManager = heapRegionManager;
			this.allocationContext = allocationContext;
			this.table = table;
		}
		
		public boolean visit(J9ObjectPointer object, VoidPointer address) {
			try {
				GCHeapRegionDescriptor objectRegion = heapRegionManager.regionDescriptorForAddress(object);
				MM_HeapRegionDescriptorVLHGCPointer vlhgcObjectRegion = MM_HeapRegionDescriptorVLHGCPointer.cast(objectRegion.getHeapRegionDescriptorPointer());
				MM_AllocationContextTarokPointer objectAllocationContext = vlhgcObjectRegion._allocateData()._owningContext();
				if (!objectAllocationContext.eq(allocationContext)) {
					/* object is not in allocationContext.
					 * Check if any of its fields point into our allocationContext.
					 */
					GCObjectIterator fieldIterator = GCObjectIterator.fromJ9Object(object, false);
					while (fieldIterator.hasNext()) {
						J9ObjectPointer field = fieldIterator.next();
						if (field.notNull()) {
							checkField(object, field);
						}
					}

					if (J9ObjectHelper.getClassName(object).equals("java/lang/Class")) {
						J9ClassPointer clazz = ConstantPoolHelpers.J9VM_J9CLASS_FROM_HEAPCLASS(object);

						// Iterate the Object slots
						GCClassIterator classIterator = GCClassIterator.fromJ9Class(clazz);
						while(classIterator.hasNext()) {
							J9ObjectPointer slot = classIterator.next();
							if (slot.notNull()) {
								checkField(object, slot);
							}				
						}

						// Iterate the Class slots
						GCClassIteratorClassSlots classSlotIterator = GCClassIteratorClassSlots.fromJ9Class(clazz);
						while (classSlotIterator.hasNext()) {
							J9ClassPointer slot = classSlotIterator.next();
							J9ObjectPointer classObject = ConstantPoolHelpers.J9VM_J9CLASS_TO_HEAPCLASS(slot);
							if (classObject.notNull()) {
								checkField(object, classObject);
							}				
						}
					}
				}
			} catch (CorruptDataException e) {
				return false;
			}
			return true;
		}

		public void finishVisit(J9ObjectPointer object, VoidPointer address) {
			// TODO Auto-generated method stub
			
		}
		
		private void checkField(J9ObjectPointer object, J9ObjectPointer field) throws CorruptDataException {
			GCHeapRegionDescriptor fieldRegion = heapRegionManager.regionDescriptorForAddress(field);
			if (fieldRegion == null) {
				fieldRegion = null;
			}
			MM_HeapRegionDescriptorVLHGCPointer vlhgcFieldRegion = MM_HeapRegionDescriptorVLHGCPointer.cast(fieldRegion.getHeapRegionDescriptorPointer());
			MM_AllocationContextTarokPointer fieldAllocationContext = vlhgcFieldRegion._allocateData()._owningContext();
			if (fieldAllocationContext.eq(allocationContext)) {
				J9ClassPointer objectClass = J9ObjectHelper.clazz(object);
				String objectClassString = J9ClassHelper.getJavaName(objectClass);

				J9ClassPointer targetObjectClass = J9ObjectHelper.clazz(field);
				String targetObjectClassString = J9ClassHelper.getJavaName(targetObjectClass);

				/* this field points at memory owned by our allocation context */
				table.row("!j9object " + object.getHexAddress() + " //" + objectClassString
						, "!j9object " + field.getHexAddress() + " //" + targetObjectClassString);
				
				numExternalReferences += 1;
			}
		}
		
		public int getNumExternalReferencesFound() {
			return numExternalReferences;
		}
	}
	
	private void dumpLiveReferences(J9JavaVMPointer vm, MM_AllocationContextPointer allocationContext, PrintStream out) throws CorruptDataException {
		if (GCExtensions.isVLHGC()) {
			MM_AllocationContextTarokPointer act = MM_AllocationContextTarokPointer.cast(allocationContext);
			MM_HeapRegionManagerPointer hrmPointer = MM_GCExtensionsPointer.cast(vm.gcExtensions()).heapRegionManager();
			GCHeapRegionManager heapRegionManager = GCHeapRegionManager.fromHeapRegionManager(hrmPointer);
			Table table = new Table("Live References into AC " + allocationContext.getHexAddress());
			table.row("Object", "Field");

			out.println("Walking live set in search of external references into ac: " + allocationContext.getHexAddress());
			
			long totalMillis = System.currentTimeMillis();
			LiveReferenceVisitor visitor = new LiveReferenceVisitor(heapRegionManager, act, table);
			LiveSetWalker.walkLiveSet(visitor, RootSetType.ALL);
			totalMillis = System.currentTimeMillis() - totalMillis;
			
			table.render(out);
			
			out.println("\nFinished live reference walk in " + totalMillis + "ms");
			
			out.println("Found " + visitor.getNumExternalReferencesFound() + " external references.");
		}
	}
	
	public void dumpACForObject(J9JavaVMPointer vm, StructurePointer objectPointer, PrintStream out) throws CorruptDataException
	{
		if (GCExtensions.isVLHGC()) {
			if ((null != objectPointer) && !objectPointer.isNull()) {
				/* get the object's region */
				GCHeapRegionDescriptor region = heapRegionManager.regionDescriptorForAddress(objectPointer);
				if (null != region) {
					MM_HeapRegionDescriptorVLHGCPointer vlhgcRegionPointer = MM_HeapRegionDescriptorVLHGCPointer.cast(region.getHeapRegionDescriptorPointer());
					/* get the region's owning allocation context */
					StructurePointer act = vlhgcRegionPointer._allocateData()._owningContext().getAsRuntimeType();
					
					out.println("Object's owning context: " + act.formatShortInteractive());

				}
			}

		}
	}
	
	

}
