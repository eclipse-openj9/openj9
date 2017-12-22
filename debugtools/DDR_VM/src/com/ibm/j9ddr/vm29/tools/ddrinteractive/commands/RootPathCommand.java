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
import java.io.StringWriter;
import java.util.Stack;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectPointer;
import com.ibm.j9ddr.vm29.events.DefaultEventListener;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.j9.LiveSetWalker;
import com.ibm.j9ddr.vm29.j9.LiveSetWalker.ObjectVisitor;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.j9.RootSet.RootSetType;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9IndexableObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;

public class RootPathCommand extends Command 
{
	boolean _verboseEnabled = false;
	DefaultEventListener _listener = new DefaultEventListener();

	public RootPathCommand()
	{
		addCommand("rootpathfindall", "<address>", "prints out all the paths from a (strong) root to an object");
		addCommand("rootpathfind", "<address>", "prints out a path from a (strong) root to an object");
		addCommand("isobjectalive", "<address>", "determines whether an object is reachable from a (strong) roots or not.  Use !rootfind <address> or !rootfindall <address> to print out the path(s)");
		
		addCommand("strongrootpathfindall", "<address>", "prints out all the paths from a (strong) root to an object");
		addCommand("strongrootpathfind", "<address>", "prints out a path from a (strong) root to an object");
		
		addCommand("anyrootpathfindall", "<address>", "same as rootpathfindall but also includes weak roots e.g. classes, classloaders, weak references");
		addCommand("anyrootpathfind", "<address>", "same as rootpathfind but also includes weak roots e.g. classes, classloaders, weak references");
		
		addCommand("weakrootpathfindall", "<address>", "same as rootpathfindall but only for weak roots e.g. classes, classloaders, weak references");
		addCommand("weakrootpathfind", "<address>", "same as rootpathfind but only weak for roots e.g. classes, classloaders, weak references");
		
		addCommand("rootpathverbose", "", "enables displaying errors when walking live set");
		addCommand("rootpathnoverbose", "", "disables displaying errors when walking live set");
	}
	
	public static String objectToString(J9ObjectPointer object) throws CorruptDataException
	{
		J9ClassPointer clazz = J9ObjectHelper.clazz(object);
		StringWriter buf = new StringWriter();
		String className;
		
		className = J9ClassHelper.getJavaName(clazz);
		if (className.equals("java/lang/Class")) {
			buf.write("class ");
			buf.write(J9ClassHelper.getJavaName(ConstantPoolHelpers.J9VM_J9CLASS_FROM_HEAPCLASS(object)));
			buf.write('@');
			buf.write(object.getHexAddress());
		} else if (className.equals("java/lang/String")) {
			buf.write('"');
			buf.write(J9ObjectHelper.stringValue(object));
			buf.write("\"@");
			buf.write(object.getHexAddress());
		} else {
			buf.write(className);	
			buf.write('@');
			buf.write(object.getHexAddress());
			
			if (ObjectModel.isIndexable(object)) {
				buf.write(" = ");
				buf.write(J9IndexableObjectHelper.getDataAsString(J9IndexableObjectPointer.cast(object)));
			}
		}
		return buf.toString();
	}
	
	private class RootPathFinder implements ObjectVisitor 
	{
		boolean _pathFound = false;
		J9ObjectPointer _objectToFind;
		Stack<J9ObjectPointer> _scanStack = new Stack<J9ObjectPointer>();
		PrintStream _out;
		
		public RootPathFinder(J9ObjectPointer objectToFind, PrintStream out) {
			_objectToFind = objectToFind;
			_out = out;
		}
		
		public boolean visit(J9ObjectPointer object, VoidPointer address) 
		{
			if (_pathFound) {
				/* Don't bother to keep visiting if we've already found our object */
				return false;
			} else {
				_scanStack.push(object);
				if (object.equals(_objectToFind)) {
					dumpTree();
					_pathFound = true;
					_scanStack.pop();
					return false;
				}
			}
			
			/* Haven't found a path yet */
			return true;
		}
		
		private void dumpTree()
		{
			_out.println("\n========================================");
			for (int i = 0; i < _scanStack.size(); i++) {
				for (int j = i; j > 0; j--) {
					_out.print("  ");
				}
				try {
					_out.println(objectToString(_scanStack.get(i)));
				} catch (CorruptDataException cde) {
					_out.println("Invalid Object");
				}
			}
		}
		
		public void finishVisit(J9ObjectPointer object, VoidPointer address)
		{
			_scanStack.pop();
		}
	}
	
	private class ObjectFinderVisitor implements ObjectVisitor 
	{
		boolean _objectFound = false;
		J9ObjectPointer _objectToFind;
		
		public ObjectFinderVisitor(J9ObjectPointer objectToFind) 
		{
			_objectToFind = objectToFind;
		}
		
		public boolean visit(J9ObjectPointer object, VoidPointer address) 
		{
			if (_objectFound) {
				/* Don't bother to keep visiting if we've already found our object */
				return false;
			} else {
				if (object.equals(_objectToFind)) {
					_objectFound = true;
					return false;
				}
			}
			return true;
		}
		
		public void finishVisit(J9ObjectPointer object, VoidPointer address) 
		{
		}
	}
	
