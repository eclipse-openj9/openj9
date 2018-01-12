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
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ROMClassesIterator;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SharedCacheHeaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SharedClassConfigPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ClassSummaryHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.RomClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper.J9ClassRegion;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper.J9ClassRegionNode;
import com.ibm.j9ddr.vm29.types.UDATA;

public class RomClassSummaryCommand extends Command
{
	private final String[] preferredOrder = 
	{
			"romHeader",
			"constantPool",
			"fields",
			"interfacesSRPs",
			"innerClassesSRPs",
			"cpNamesAndSignaturesSRPs",
			"methods",
			"cpShapeDescription",
			"classAnnotations",
			"enclosingObject",
			"optionalInfo",
			"UTF8"
	};

	public RomClassSummaryCommand()
	{
		addCommand("romclasssummary", "[local|shared] [nas]", "Display romclass summary");
	}
	
	public void run(String command, String[] args, Context context,	PrintStream out) throws DDRInteractiveCommandException {
		try {
			boolean requiredLocal = false;
			boolean requiredShared = false;
			boolean required16bitnas = false;
			J9SharedClassConfigPointer sc = J9SharedClassConfigPointer.NULL;
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			long startAddress = 0, endAddress = 0;
			
			if ((args != null) && (args.length >= 1)) {
				if (args[0].equals("shared")) {
					requiredShared = true;
				} else if (args[0].equals("local")) {
					requiredLocal = true;
				} else if (args[0].equals("16bitnas")) {
					required16bitnas = true;
				} else {
					out.println("USAGE: !romclasssummary [local|shared] [16bitnas]");
					return;
				}
				if ((args.length >= 2) && (args[1].equals("16bitnas"))) {
					required16bitnas = true;
				}
				
				if (requiredShared || requiredLocal) {
					sc = vm.sharedClassConfig();
					if (requiredShared && sc.isNull()) {
						out.println("The request for '" + args[0] + " classes' failed, because " +
								"shared classes were not enabled on that dump file.");
						return;
					}
					if (sc.notNull()) {
						startAddress = UDATA.cast(sc.cacheDescriptorList().romclassStartAddress()).longValue();
						J9SharedCacheHeaderPointer header = sc.cacheDescriptorList().cacheStartAddress();
						endAddress = UDATA.cast(header).add(header.segmentSRP()).longValue();
					}
				}
			}
			
			
			ClassSummaryHelper classSummaryHelper = new ClassSummaryHelper(preferredOrder);
			Statistics statistics = new Statistics();
			ROMClassesIterator classSegmentIterator = new ROMClassesIterator(out, vm.classMemorySegments());
			while (classSegmentIterator.hasNext()) {
				J9ROMClassPointer classPointer = (J9ROMClassPointer) classSegmentIterator.next();

				if (requiredShared || requiredLocal) {
					boolean isShared;
					if (sc.notNull()) {
						long classAddress = classPointer.getAddress();
						isShared = classAddress >= startAddress && classAddress < endAddress;
					} else {
						isShared = false;
					}
					
					if (requiredShared && !isShared) continue;
					if (requiredLocal && isShared) continue;
				}
				
				ClassWalker classWalker = new RomClassWalker(classPointer, context);
				LinearDumper linearDumper = new LinearDumper();
				J9ClassRegionNode allRegionsNode = linearDumper.getAllRegions(classWalker);

				classSummaryHelper.addRegionsForClass(allRegionsNode);
				if (required16bitnas) {
					statistics.add(allRegionsNode);
				}
			}
			classSummaryHelper.printStatistics(out);
			if (statistics.nameAndSignatureSRP16bitSize != -1){
				out.println();
				out.println("<Total 16bit nameAndSignatureSRPs Size>");
				out.println(statistics.nameAndSignatureSRP16bitSize);
			}
			if (statistics.nameAndSignatureSRPCount != -1 && statistics.cpFieldNASCount != -1){
				out.println();
				out.println("<Shared nameAndSignatureSRPs>");
				// nameAndSignatureSRPCount is divided by 2, because there is a name and a signature per NAS field 
				out.println(statistics.cpFieldNASCount - statistics.nameAndSignatureSRPCount/2);
			}

		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}	
	}
	
	private class Statistics {
		public long nameAndSignatureSRP16bitSize = -1, nameAndSignatureSRPCount = -1, cpFieldNASCount = -1;
		void add(J9ClassRegionNode allRegionsNode) throws CorruptDataException {
			addRecursive(allRegionsNode, null);
		}
		void addRecursive(J9ClassRegionNode allRegionsNode, String parent) throws CorruptDataException {
			final J9ClassRegion nodeValue = allRegionsNode.getNodeValue();
			String name = null;
			if (nodeValue != null) {
				name = nodeValue.getName();
				if (parent != null && parent.equals("cpNamesAndSignaturesSRPs")) {
					long srpValue = I32Pointer.cast(nodeValue.getSlotPtr()).at(0).longValue();

					// Checks if the value is 16bits 
					if ((short)srpValue == (long)srpValue) {
						nameAndSignatureSRPCount++;
						nameAndSignatureSRP16bitSize += nodeValue.getLength();
					}
				}
				if (name.equals("cpFieldNAS")) {
					cpFieldNASCount++;
				}
			}
			for (J9ClassRegionNode classRegionNode : allRegionsNode.getChildren()) {
				if (name == null || 
						parent == null || 
						name.trim().equals("constantPool") || 
						parent.trim().equals("constantPool") || 
						name.trim().equals("cpNamesAndSignaturesSRPs")) {
					addRecursive(classRegionNode, name);
				}
			}
		}
	}
}

