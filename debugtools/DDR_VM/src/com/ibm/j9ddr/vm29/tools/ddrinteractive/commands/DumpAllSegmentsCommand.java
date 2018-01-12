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
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

public class DumpAllSegmentsCommand extends Command 
{
	private static final String nl = System.getProperty("line.separator");

	public DumpAllSegmentsCommand()
	{
		addCommand("dumpallsegments", "", "dump all segments in the VM");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			out.append(String.format("memorySegments - !j9memorysegmentlist 0x%s\n", Long.toHexString(vm.memorySegments().getAddress())));
			SegmentsUtil.dbgDumpSegmentList(out, vm.memorySegments());
			out.append(String.format("classMemorySegments - !j9memorysegmentlist 0x%s\n", Long.toHexString(vm.classMemorySegments().getAddress())));
			SegmentsUtil.dbgDumpSegmentList(out, vm.classMemorySegments());

			if (J9BuildFlags.interp_nativeSupport) {
				/* readJavaVM also reads converts the JITConfig pointer. */
				if (!vm.jitConfig().isNull()) {
					out.append(String.format("jit code segments - !j9memorysegmentlist 0x%s\n", Long.toHexString(vm.jitConfig().codeCacheList().getAddress())));
					SegmentsUtil.dbgDumpJITCodeSegmentList(out, vm.jitConfig().codeCacheList());

					out.append(String.format("jit data segments - !j9memorysegmentlist 0x%s\n", Long.toHexString(vm.jitConfig().dataCacheList().getAddress())));
					SegmentsUtil.dbgDumpSegmentList(out, vm.jitConfig().dataCacheList());
				} else {
					out.append("JIT not enabled\n");
				}
			}
			out.append(nl);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
