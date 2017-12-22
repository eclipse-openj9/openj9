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
import com.ibm.j9ddr.vm29.j9.LiveSetWalker;
import com.ibm.j9ddr.vm29.j9.LiveSetWalker.ObjectVisitor;
import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionManager;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectHeapIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectIterator;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_AllocationContextTarokPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorVLHGCPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionManagerPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

public class ObjectRefsCommand extends Command {

	public ObjectRefsCommand() {
		addCommand("objectrefs", "<address> [ heapWalk ] [ rootWalk ]", "Find and list all references to specified object");
	}

	public void run(String command, String[] args, Context context, PrintStream out)
			throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());			
			if (args.length < 1) {
				throw new DDRInteractiveCommandException("This debug extension takes an address argument \" !objectrefs <address> [ heapWalk ] [ rootWalk ]\"");
			}
				
			long addr = Long.decode(args[0]);
			J9ObjectPointer targetObject = J9ObjectPointer.cast(addr);
			
			boolean dumpHeap = false;
			boolean dumpRoots = false;
			
			if (1 == args.length) {
				dumpHeap = true;
				dumpRoots = true;
			} else {
				for (int i = 1; i < args.length ; i++) {
					if ("heapWalk".equals(args[i])) {
						dumpHeap = true;
					} else if ("rootWalk".equals(args[i])) {
						dumpRoots = true;
					}
				}
			}
			
			if (dumpHeap) {
				try {
					dumpHeapReferences(vm, targetObject, out);
				} catch(CorruptDataException cde) { 
					cde.printStackTrace();
				}
			}
			
			if (dumpRoots) {
				try {
					dumpLiveReferences(vm, targetObject, out);
				} catch(CorruptDataException cde) { 
					cde.printStackTrace();
				}
			}
		} catch (DDRInteractiveCommandException e) {
			throw e;
		} catch (Throwable e) {
			e.printStackTrace(out);
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	/**
	 * Write the on heap references stanza to the output stream.
	 * @param vm
	 * @param targetObject
	 * @param out
	 * @throws CorruptDataException
	 */
	private void dumpHeapReferences(J9JavaVMPointer vm, J9ObjectPointer targetObject, PrintStream out) throws CorruptDataException
	{
		if (GCExtensions.isVLHGC()) {
			Table table = new Table("On Heap References");


			table.row("object (!j9object)", "field (!j9object)"
					, "!mm_heapregiondescriptorvlhgc" ,"AC (type)");

			/* iterate over all heap regions */
			GCHeapRegionIterator regionIterator = GCHeapRegionIterator.from();
			while (regionIterator.hasNext()) {
				GCHeapRegionDescriptor region = regionIterator.next();
				if (region.containsObjects()) {
					MM_HeapRegionDescriptorVLHGCPointer vlhgcRegion = MM_HeapRegionDescriptorVLHGCPointer.cast(region.getHeapRegionDescriptorPointer());
					MM_AllocationContextTarokPointer currentAllocationContextTarok = vlhgcRegion._allocateData()._owningContext();

					/* iterate over all objects in region */
					GCObjectHeapIterator heapObjectIterator = region.objectIterator(true, false);
					while (heapObjectIterator.hasNext()) {
						J9ObjectPointer currentObject = heapObjectIterator.next();

						/* Iterate over the object's fields and list any that point at @ref targetObject */
						GCObjectIterator fieldIterator = GCObjectIterator.fromJ9Object(currentObject, false);
						while (fieldIterator.hasNext()) {
							J9ObjectPointer currentTargetObject = fieldIterator.next();
							if (currentTargetObject.eq(targetObject)) {
								/* found a reference to our targetObject, add it to the table */
								J9ClassPointer objectClass = J9ObjectHelper.clazz(currentObject);
								String objectClassString = J9ClassHelper.getJavaName(objectClass);

								table.row(currentObject.getHexAddress() + " //" + objectClassString
										, currentTargetObject.getHexAddress()
										, vlhgcRegion.getHexAddress()
										, currentAllocationContextTarok.getHexAddress() + " (" + currentAllocationContextTarok._allocationContextType() + ")");	
							}
						}
					}					
				}
			}

			table.render(out);
		}
	}
	
	class LiveReferenceVisitor implements ObjectVisitor {
		GCHeapRegionManager heapRegionManager;
		J9ObjectPointer mainObject;
		Table table;
		
		public LiveReferenceVisitor(GCHeapRegionManager heapRegionManager, J9ObjectPointer object, Table table) {
			this.heapRegionManager = heapRegionManager;
			this.mainObject = object;
			this.table = table;
		}
		
		public boolean visit(J9ObjectPointer object, VoidPointer address) {
			try {
				GCObjectIterator fieldIterator = GCObjectIterator.fromJ9Object(object, false);
				while (fieldIterator.hasNext()) {
					J9ObjectPointer fieldObject = fieldIterator.next();
					if (mainObject.eq(fieldObject)) {
						J9ClassPointer objectClass = J9ObjectHelper.clazz(object);
						String objectClassString = J9ClassHelper.getJavaName(objectClass);

						/* this object points at the object we care about */
						table.row("!j9object " + object.getHexAddress() + " //" + objectClassString);
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
		
	}
	
	private void dumpLiveReferences(J9JavaVMPointer vm, J9ObjectPointer targetObject, PrintStream out) throws CorruptDataException {
		MM_HeapRegionManagerPointer hrmPointer = MM_GCExtensionsPointer.cast(vm.gcExtensions()).heapRegionManager();
		GCHeapRegionManager heapRegionManager = GCHeapRegionManager.fromHeapRegionManager(hrmPointer);
		Table table = new Table("All Live Objects That Refer To !j9object " + targetObject.getHexAddress());
		table.row("Object");
		
		LiveSetWalker.walkLiveSet(new LiveReferenceVisitor(heapRegionManager, targetObject, table));
		
		table.render(out);

	}
}
