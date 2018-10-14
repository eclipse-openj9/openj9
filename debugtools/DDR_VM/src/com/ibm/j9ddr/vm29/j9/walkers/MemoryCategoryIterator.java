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
package com.ibm.j9ddr.vm29.j9.walkers;

import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.util.IteratorHelpers;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9PortControlDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PortLibraryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OMRMemCategoryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OMRMemCategorySetPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OMRPortLibraryGlobalDataPointer;
import com.ibm.j9ddr.vm29.pointer.helper.OMRMemCategoryHelper;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Walker for J9MemCategories
 */
public abstract class MemoryCategoryIterator implements Iterator<OMRMemCategoryPointer> {

	/**
	 * Implementation that walks the memory category set registered with the port library, then over
	 * the categories stored in the port library.
	 */
	private static class MemoryCategorySetIterator extends MemoryCategoryIterator {
		private int index = 0;
		private final OMRMemCategorySetPointer categorySet;
		private OMRMemCategoryPointer next;

		MemoryCategorySetIterator(OMRMemCategorySetPointer categorySet) throws CorruptDataException {
			this.categorySet = categorySet;
		}

		public boolean hasNext() {
			if (next != null) {
				return true;
			}

			next = internalNext();

			return next != null;
		}

		private OMRMemCategoryPointer internalNext() {
			try {
				if (index >= categorySet.numberOfCategories().intValue()) {
					return null;
				}
			} catch (CorruptDataException e1) {
				return null;
			}

			try {
				VoidPointer nextAtIndex = categorySet.categories().at(index++);
				while (nextAtIndex.isNull() && index < categorySet.numberOfCategories().intValue()) {
					nextAtIndex = categorySet.categories().at(index++);
				}
				return OMRMemCategoryPointer.cast(nextAtIndex);
			} catch (CorruptDataException e) {
				EventManager.raiseCorruptDataEvent("CorruptData encountered walking port library memory category set.", e, false);
				return null;
			}
		}

		public OMRMemCategoryPointer next() {
			if (hasNext()) {
				OMRMemCategoryPointer toReturn = next;
				next = null;
				return toReturn;
			} else {
				throw new NoSuchElementException();
			}
		}

	}

	/**
	 * Implementation that iterates over the categories stored in the port library.
	 */
	private static class PortLibraryCategoryIterator extends MemoryCategoryIterator {
		protected final J9PortLibraryPointer portLib;
		private State state;

		private static enum State {
			TERMINAL_STATE(null),

			UNUSED_SLAB_CATEGORY(TERMINAL_STATE),

			UNKNOWN_CATEGORY(J9BuildFlags.env_data64 ? UNUSED_SLAB_CATEGORY : TERMINAL_STATE),

			PORT_LIBRARY_CATEGORY(UNKNOWN_CATEGORY);

			private final State nextState;

			State(State nextState) {
				this.nextState = nextState;
			}

			public State getNextState() {
				if (nextState == null) {
					return this;
				} else {
					return nextState;
				}
			}

		}

		public PortLibraryCategoryIterator(J9PortLibraryPointer portLib) throws CorruptDataException {
			this.portLib = portLib;
			this.state = State.PORT_LIBRARY_CATEGORY;
		}

		public boolean hasNext() {
			return state != State.TERMINAL_STATE;
		}

		private OMRPortLibraryGlobalDataPointer getPortGlobals() throws CorruptDataException {
			return portLib.omrPortLibrary().portGlobals();
		}

		public OMRMemCategoryPointer next() {
			OMRMemCategoryPointer toReturn = null;

			while (toReturn == null) {
				try {
					switch (state) {
					case PORT_LIBRARY_CATEGORY:
						toReturn = getPortGlobals().portLibraryMemoryCategory();
						break;
					case UNKNOWN_CATEGORY:
						toReturn = getPortGlobals().unknownMemoryCategory();
						break;
					case UNUSED_SLAB_CATEGORY:
						toReturn = OMRMemCategoryHelper.getUnusedAllocate32HeapRegionsMemoryCategory(getPortGlobals());
						break;
					case TERMINAL_STATE:
						return null;
					default:
						throw new IllegalStateException("Unknown state: " + state);
					}
				} catch (CorruptDataException ex) {
					EventManager.raiseCorruptDataEvent("CorruptData encountered walking port library memory categories.", ex, false);
				}

				state = state.getNextState();
			}

			return toReturn;
		}

	}

	public final void remove() {
		throw new UnsupportedOperationException("Remove not supported");
	}

	@SuppressWarnings("unchecked")
	public static Iterator<OMRMemCategoryPointer> iterateAllCategories(J9PortLibraryPointer portLibrary)
			throws CorruptDataException {
		J9PortControlDataPointer control = portLibrary.omrPortLibrary().portGlobals().control();
		OMRMemCategorySetPointer omr_categories = control.omr_memory_categories();
		OMRMemCategorySetPointer language_categories = control.language_memory_categories();
		if (omr_categories.notNull() && language_categories.notNull()) {
			return IteratorHelpers.combineIterators(
					new MemoryCategorySetIterator(omr_categories),
					new MemoryCategorySetIterator(language_categories));
		} else if (omr_categories.notNull()) {
			return new MemoryCategorySetIterator(omr_categories);
		} else if (language_categories.notNull()) {
			return new MemoryCategorySetIterator(language_categories);
		} else {
			return new PortLibraryCategoryIterator(portLibrary);
		}
	}

	public static Iterator<? extends OMRMemCategoryPointer> iterateCategoryRootSet(J9PortLibraryPointer portLibrary)
			throws CorruptDataException {
		Map<UDATA, OMRMemCategoryPointer> rootSetMap = new HashMap<UDATA, OMRMemCategoryPointer>();
		Iterator<? extends OMRMemCategoryPointer> categoryIt = iterateAllCategories(portLibrary);

		/* Store all categories in a map */
		while (categoryIt.hasNext()) {
			OMRMemCategoryPointer thisCategory = categoryIt.next();

			rootSetMap.put(thisCategory.categoryCode(), thisCategory);
		}

		/* Remove any categories that are listed as children of any other category */
		categoryIt = iterateAllCategories(portLibrary);

		while (categoryIt.hasNext()) {
			OMRMemCategoryPointer thisCategory = categoryIt.next();

			final int numberOfChildren = thisCategory.numberOfChildren().intValue();

			for (int i = 0; i < numberOfChildren; i++) {
				UDATA childCode = thisCategory.children().at(i);

				rootSetMap.remove(childCode);
			}
		}

		return Collections.unmodifiableCollection(rootSetMap.values()).iterator();
	}

}
