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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_DO_ATTACH_THREAD;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_DO_COMPACT_HEAP;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_DO_MULTIPLE_HEAPS;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_DO_PREEMPT_THREADS;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_ABORT_SIGNAL;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_CLASS_LOAD;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_CLASS_UNLOAD;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_CORRUPT_CACHE;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_EXCEPTION_CATCH;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_EXCEPTION_DESCRIBE;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_EXCEPTION_SYSTHROW;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_EXCEPTION_THROW;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_GLOBAL_GC;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_GP_FAULT;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_OBJECT_ALLOCATION;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_THREAD_BLOCKED;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_THREAD_END;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_THREAD_START;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_TRACE_ASSERT;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_USER_SIGNAL;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_VM_SHUTDOWN;
import static com.ibm.j9ddr.vm29.structure.J9RASdumpAgent.J9RAS_DUMP_ON_VM_STARTUP;

import java.io.PrintStream;
import java.util.HashMap;
import java.util.Map;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RASdumpAgentPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RASdumpFunctionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RASdumpQueuePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.UDATA;

public class ShowDumpAgentsCommand extends Command
{
	
	private static final UDATA DUMP_FACADE_KEY = UDATA.roundToSizeofU32(UDATA.cast(VoidPointer.cast(0xFacadeDAl)));

	private Map<Long, String> rasDumpEventsToNames= new HashMap<Long, String>();
	
	private long rasDumpEvents[] = { J9RAS_DUMP_ON_GP_FAULT,
			J9RAS_DUMP_ON_USER_SIGNAL, J9RAS_DUMP_ON_ABORT_SIGNAL,
			J9RAS_DUMP_ON_VM_STARTUP, J9RAS_DUMP_ON_VM_SHUTDOWN,
			J9RAS_DUMP_ON_CLASS_LOAD, J9RAS_DUMP_ON_CLASS_UNLOAD,
			J9RAS_DUMP_ON_EXCEPTION_THROW, J9RAS_DUMP_ON_EXCEPTION_CATCH,
			J9RAS_DUMP_ON_THREAD_START, J9RAS_DUMP_ON_THREAD_BLOCKED,
			J9RAS_DUMP_ON_THREAD_END, J9RAS_DUMP_ON_GLOBAL_GC,
			J9RAS_DUMP_ON_EXCEPTION_DESCRIBE,
			J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER,
			J9RAS_DUMP_ON_EXCEPTION_SYSTHROW, J9RAS_DUMP_ON_TRACE_ASSERT,
			J9RAS_DUMP_ON_OBJECT_ALLOCATION, J9RAS_DUMP_ON_CORRUPT_CACHE };
	
	private int J9RAS_DUMP_KNOWN_EVENTS = rasDumpEvents.length;
	
	private Map<Long, String> rasDumpRequestsToNames= new HashMap<Long, String>();
	
	private long rasDumpRequests[] = { J9RAS_DUMP_DO_MULTIPLE_HEAPS,
			J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS, J9RAS_DUMP_DO_COMPACT_HEAP,
			J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK,
			J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS, J9RAS_DUMP_DO_ATTACH_THREAD,
			J9RAS_DUMP_DO_PREEMPT_THREADS };
	
	private int J9RAS_DUMP_KNOWN_REQUESTS= rasDumpRequests.length;
	
