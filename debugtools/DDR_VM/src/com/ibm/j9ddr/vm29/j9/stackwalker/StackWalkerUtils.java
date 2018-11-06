/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.stackwalker;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_ROM_METHOD_FROM_RAM_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_ITERATE_O_SLOTS;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_MAINTAIN_REGISTER_MAP;
import static com.ibm.j9ddr.vm29.structure.J9StackWalkState.J9_STACKWALK_SLOT_TYPE_INTERNAL;
import static java.util.logging.Level.FINE;
import static java.util.logging.Level.FINER;
import static java.util.logging.Level.FINEST;

import java.util.logging.Level;
import java.util.logging.Logger;

import java.io.PrintStream;
import java.text.MessageFormat;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.J9ConfigFlags;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Utility methods shared between stack walkers.
 * 
 * @author andhall
 *
 */
public class StackWalkerUtils
{
	/**
	 * Stack walker logger, can be enabled for debugging stack walker issues when
	 * running inside other tools, for example Memory Analyzer.
	 */
	public static final Logger logger = Logger.getLogger("j9ddr.stackwalker");
	
	private static int messageLevel = 0;
	
	private static PrintStream messageStream = null;
	
	/**
	 * Number of CorruptDataExceptions from iterate-o-slots logic that we report before disabling
	 * the o-slots walk
	 */
	public static final int INITIAL_O_SLOTS_CORRUPTION_THRESHOLD = 5;
	
	/**
	 * Counter for how many times we log and continue when walking O-slots is
	 * causing CorruptDataExceptions.
	 */
	private static int oslotsCorruptionThreshold = INITIAL_O_SLOTS_CORRUPTION_THRESHOLD;
	
	public static final boolean DEBUG_STACKMAP = false;
	
	public static final boolean DEBUG_LOCALMAP = false;
	
	static final int jitArgumentRegisterNumbers[];
	
	static {
		if (J9ConfigFlags.arch_x86) {
			if (J9BuildFlags.env_data64) {
				jitArgumentRegisterNumbers = new int[] { 0, 5, 3, 2 };
			} else {
				// 32 bit X86 doesn't use jitArgumentRegisterNumbers
				jitArgumentRegisterNumbers = new int[0];
			}
		} else if (J9ConfigFlags.arch_power) {
			jitArgumentRegisterNumbers = new int[] { 3, 4, 5, 6, 7, 8, 9, 10 };
		} else if (J9ConfigFlags.arch_s390) {
			jitArgumentRegisterNumbers = new int[] { 1, 2, 3 };
		} else {
			throw new IllegalArgumentException("Unsupported platform");
		}
	}
	
	/**
	 * This function is a little overloaded. As well as determining if a message should
	 * be printed out for the message level chosen by the tool running the stack walker
	 * we also allow for someone turning on logging for debug to get details about the
	 * stack walker while running (for example) Memory Analyzer.
	 * 
	 * @param walkState
	 * @param level
	 * @param message
	 * @param args
	 */
	public static void swPrintf(WalkState walkState, int level,
			String message, Object... args)
	{

		// Print output for !stack debugging commands to the printstream.
		if( messageLevel >= level && messageStream != null ) {
			String output = MessageFormat.format("<"
					+ Long.toHexString(walkState.walkThread.getAddress())
					+ "> " + message, args);
			messageStream.println(output);
		}

		// Write output for to logger for debugging stack walker inside
		// larger applications like memory analyzer.
		Level utilLoggingLevel = level == 1 ? FINE : level == 2 ? FINER
				: FINEST;
		/* Initial check to avoid marshalling the arguments if we don't have to */
		if (logger.isLoggable(utilLoggingLevel)) {
			logger.logp(utilLoggingLevel, null, null, "<"
					+ Long.toHexString(walkState.walkThread.getAddress())
					+ "> " + message, args);
		}
	}
	
	public static J9ConstantPoolPointer UNTAGGED_METHOD_CP(
			J9MethodPointer method) throws CorruptDataException
	{
		if (J9BuildFlags.interp_nativeSupport) {
			return method.constantPool().untag(J9Consts.J9_STARTPC_STATUS);
		} else {
			return method.constantPool();
		}
	}
	
	public static void swPrintMethod(WalkState walkState)
	throws CorruptDataException
	{
		swPrintMethod(walkState, walkState.method);
	}

	public static void swPrintMethod(WalkState walkState, J9MethodPointer method)
	throws CorruptDataException
	{
		if (method.notNull()) {
			J9UTF8Pointer className = UNTAGGED_METHOD_CP(method).ramClass()
			.romClass().className();
			J9ROMMethodPointer romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			J9UTF8Pointer name = romMethod.nameAndSignature().name();
			J9UTF8Pointer sig = romMethod.nameAndSignature().signature();
			swPrintf(walkState, 2, "\tMethod: {0}.{1}{2} !j9method {3}",
				J9UTF8Helper.stringValue(className), J9UTF8Helper.stringValue(name), J9UTF8Helper.stringValue(sig),
				method.getHexAddress());
		}
	}
	
	public static void WALK_METHOD_CLASS(WalkState walkState)
	throws CorruptDataException
	{
		if ((walkState.flags & J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS) != 0) {
			SWALK_PRINT_CLASS_OF_RUNNING_METHOD(walkState);
			walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_INTERNAL;
			walkState.slotIndex = -1;			
			WALK_O_SLOT(walkState, walkState.constantPool.ramClass().classObjectEA());
		}
	}
	
