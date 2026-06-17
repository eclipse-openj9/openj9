/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.gc.GCIterator;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

public final class RootSet {

	public static enum RootSetType {
		ALL,
		STRONG_REACHABLE,
		WEAK_REACHABLE,
		ALL_SLOTS_EXCLUDING_STACK_SLOTS,
		STRONG_REACHABLE_EXCLUDING_STACK_SLOTS;

		private RootSet rootSetCache;

		private RootSetType() {
			rootSetCache = null;
		}

		RootSet createRootSet(boolean useSingleton) throws CorruptDataException {
			RootSet rootSet = rootSetCache;

			if ((rootSet == null) || !useSingleton) {
				rootSet = new RootSet(this);
			}

			if (rootSetCache == null) {
				rootSetCache = rootSet;
			}

			return rootSet;
		}

	}

	private final List<J9ObjectPointer> _allRoots;
	private final List<VoidPointer> _allAddresses;

	private final class RootSetScanner extends SimpleRootScanner {

		private final boolean onlyStronglyReachable;

		RootSetScanner(RootSetType rootSetType) throws CorruptDataException {
			super();
			this.onlyStronglyReachable =
					   (rootSetType == RootSetType.STRONG_REACHABLE)
					|| (rootSetType == RootSetType.STRONG_REACHABLE_EXCLUDING_STACK_SLOTS);
		}

		@Override
		protected void doClassSlot(J9ClassPointer slot, VoidPointer address) {
			if (slot.notNull()) {
				doClass(slot, address);
			}
		}

		@Override
		protected void doSlot(J9ObjectPointer slot, VoidPointer address) {
			if (slot.notNull()) {
				_allRoots.add(slot);
				_allAddresses.add(address);
			}
		}

		@Override
		protected void doClass(J9ClassPointer slot, VoidPointer address) {
			J9ObjectPointer classObject;
			VoidPointer classObjectSlot;
			try {
				classObject = slot.classObject();
				classObjectSlot = VoidPointer.cast(slot.classObjectEA());
			} catch (CorruptDataException cde) {
				raiseCorruptDataEvent("Class: " + slot.getHexAddress() + " has invalid classObject slot", cde, false);
				return;
			}

			doSlot(classObject, classObjectSlot);
		}

		@Override
		protected void doPhantomReferenceSlot(J9ObjectPointer slot, VoidPointer address) {
			if (!onlyStronglyReachable) {
				doSlot(slot, address);
			}
		}

		@Override
		protected void doSoftReferenceSlot(J9ObjectPointer slot, VoidPointer address) {
			if (!onlyStronglyReachable) {
				doSlot(slot, address);
			}
		}

		@Override
		protected void doWeakReferenceSlot(J9ObjectPointer slot, VoidPointer address) {
			if (!onlyStronglyReachable) {
				doSlot(slot, address);
			}
		}

	}

	private RootSet(RootSetType rootSetType) throws CorruptDataException {
		super();
		_allRoots = new ArrayList<>();
		_allAddresses = new ArrayList<>();

		RootSetScanner rootSetScanner = new RootSetScanner(rootSetType);

		switch (rootSetType) {
		case ALL:
			rootSetScanner.scanAllSlots();
			break;
		case STRONG_REACHABLE:
			rootSetScanner.scanRoots();
			break;
		case WEAK_REACHABLE:
			rootSetScanner.scanClearable();
			break;
		case ALL_SLOTS_EXCLUDING_STACK_SLOTS:
			rootSetScanner.setScanStackSlots(false);
			rootSetScanner.scanAllSlots();
			break;
		case STRONG_REACHABLE_EXCLUDING_STACK_SLOTS:
			rootSetScanner.setScanStackSlots(false);
			rootSetScanner.scanRoots();
			break;
		default:
			throw new UnsupportedOperationException("Invalid rootSetType");
		}
	}

	/*
	 * useSingleton parameter exists primarily so that we can be guaranteed to catch
	 * corruptData events (see EventManager) for users who want to notified.
	 */
	public static RootSet from(RootSetType rootSetType, boolean useSingleton) throws CorruptDataException {
		return rootSetType.createRootSet(useSingleton);
	}

	public static List<J9ObjectPointer> allRoots(RootSetType rootSetType) throws CorruptDataException {
		RootSet rootSet = new RootSet(rootSetType);
		return Collections.unmodifiableList(rootSet._allRoots);
	}

	public GCIterator gcIterator(RootSetType rootSetType) throws CorruptDataException {
		final Iterator<J9ObjectPointer> rootSetIterator = _allRoots.iterator();
		final Iterator<VoidPointer> rootSetAddressIterator = _allAddresses.iterator();

		return new GCIterator() {
			@Override
			public boolean hasNext() {
				return rootSetIterator.hasNext();
			}

			@Override
			public VoidPointer nextAddress() {
				rootSetIterator.next();
				return rootSetAddressIterator.next();
			}

			@Override
			public Object next() {
				rootSetAddressIterator.next();
				return rootSetIterator.next();
			}
		};
	}

	public static GCIterator rootIterator(RootSetType rootSetType) throws CorruptDataException {
		final RootSet rootSet = RootSet.from(rootSetType, true);
		return rootSet.gcIterator(rootSetType);
	}

}