	/**
	 * This command prints out the dump agents.
	 * It's run method is basically a C to Java port of the showDumpAgents function found in 
	 * rasdump/dmpsup.c:showDumpAgents.
	 * Other code has been added just to support mapping constants to strings.
	 * 
	 * It is the equivalent of running -Xdump:what however it will also include settings configured
	 * after startup via JVMTI that -Xdump:what would miss.
	 */
	public ShowDumpAgentsCommand()
	{
		addCommand("showdumpagents", "", "print the dump agent settings in force when this dump was taken, like -Xdump:what");
		
		/* Known dump events */
//		{ "gpf",         "ON_GP_FAULT",             J9RAS_DUMP_ON_GP_FAULT },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_GP_FAULT, "gpf");
//		{ "user",        "ON_USER_SIGNAL",          J9RAS_DUMP_ON_USER_SIGNAL },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_USER_SIGNAL,"user");
//		{ "abort",       "ON_ABORT_SIGNAL",         J9RAS_DUMP_ON_ABORT_SIGNAL },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_ABORT_SIGNAL,"abort");
//		{ "vmstart",     "ON_VM_STARTUP",           J9RAS_DUMP_ON_VM_STARTUP },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_VM_STARTUP, "vmstart");
//		{ "vmstop",      "ON_VM_SHUTDOWN",          J9RAS_DUMP_ON_VM_SHUTDOWN },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_VM_SHUTDOWN, "vmstop");
//		{ "load",        "ON_CLASS_LOAD",           J9RAS_DUMP_ON_CLASS_LOAD },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_CLASS_LOAD, "load");
//		{ "unload",      "ON_CLASS_UNLOAD",         J9RAS_DUMP_ON_CLASS_UNLOAD },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_CLASS_UNLOAD, "unload");
//		{ "throw",       "ON_EXCEPTION_THROW",      J9RAS_DUMP_ON_EXCEPTION_THROW },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_EXCEPTION_THROW, "throw");
//		{ "catch",       "ON_EXCEPTION_CATCH",      J9RAS_DUMP_ON_EXCEPTION_CATCH },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_EXCEPTION_CATCH, "catch");
//		{ "thrstart",    "ON_THREAD_START",         J9RAS_DUMP_ON_TH/READ_START },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_THREAD_START, "thrstart");
//		{ "blocked",     "ON_THREAD_BLOCKED",       J9RAS_DUMP_ON_THREAD_BLOCKED },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_THREAD_BLOCKED,  "blocked");
//		{ "thrstop",     "ON_THREAD_END",           J9RAS_DUMP_ON_THREAD_END },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_THREAD_END,"thrstop");
//		{ "fullgc",      "ON_GLOBAL_GC",            J9RAS_DUMP_ON_GLOBAL_GC },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_GLOBAL_GC, "fullgc");
//		{ "uncaught",    "ON_EXCEPTION_DESCRIBE",   J9RAS_DUMP_ON_EXCEPTION_DESCRIBE },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_EXCEPTION_DESCRIBE, "uncaught");
//		{ "slow",        "ON_SLOW_EXCLUSIVE_ENTER", J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER, "slow");
//		{ "systhrow",    "ON_EXCEPTION_SYSTHROW",   J9RAS_DUMP_ON_EXCEPTION_SYSTHROW },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_EXCEPTION_SYSTHROW, "systhrow");
//		{ "traceassert", "ON_TRACE_ASSERT",         J9RAS_DUMP_ON_TRACE_ASSERT },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_TRACE_ASSERT, "traceassert");
//		{ "allocation",  "ON_OBJECT_ALLOCATION",    J9RAS_DUMP_ON_OBJECT_ALLOCATION },
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_OBJECT_ALLOCATION,  "allocation");
//		{ "corruptcache",  "ON_CORRUPT_CACHE",   	J9RAS_DUMP_ON_CORRUPT_CACHE }
		rasDumpEventsToNames.put(J9RAS_DUMP_ON_CORRUPT_CACHE, "corruptcache");

