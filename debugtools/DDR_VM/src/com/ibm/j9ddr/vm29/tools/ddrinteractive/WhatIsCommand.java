/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.GeneratedFieldAccessor;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ClassIterator;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Implements !whatis and !whatissetdepth
 * 
 * @author andhall
 *
 */
public class WhatIsCommand extends Command
{
	
	private static final int DEFAULT_MAXIMUM_DEPTH = 6;
	public static final String WHATIS_COMMAND = "!whatis";
	public static final String WHATIS_SET_DEPTH_COMMAND = "!whatissetdepth";
	
	private int maxDepth = DEFAULT_MAXIMUM_DEPTH;
	
	private UDATA searchValue;
	
	private int skipCount = 0;
	private int foundCount = 0;
	private long fieldCount = 0;
	
	private static final Map<Class<?>, Method[]> fieldAccessorMap = new HashMap<Class<?>, Method[]>();
	
	private UDATA closestBelow;
	private SearchStack closestBelowStack;
	
	private UDATA closestAbove;
	private SearchStack closestAboveStack;
	
	private UDATA shortestHammingDistance;
	private SearchStack shortestHammingDistanceStack;
	private int hammingDistance;
	
	private PrintStream out;
	
	public WhatIsCommand()
	{
		addCommand("whatis", "<address>", "Recursively searches fields for UDATA value" );
		addCommand("whatissetdepth", "<n>", "Sets the maximum depth of the whatis search" );

	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.tools.ddrinteractive.ICommand#run(java.lang.String, java.lang.String[], com.ibm.j9ddr.tools.ddrinteractive.Context, java.io.PrintStream)
	 */
	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException
	{
		this.out = out;
		
		if (command.equals(WHATIS_COMMAND)) {
			runWhatIs(args, context, out);
		} else if (command.equals(WHATIS_SET_DEPTH_COMMAND)) {
			runWhatIsSetDepth(args, context, out);
		} else {
			throw new DDRInteractiveCommandException("WhatIsCommand plugin does not recogise command: " + command);
		}
	}

	private void runWhatIsSetDepth(String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException
	{	
		if (args.length == 0) {
			out.println("No argument supplied.");
			return;
		}
		
		int depth = 0;
		try {
			depth = Integer.parseInt(args[0]);
		} catch (NumberFormatException ex) {
			out.println("Could not format " + args[0] + " as an integer");
			return;
		}
		
		if (depth <= 0) {
			out.println("Depth must be > 0");
			return;
		}
		
		maxDepth = depth;
		
		out.println("Max depth set to " + maxDepth);
	}

	private void runWhatIs(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		if (args.length == 0) {
			badOrMissingSearchValue(out);
			return;
		}
		
		long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
		
		UDATA localSearchValue = new UDATA(address);
		
		if (localSearchValue.eq(0)) {
			badOrMissingSearchValue(out);
			return;
		}
		
		if (searchValue == null || ! searchValue.eq(localSearchValue)) {
			searchValue = localSearchValue;
		} else {
			out.println("Skip count now " + (++skipCount) + ". Run !whatis 0 to reset it.");
		}
	
		resetFieldData();
		
		long startTime = System.currentTimeMillis();
		
		//Walk from the VM
		J9JavaVMPointer vm = null;
		try {
			vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException("Couldn't get VM", e);
		}
		boolean found = walkStructuresFrom(vm);
		
		//Walk from each VM thread
		if (! found) {
			try {
				J9VMThreadPointer mainThread = vm.mainThread();
				List<J9VMThreadPointer> threads = new LinkedList<J9VMThreadPointer>();
				if (mainThread.notNull()) {
					J9VMThreadPointer threadCursor = vm.mainThread();
		
					do {
						threads.add(threadCursor);
						
						threadCursor = threadCursor.linkNext();
					} while (!threadCursor.eq(mainThread) && ! found);
					
					/* Walk the thread list backwards so we will find the match next to the closest thread (prevents walkStructures from doing anything useful with the linkNext list) */
					Collections.reverse(threads);
					
					for (J9VMThreadPointer thisThread : threads) {
						found = walkStructuresFrom(thisThread);
						
						if (found) {
							break;
						}
					}
					
				}
			} catch (CorruptDataException e) {
				out.println("CDE walking thread list.");
				e.printStackTrace(out);
			}
		}
		
		//Walk from each class
		if (! found) {
			try {
				GCClassLoaderIterator it = GCClassLoaderIterator.from();
				
				OUTER: while (it.hasNext()) {
					J9ClassLoaderPointer loader = it.next();
					
					Iterator<J9ClassPointer> classIt = ClassIterator.fromJ9Classloader(loader);
					
					while (classIt.hasNext()) {
						J9ClassPointer clazz = classIt.next();
						
						found = walkStructuresFrom(clazz);
						
						if (found) {
							break OUTER;
						}
					}
					
				}
			} catch (CorruptDataException e) {
				out.println("CDE walking classes.");
				e.printStackTrace(out);
			}
		}
		
		long stopTime = System.currentTimeMillis();
		
		if (found) {
			out.println("Match found");
		} else {
			out.println("No match found");
			
			if (closestAboveStack != null) {
				out.print("Closest above was: ");
				closestAboveStack.dump(out);
				out.print(" at " + closestAbove.getHexValue());
				out.println();
			} else {
				out.println("No values found above search value");
			}
			
			if (closestBelowStack != null) {
				out.print("Closest below was: ");
				closestBelowStack.dump(out);
				out.print(" at " + closestBelow.getHexValue());
				out.println();
			} else {
				out.println("No values found below search value");
			}
			
			if (shortestHammingDistanceStack != null) {
				out.print("Value with shortest hamming distance (fewest single-bit changes required) was: ");
				shortestHammingDistanceStack.dump(out);
				out.print(" at " + shortestHammingDistance.getHexValue());
				out.print(". Hamming distance = " + hammingDistance);
				out.println();
			}
			
			/* Reset search value - so if someone reruns the same (unsuccessful) search again it won't set skipCount to 1 */
			searchValue = null;
		}
		
		out.println("Searched " + fieldCount + " fields to a depth of " + maxDepth + " in " + (stopTime - startTime) + " ms");
		resetFieldData();
	}

	private void resetFieldData()
	{
		foundCount = 0;
		fieldCount = 0;
		closestAbove = new UDATA(-1);
		closestAboveStack = null;
		closestBelow = new UDATA(0);
		closestBelowStack = null;
		shortestHammingDistance = new UDATA(0);
		shortestHammingDistanceStack = null;
		hammingDistance = Integer.MAX_VALUE;
		/* Clear the fieldAccessorMap to avoid hogging memory */
		fieldAccessorMap.clear();
	}
	
	private boolean walkStructuresFrom(StructurePointer startPoint) throws DDRInteractiveCommandException
	{
		Set<AbstractPointer> walked = new HashSet<AbstractPointer>();
		SearchStack searchStack = new SearchStack(maxDepth);
		
		if (UDATA.cast(startPoint).eq(searchValue)) {
			out.println("Found " + searchValue.getHexValue() + " as " + startPoint.formatShortInteractive());
			return true;
		}
		
		/* Seed with startPoint */
		searchStack.push(new SearchFrame(startPoint));
		walked.add(startPoint);
		
		boolean found = false;
		
		while (! searchStack.isEmpty() && !found) {
			SearchFrame current = searchStack.peek();
			
			int fieldIndex = current.fieldIndex++;
			
			if (current.fieldAccessors.length <= fieldIndex) {
				//We've walked all the fields on this object
				searchStack.pop();
				continue;
			}
			
			try {
				current.fieldName = current.fieldAccessors[fieldIndex].getName();
				Object result = current.fieldAccessors[fieldIndex].invoke(current.ptr);
				if (result == null) {
					continue;
				}
				fieldCount++;
				
				if (result instanceof StructurePointer) {
					StructurePointer ptr = (StructurePointer) result;
					
					found = checkPointer(searchStack, ptr);
					
					if (! searchStack.isFull()&& ! walked.contains(ptr)) {
						walked.add(ptr);
						searchStack.push(new SearchFrame(ptr));
					}
				} else if (result instanceof AbstractPointer) {
					AbstractPointer ptr = (AbstractPointer) result;
					
					found = checkPointer(searchStack, ptr);
				} else if (result instanceof Scalar) {
					Scalar s = (Scalar) result;
					
					found = checkScalar(searchStack, s);
				} else {
					out.println("Unexpected type walked: " + result.getClass().getName());
					continue;
				}
			} catch (InvocationTargetException e) {
				Throwable cause = e.getCause();
				
				if (cause instanceof CorruptDataException || cause instanceof NoSuchFieldError || cause instanceof NoClassDefFoundError) {
					//Skip this field
					continue;
				} else {
					throw new DDRInteractiveCommandException("Unexpected exception during walk",cause);
				}
			} catch (Exception e) {
				throw new DDRInteractiveCommandException("Unexpected exception during !whatis walk",e);
			}
		}
		
		return found;
	}

	private boolean checkPointer(SearchStack searchStack, AbstractPointer ptr)
	{
		UDATA cmpValue = UDATA.cast(ptr);
		if (searchValue.eq(cmpValue)) {
			if (++foundCount > skipCount) {
				out.print("Found " + searchValue.getHexValue() + " as " + ptr.formatShortInteractive() + ": ");
				searchStack.dump(out);
				out.println();
				return true;
			}
		} else {
			updateClosest(searchStack, cmpValue);
		}
		
		return false;
	}

	private boolean checkScalar(SearchStack searchStack, Scalar s)
	{
		UDATA cmpValue = new UDATA(s);
		if (searchValue.eq(s)) {
			if (++foundCount > skipCount) {
				out.print("Found " + searchValue.getHexValue() + " as " + s + ": ");
				searchStack.dump(out);
				out.println();
				return true;
			}
		} else {
			updateClosest(searchStack, cmpValue);
		}
		
		return false;
	}
	
	private void updateClosest(SearchStack searchStack, UDATA value)
	{
		if (value.gt(searchValue)) {
			if (value.lt(closestAbove)) {
				closestAbove = value;
				closestAboveStack = searchStack.copy();
			}
		} else {
			if (value.gt(closestBelow)) {
				closestBelow = value;
				closestBelowStack = searchStack.copy();
			}
		}
		
		int hd = hammingDistance(searchValue, value);
		if (hd < hammingDistance) {
			shortestHammingDistance = value;
			shortestHammingDistanceStack = searchStack.copy();
			hammingDistance = hd;
		}
	}
	
	private static int hammingDistance(UDATA v1, UDATA v2)
	{
		long differences = v1.longValue() ^ v2.longValue();
		
		int hammingDistance = 0;
		
		while (differences != 0) {
			if ( (differences & 1) == 1 ) {
				hammingDistance++;
			}
			
			differences >>>= 1;
		}
		
		return hammingDistance;
	}
	
	static final class SearchFrame
	{
		final StructurePointer ptr;
		final Method[] fieldAccessors;
		String fieldName = null;
		int fieldIndex = 0;
		
		SearchFrame(StructurePointer ptr)
		{
			this.ptr = ptr;
			this.fieldAccessors = getFieldAccessors();
		}
		
		private SearchFrame(StructurePointer ptr, Method[] fieldAccessors, String fieldName, int fieldIndex)
		{
			this.ptr = ptr;
			this.fieldAccessors = fieldAccessors;
			this.fieldName = fieldName;
			this.fieldIndex = fieldIndex;
		}
		
		private Method[] getFieldAccessors()
		{
			Class<? extends StructurePointer> ptrClass = ptr.getClass();
			
			if (fieldAccessorMap.containsKey(ptrClass)) {
				return fieldAccessorMap.get(ptrClass);
			} else {
				Method[] allMethods = ptr.getClass().getMethods();
				List<Method> fieldAccessors = new LinkedList<Method>();
				
				for (Method m : allMethods) {
					if (m.isAnnotationPresent(GeneratedFieldAccessor.class)) {
						fieldAccessors.add(m);
					}
				}
				
				Method[] accessorFieldArray = fieldAccessors.toArray(new Method[fieldAccessors.size()]);
				fieldAccessorMap.put(ptrClass, accessorFieldArray);
				
				return accessorFieldArray;
			}
		}
		
		public SearchFrame copy()
		{
			return new SearchFrame(ptr, fieldAccessors, fieldName, fieldIndex);
		}
	}
	
	static final class SearchStack
	{
		private final SearchFrame[] storage;
		private int stackTop;
		
		public SearchStack(int capacity)
		{
			storage = new SearchFrame[capacity];
			stackTop = 0;
		}
		
		private SearchStack(SearchFrame[]storage, int stackTop)
		{
			this.storage = storage;
			this.stackTop = stackTop;
		}
		
		public SearchFrame pop()
		{
			return storage[--stackTop];
		}
		
		public SearchFrame peek()
		{
			return storage[stackTop - 1];
		}
		
		public void push(SearchFrame obj)
		{
			storage[stackTop++] = obj;
		}
		
		public boolean isEmpty()
		{
			return stackTop <= 0;
		}
		
		public boolean isFull()
		{
			return stackTop >= storage.length;
		}
		
		public void dump(PrintStream out)
		{
			out.print(storage[0].ptr.formatShortInteractive());
			
			for (int i=0; i < stackTop ; i++) {
				out.print("->");
				out.print(storage[i].fieldName);
			}
		}
		
		public SearchStack copy()
		{
			SearchFrame[] copy = new SearchFrame[storage.length];
			
			for (int i=0; i < copy.length; i++) {
				if (storage[i] != null) {
					copy[i] = storage[i].copy();
				}
			}
			
			return new SearchStack(copy, stackTop);
		}
	}

	private void badOrMissingSearchValue(PrintStream out)
	{
		out.println("Bad or missing search value. Skip count reset to 0.");
		skipCount = 0;
		searchValue = null;
	}

}