	private class RootPathsFinder implements ObjectVisitor 
	{
		Stack<J9ObjectPointer> _scanStack = new Stack<J9ObjectPointer>();
		J9ObjectPointer _objectToFind;
		PrintStream _out;
		
		public RootPathsFinder(J9ObjectPointer objectToFind, PrintStream out) {
			_objectToFind = objectToFind;
			_out = out;
		}
		
		public boolean visit(J9ObjectPointer object, VoidPointer address) {
			_scanStack.push(object);
			if (object.equals(_objectToFind)) {
				dumpTree();
				_scanStack.pop();
				return false;
			} else {
				return true;
			}
		}
		
		private void dumpTree() {
			_out.println("\n========================================");
			for (int i = 0; i < _scanStack.size(); i++) {
				for (int j = i; j > 0; j--) {
					_out.print("  ");
				}
				try {
					_out.println(objectToString(_scanStack.get(i)));
				} catch (CorruptDataException cde) {
					_out.println("Invalid Object");
				}
			}
		}
		
		public void finishVisit(J9ObjectPointer object, VoidPointer address) {
			_scanStack.pop();
		}
	}
	
	private class RootPathCommandListener implements IEventListener
	{
		public boolean _corruptionFound = false;
		
		public void corruptData(String message, CorruptDataException e, boolean fatal) 
		{
			_corruptionFound = true;
			if (_verboseEnabled) {
				/* re-using DefaultListener to log error */
				_listener.corruptData(message, e, fatal);
			} 
		}
	}

	public void run(String command, String[] args, Context context,
			final PrintStream out) throws DDRInteractiveCommandException 
	{
		switch (args.length) {
		case 0:
			if (args.length == 0) {
				if (command.equals("!rootpathverbose")) {
					out.println("verbose output enabled for rootpath command.  run !rootpathnoverbose to disable verbose output");
					_verboseEnabled = true;
				} else if (command.equals("!rootpathnoverbose")) {
					out.println("verbose output disabled for rootpath command.  run !rootpathverbose to enable verbose output");
					_verboseEnabled = false;
				} else {
					throw new UnsupportedOperationException("Unrecognized command passed to RoothPathCommand");
				}
			}
			break;
		case 1:
			long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
			final J9ObjectPointer objectToFind = J9ObjectPointer.cast(address);
			
			try {
				J9ClassPointer clazz = J9ObjectHelper.clazz(objectToFind);
				if (!J9ClassHelper.hasValidEyeCatcher(clazz)) {
					throw new DDRInteractiveCommandException("object class is not valid (eyecatcher is not 0x99669966)");
				}
			} catch (CorruptDataException cde) {
				throw new DDRInteractiveCommandException("memory fault de-referencing address argument", cde); 
			}
			
			try {
				/* Create a custom event listener so that CorruptDataExceptions that are found
				 * while iterating through the live set aren't all printed to the output stream
				 */
				RootPathCommandListener listener = new RootPathCommandListener();
				EventManager.register(listener);
				
				if (command.equals("!rootpathfindall") || command.equals("!strongrootpathfindall")) {
					LiveSetWalker.walkLiveSet(new RootPathsFinder(objectToFind, out), RootSetType.STRONG_REACHABLE);
				} else if (command.equals("!anyrootpathfindall")) {
					LiveSetWalker.walkLiveSet(new RootPathsFinder(objectToFind, out), RootSetType.ALL);
				} else if (command.equals("!weakrootpathfindall")) {
					LiveSetWalker.walkLiveSet(new RootPathsFinder(objectToFind, out), RootSetType.WEAK_REACHABLE);
				} else if (command.equals("!rootpathfind") || command.equals("!strongrootpathfind")) {
					RootPathFinder pathFinder = new RootPathFinder(objectToFind, out);
					LiveSetWalker.walkLiveSet(pathFinder, RootSetType.STRONG_REACHABLE);
					if (!pathFinder._pathFound) {
						out.println("No paths from roots found");
					}
				} else if (command.equals("!anyrootpathfind")) {
					RootPathFinder pathFinder = new RootPathFinder(objectToFind, out);
					LiveSetWalker.walkLiveSet(pathFinder, RootSetType.ALL);
					if (!pathFinder._pathFound) {
						out.println("No paths from roots found");
					}
				} else if (command.equals("!weakrootpathfindall")) {
					RootPathFinder pathFinder = new RootPathFinder(objectToFind, out);
					LiveSetWalker.walkLiveSet(pathFinder, RootSetType.WEAK_REACHABLE);
					if (!pathFinder._pathFound) {
						out.println("No paths from roots found");
					}
				} else if (command.equals("!isobjectalive")) {
					ObjectFinderVisitor objectFinder = new ObjectFinderVisitor(objectToFind);
					LiveSetWalker.walkLiveSet(objectFinder);
					if (objectFinder._objectFound) {
						out.println("Object is live");
					} else {
						out.println("Object is not live");
					}
				}
				
				if (listener._corruptionFound) {
					out.println("Corruption detected walking live set");
					if (!_verboseEnabled) {		
						out.println("run !rootpathverbose and re-run command to see corruptions");
					}
				}
				EventManager.unregister(listener);
			} catch (CorruptDataException cde) {
				throw new DDRInteractiveCommandException("Memory fault while walking live set", cde);
			}
			break;
			
			default:
				throw new DDRInteractiveCommandException("Invalid number of arguments");
		}
		
		
	}
}