		/* Known VM requests */
//		{ "multiple",  "DO_MULTIPLE_HEAPS",        J9RAS_DUMP_DO_MULTIPLE_HEAPS },
		rasDumpRequestsToNames.put(J9RAS_DUMP_DO_MULTIPLE_HEAPS, "multiple");
//		{ "exclusive", "DO_EXCLUSIVE_VM_ACCESS",     J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS },
		rasDumpRequestsToNames.put(J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS, "exclusive");
//		{ "compact",   "DO_COMPACT_HEAP",            J9RAS_DUMP_DO_COMPACT_HEAP },
		rasDumpRequestsToNames.put(J9RAS_DUMP_DO_COMPACT_HEAP, "compact");
//		{ "prepwalk",  "DO_PREPARE_HEAP_FOR_WALK",   J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK },
		rasDumpRequestsToNames.put(J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK, "prepwalk");
//		{ "serial",    "DO_SUSPEND_OTHER_DUMPS",     J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS },
		rasDumpRequestsToNames.put(J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS, "serial");
//		{ "attach",    "DO_ATTACH_THREAD",           J9RAS_DUMP_DO_ATTACH_THREAD },
		rasDumpRequestsToNames.put(J9RAS_DUMP_DO_ATTACH_THREAD, "attach");
//		{ "preempt",   "DO_PREEMPT_THREADS",         J9RAS_DUMP_DO_PREEMPT_THREADS }
		rasDumpRequestsToNames.put(J9RAS_DUMP_DO_PREEMPT_THREADS, "preempt");
	}
	
	public void run(String command, String[] args, Context context,	PrintStream out) throws DDRInteractiveCommandException 
	{
		
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9RASdumpAgentPointer agent = J9RASdumpAgentPointer.NULL;

			out.println("Registered dump agents\n----------------------");

			while ( ((agent = seekDumpAgent(vm, agent, null)) != null) && !agent.isNull())
			{
				printDumpAgent(vm, agent, context, out);
				out.println("----------------------");
			}

			out.println();

		} catch (CorruptDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
			
	}
	
	private J9RASdumpAgentPointer seekDumpAgent(J9JavaVMPointer vm, J9RASdumpAgentPointer agentPtr, J9RASdumpFunctionsPointer dumpFn) throws CorruptDataException
	{

		J9RASdumpFunctionsPointer func = vm.j9rasDumpFunctions();
		J9RASdumpQueuePointer queue = J9RASdumpQueuePointer.cast(func);
		// Windows 64 bit returns a key of 0xFFFFFFFFFACADEDC
		// Other 64 bit platforms return 0x00000000FACADEDC.
		// Mask off the top bits since the key is 32 bits anyway.
		UDATA reserved = UDATA.roundToSizeofU32(UDATA.cast(queue.facade().reserved()));
		reserved = reserved.bitAnd(0x00000000FFFFFFFFl);
		if( !DUMP_FACADE_KEY.equals(reserved) ) {
			// Dump functions have not been configured. (-Xdump:none was probably specified.)#
			return null;
		}
		/*
		 * Sanity check
		 */
		if ( !queue.isNull() ) {

			J9RASdumpAgentPointer node = agentPtr;

			/* Start from next agent, else start from the beginning */
			node = (node != null && !node.isNull()) ? node.nextPtr() : queue.agents();

			/* Agents with same dumpFn will be found in priority order */
			while ( node != null && !node.isNull() && dumpFn != null && !dumpFn.isNull() && node.dumpFn().getAddress() != dumpFn.getAddress() ) {
				node = node.nextPtr();
			}

			/* Update the current position */
			agentPtr = node;

			return node;
		}

		/* Blank current position */
		agentPtr = J9RASdumpAgentPointer.NULL;

		return agentPtr;
	}
	
	private void printDumpAgent(J9JavaVMPointer vm, J9RASdumpAgentPointer agent, Context context, PrintStream out) throws CorruptDataException
	{
		out.println("-Xdump:");
		
		String dumpFn = "<unknown>";
		String dumpFnAddress = "<unknown>";
		try {
			dumpFnAddress = agent.dumpFn().getHexAddress();
			dumpFn = context.process.getProcedureNameForAddress(agent.dumpFn().getAddress());
		} catch (DataUnavailableException e) {
			// Just leave the string null.
		}

		out.printf("dumpFn=%s (%s)\n", dumpFnAddress, dumpFn );

		out.print("    events=");
		printDumpEvents(vm, agent.eventMask(), out);
		out.println(",");

		if (!agent.detailFilter().isNull()) {
			out.printf("    filter=%s,\n",	agent.detailFilter().getCStringAtOffset(0));
		}
		
		if (!agent.subFilter().isNull()) {
			out.printf("    msg_filter=%s,\n",	agent.subFilter().getCStringAtOffset(0));
		}

		out.printf(
			"    label=%s,\n" +
			"    range=%d..%d,\n" +
			"    priority=%d,\n",
			!agent.labelTemplate().isNull()? agent.labelTemplate().getCStringAtOffset(0): "-",
			agent.startOnCount().intValue(), agent.stopOnCount().intValue(),
			agent.priority().intValue()
		);

		out.print("    request=");
		printDumpRequests(vm, agent.requestMask(), out);

		if (!agent.dumpOptions().isNull()) {
			out.println(",");
			out.printf("    opts=%s",
				agent.dumpOptions().getCStringAtOffset(0));
		}
		out.println();

	}
	
	private void printDumpEvents(J9JavaVMPointer vm, UDATA bits, PrintStream out)
	{
		String separator = "";
		
		/* Events */
		for (int i = 0; i < J9RAS_DUMP_KNOWN_EVENTS; i++) {
			if ((bits.longValue() & rasDumpEvents[i]) != 0) {
				out.printf("%s%s", separator, rasDumpEventsToNames.get(rasDumpEvents[i]));
				separator = "+";
			}
		}
	}
	
	private void printDumpRequests(J9JavaVMPointer vm, UDATA bits, PrintStream out)
	{
		String separator = "";

		for (int i = 0; i < J9RAS_DUMP_KNOWN_REQUESTS; i++)
		{
			if ( (bits.longValue() & rasDumpRequests[i]) != 0)
			{
				/* Switch between multi-line and single-line styles */
				out.printf("%s%s", separator, rasDumpRequestsToNames.get(rasDumpRequests[i]) );
				separator = "+";
			}
		}
	}
	
}

