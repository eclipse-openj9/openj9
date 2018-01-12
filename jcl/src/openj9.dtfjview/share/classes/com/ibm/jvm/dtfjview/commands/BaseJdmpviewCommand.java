/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands;

import java.io.PrintStream;
import java.util.logging.Logger;

import com.ibm.dtfj.java.JavaThread;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.IDTFJContext;
import com.ibm.java.diagnostics.utils.commands.BaseCommand;
import com.ibm.java.diagnostics.utils.commands.CommandException;

/**
 * This the base command for all jdmpview commands. It ensures consistent 
 * behaviour with the legacy commands without breaking the generic command
 * encapsulation.
 * 
 * There are a number of methods which allow DTFJ objects to be consistently
 * displayed across different plugins e.g. getClassInfo(). Where the method
 * name starts with getXYZ the output will be returned for further processing, 
 * however where the method starts with writeXYZ then the output is written 
 * immediately to the command's print stream.
 * 
 * @author adam
 *
 */
public abstract class BaseJdmpviewCommand extends BaseCommand {
	private static final String CMD_HELP = "help";
	private static final String CMD_QMARK = "?";
	private static final String HEX_PREFIX = "0x";
	protected PrintStream out = null;			//the object to print out to
	protected IDTFJContext ctx = null;			//the context within which to run
	protected Logger logger = Logger.getLogger("com.ibm.jvm.dtfjview.logger.command");
	protected String hexfmt = "0x%016x";		//default 64bit format for hex strings
	
	/**
	 * Print detailed help for a given command
	 * 
	 * @param out stream to write the output to
	 */
	public abstract void printDetailedHelp(PrintStream out);

	/**
	 * Common processing to initialize the command and make the porting of 
	 * existing jdmpview commands easier.
	 * 
	 * @param command	the command being executed
	 * @param args		the supplied arguments
	 * @param context	the context within which it is operating
	 * @param out		where to write the output
	 * @return			returns true if this method has handled the output and the subclass should exit
	 * @throws CommandException
	 */
	public final boolean initCommand(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(!(context instanceof IDTFJContext)) {
			throw new CommandException("This command works within a DTFJ context, the supplied context was an instance of " + context.getClass().getName());
		}
		this.out = out;
		ctx = (IDTFJContext) context;
		if(ctx.getProcess() != null) {
			hexfmt = "0x%0" + ((ctx.getProcess().getPointerSize() / 8) * 2) + "x";
		}
		if((args.length == 1) && (args[0].equalsIgnoreCase(CMD_HELP) || args[0].equalsIgnoreCase(CMD_QMARK))) {
			printDetailedHelp(out);
			return true;
		}
		return false;
	}
	
	/**
	 * Typically used when an extension takes a numeric argument, and wants to
	 * accept a variety of formats. Currently it will support decimal or
	 * hexadecimal formats.
	 * 
	 * @param number
	 *            String representation of the number to parse
	 * @return the numeric value
	 */
	public long toLong(String number) {
		if (number.toLowerCase().startsWith(HEX_PREFIX)) {
			return Long.parseLong(number.substring(HEX_PREFIX.length()), 16);
		} else {
			return Long.parseLong(number);
		}
	}
	
	/**
	 * Enum for identifying the type of artifact that is currently being analyzed.
	 * 
	 * @author adam
	 *
	 */
	protected enum ArtifactType {
		core, phd, javacore, unknown
	}
	
	/*
	 * This will be changed internally when the DTFJ Image.getType() call is implemented
	 */
	/**
	 * Determine the type of artifact currently being operated on.
	 * 
	 * @return artifact type
	 */
	protected ArtifactType getArtifactType() {
		String name = ctx.getAddressSpace().getClass().getName();
		int index = name.lastIndexOf('.');
		String className = name.substring(++index);
		if (className.startsWith("JC")) {
			return ArtifactType.javacore;
		}
		if (className.startsWith("PHD")) {
			return ArtifactType.phd;
		}
		//DDR based implementations of DTFJ
		if (className.startsWith("J9DDR")) {
			return ArtifactType.core;
		}
		//Legacy implementations of DTFJ
		if (className.startsWith("Image")) {
			return ArtifactType.core;
		}
		return ArtifactType.unknown;
	}

	/**
	 * Format an address as hex padded with zeros according to the size of the process.
	 * e.g. 17 would be formatted as 0x00000011 for 32 bit and 0x0000000000000011 for 64 bit 
	 * @param num the number to format
	 * @return formatted number
	 */
	public String toHexStringAddr(long num) {
		return String.format(hexfmt, num);
	}

