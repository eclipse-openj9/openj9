/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.helpers.ClassOutput;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class XJCommand extends XCommand {
	{
		addCommand("x/j", "<object address> | <class name>", "displays information about a particular object or all objects of a class");	
	}
	
	
	@Override
	public boolean recognises(String command, IContext context) {
		if(super.recognises(command, context)) {
			return command.toLowerCase().endsWith("j");
		}
		return false;
	}
	
	public void doCommand(String[] args)
	{
		boolean supers = false;
		String param = args[0];
		Long objAddress;
		String objName;
		
		
		objAddress = Utils.longFromStringWithPrefix(param);
		if (null == objAddress)
		{
			objName = param;

			if (args.length >= 2)
			{
				String option = args[1];
				if (option.equals("super"))
				{
					supers = true;
				}
				else if (option.equals("nosuper"))
				{
					supers = false;
				}
				else
				{
					out.println("second parameter for \"x/j\" must be \"super\" or" +
							" \"nosuper\"");
					return;
				}
			}
		} else {
			objName = null;
		}

		printHeapObjects(objAddress, objName, out, supers);
	
	}
	
	private void printHeapObjects(Long objAddress, String objName,
			PrintStream out, boolean supers)
	{
		JavaRuntime jr = ctx.getRuntime(); 
		JavaHeap jh;
		Iterator<?> itHeap = jr.getHeaps();
		int count = 1;
		
		while (itHeap.hasNext()) {
			Object obj = itHeap.next();
			if(obj instanceof JavaHeap) {
				jh = (JavaHeap)obj;
			
				out.print("\t heap #" + count + " - name: ");
				out.print(jh.getName());
				out.print("\n\n");
			
				printObjects(jh, objAddress, objName, out, supers, jr);
				count++;
			} else {
				out.println("\t\tWarning : skipping corrupt heap");
			}
		}
	}
	
	private void printObjects(JavaHeap jh, Long objAddress, String objName, PrintStream out,
			boolean supers, JavaRuntime jr) {
		if( objName !=  null  ) {
			printObjectsFromName(jh, objName, out, supers, jr);
		} else {
			printObjectsFromAddress(jh, objAddress, out, jr);
		}
	}
	
	private void printObjectsFromName(JavaHeap jh, String objName, PrintStream out,
			boolean supers, JavaRuntime jr)
	{

		if (objName != null)
		{
			JavaClass[] classes = Utils.getClassGivenName(objName, jr, out);
			
			// if we couldn't find a class of that name, return; the passed in class name could
			//  still be an array type or it might not exist
			if( classes == null || classes.length == 0 ) {						
				out.print("\t  could not find class with name \"" + objName + "\"\n\n");
				return;
			}
			for( int i = 0; i < classes.length; i++ ) {
				boolean foundInstance = false;
				JavaClass objClass = classes[i];
				if( classes.length > 1 ) {
					ClassOutput.printRuntimeClassAndLoader(objClass, out);
				}
				ClassOutput.printStaticFields(objClass, out);

				Iterator<?> itObject = jh.getObjects();
				int corruptObjectCount = 0;
				while (itObject.hasNext()) {
					Object obj = itObject.next();
					if(obj instanceof CorruptData) {
						//skip the corrupt object
						corruptObjectCount++;
						continue;		
					}
					JavaObject jo = (JavaObject)obj;
					String hierarchy = "";
					JavaClass jc;

					try {
						jc = jo.getJavaClass();
					} catch (CorruptDataException e) {
						out.print("\t  <error getting class while traversing objects: ");
						out.print(Exceptions.getCorruptDataExceptionString());
						out.print(">\n");
						jc = null;
					}

					boolean foundSuperclass = false;
					while (jc != null && !foundSuperclass) {
						String className;
						try {
							className = jc.getName();
						} catch (CorruptDataException e) {
							out.print("\t  <error getting class name while traversing objects: ");
							out.print(Exceptions.getCorruptDataExceptionString());
							out.print(">\n");
							jc = null;
							continue;
						}

						if (hierarchy.equals("")) {
							hierarchy = className;
						} else {
							hierarchy = className + " => " + hierarchy;
						}

						if (jc.equals(objClass)) {
							foundInstance = true;
							foundSuperclass = true;
							out.print("\t  ");
							out.print(hierarchy);
							out.print(" @ ");
							out.print(Utils.toHex(jo.getID().getAddress()));
							out.print("\n");
							
							ClassOutput.printFields(jo, jc, jr, out);
							printReferences(jo, out);
						} else {
							if (supers)	{
								try {
									jc = jc.getSuperclass();
								} catch (CorruptDataException e) {
									out.print("\t  <error getting superclass while traversing objects: ");
									out.print(Exceptions.getCorruptDataExceptionString());
									out.print(">\n");
									jc = null;
								}
							} else {
								jc = null;
							}
						}
					}
				}
				if (!foundInstance) {
					out.print("\t  <no object of class \"");
					out.print(objName);
					out.print("\" exists>\n\n");
				}
				if(corruptObjectCount != 0) {
					out.println("\t Warning : " + corruptObjectCount + " corrupt objects were skipped\n");
				}
			}
		}
	}

	private void printObjectsFromAddress(JavaHeap jh, Long objAddress, PrintStream out,
			JavaRuntime jr)
	{
		JavaObject jo;
		Iterator<?> itObject = jh.getObjects();
		boolean found = false;
	
		boolean done = false;
		
		int corruptObjectCount = 0;
		while (itObject.hasNext() && !done)
		{
			Object obj = itObject.next();
			if(obj instanceof CorruptData) {
				//skip the corrupt object
				corruptObjectCount++;
				continue;		
			}
			
			jo = (JavaObject)obj;


			if (jo.getID().getAddress() == objAddress.longValue())
			{
				JavaClass jc;

				found = true;
				out.print("\t  ");
				try {
					jc = jo.getJavaClass();
				} catch (CorruptDataException e) {
					out.print("\t  <error getting class while traversing objects: ");
					out.print(Exceptions.getCorruptDataExceptionString());
					out.print(">");
					jc = null;
				}

				if (null != jc)
				{
					try {
						out.print(jc.getName());
					} catch (CorruptDataException e) {
						out.print("\t  <error getting class name while traversing objects: ");
						out.print(Exceptions.getCorruptDataExceptionString());
						out.print(">");
					}
					out.print(" @ ");
					out.print(Utils.toHex(objAddress.longValue()));
					out.print("\n");
					
					ClassOutput.printFields(jo, jc, jr, out);
					printReferences(jo, out);

					done = true;	// assumes only one object can exist at a specific memory address
				}
			}
		}

		if (!found)
		{
			out.print("\t  <no object found at address ");
			out.print(Utils.toHex(objAddress.longValue()));
			out.print(">\n\n");
		}
		if(corruptObjectCount != 0) {
			out.println("\t Warning : " + corruptObjectCount + " corrupt objects were skipped\n");
		}
	}
	
	
	@Override
	public void printDetailedHelp(PrintStream out) {
		super.printDetailedHelp(out);
		out.println("displays information about a particular object or all " +
				"objects of a class\n\n" +
				
				"parameters: 0x<object_addr> | <class_name> [super | nosuper]\n\n" +
				"If given class name, all static fields with their values will be " +
				"printed, followed by all objects of that class with their fields " +
				"and values for each class of that name (if multiple classloaders " +
				"have loaded classes with that name).\n\n" +
				"If \"super\" is specifed with \"x/j < class_name>\" it is interpreted as a superclass name, and all instances of any subclasses will be printed.\n\n" +
				"If given an object address (in hex), static fields for that object's " +
				"class will not be printed; the other fields and values of that object " +
				"will be printed along with its address.\n\n" +
				"Note: this command ignores the number of items and unit size passed " +
				"to it by the \"x/\" command.\n\n"
		);
	}
	
	/**
	 * Print the references from the given object. Omit the first reference: this is always a reference to the 
	 * object's class. 
	 */
	public static void printReferences(JavaObject jo, PrintStream out) {
		Iterator<?> references = jo.getReferences();
		if (references.hasNext()) {			
			references.next(); // reference to the class, ignore this one
		}
		if ( ! references.hasNext()) {
			out.print("\t    references: <none>\n ");
			out.print("\t     ");			
		} else {
			out.print("\t    references:\n ");
			out.print("\t     ");
			while (references.hasNext()) {
				Object potential_reference = references.next();
				if (potential_reference instanceof JavaReference) {
					JavaReference reference = (JavaReference) potential_reference; 
					try {
						Object target = reference.getTarget();
						if (target instanceof JavaObject) {
							out.print(" 0x" +   Long.toHexString(((JavaObject)target).getID().getAddress()));
						} else if (target instanceof JavaClass) {
							out.print(" 0x" +   Long.toHexString(((JavaClass)target).getID().getAddress()));
						}
					} catch (DataUnavailable e) {
						// don't print anything
					} catch (CorruptDataException e) {
						out.print(Exceptions.getCorruptDataExceptionString());
					}
				}

			}
		}
		out.print("\n\n");
	}

}
