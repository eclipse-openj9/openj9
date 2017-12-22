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
package com.ibm.j9ddr.util;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.NoSuchElementException;

/**
 * Utility functions for working with iterators
 */
public class IteratorHelpers
{
	
	/**
	 * Takes a list of iterators and returns one iterator that iterates over each iterator in turn.
	 */
	public static <T> Iterator<T> combineIterators(final Iterator<T> ... iterators)
	{
		return new Iterator<T>(){

			private int index = 0;
			
			public boolean hasNext()
			{
				do {
					if (index < iterators.length) {
						if (iterators[index].hasNext()) {
							return true;
						} else {
							index++;
							continue;
						}
					} else {
						return false;
					}
				} while (true);
			}

			public T next()
			{
				if (hasNext()) {
					return iterators[index].next();
				} else {
					throw new NoSuchElementException();
				}
			}

			public void remove()
			{
				throw new UnsupportedOperationException();
			}};
	}
	
	public static <T> List<T> toList(Iterator<T> it)
	{
		ArrayList<T> list = new ArrayList<T>();
		
		while(it.hasNext()) {
			list.add(it.next());
		}
		return list;
	}
	
	/**
	 * Filters an iterator
	 * @param <T> Generic type of iterator
	 * @param it Iterator to be filtered
	 * @param filter Filter object to select which objects to be passed through
	 * @return Filtered iterator
	 */
	public static <T> Iterator<T> filterIterator(final Iterator<T> it, final IteratorFilter<T> filter)
	{
		return new Iterator<T>() {
			
			private T current;

			public boolean hasNext()
			{
				if (current != null) {
					return true;
				}
				
				while (current == null && it.hasNext()) {
					T possible = it.next();
					
					if (filter.accept(possible)) {
						current = possible;
						return true;
					}
				}
				
				return false;
			}

			public T next()
			{
				if (hasNext()) {
					T toReturn = current;
					current = null;
					return toReturn;
				} else {
					throw new NoSuchElementException();
				}
			}

			public void remove()
			{
				throw new UnsupportedOperationException();
			}};
	}
	
	/**
	 * Interface for filtering iterators
	 */
	public static interface IteratorFilter<T>
	{
		/**
		 * 
		 * @param obj Object under test
		 * @return True if this object passed the filter (and should be included in the iterator), false otherwise
		 */
		public boolean accept(T obj);
	}
}
