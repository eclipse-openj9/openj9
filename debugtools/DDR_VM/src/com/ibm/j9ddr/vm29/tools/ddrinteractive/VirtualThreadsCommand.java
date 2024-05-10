/*
 * Copyright IBM Corp. and others 2022
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.gc.GCBase;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_ContinuationObjectListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * vthreads command lists all the virtual threads.
 *
 * Example:
 *     !vthreads
 * Example output:
 *     !continuationstack 0x00007fe78c0f9600 !j9vmcontinuation 0x00007fe78c0f9600 !j9object 0x0000000706401588 (Continuation) !j9object 0x0000000706400FB0 (VThread) - name1
 *     !continuationstack 0x00007fe78c23aa80 !j9vmcontinuation 0x00007fe78c23aa80 !j9object 0x0000000706424F90 (Continuation) !j9object 0x0000000706424EF0 (VThread) - name2
 *     !continuationstack 0x00007fe78c244ac0 !j9vmcontinuation 0x00007fe78c244ac0 !j9object 0x00000007064250D8 (Continuation) !j9object 0x0000000706425038 (VThread) - name3
 *     ...
 */
public class VirtualThreadsCommand extends Command {
	private static J9ObjectFieldOffset vthreadOffset;
	private static J9ObjectFieldOffset vmRefOffset;
	private static J9ObjectFieldOffset nameOffset;

	private static J9ObjectPointer getVirtualThread(J9ObjectPointer continuation) throws CorruptDataException {
		if (vthreadOffset == null) {
			vthreadOffset = J9ObjectHelper.getFieldOffset(continuation, "vthread", "Ljava/lang/Thread;");
		}
		return J9ObjectHelper.getObjectField(continuation, vthreadOffset);
	}

	private static long getVmRef(J9ObjectPointer continuation) throws CorruptDataException {
		if (vmRefOffset == null) {
			vmRefOffset = J9ObjectHelper.getFieldOffset(continuation, "vmRef", "J");
		}
		return J9ObjectHelper.getLongField(continuation, vmRefOffset);
	}

	private static String getName(J9ObjectPointer vthread) throws CorruptDataException {
		String name = null;
		if (vthread.notNull()) {
			if (nameOffset == null) {
				nameOffset = J9ObjectHelper.getFieldOffset(vthread, "name", "Ljava/lang/String;");
			}
			name = J9ObjectHelper.getStringField(vthread, nameOffset);
		}

		return (name != null) ? name : "<N/A>";
	}

	public VirtualThreadsCommand() {
		addCommand("vthreads", "", "Lists virtual threads");
	}

	@Override
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (JavaVersionHelper.ensureMinimumJavaVersion(19, vm, out)) {
				displayVirtualThreads(vm, out);
			}
		} catch (CorruptDataException | NoSuchFieldException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	/**
	 * Prints all the live virtual threads.
	 *
	 * @param vm the J9JavaVMPointer of the virtual machine
	 * @param out the PrintStream to write output to
	 */
	private static void displayVirtualThreads(J9JavaVMPointer vm, PrintStream out) throws CorruptDataException, NoSuchFieldException {
		String addressFormat = "0x%0" + (UDATA.SIZEOF * 2) + "x";
		String outputFormat = "!continuationstack " + addressFormat
				+ " !j9vmcontinuation " + addressFormat
				+ " !j9object %s (Continuation) !j9object %s (VThread) - %s%n";
		MM_GCExtensionsPointer extensions = GCBase.getExtensions();
		UDATA linkOffset = extensions.accessBarrier()._continuationLinkOffset();
		MM_ContinuationObjectListPointer continuationObjectList = extensions.continuationObjectLists();

		while (continuationObjectList.notNull()) {
			J9ObjectPointer continuation = continuationObjectList._head();
			while (continuation.notNull()) {
				long vmRef = getVmRef(continuation);
				J9ObjectPointer vthread = getVirtualThread(continuation);
				String name = getName(vthread);

				out.format(
						outputFormat,
						vmRef,
						vmRef,
						continuation.getHexAddress(),
						vthread.getHexAddress(),
						name);
				continuation = ObjectReferencePointer.cast(continuation.addOffset(linkOffset)).at(0);
			}
			continuationObjectList = continuationObjectList._nextList();
		}
	}
}
