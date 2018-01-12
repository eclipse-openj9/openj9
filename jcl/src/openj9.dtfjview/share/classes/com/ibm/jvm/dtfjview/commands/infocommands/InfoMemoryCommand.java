/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2013, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands.infocommands;

import java.io.PrintStream;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.logging.Level;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaRuntimeMemoryCategory;
import com.ibm.dtfj.java.JavaRuntimeMemorySection;
import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.dtfj.java.JavaVMOption;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;

@DTFJPlugin(version="1.*", runtime=false)
public class InfoMemoryCommand extends BaseJdmpviewCommand {
		
	private static final String COM_IBM_DBGMALLOC_PROPERTY = "-Dcom.ibm.dbgmalloc=true";

	{
		addCommand("info memory", "", "Provides information about the native memory usage in the Java Virtual Machine");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if (initCommand(command, args, context, out)) {
			return; // processing already handled by super class
		}
		JavaRuntime runtime = ctx.getRuntime();

		try {
			Iterator memoryCategories = runtime.getMemoryCategories();
			printAllMemoryCategories(out, memoryCategories);
			printDbgmallocWarning(out, runtime);
		} catch (DataUnavailable du) {
			out.println("Memory categories information unavailable.");
			logger.log(Level.FINE, Exceptions.getDataUnavailableString(), du);
		}

	}

	private void printDbgmallocWarning(PrintStream out, JavaRuntime runtime) {
		try {
			boolean dbgmalloc = false;
			JavaVMInitArgs args = runtime.getJavaVMInitArgs();
			if(args != null) {
				Iterator<?> opts = args.getOptions();
				while(opts.hasNext()) {
					Object obj = opts.next();
					if(obj instanceof JavaVMOption) {
						JavaVMOption opt = (JavaVMOption) obj;
						if (COM_IBM_DBGMALLOC_PROPERTY.equals(opt.getOptionString()) ) {
							dbgmalloc = true;
						}
					}
				}
			}
			if( !dbgmalloc ) {
				out.println();
				out.printf("Note: %s was not found in the Java VM init options.\nMemory allocated by some class library components will not have been recorded.\n", COM_IBM_DBGMALLOC_PROPERTY);
			}
		} catch (DataUnavailable du) {
			out.println("Java VM init options information unavailable.");
			logger.log(Level.FINE, Exceptions.getDataUnavailableString(), du);
		} catch (CorruptDataException cde) {
			out.println("Corrupt Data encountered walking Java VM init options");
			logger.log(Level.FINE, Exceptions.getCorruptDataExceptionString(), cde);
		}
	}

	/**
	 * Print out the memory categories in nice ASCII art tree similar to the ones in javacore.
	 * @param out the PrintStream to write to.
	 * @param memoryCategories the memory categories to write
	 */
	private void printAllMemoryCategories(PrintStream out, Iterator memoryCategories) {
		try {
			while (memoryCategories.hasNext()) {
				Object obj = memoryCategories.next();
				if (obj instanceof JavaRuntimeMemoryCategory) {
					JavaRuntimeMemoryCategory category = (JavaRuntimeMemoryCategory) obj;
					LinkedList<Integer> stack = new LinkedList<Integer>();
					int childCount = countPrintableChildren(category);
					stack.addLast(childCount);
					printCategory(category, stack, out);
				}
			}
		} catch (CorruptDataException cde) {
			out.println("Corrupt Data encountered walking memory categories");
			logger.log(Level.FINE, Exceptions.getCorruptDataExceptionString(),
					cde);
		}
	}

