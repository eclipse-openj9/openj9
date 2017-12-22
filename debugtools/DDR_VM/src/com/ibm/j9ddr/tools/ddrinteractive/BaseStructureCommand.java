/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;

/**
 * Base class for structure formatting commands
 *
 */
public abstract class BaseStructureCommand implements ICommand
{

	private final List<IFieldFormatter> fieldFormatters = new LinkedList<IFieldFormatter>();
	private final List<IStructureFormatter> structureFormatters = new LinkedList<IStructureFormatter>();
	
	{
		registerStructureFormatter(new DefaultStructureFormatter());
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.tools.ddrinteractive.ICommand#recognises(java.lang.String)
	 */
	public final boolean recognises(String command, Context context)
	{
		if (command.length() < 2) {
			return false;
		}
		
		//Strip the !
		command = command.substring(1);
		
		StructureDescriptor desc = StructureCommandUtil.getStructureDescriptor(command, context);
		
		return desc != null;
	}
	
	/**
	 * Inserts supplied field formatter at the head of the list.
	 * @param formatter
	 */
	protected void registerFieldFormatter(IFieldFormatter formatter)
	{
		if (formatter == null) {
			throw new IllegalArgumentException("Field formatter cannot be null");
		}
		
		fieldFormatters.add(0, formatter);
	}
	
	/**
	 * Inserts supplied structure formatter at the head of the list.
	 * @param formatter
	 */
	protected void registerStructureFormatter(IStructureFormatter formatter)
	{
		if (formatter == null) {
			throw new IllegalArgumentException("Structure formatter cannot be null");
		}
		
		structureFormatters.add(0, formatter);
	}
	
	/**
	 * Inserts supplied structure formatter at the end of the list.
	 * @param formatter
	 */
	protected void registerDefaultStructureFormatter(IStructureFormatter formatter)
	{
		if (formatter == null) {
			throw new IllegalArgumentException("Structure formatter cannot be null");
		}
		
		if(structureFormatters.size() > 0) {
			structureFormatters.remove(structureFormatters.size() - 1);
		}
		structureFormatters.add(formatter);
	}
	

	public final void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		if (args.length < 1) {
			out.println("Missing address argument. Usage: " + command + " <address>");
			return;
		}
		
		boolean is64BitPlatform = (context.process.bytesPerPointer() == 8) ? true : false;
		long address = CommandUtils.parsePointer(args[0], is64BitPlatform);

		String[] extraArgs = new String[args.length - 1];
		
		for (int i = 0; i < extraArgs.length; i++) {
			extraArgs[i] = args[i + 1];
		}
		
		formatStructure(command.substring(1), address, out, context, extraArgs);
	}
	
	protected class DefaultStructureFormatter extends BaseStructureFormatter
	{

		@Override
		public FormatWalkResult format(String type, long address, PrintStream out, Context context, List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
		{
			setFieldFormatters(fieldFormatters);
			
			List<StructureDescriptor> inheritanceStack = new LinkedList<StructureDescriptor>();
			
			String current = type;
			
			while (current != null && current.length() > 0) {
				StructureDescriptor desc = StructureCommandUtil.getStructureDescriptor(current, context);
				
				inheritanceStack.add(0, desc);
				
				current = desc.getSuperName();
			}
			
			StructureDescriptor specifiedType = inheritanceStack.get(inheritanceStack.size() - 1);
			
			out.print(specifiedType.getName());
			out.print(" at ");
			out.print("0x");
			out.print(Long.toHexString(address));
			out.print(" {");
			out.println();
			
			for (StructureDescriptor desc : inheritanceStack) {
				out.println(String.format("  Fields for %s:", desc.getName()));
				for (FieldDescriptor thisField : desc.getFields()) {
					out.print("\t0x");
					out.print(Integer.toHexString(thisField.getOffset()));
					out.print(": ");
					out.print(thisField.getDeclaredType());
					out.print(" ");
					out.print(thisField.getName());
					out.print(" = ");

					try {
						formatField(thisField.getName(), thisField.getType(), thisField.getDeclaredType(), address + thisField.getOffset(), out, context);
					} catch (CorruptDataException e) {
						out.print("<FAULT>");
					}
					out.println();
				}
			}
			
			out.println("}");
			
			return FormatWalkResult.STOP_WALKING;
		}
	}

	private void formatStructure(String type, long address, PrintStream out, Context context, String[] extraArgs) 
	{
		/* Take a copy of the field formatters list - it could be changed as part of the walk */
		List<IFieldFormatter> localFieldFormatters = new ArrayList<IFieldFormatter>(fieldFormatters);
		
		for (IStructureFormatter formatter : structureFormatters) {
			FormatWalkResult result = formatter.preFormat(type, address, out, context, localFieldFormatters, extraArgs);
			
			if (result == FormatWalkResult.STOP_WALKING) {
				break;
			}
		}
		
		boolean found = false;
		for (IStructureFormatter formatter : structureFormatters) {
			FormatWalkResult result = formatter.format(type, address, out, context, localFieldFormatters, extraArgs);
			
			if (result == FormatWalkResult.STOP_WALKING) {
				found = true;
				break;
			}
		}
		
		if (! found) {
			out.println("<<Couldn't format " + type + ">>");
		}
		
		for (IStructureFormatter formatter : structureFormatters) {
			FormatWalkResult result = formatter.postFormat(type, address, out, context, localFieldFormatters, extraArgs);
			
			if (result == FormatWalkResult.STOP_WALKING) {
				break;
			}
		}
	}

	public final Collection<String> getCommandDescriptions()
	{
		return Collections.singleton("<struct> <address>\t\tFormat <struct> at <address>");
	}
	
	public Set<String> getCommandNames()
	{
		/* TODO: reach for structure names here */
		return null;
	}

}
