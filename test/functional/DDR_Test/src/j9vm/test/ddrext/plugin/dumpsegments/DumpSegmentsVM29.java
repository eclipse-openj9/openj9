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
package j9vm.test.ddrext.plugin.dumpsegments;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.annotations.DebugExtension;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.walkers.MemorySegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

@DebugExtension(VMVersion="29")
public class DumpSegmentsVM29 extends Command {
	private J9JavaVMPointer jvm = J9JavaVMPointer.NULL;
//	private J9JavaVMPointer jvm = null;
	
	{
		CommandDescription cd = addCommand("dumpsegs", "<segname(s)>", "Dumps out the segments for a given name");
		cd.addSubCommand("all", "", "Dumps all memory segments");
		cd.addSubCommand("int", "", "Internal Memory");
		cd.addSubCommand("obj", "", "Object Memory");
		cd.addSubCommand("class", "", "Class Memory");
		cd.addSubCommand("jitc", "", "JIT code cache");
		cd.addSubCommand("Ex", "", "shortcut for !dumpsegs obj,jitc,jitd");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out)
		throws DDRInteractiveCommandException {
			// TODO Auto-generated method stub

			try {
				jvm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			} catch (Exception e) {
				e.printStackTrace();
				throw new DDRInteractiveCommandException("Could not locate the JVM pointer", e);
			}
			if (command.equalsIgnoreCase("!dumpallsegments")) {
				try {
					printAllMemorySegments(out);
					return;
				} catch (CorruptDataException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
			if( args.length == 0) {
				printError("Syntax error!", out);
				return;
			}
			for(int i=0 ; i< args.length; i++) {
				String argument = args[i].toLowerCase();
				int flag = 0;

				if(argument.contains("all"))
					try {
						printAllMemorySegments(out);
						return;
					} catch (CorruptDataException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
					if(argument.contains("obj"))
						try {
							printObjectMemory(out);
							flag = 1;
							out.println();
						} catch (CorruptDataException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
						if(argument.contains("int"))
							try {
								printInternalMemory(out);
								flag = 1;
								out.println();
							} catch (CorruptDataException e) {
								// TODO Auto-generated catch block
								e.printStackTrace();
							}
							if(argument.contains("class"))
								try {
									printClassMemory(out);
									flag = 1;
									out.println();
								} catch (CorruptDataException e) {
									// TODO Auto-generated catch block
									e.printStackTrace();
								}
								if(argument.contains("jitc"))
									try {
										printJitCodeCache(out);
										flag = 1;
										out.println();
									} catch (CorruptDataException e) {
										// TODO Auto-generated catch block
										e.printStackTrace();
									}
									if(argument.contains("jitd"))
										try {
											printJitDataCache(out);
											flag = 1;
											out.println();
										} catch (CorruptDataException e) {
											// TODO Auto-generated catch block
											e.printStackTrace();
										}
										if(flag!=1) {
											printError("Unknown argument "+args[i],out);
											return;
										}
			}
		}

		private void printError(String errString, PrintStream out) {
			// TODO Auto-generated method stub
			out.println("Err! : "+errString);
		}

		private void printObjectMemory(PrintStream out) throws CorruptDataException {
			//		out.println("--Object memory unavailable. Please use !dumpallregions--");
			try {
				objectSegmentIterateAndPrint(jvm.objectMemorySegments().longValue(), out, "Object Memory");
			} catch (DDRInteractiveCommandException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

		private void printInternalMemory(PrintStream out) throws CorruptDataException {
			segmentIterateAndPrint(jvm.memorySegments(), out, "Internal Memory");
		}

		private void printClassMemory(PrintStream out) throws CorruptDataException {
			segmentIterateAndPrint(jvm.classMemorySegments(), out, "Class Memory");
		}

		private void printJitCodeCache(PrintStream out) throws CorruptDataException {
			if (J9BuildFlags.interp_nativeSupport) {
				if (jvm.jitConfig().isNull() == false) {
					segmentIterateAndPrint(jvm.jitConfig().codeCacheList(), out, "JIT Code Cache");
				}
			}
		}

		private void printJitDataCache(PrintStream out) throws CorruptDataException {
			if (J9BuildFlags.interp_nativeSupport) {
				if (jvm.jitConfig().isNull() == false) {
					segmentIterateAndPrint(jvm.jitConfig().dataCacheList(), out, "JIT Data Cache");
				}
			}
		}

		private void printAllMemorySegments(PrintStream out) throws CorruptDataException {
			printInternalMemory(out);
			out.println();
			printObjectMemory(out);
			out.println();
			printClassMemory(out);
			out.println();
			printJitCodeCache(out);
			out.println();
			printJitDataCache(out);
			out.println();
		}

		private void segmentIterateAndPrint(J9MemorySegmentListPointer segmentListPointer, PrintStream out, String memoryDescription) {
			if(segmentListPointer == null) {
				return ;
			}
			boolean reportFlag = false;
			MemorySegmentIterator segmentIterator = new MemorySegmentIterator(segmentListPointer, MemorySegmentIterator.MEMORY_ALL_TYPES, false);
			while (segmentIterator.hasNext()) {
				J9MemorySegmentPointer nextSegment = (J9MemorySegmentPointer) segmentIterator.next();
				try {
					long start = nextSegment.heapBase().longValue();
					long end = nextSegment.heapTop().longValue();
					long segment = nextSegment.getAddress();
					long alloc = nextSegment.heapAlloc().longValue();
					long type = nextSegment.type().longValue();
					long size = nextSegment.size().longValue();
					if(!reportFlag) {
						reportHeader(segment,out,memoryDescription,J9BuildFlags.env_data64);
						reportFlag = true;
					}
					report(segment, start, alloc, end, type, size, out, memoryDescription, J9BuildFlags.env_data64);
				} catch (CorruptDataException e) {
					e.printStackTrace(out);
				}
			}

			return ;
		}

		private void objectSegmentIterateAndPrint(long addr, PrintStream out, String memoryDescription) 
		throws DDRInteractiveCommandException
		{
			boolean reportFlag = false;
			try {
				GCHeapRegionIterator gcHeapRegionIterator = GCHeapRegionIterator.from();
				while(gcHeapRegionIterator.hasNext()) {
					GCHeapRegionDescriptor heapRegionDescriptorPointer = gcHeapRegionIterator.next();

					long start = heapRegionDescriptorPointer.getLowAddress().getAddress();
					long end = heapRegionDescriptorPointer.getHighAddress().getAddress();
					long region = heapRegionDescriptorPointer.getHeapRegionDescriptorPointer().getAddress();
					long subspace = heapRegionDescriptorPointer.getHeapRegionDescriptorPointer()._memorySubSpace().getAddress();
					long type = heapRegionDescriptorPointer.getRegionType();
					long size = heapRegionDescriptorPointer.getSize().longValue();
					if(!reportFlag) {
						reportObjectHeader(out, J9BuildFlags.env_data64);
						reportFlag = true;
					}
					reportObjectRegion(region, start, end, subspace, type, size, addr, out, J9BuildFlags.env_data64);

				}
			} catch (CorruptDataException e) {
				throw new DDRInteractiveCommandException(e);
			}
			return ;
		}

		private void reportObjectRegion(long region, long start, long end, long subspace, long type, long size, long addr, PrintStream out, boolean is64Bit) 
		{
			if (is64Bit == true) {
				out.append(String.format(" 0x%016X 0x%016X 0x%016X 0x%016X 0x%016X 0x%016X\n",  region, start, end, subspace, type, size));
			} else {
				out.append(String.format(" 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",  region, start, end, subspace, type, size));
			}
		}
		
		private void reportObjectHeader(PrintStream out, boolean is64Bit) 
		{
			out.append(String.format("%s \n", "Object Memory"));
			if (is64Bit == true) {
				
				out.append("+------------------+------------------+------------------+------------------+------------------+------------------+\n");
				out.append("|     region       |      start       |       end        |     subspace     |      type        |        size      |\n");
				out.append("+------------------+------------------+------------------+------------------+------------------+------------------+\n");
				
			} else {
				
				out.append("+----------+----------+----------+----------+----------+----------+\n");
				out.append("|  region  |  start   |   end    | subspace |   type   |   size   |\n");
				out.append("+----------+----------+----------+----------+----------+----------+\n");
				
			}
		}
		
		private void reportHeader(long segment, PrintStream out, String memoryDescription, boolean is64Bit) 
		{
			if (is64Bit == true) {
				out.append(String.format("%s : - !j9memorysegment 0x%016X\n", memoryDescription, segment));
				out.append("+------------------+------------------+------------------+------------------+------------------+------------------+\n");
				out.append("|     segment      |      start       |      alloc       |       end        |      type        |        size      |\n");
				out.append("+------------------+------------------+------------------+------------------+------------------+------------------+\n");
			} else {
				out.append(String.format("%s : - !j9memorysegment 0x%08X\n", memoryDescription, segment));
				out.append("+----------+----------+----------+----------+----------+----------+\n");
				out.append("| segment  |  start   |  alloc   |   end    |   type   |   size   |\n");
				out.append("+----------+----------+----------+----------+----------+----------+\n");
			}
		}


		private void report(long segment, long start, long alloc, long end, long type, long size, PrintStream out, String memoryDescription, boolean is64Bit) 
		{
			if (is64Bit == true) {
				out.append(String.format(" 0x%016X 0x%016X 0x%016X 0x%016X 0x%016X 0x%016X\n",  segment, start, alloc,end,type, size));
			} else {
				out.append(String.format(" 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",  segment, start, alloc,end,type, size));
			}
		}

}