	/**
	 * Print a category and any of it's children. The stack argument is a list of how many children
	 * each parent category has so we can work out how to print the ascii art lines from parents
	 * to children.
	 * 
	 * @param category
	 * @param stack
	 * @param out
	 */
	private void printCategory(JavaRuntimeMemoryCategory category, LinkedList<Integer> stack, PrintStream out) {
		int depth = stack.size() - 1;
		try {
			if(category.getShallowBytes() > 0 || category.getDeepBytes() > 0 ) {
				/* Do the indenting and the ASCII art tree */
				indentToDepth(new String[] {"|  ", "|  ", "   " }, stack, out, depth);
				if( depth > 0 ) {
					out.println();
				}
				indentToDepth(new String[] {"+--", "|  ", "   " }, stack, out, depth);

				/* Actually print out the interesting data */
				out.printf("%s: %d bytes / %d allocations\n",
						category.getName(), category.getDeepBytes(),
						category.getDeepAllocations());			
		
				/* Only descend if there's really more data in sub categories unaccounted for. */
				if( category.getDeepBytes() > category.getShallowBytes() ) {
					
					Iterator memoryCategories = category.getChildren();
					JavaRuntimeMemoryCategory other = getOtherCategory(category);
					
					while (memoryCategories.hasNext()) {
						int parentCount = stack.get(depth);
						parentCount--;
						stack.set(depth, parentCount);
						Object obj = memoryCategories.next();
						if( obj instanceof JavaRuntimeMemoryCategory ) {
							JavaRuntimeMemoryCategory next = (JavaRuntimeMemoryCategory) obj;
							int childCount = countPrintableChildren(next);
							stack.addLast(childCount);
							printCategory(next, stack, out);
							stack.removeLast();
						}
					}
					/* Add magic "Other" category for unaccounted memory. (Like javacore) */
					if( other != null ) {
						stack.addLast(0); // "Other" will never have children.
						printCategory(other, stack, out);
						stack.removeLast();
					}
				}
			}
		} catch (CorruptDataException cde) {
			out.println("Corrupt Data encountered walking memory categories");
			logger.log(Level.FINE, Exceptions.getCorruptDataExceptionString(), cde);
		}
	}
	
	/**
	 * Count how many children a given category will print out.
	 * 
	 * @param category
	 * @return
	 * @throws CorruptDataException
	 */
	private static int countPrintableChildren(JavaRuntimeMemoryCategory category) throws CorruptDataException {
		int children = 0;
		if (category.getDeepBytes() > category.getShallowBytes()) {
			Iterator memoryCategories = category.getChildren();
			while (memoryCategories.hasNext()) {
				Object obj = memoryCategories.next();
				if( obj instanceof JavaRuntimeMemoryCategory) {
					JavaRuntimeMemoryCategory next = (JavaRuntimeMemoryCategory) obj;
					if (next.getShallowBytes() > 0 || next.getDeepBytes() > 0) {
						children++;
					}
				}
			}
		}
		// Will this have an "Other" category.
		if (category.getShallowAllocations() > 0) {
			children++;
		}
		return children;
	}
	
	/**
	 * Build an "Other" category for Memory Categories that have children that don't account for
	 * all their allocations.
	 * 
	 * Possibly overkill but it saves having lots of special cases for if there is an "Other"
	 * when printing the ascii art tree.
	 * 
	 * @param category
	 * @return the "Other" category containing left over memory or null.
	 * @throws CorruptDataException 
	 */
	private static JavaRuntimeMemoryCategory getOtherCategory(final JavaRuntimeMemoryCategory category) throws CorruptDataException {
		if (category.getShallowAllocations() == 0) {
			return null;
		}
		JavaRuntimeMemoryCategory other = new JavaRuntimeMemoryCategory() {

			public long getShallowBytes() throws CorruptDataException {
				return 0;
			}

			public long getShallowAllocations() throws CorruptDataException {
				return 0;
			}

			public String getName() throws CorruptDataException {
				return "Other";
			}

			public Iterator<JavaRuntimeMemorySection> getMemorySections(
					boolean includeFreed) throws CorruptDataException,
					DataUnavailable {
				return null;
			}

			public long getDeepBytes() throws CorruptDataException {
				return category.getShallowBytes();
			}

			public long getDeepAllocations() throws CorruptDataException {
				return category.getShallowAllocations();
			}

			public Iterator<JavaRuntimeMemoryCategory> getChildren()
					throws CorruptDataException {
				return new Iterator<JavaRuntimeMemoryCategory>() {

					public boolean hasNext() {
						return false;
					}

					public JavaRuntimeMemoryCategory next() {
						return null;
					}

					public void remove() {
					}
				};
			}
		};
		return other;
	}
	
	private static void indentToDepth(String[] indents, LinkedList<Integer> stack,
			PrintStream out, int depth) {
		for (int i = 0; i < depth; i++) {
			if( i == depth-1 ) {
				out.print(indents[0]);
			} else if( stack.get(i) >  0 ) {
				out.print(indents[1]);
			} else {
				out.print(indents[2]);
			}
		}
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("outputs a tree of native memory usage by each component in the Java Virtual Machine \n\n" +
				"parameters: none\n");
	}
}
