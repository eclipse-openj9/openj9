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
package com.ibm.jvm.dtfjview.commands.infocommands;

import java.io.PrintStream;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.helpers.ClassOutput;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class InfoClassCommand extends BaseJdmpviewCommand {
	/**
	 * Cache of all classes and their respective ClassStatistics, organized by JavaRuntime.
	 */
	private static Map<JavaRuntime, Map<JavaClass, ClassStatistics>> classInstanceCounts;
	
	{
		addCommand("info class", "[Java class name] [-sort:<name|count|size>]", "Provides information about the specified Java class");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		String className = null;
		Comparator<JavaClass> sortOrder = new ClassNameComparator();
		
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if (classInstanceCounts == null) {
			cacheRuntimeClasses();
			countClassInstances();
		}
		for( String arg: args) {
			if (Utils.SORT_BY_SIZE_FLAG.equals(arg)) {
				sortOrder = new TotalSizeComparator();
			} else if (Utils.SORT_BY_COUNT_FLAG.equals(arg)) {
				sortOrder = new InstanceCountComparator();
			} else if (Utils.SORT_BY_NAME_FLAG.equals(arg)) {
				sortOrder = new ClassNameComparator();
			} else {
				className = arg;
			}
		}
		
		if( className == null ) {
			printAllRuntimeClasses(sortOrder);
		} else {
			printSingleRuntimeClassInfo(className);
		}

	}
	
	private void printSingleRuntimeClassInfo(String className)
	{
		JavaRuntime jr = ctx.getRuntime();
		Long objAddress = Utils.longFromStringWithPrefix(className);
		JavaClass[] classes = null;
		if( objAddress != null  ) {
			JavaClass jc = Utils.getClassGivenAddress(objAddress.longValue(), jr);
			if( jc != null ) {
				classes = new JavaClass[1];
				classes[0] = jc;
			}
		} else {
			classes = Utils.getClassGivenName(className, jr, out);
		}
		
		// if we couldn't find a class of that name, return; the passed in class name could
		//  still be an array type or it might not exist
		if (null == classes || classes.length == 0 ) {
			out.print("\t  could not find class with name \"" + className + "\"\n\n");
		} else if ( classes.length == 1 ) {
			JavaClass jc = classes[0];
			printClassDetails(jr, className, jc);
		} else {
			out.println("name = " + className + " found " + classes.length + " on different class loaders");
			for( int i = 0; i < classes.length; i++ ) {
				JavaClass jc = classes[i];
				 ClassOutput.printRuntimeClassAndLoader(jc, out);
			}
			out.println("Use info class with the class ID to print out information on a specific class.");
			out.println("For example: info class 0x" + Long.toHexString(classes[0].getID().getAddress()));
		}
	}

	private void printClassDetails(JavaRuntime jr, String className, JavaClass jc) {
		String spaces = "    ";
		String cdeInfo = "N/A (CorruptDataException occurred)";
		try {
			out.print("name = " + jc.getName());
		} catch (CorruptDataException dce){
			out.print("name = " + cdeInfo);
		}
		out.print("\n\n\t");
		out.print("ID = " + Utils.toHex(jc.getID()));

		String superClassId;
		try{
			JavaClass superClass = jc.getSuperclass();
			if (null == superClass) {
				superClassId = "<no superclass>";
			} else {
				superClassId = Utils.toHex(superClass.getID());
			}
		}catch (CorruptDataException dce){
			superClassId = cdeInfo;
		}
		out.print(spaces);
		out.print("superID = " + superClassId);

		// Omitting size of class because that might differ between instances (i.e. arrays)

		String classLoaderId;
		try{
			JavaClassLoader jClassLoader = jc.getClassLoader();
			JavaObject jo = jClassLoader.getObject();
			if (jo != null) {
				classLoaderId = Utils.toHex(jo.getID());
			} else {
				classLoaderId = "<data unavailable>";
			}
		}catch (CorruptDataException cde){
			classLoaderId = cdeInfo;
		}
		out.print(spaces);
		out.print("\n\t");
		out.print("classLoader = " + classLoaderId);

		String modifiersInfo;
		try{
			modifiersInfo = Utils.getClassModifierString(jc);
		} catch (CorruptDataException cde) {
			modifiersInfo = cdeInfo;
		}
		out.print(spaces);
		out.print("modifiers: " + modifiersInfo);
		out.print("\n\n");

		ClassStatistics d = getClassStatisticsFor(jr, jc);

		out.print("\tnumber of instances:     " + d.getCount() + "\n");
		out.print("\ttotal size of instances on the heap: " + d.getSize() + " bytes");

		out.print("\n\n");

		printClassHierarchy(jc);
		out.print("\n");
		printFields(jc);
		out.print("\n");
		printMethods(jc);
	}

	private void printClassHierarchy(JavaClass jClass) {
		Stack<String> stack = new Stack<String>();
		while (null != jClass){
			try{
				stack.add(jClass.getName());
				jClass = jClass.getSuperclass(); 
			}catch(CorruptDataException cde){
				stack.add("N/A (CorruptDataException occurred)");
				break;
			}
		}
		printStack(stack);
	}
	
	private void printStack(Stack<String> stack){
		out.print("Inheritance chain....\n\n");
		String tab = "\t";
		String spaces = "";
		while(stack.size() > 0){
			out.print(tab + spaces + stack.pop() + "\n");
			spaces += "   ";
		}
	}
	
	private void printFields(JavaClass jClass) {
		out.print("Fields......\n\n");
		ClassOutput.printStaticFields(jClass, out);
		ClassOutput.printNonStaticFields(jClass, out);
	}
	
	private void printMethods(JavaClass jClass){
		out.print("Methods......\n\n");
		ClassOutput.printMethods(jClass.getDeclaredMethods(), out);
		out.print("\n");
	}


	/**
	 * Pre-populates the classInstanceCounts map with just classes, and empty ClassStatistics objects.
	 * The key-set of classInstanceCounts.get(someRuntime) can be then treated as a cache of all classes for that runtime,
	 * eliminating the need to iterate over all class loaders (see getRuntimeClasses()).
	 * @param jr
	 * @param errOut
	 * @return
	 */
	private void cacheRuntimeClasses() {

		classInstanceCounts = new HashMap<JavaRuntime, Map<JavaClass,ClassStatistics>>();
		long corruptClassCount = 0;
		
		Map<JavaClass, ClassStatistics> classesOfThisRuntime = new HashMap<JavaClass, ClassStatistics>();
		JavaRuntime runtime = ctx.getRuntime();
		classInstanceCounts.put(runtime, classesOfThisRuntime);

		Iterator itClassLoader = runtime.getJavaClassLoaders();

		while (itClassLoader.hasNext()) {
			JavaClassLoader jcl = (JavaClassLoader)itClassLoader.next();
			
			Iterator itClass = jcl.getDefinedClasses();
			while (itClass.hasNext()) {
				Object obj = itClass.next();
				if(obj instanceof JavaClass) {
					//check that a cast can be made to JavaClass
					classesOfThisRuntime.put((JavaClass)obj, new ClassStatistics());
				} else {
					corruptClassCount++;
				}
			}
		}
		if (corruptClassCount > 0) {
			out.print("Warning, found " + corruptClassCount + " corrupt classes during classloader walk\n");
		}
	}
	
	private void printAllRuntimeClasses(Comparator<JavaClass> sortOrder) {
		JavaRuntime jr = ctx.getRuntime();
		Collection<JavaClass> javaClasses = getRuntimeClasses(jr);
		
		long objCount = 0;
		long totalSize = 0;
		Iterator<JavaClass> itClass;
		if( sortOrder == null ) {
			itClass = javaClasses.iterator();
		} else {
			List<JavaClass> sortedList = new LinkedList<JavaClass>();
			Iterator<JavaClass> itr = javaClasses.iterator();
			while (itr.hasNext()) {
				sortedList.add(itr.next());
			}
			Collections.sort(sortedList, sortOrder);
			itClass = sortedList.iterator();
		}
		if (itClass.hasNext()) {
			printClassListHeader();
		} else {
			out.print("\n\t No information found for loaded classes\n");
		}
		while (itClass.hasNext()) {
			JavaClass jc = itClass.next();
						
			ClassStatistics d = getClassStatisticsFor(jr, jc);
			totalSize += d.getSize();
			objCount += d.getCount();
			printOneClass(jc, d);
		}
		
		out.print("\n");
		out.print("\t Total number of objects: " + objCount + "\n");
		out.print("\t Total size of objects: " + totalSize + "\n");
	}
	
	private static Collection<JavaClass> getRuntimeClasses(JavaRuntime jr) {
		return classInstanceCounts.get(jr).keySet();
	}

	private ClassStatistics getClassStatisticsFor(JavaRuntime jr, JavaClass jc) {
		return classInstanceCounts.get(jr).get(jc);
	}

	private void countClassInstances() {
		JavaRuntime runtime = ctx.getRuntime();
		Map<JavaClass, ClassStatistics> thisRuntimeClasses = classInstanceCounts.get(runtime);
		final Collection<JavaClass> javaClasses = getRuntimeClasses(runtime);
		long corruptObjectCount = 0;
		long corruptClassCount = 0;
		long corruptClassNameCount = 0;
		
		Iterator itHeap = runtime.getHeaps();
		while (itHeap.hasNext()) {
			Object heap = itHeap.next();
			if(heap instanceof CorruptData) {
				out.println("[skipping corrupt heap]");
				continue;
			}
			JavaHeap jh = (JavaHeap)heap;
			Iterator itObject = jh.getObjects();
			
			// Walk through all objects in this heap, accumulating counts and total memory size by class
			while (itObject.hasNext()) {
				Object next = itObject.next();
				// Check that this is a JavaObject (there may be CorruptData objects in the 
				// JavaHeap, we don't attempt to count these as instances of known classes). 
				if (next instanceof JavaObject) {
					JavaObject jo = (JavaObject)next;
					ClassStatistics stats = null;
					
					try {
						// Check whether we found this class in the classloaders walk earlier
						JavaClass jc = jo.getJavaClass();
						if (javaClasses.contains(jc)) {
							stats = thisRuntimeClasses.get(jc);
						} else {
							// Class not found in the classloaders, create a statistic for it now, and issue a warning message
							stats = new ClassStatistics();
							thisRuntimeClasses.put(jc, stats);
							String classname;
							try {
								classname = jc.getName();
								out.println("Warning, class: " + classname + " found when walking the heap was missing from classloader walk");
							} catch (CorruptDataException cde) {
								corruptClassNameCount++;
							}
						}
						
						// Increment the statistic for objects of this class (accumulated count and size)
						stats.incrementCount();
						try {
							stats.addToSize(jo.getSize());
						} catch (CorruptDataException cde) {
							corruptObjectCount++; // bad size, count object as corrupt
						}
					} catch (CorruptDataException cde) {
						corruptClassCount++;
					}
				} else {
					corruptObjectCount++;
				}
			}
		}
		if (corruptObjectCount != 0) {
			out.println("Warning, found " + corruptObjectCount + " corrupt objects during heap walk");
		}
		if (corruptClassCount != 0) {
			out.println("Warning, found " + corruptClassCount + " corrupt class references during heap walk");
		}
		if (corruptClassNameCount != 0) {
			out.println("Warning, found " + corruptClassNameCount + " corrupt class names during heap walk");
		}
	}
	
	private void printClassListHeader() {
		out.print("\n" + Utils.prePadWithSpaces("instances", 16));
		out.print(Utils.prePadWithSpaces("total size on heap", 20));
		out.print("  class name");
		out.print("\n");
	}

	private void printOneClass(JavaClass jc, ClassStatistics datum){
		String className;
		try {
			className = jc.getName();
		} catch (CorruptDataException cde) {
			className = Exceptions.getCorruptDataExceptionString();
		}

		out.print(Utils.prePadWithSpaces(String.valueOf(datum.getCount()), 16));
		out.print(Utils.prePadWithSpaces(String.valueOf(datum.getSize()), 20));
		out.print("  " + className);
		out.print("\n");
	}
	
	public static class ClassStatistics {
		private int count;
		private long totalSize;
		
		public ClassStatistics(){
			this.count = 0;
			this.totalSize = 0;
		}
		
		public int getCount(){
			return this.count;
		}
		
		public long getSize(){
			return this.totalSize;
		}
		
		public void incrementCount(){
			this.count++;
		}
		
		public void addToSize(long sizeToAdd){
			this.totalSize += sizeToAdd;
		}
	}

	private static int cmp(long n1, long n2) {
		if( n1 == n2 ) {
			return 0;
		} else if( n1 > n2 ) {
			return 1;
		} else {
			return -1;
		}
	}
	
	private class TotalSizeComparator implements Comparator<JavaClass> {

		public int compare(JavaClass o1, JavaClass o2) {
			long s1 = getClassStatisticsFor(ctx.getRuntime(), o1).getSize();
			long s2 = getClassStatisticsFor(ctx.getRuntime(), o2).getSize();
			return cmp(s1, s2);
		}
	}
	
	private class InstanceCountComparator implements Comparator<JavaClass> {

		public int compare(JavaClass o1, JavaClass o2) {
			long s1 = getClassStatisticsFor(ctx.getRuntime(), o1).getCount();
			long s2 = getClassStatisticsFor(ctx.getRuntime(), o2).getCount();
			return cmp(s1, s2);
		}
	}
	
	private class ClassNameComparator implements Comparator<JavaClass> {

		public int compare(JavaClass o1, JavaClass o2) {
			String s1 = "";
			String s2 = "";
			try {
				s1 = o1.getName();
			} catch (CorruptDataException cde ) {
			}
			try {
				s2 = o2.getName();
			} catch (CorruptDataException cde ) {
			}
			return s1.compareTo(s2);
		}
	}
	
	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("Prints inheritance chain and other data for a given class\n\n" + 
					"Parameters: none, class name or ID, sort flags\n\n" +
					"If name or id parameters are omitted then \"info class\", prints the " +
					"number of instances of each class and the total size of all " +
					"instances of each class as well as the total number of instances " +
					"of all classes and the total size of all objects.\n" +
					"This output may be sorted using the " + Utils.SORT_BY_NAME_FLAG + ", " +
					Utils.SORT_BY_SIZE_FLAG + " or " + Utils.SORT_BY_COUNT_FLAG + " flags.\n\n" +
					"If a class name or hex address is passed to \"info class\", it prints " +
					"the following information about that class:\n" +
					"  - name\n" +
					"  - ID\n" +
					"  - superclass ID\n" +
					"  - class loader ID\n" +
					"  - modifiers\n" +
					"  - number of instances and total size of instances\n" +
					"  - inheritance chain\n" +
					"  - fields with modifiers (and values for static fields)\n" +
					"  - methods with modifiers\n" +
					"If multiple classes with the same name (on different class loaders) are found " +
					"then the class ID and class loader are printed for each class. Use info class " +
					"with the class ID to print out information on a specific class.");
		
	}
}
