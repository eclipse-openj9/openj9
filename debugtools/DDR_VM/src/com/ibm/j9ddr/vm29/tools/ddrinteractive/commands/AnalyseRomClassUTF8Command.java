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
import java.text.DecimalFormat;
import java.text.ParseException;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.TreeMap;
import java.util.TreeSet;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ROMClassesIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.RomClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.IClassWalkCallbacks.SlotType;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper.J9ClassRegion;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper.J9ClassRegionNode;

public class AnalyseRomClassUTF8Command extends Command {

	public AnalyseRomClassUTF8Command()
	{
		addCommand("analyseromClassutf8", "[WeightList] [maxDistribution%]", "Analyze ROM Class UTF8 distribution (max defaults to 85)");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			boolean printUTF8WeightList = false;
			int maxDistributionPercent = 85;
			if ((args.length >= 1) && (args[0].equals("UTF8WeightList"))) {
				printUTF8WeightList = true;
			}
			for (int i = 0; i < args.length; i++) {
				if (args[i].endsWith("%")) {
					try {
						// Parses the maxDistribution string such as 85%
						maxDistributionPercent = DecimalFormat.getInstance().parse(args[i]).intValue();
					} catch (ParseException e) {
						out.println("Usage: !analyseromClassutf8 [UTF8WeightList] [maxDistribution%]		maxDistribution defaults to 85%");
					}
				}
			}
			Statistics statistics = new Statistics();
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			ROMClassesIterator classSegmentIterator = new ROMClassesIterator(out, vm.classMemorySegments());
			J9ObjectPointer bootstraploader = vm.systemClassLoader().classLoaderObject();

			while (classSegmentIterator.hasNext()) {
				J9ROMClassPointer classPointer = (J9ROMClassPointer) classSegmentIterator.next();
				
				ClassWalker classWalker = new RomClassWalker(classPointer, context);
				LinearDumper linearDumper = new LinearDumper();
				J9ClassRegionNode allRegionsNode = linearDumper.getAllRegions(classWalker);

				statistics.add(allRegionsNode, classSegmentIterator, bootstraploader);
			}
			
			statistics.getResult(printUTF8WeightList, maxDistributionPercent, out);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}
	
	private class Statistics {
		LinkedHashMap<String, Long[]> utf8global = new LinkedHashMap<String, Long[]>(), 
		utf8classloader = new LinkedHashMap<String, Long[]>(),
		utf8bootstraploader = new LinkedHashMap<String, Long[]>();
		
