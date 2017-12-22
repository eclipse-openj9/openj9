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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.io.PrintStream;
import java.math.BigInteger;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Set;

import com.ibm.j9ddr.corereaders.memory.MemoryFault;


public abstract class Command implements ICommand
{
	protected static final String nl = System.getProperty("line.separator");

	private HashMap<String,CommandDescription> _commandNames = new HashMap<String,CommandDescription>();
		
	/**
	 * 
	 * @param name				Command Name
	 * @param argDescription	Brief name of any optional or required arguments 
	 * @param helpDescription	One-liner Description of the command
	 * 
	 * argDescription should be a word describing the argument name. 
	 * 	e.g:  <address>    to specify an address argument that is mandatory
	 *        [address]    to specify an address argument that is optional 
	 * 	   
	 */
	public CommandDescription addCommand(String name, String argDescription, String helpDescription)
	{
		CommandDescription description = new CommandDescription(name, argDescription, helpDescription);
		_commandNames.put(name, description);
		return description;
	}
	
	public void addSubCommand(CommandDescription command, String name, String argDescription, String helpDescription)
	{
		command.addSubCommand(name, argDescription, helpDescription);
	}
	
	public boolean recognises(String command, Context context)
	{		
		if (command.charAt(0) == '!') {
			command = command.substring(1);
		}

		Iterator<String> iterator = _commandNames.keySet().iterator();
		while (iterator.hasNext()) {       
			if (command.equalsIgnoreCase(iterator.next())) {
				return true;
			}
		}

		return false;
	}
	
	public Collection<String> getCommandDescriptions()
	{
		String desc = new String();
		Iterator<CommandDescription> iterator = _commandNames.values().iterator();
		while (iterator.hasNext()) { 
			CommandDescription cmd = iterator.next();
			desc = desc + String.format("%-25s %-20s %s\n", cmd.commandName, cmd.argumentDescription, cmd.helpDescription); 
		}
				
		return Collections.singleton(desc);
	}
	
	public Set<String> getCommandNames()
	{
		return _commandNames.keySet();
	}
	
	/**
	 * This prints the detailed help for the command by listing all the names
	 * that this command can be invoked by and also any sub commands that have been
	 * defined.
	 * 
	 * @param out stream to write the output on
	 */
	public void printDetailedHelp(PrintStream out) {
		for(CommandDescription cmd : _commandNames.values()) {
			out.println(cmd.getCommandName() + "\t" + cmd.getArgumentDescription() + "\t\t" + cmd.helpDescription);
			for(CommandDescription subcmd : cmd._subCommandNames.values()) {
				out.println("\t" + subcmd.getCommandName() + "\t" + subcmd.getArgumentDescription() + "\t\t" + subcmd.helpDescription);
			}
		}
	}
	
	
	/*
	 * pattern: a pointer to the eyecatcher pattern to search for
	 * patternAlignment: guaranteed minimum alignment of the pattern (must be a
	 * power of 2) startSearchFrom: minimum address to search at (useful for
	 * multiple occurrences of the pattern) bytesToSearch: maximum number of
	 * bytes to search
	 * 
	 * Returns: The address of the eyecatcher in TARGET memory space or 0 if it
	 * was not found.
	 * 
	 * NOTES: Currently, this may fail to find the pattern if patternLength >
	 * patternAlignment Generally, dbgFindPattern should be called instead of
	 * dbgFindPatternInRange. It can be more clever about not searching ranges
	 * which aren't in use.
	 * 
	 * @param context Current context. 
	 * @param pattern Pattern to be searched.
	 * @param patternAlignment	Pattern alignment number. 
	 * @param startSearchFrom	Where to start the search.
	 * @param bytesToSearch		number of bytes to search.
	 * @return First memory address where pattern is found.
	 * @throws MemoryFault
	 * 
	 */
	protected long dbgFindPatternInRange(Context context, byte[] pattern, int patternAlignment, long startSearchFrom, BigInteger bytesToSearch) throws MemoryFault 
	{
		long page = startSearchFrom;
		
		BigInteger startSearchFrom2 = new BigInteger(Long.toBinaryString(startSearchFrom), CommandUtils.RADIX_BINARY);
		
		BigInteger udataMax;
		if (context.process.bytesPerPointer() == 4) {
			udataMax = CommandUtils.UDATA_MAX_32BIT;
		} else {
			udataMax = CommandUtils.UDATA_MAX_64BIT;
		}
		/* if bytes to be searched exceeds the allowed max pointer value on this platform, than adjust the value */
		if (startSearchFrom2.add(bytesToSearch).compareTo(udataMax) == 1) {
			bytesToSearch = udataMax.subtract(startSearchFrom2);
		}
		/* round to a page */
		while ((page & 4095) != 0) {
			page--;
			bytesToSearch = bytesToSearch.add(BigInteger.ONE);
		}

		BigInteger bytesSearched = BigInteger.ZERO;

		for (;;) {
			byte[] data = new byte[4096];
			try {
				int bytesRead = context.process.getBytesAt(page, data);
				bytesSearched = bytesSearched.add(new BigInteger(Integer.toString(bytesRead)));
				for (int i = 0; i < bytesRead - pattern.length; i += patternAlignment) {

					if (compare(data, pattern, i)) {
						/* in case the page started before startSearchFrom */
						if (page + i >= startSearchFrom) {
							return page + i;
						}
					}
				}
			} catch (MemoryFault e) {
			}

			if (bytesToSearch.compareTo(new BigInteger("4096")) == -1) {
				break;
			}

			page += 4096;
			bytesToSearch = bytesToSearch.subtract(new BigInteger("4096"));
		}
		return 0;
	}

	private boolean compare(byte[] data, byte[] pattern, int dataStart) 
	{
		for (int i = 0; i < pattern.length; i++) {
			if (data[dataStart + i] != pattern[i]) {
				return false;
			}
		}
		return true;
	}
	
	protected class CommandDescription 
	{
		private String commandName;
		private String argumentDescription;
		private String helpDescription;
		
		private HashMap<String,CommandDescription> _subCommandNames = new HashMap<String,CommandDescription>();
		
		public CommandDescription(String name, String argDescription, String help)
		{
			commandName = name;
			argumentDescription = argDescription;
			helpDescription = help;
		}
		
		public void addSubCommand(String name, String argDescription, String help)
		{
			CommandDescription subCommand = new CommandDescription(name, argDescription, help);
			_subCommandNames.put(name, subCommand);
		}
		
		public String getCommandName() {
			return commandName;
		}

		public String getArgumentDescription() {
			return argumentDescription;
		}

		public String getCommandDescription() {
			return helpDescription;
		}
	}
	
}