	/**
	 * Lots of DTFJ methods will throw a standard set of exceptions such as corrupt data
	 * or data unavailable. This method provides a central point for handling these exceptions
	 * and attempts to provide more context as to whether or not this is an expected occurrence 
	 * for the artifact type under analysis.
	 * 
	 * @param cause the exception returned from a call to the DTFJ API
	 * @return formatted string for displaying to the user
	 */
	public String handleException(Throwable cause) {
		return ExceptionHandler.handleException(this, cause);
	}
	
	/**
	 * Textual description of a thread state.
	 * 
	 * @param state state to convert
	 * @return cumulative description of the states 
	 */
	public String decodeThreadState(int state) {
		int[] states = { JavaThread.STATE_ALIVE,
				JavaThread.STATE_BLOCKED_ON_MONITOR_ENTER,
				JavaThread.STATE_IN_NATIVE, JavaThread.STATE_IN_OBJECT_WAIT,
				JavaThread.STATE_INTERRUPTED, JavaThread.STATE_PARKED,
				JavaThread.STATE_RUNNABLE, JavaThread.STATE_SLEEPING,
				JavaThread.STATE_SUSPENDED, JavaThread.STATE_TERMINATED,
				JavaThread.STATE_VENDOR_1, JavaThread.STATE_VENDOR_2,
				JavaThread.STATE_VENDOR_3, JavaThread.STATE_WAITING,
				JavaThread.STATE_WAITING_INDEFINITELY,
				JavaThread.STATE_WAITING_WITH_TIMEOUT };
		String[] stateDescriptions = { "STATE_ALIVE",
				"STATE_BLOCKED_ON_MONITOR_ENTER", "STATE_IN_NATIVE",
				"STATE_IN_OBJECT_WAIT", "STATE_INTERRUPTED", "STATE_PARKED",
				"STATE_RUNNABLE", "STATE_SLEEPING", "STATE_SUSPENDED",
				"STATE_TERMINATED", "STATE_VENDOR_1", "STATE_VENDOR_2",
				"STATE_VENDOR_3", "STATE_WAITING",
				"STATE_WAITING_INDEFINITELY", "STATE_WAITING_WITH_TIMEOUT" };
		StringBuffer sb = new StringBuffer();
		for (int i = 0; i < states.length; i++) {
			if ((state & states[i]) == states[i]) {
				if (sb.length() > 0) {
					sb.append(", ");
				}
				sb.append(stateDescriptions[i]);
			}
		}
		return sb.toString();
	}
	
	/**
	 * Enumeration for the various formats that can be applied to the
	 * output strings.
	 * 
	 * @author adam
	 *
	 */
	private enum FormatEnum {
		HEX_LONG("0x%016x"),
		HEX_SHORT("0x%04x"),
		HEX_BYTE("0x%02x"),
		HEX_INT("0x%08x"),
		DEC_FLOAT("%e"),
		DEC_DOUBLE("%e");
		
		FormatEnum(String format) {
			this.format = format;
		}
		
		private final String format;
		
		public String getFormat() {
			return format;
		}
	}
	
	/**
	 * Format a number with the correct number of zeros as padding according 
	 * to the data type passed in.
	 * @param num the number to format
	 * @return formatted number
	 */
	public String toHexString(long num) {
		return String.format(FormatEnum.HEX_LONG.getFormat(), num);
	}
	
	/**
	 * Format a number with the correct number of zeros as padding according 
	 * to the data type passed in.
	 * @param num the number to format
	 * @return formatted number
	 */
	public String toHexString(short num) {
		return String.format(FormatEnum.HEX_SHORT.getFormat(), num);
	}

	/**
	 * Format a number with the correct number of zeros as padding according 
	 * to the data type passed in.
	 * @param num the number to format
	 * @return formatted number
	 */
	public String toHexString(int num) {
		return String.format(FormatEnum.HEX_INT.getFormat(), num);
	}
	
	/**
	 * Format a number with the correct number of zeros as padding according 
	 * to the data type passed in.
	 * @param num the number to format
	 * @return formatted number
	 */
	public String toHexString(byte num) {
		return String.format(FormatEnum.HEX_BYTE.getFormat(), num);
	}

	/**
	 * Convenience method to convert a number into a String to display.
	 * @param num the number to format
	 * @return formatted number
	 */
	public String toString(float num) {
		return String.format(FormatEnum.DEC_FLOAT.getFormat(), num);
	}

	/**
	 * Convenience method to convert a number into a String to display.
	 * @param num the number to format
	 * @return formatted number
	 */
	public String toString(double num) {
		return String.format(FormatEnum.DEC_DOUBLE.getFormat(), num);
	}
}
