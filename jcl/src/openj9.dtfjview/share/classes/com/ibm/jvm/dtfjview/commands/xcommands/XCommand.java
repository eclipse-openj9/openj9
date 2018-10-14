/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands.xcommands;

import java.io.PrintStream;
import java.util.Properties;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;

/**
 * Abstract superclass for all x/ commands 
 */
public abstract class XCommand extends BaseJdmpviewCommand {
	protected int argUnitSize = 1;
	protected int argUnitNumber = 1;
	protected String argDisplayFormat = "";
		
		// note: do NOT add any of the following:
		//  - XBCommand
		//  - XHCommand
		//  - XWCommand
		//  - XGCommand
		
		// why not?  because 'b', 'h', 'w', and 'g' represent unit sizes and
		//  the parser won't know whether you mean a unit size or a display format
		//  if you use any of the above characters for a display format

	/* (non-Javadoc)
	 * @see com.ibm.java.diagnostics.commands.BaseCommand#recognises(java.lang.String, com.ibm.java.diagnostics.IContext)
	 * 
	 * Override this because this command needs to implement wild card matching of any command that starts x/
	 */
	@Override
	public boolean recognises(String command, IContext context) {
		if(command.length() < 3) {
			return false;		//needs to match x/?
		}
		return command.toLowerCase().startsWith("x/");
	}

	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(command.length() > 3) {
			//additional formatting options have been specified after the x/...?
			String arg = command.substring(2, command.length());
			parseArgs(arg);
		}
		doCommand(args);
	}
	
	protected abstract void doCommand(String[] args);
	
	protected void parseArgs(String arg)
	{
				
		int n;
		Character unitSize;
		Character displayFormat;
		
		if (null == arg || arg.equals(""))
		{
			// user didn't specify any parameters; use the defaults
			n = 1;
			unitSize = getDefaultUnitSize(ctx.getProperties());
			displayFormat = getDefaultDisplayFormat(ctx.getProperties());
		}
		else
		{
			n = 0;
			int i;
			for (i = 0; i < arg.length() && Character.isDigit(arg.charAt(i)); i++)
			{
				n *= 10;
				n += Character.getNumericValue(arg.charAt(i)); 
			}
		
			if (0 == n)
				n = 1;
			
			displayFormat = null;
			unitSize = null;
			
			if (i < arg.length())
			{
				char currChar = arg.charAt(i);
				switch (currChar)
				{
				case 'b':
				case 'h':
				case 'w':
				case 'g':
					unitSize = Character.valueOf(currChar);
					break;
				default:
					displayFormat = Character.valueOf(currChar);
					break;
				}
				i++;
			}
			
			if (i < arg.length())
			{
				char currChar = arg.charAt(i);
				switch (currChar)
				{
				case 'b':
				case 'h':
				case 'w':
				case 'g':
					if (null == unitSize)
					{
						unitSize = Character.valueOf(currChar);
					}
					else
					{
						out.println("first letter after \"x/\" was a unit size character; " +
								"second letter (if specified) must be a display " +
								"format letter but it was also a unit size character");
						return;
					}
					break;
				default:
					if (null == displayFormat)
					{
						displayFormat = Character.valueOf(currChar);
					}
					else
					{
						out.println("first letter after \"x/\" was a display format character; " +
								"second letter (if specified) must be a unit size " +
								"letter but it was also a display format character");
						return;
					}
					break;
				}
				i++;
			}
			
			if (arg.length() != i)
			{
				out.println("too many letters after \"x/\"; the \"x/\" command accepts at " +
						"most two letters, a display format character and a unit " +
						"size character");
				return;
			}
			
			// we now have all the necessary information to put on the stack (except
			//  for the unspecified parameters that assume use of defaults) so let's
			//  get the required default values and push some parameters back on to
			//  the argument stack
			
			if (null == unitSize) {
				unitSize = getDefaultUnitSize(ctx.getProperties());
			} else {
				setDefaultUnitSize(ctx.getProperties(), unitSize);
			}
			
			if (null == displayFormat) {
				displayFormat = getDefaultDisplayFormat(ctx.getProperties());
			} else {
				setDefaultDisplayFormat(ctx.getProperties(), displayFormat);
			}
		}
		
		int nUnitSize = 1;
		char cUnitSize = unitSize.charValue();
		
		switch (cUnitSize)
		{
		case 'b':
		default:
			nUnitSize = 1;
			break;
		case 'h':
			nUnitSize = 2;
			break;
		case 'w':
			nUnitSize = 4;
			break;
		case 'g':
			nUnitSize = 8;
			break;
		}

		argUnitSize = nUnitSize;			// add the unit size to the stack
		argUnitNumber = n;					// add the number of units to print to the stack
		argDisplayFormat = displayFormat.toString();	// add the display format as a String	
		
	}

	private Character getDefaultUnitSize(Properties properties)
	{
		Character defaultUnitSize = (Character)properties.get("x_default_unit_size");
		if (null == defaultUnitSize)
			return Character.valueOf('w');
		else
			return defaultUnitSize;
	}
	
	private Character getDefaultDisplayFormat(Properties properties)
	{
		Character defaultDisplayFormat = (Character)properties.get("x_default_display_format");
		if (null == defaultDisplayFormat)
			return Character.valueOf('x');
		else
			return defaultDisplayFormat;
	}
	
	private void setDefaultUnitSize(Properties properties, Character defaultUnitSize)
	{
		properties.put("x_default_unit_size", defaultUnitSize);
	}
	
	private void setDefaultDisplayFormat(Properties properties, Character defaultDisplayFormat)
	{
		properties.put("x_default_display_format", defaultDisplayFormat);
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("works like \"x/\" in gdb (including use of defaults): passes " +
				"number of items to display and unit " +
				"size ('b' for byte, 'h' for halfword, 'w' for word, 'g' for giant " +
				"word) to sub-command (ie. x/12bd)");
	}
}