	public static void WALK_NAMED_INDIRECT_O_SLOT(WalkState walkState,
			PointerPointer objectSlot, VoidPointer indirectSlot, String tag)
			throws CorruptDataException
	{
		UDATA value;

		value = UDATAPointer.cast(objectSlot).at(0);

		if (indirectSlot.notNull()) {
			swPrintf(walkState, 4, "\t\t{0}[{1} -> {2}] = {3}", (tag != null ? tag : "O-Slot"),
					indirectSlot.getHexAddress(), 
					objectSlot.getHexAddress(),
					value.getHexValue());
		} else {
			swPrintf(walkState, 4, "\t\t{0}[{1}] = {2}",
					(tag != null ? tag : "O-Slot"),
					objectSlot.getHexAddress(), 
					value.getHexValue());
		}
		walkState.callBacks.objectSlotWalkFunction(walkState.walkThread, walkState, objectSlot, VoidPointer.cast(objectSlot));
	}

	public static void WALK_NAMED_INDIRECT_I_SLOT(WalkState walkState,
			PointerPointer intSlot, VoidPointer indirectSlot, String tag)
			throws CorruptDataException
	{
		if (indirectSlot.notNull()) {
			swPrintf(walkState, 4, "\t\t{0}[{1} -> {2}] = {3}", (tag != null? tag : "I-Slot"),
					indirectSlot.getHexAddress(), 
					intSlot.getHexAddress(),
					UDATAPointer.cast(intSlot).at(0).getHexValue());
		} else {
			swPrintf(walkState, 4, "\t\t{0}[{1}] = {2}",
					(tag != null ? tag : "I-Slot"),
					intSlot.getHexAddress(), 
					UDATAPointer.cast(intSlot).at(0).getHexValue());
		}
	}

	
	public static void WALK_INDIRECT_O_SLOT(WalkState walkState,
			PointerPointer slot, VoidPointer ind) throws CorruptDataException
	{
		WALK_NAMED_INDIRECT_O_SLOT(walkState, slot, ind, null);
	}

	public static void WALK_INDIRECT_I_SLOT(WalkState walkState,
			PointerPointer slot, VoidPointer ind) throws CorruptDataException
	{
		WALK_NAMED_INDIRECT_I_SLOT(walkState, slot, ind, null);
	}

	public static void WALK_NAMED_O_SLOT(WalkState walkState,
			PointerPointer slot, String tag) throws CorruptDataException
	{
		WALK_NAMED_INDIRECT_O_SLOT(walkState, slot, VoidPointer.cast(0), tag);
	}

	public static void WALK_NAMED_I_SLOT(WalkState walkState,
			PointerPointer slot, String tag) throws CorruptDataException
	{
		WALK_NAMED_INDIRECT_I_SLOT(walkState, slot, VoidPointer.cast(0), tag);
	}

	public static void WALK_O_SLOT(WalkState walkState, PointerPointer slot)
			throws CorruptDataException
	{
		WALK_INDIRECT_O_SLOT(walkState, slot, VoidPointer.cast(0));
	}

	public static void WALK_I_SLOT(WalkState walkState, PointerPointer slot)
			throws CorruptDataException
	{
		WALK_INDIRECT_I_SLOT(walkState, slot, VoidPointer.cast(0));
	}
	
	private static void SWALK_PRINT_CLASS_OF_RUNNING_METHOD(WalkState walkState)
	{
		swPrintf(walkState, 4, "\tClass of running method");
	}
	
	public static UDATA JIT_RESOLVE_PARM(WalkState walkState, int parmNumber) throws CorruptDataException
	{
		if (J9ConfigFlags.arch_x86 && !J9BuildFlags.env_data64) {
			return walkState.bp.at(parmNumber);
		} else {
			return walkState.walkedEntryLocalStorage.jitGlobalStorageBase().at(jitArgumentRegisterNumbers[parmNumber - 1]);
		}
	}

	
	public static void resetOSlotsCorruptionThreshold()
	{
		oslotsCorruptionThreshold = INITIAL_O_SLOTS_CORRUPTION_THRESHOLD;
	}
	
	public static boolean oSlotsCorruptionThresholdReached()
	{
		return oslotsCorruptionThreshold < 0;
	}
	
	/**
	 * Since walking OSlots touches a lot more code than just doing a frame walk, we have a layer of corruption handling
	 * just above the o-slots logic. We have oslotsCorruptionThreshold attempts, then give up.
	 * 
	 * This method is called in the catch block just below the top-level if (J9_STACKWALK_ITERATE_O_SLOTS) {} conditional.
	 */
	public static void handleOSlotsCorruption(WalkState walkState, String className, String methodName,
			CorruptDataException ex)
	{
		if (oslotsCorruptionThreshold > 0) {
			oslotsCorruptionThreshold--;
			raiseCorruptDataEvent("CorruptData encountered iterating o-slots. walkThread = " + walkState.walkThread.getHexAddress(), ex, false);
		}
		
		if (oslotsCorruptionThreshold == 0) {
			raiseCorruptDataEvent("Corruption threshold hit. Will stop walking object slots on this thread. walkThread = " + walkState.walkThread.getHexAddress(), ex, false);
			walkState.flags &= ~(J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_MAINTAIN_REGISTER_MAP);
			oslotsCorruptionThreshold = -1;
		}
	}
	
	/**
	 * Enables stackwalk verbose logging through j.u.logging - similar in appearance to -verbose:stackwalk output from native.
	 */
	public static void enableVerboseLogging(int level)
	{
		enableVerboseLogging(level, System.err);
	}
	
	public static void enableVerboseLogging(int level, PrintStream os)
	{
		messageLevel = level;
		messageStream = os;
	}
	
	public static void disableVerboseLogging()
	{
		messageLevel = 0;
		messageStream = null;
	}
}