		void add(J9ClassRegionNode allRegionsNode, ROMClassesIterator classSegmentIterator, J9ObjectPointer bootstraploader) throws CorruptDataException {
			addRecursive(allRegionsNode, classSegmentIterator, bootstraploader, null);
		}
		void addRecursive(J9ClassRegionNode allRegionsNode, ROMClassesIterator classSegmentIterator, J9ObjectPointer bootstraploader, String parent) throws CorruptDataException {
			final J9ClassRegion nodeValue = allRegionsNode.getNodeValue();
			String name = null;
			if (nodeValue != null) {
				name = nodeValue.getName();
				if ((nodeValue.getType() == SlotType.J9_ROM_UTF8) && (parent!= null && parent.equals("UTF8"))) {
					long length = nodeValue.getLength();
					String csectionName = J9UTF8Helper.stringValue(J9UTF8Pointer.cast(nodeValue.getSlotPtr()));
					
					//global utf-8
					Long[] cpsavedSize = utf8global.get(csectionName);
					long sizet = 0;
					long dup = 1;
					if (cpsavedSize == null) {
						sizet = new Long(length);
					} else {
						sizet = new Long(length) + cpsavedSize[0];
						dup += cpsavedSize[1];
					}
					utf8global.put(csectionName, new Long[]{sizet, dup});
					
					//by classloader utf-8
					J9ClassLoaderPointer cloader = classSegmentIterator.getMemorySegmentPointer().classLoader();
					if (!cloader.classLoaderObject().equals(bootstraploader)){
						dup = 1;
						String key = csectionName + "-" + cloader;
						cpsavedSize = utf8classloader.get(key);
						if (cpsavedSize == null) {
							sizet = new Long(length);
						} else {
							sizet = new Long(length) + cpsavedSize[0];
							dup += cpsavedSize[1];
						}
						utf8classloader.put(key, new Long[] {sizet, dup});
					} else {
						dup = 1;
						cpsavedSize = utf8bootstraploader.get(csectionName);
						if (cpsavedSize == null) {
							sizet = new Long(length);
						} else {
							sizet = new Long(length) + cpsavedSize[0];
							dup += cpsavedSize[1];
						}
						utf8bootstraploader.put(csectionName, new Long[] {sizet, dup});
					}
				}
			}
			for (J9ClassRegionNode classRegionNode : allRegionsNode.getChildren()) {
				String childName = classRegionNode.getNodeValue().getName();
				if (nodeValue == null ||
						(name != null && name.equals("UTF8")) ||
						(childName != null && childName.equals("UTF8"))) {
					addRecursive(classRegionNode, classSegmentIterator, bootstraploader, name);
				}
			}
		}
		public void getResult(boolean printUTF8WeightList, int maxDistributionPercent, PrintStream out) {
			if (!utf8global.isEmpty()) {
				out.println();
				TreeMap<Long, Long[]> distrmap = new TreeMap<Long, Long[]>();
				TreeMap<Long, TreeSet<String>> outputmap = new TreeMap<Long, TreeSet<String>>();
				long dsize = 0, dcount = 0, usize = 0, ucount = 0;
				for(Map.Entry<String, Long[]> entry : utf8global.entrySet()) {
					if (entry.getValue()[1] == 1){
						ucount++;
						usize += entry.getValue()[0];
					} else {
						dcount++;
						dsize += entry.getValue()[0];
						long diskey = entry.getValue()[1];
						Long[] nodecountsize = distrmap.get(diskey);
						long nodesize = 0;
						long nodecount= 1;
						if (nodecountsize == null) {
							nodesize = new Long(entry.getValue()[0]);
						} else {
							nodesize = new Long(entry.getValue()[0]) + nodecountsize[1];
							nodecount += nodecountsize[0];
						}
						distrmap.put(diskey, new Long[] {nodecount, nodesize});
						TreeSet<String> cu = outputmap.get(diskey);
						if (cu != null){
							cu.add(entry.getKey());
						} else {
							cu = new TreeSet<String>();
							cu.add(entry.getKey());
						}
						outputmap.put(diskey, cu);
					}
				}
				out.println("<Global Unique UTF-8 (count, total size)>");
				out.println(ucount + ", " + usize/1024 + "K");
				out.println("\n<Global Duplicated UTF-8 (count, total size)>");
				out.println(dcount + ", " + dsize/1024 + "K");	
				out.println();
				out.println("<Distribution of Global Duplicated UTF-8>");
				out.println("(duplicated times, # UTF-8s, total size)");
				long listshowsize = 0, listshowcount = 0;
				for(Map.Entry<Long, Long[]> entry : distrmap.entrySet()) {
					out.println(entry.getKey() + ", " + entry.getValue()[0] + ", " + 
							((entry.getValue()[1]) == 0 ? "0K" : ((entry.getValue()[1]/1024) < 1 ? "<1K" : entry.getValue()[1]/1024 + "K")));
					listshowsize += entry.getValue()[1];
					listshowcount += entry.getValue()[0];
					if (((double)listshowsize/dsize*100) > maxDistributionPercent){
						out.println((entry.getKey() + 1) + "+, " + (dcount - listshowcount) + ", " + (dsize - listshowsize)/1024 + "K");
						break;
					}
				}
				if (printUTF8WeightList) {
					out.println("<Global Duplicated UTF-8s>");
					out.println("      times");
					out.println("id    duplicated  weight string");
					for(Map.Entry<Long, TreeSet<String>> entry : outputmap.entrySet()) {
						Long timesDuplicated = entry.getKey();
						int count = 0;
						for (String string : entry.getValue()) {
							long weight = (timesDuplicated - 1) * string.length();
							out.println(String.format("%-5d %-11d %-5d \"%s\"", count++, timesDuplicated, weight, string));
						}
					}
				}
			}
			if (!utf8classloader.isEmpty()){
				out.println();
				TreeMap<Long, Long[]> distrmap = new TreeMap<Long, Long[]>();
				TreeMap<Long, TreeSet<String>> outputmap = new TreeMap<Long, TreeSet<String>>();
				long dsize = 0, dcount = 0, usize = 0, ucount = 0;
				Iterator<Map.Entry<String, Long[]>> it = utf8classloader.entrySet().iterator();
				while (it.hasNext()) {
					Map.Entry<String,Long[]> entry = it.next();
					long isDup = entry.getValue()[1], allsize = entry.getValue()[0];
					String utf8 = entry.getKey().split("-")[0];
					Long[] bootstraputf8 = utf8bootstraploader.get(utf8);
					if (bootstraputf8 != null){
						bootstraputf8[0] += allsize;
						bootstraputf8[1] += isDup;
						it.remove();
					} 	
				}
				for(Map.Entry<String, Long[]> entry : utf8bootstraploader.entrySet()) {
					if (entry.getValue()[1] == 1) {
						ucount++;
						usize += entry.getValue()[0];
					} else {
						dcount++;
						dsize += entry.getValue()[0];
						long diskey = entry.getValue()[1];
						Long[] nodecountsize = distrmap.get(diskey);
						long nodesize = 0;
						long nodecount= 1;
						if (nodecountsize == null) {
							nodesize = new Long(entry.getValue()[0]);
						} else {
							nodesize = new Long(entry.getValue()[0]) + nodecountsize[1];
							nodecount += nodecountsize[0];
						}
						distrmap.put(diskey, new Long[] {nodecount, nodesize});
						TreeSet<String> cu = outputmap.get(diskey);
						if (cu != null){
							cu.add(entry.getKey());
						} else {
							cu = new TreeSet<String>();
							cu.add(entry.getKey());
						}
						outputmap.put(diskey, cu);
					}
				}
				for(Map.Entry<String, Long[]> entry : utf8classloader.entrySet()) {
					if (entry.getValue()[1] == 1) {
						ucount++;
						usize += entry.getValue()[0];
					} else {
						dcount++;
						dsize += entry.getValue()[0];
						long diskey = entry.getValue()[1];
						Long[] nodecountsize = distrmap.get(diskey);
						long nodesize = 0;
						long nodecount= 1;
						if (nodecountsize == null) {
							nodesize = new Long(entry.getValue()[0]);
						} else {
							nodesize = new Long(entry.getValue()[0]) + nodecountsize[1];
							nodecount += nodecountsize[0];
						}
						distrmap.put(diskey, new Long[] {nodecount, nodesize});
						TreeSet<String> cu = outputmap.get(diskey);
						if (cu != null){
							cu.add(entry.getKey());
						} else {
							cu = new TreeSet<String>();
							cu.add(entry.getKey());
						}
						outputmap.put(diskey, cu);
					}
				}
				out.println("<Classloader Unique UTF-8 (count, total size)>");
				out.println(ucount + ", " + usize/1024 + "K");
				out.println("\n<Classloader Duplicated UTF-8 (count, total size)>");
				out.println(dcount + ", " + dsize/1024 + "K");	
				out.println();
				out.println("<Distribution of Classloader Duplicated UTF-8>");
				out.println("(duplicated times, # UTF-8s, total size)");
				long listshowsize = 0, listshowcount = 0;
				for(Map.Entry<Long, Long[]> entry : distrmap.entrySet()) {
					out.println(entry.getKey() + ", " + entry.getValue()[0] + ", " + 
							((entry.getValue()[1]) == 0 ? "0K" : ((entry.getValue()[1]/1024) < 1 ? "<1K" : entry.getValue()[1]/1024 + "K")));
					listshowsize += entry.getValue()[1];
					listshowcount += entry.getValue()[0];
					if (((double)listshowsize/dsize*100) > maxDistributionPercent){
						out.println((entry.getKey() + 1) + "+, " + (dcount - listshowcount) + ", " + (dsize - listshowsize)/1024 + "K");
						break;
					}
				}
				if (printUTF8WeightList) {
					out.println("<Duplicated UTF-8s by Classloader>");
					out.println("      times");
					out.println("id    duplicated  weight string");
					for(Map.Entry<Long, TreeSet<String>> entry : outputmap.entrySet()) {
						Long timesDuplicated = entry.getKey();
						int count = 0;
						for (String string : entry.getValue()) {
							long weight = (timesDuplicated - 1) * string.length();
							out.println(String.format("%-5d %-11d %-5d \"%s\"", count++, timesDuplicated, weight, string));
						}
					}
				}
			}
		}
	}
}
