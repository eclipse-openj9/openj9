/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package jit.test.vich;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jit.test.vich.utils.Timer;

public class Allocation {

private static Logger logger = Logger.getLogger(Allocation.class);
Timer timer;

public Allocation() {
	timer = new Timer ();
}

public void allocArrays(int loopCount, int size) {
	Object o[];

	for (int i = 0 ; i < loopCount; i++) {
		o = new Object[size];
	}
	return;
}

public void allocObjects(int loopCount) {
	Object o;

	for (int i = 0 ; i < loopCount; i++) {
		o = new Object();
	}
	return;
}

public void allocObjectWithOneIntSlot (int loopCount ) {

	class OneIntSlotObject {
		int i;
	}
	
	OneIntSlotObject o;
	for (int i = 0 ; i < loopCount; i++) {
		o = new OneIntSlotObject();
	}
	return;
}

public void allocObjectWithOneSlot(int loopCount) 
{	class OneSlotObject {
		Object slot;
	}
	OneSlotObject o;

	for (int i = 0 ; i < loopCount; i++) {
		o = new OneSlotObject();
	}
	return;
}

public void allocObjectWithTenIntSlots (int loopCount ) {
	class TenIntSlotObject {
	int slot1;
	int slot2;
	int slot3;
	int slot4;
	int slot5;
	int slot6;
	int slot7;
	int slot8;
	int slot9;
	int slot10;}
	
	TenIntSlotObject o;

	for (int i = 0 ; i < loopCount; i++) {
		o = new TenIntSlotObject();
	}
	return;
}

public void allocObjectWithTenSlots(int loopCount) {
	class TenSlotObject {
		Object slot1;
		Object slot2;
		Object slot3;
		Object slot4;
		Object slot5;
		Object slot6;
		Object slot7;
		Object slot8;
		Object slot9;
		Object slot10;
	}
	TenSlotObject o;
	for (int i = 0; i < loopCount; i++) {
		o = new TenSlotObject();
	}
	return;
}

public void allocObjectWithTwoIntSlots(int loopCount) {
	class TwoIntSlotObject {
		int slot1, slot2;
	}
	TwoIntSlotObject o;
	for (int i = 0; i < loopCount; i++) {
		o = new TwoIntSlotObject();
	}
	return;
}

public void allocObjectWithTwoSlots(int loopCount) 
{
	class TwoSlotObject{
		Object slot1, slot2;
	}
	TwoSlotObject o;
	for (int i = 0 ; i < loopCount; i++) {
		o = new TwoSlotObject();
	}
	return;
}

@Test(groups = { "level.sanity","level.extended","component.jit" })
public void testAllocation() {
	int loopCount = 100000;
	for (int i = 0; i <= 1000; i += 100) {
		timer.reset();
		allocArrays(loopCount, i);
		timer.mark();
		logger.info(Integer.toString(loopCount) + " x ObjectAllocation.allocArrays (" + Integer.toString(i) + ") = " + Long.toString(timer.delta()));
	}
	loopCount = 1000000;
	timer.reset();
	allocObjects(loopCount);
	timer.mark();
	logger.info(Integer.toString(loopCount) + " ObjectAllocation.allocObjects() = " + Long.toString(timer.delta()));
	timer.reset();
	allocObjectWithOneSlot(loopCount);
	timer.mark();
	logger.info(Integer.toString(loopCount) + " ObjectAllocation.allocObjectWithOneSlot() = " + Long.toString(timer.delta()));
	timer.reset();
	allocObjectWithTwoSlots(loopCount);
	timer.mark();
	logger.info(Integer.toString(loopCount) + " ObjectAllocation.allocObjectWithTwoSlots() = " + Long.toString(timer.delta()));
	timer.reset();
	allocObjectWithTenSlots(loopCount);
	timer.mark();
	logger.info(Integer.toString(loopCount) + " ObjectAllocation.allocObjectWithTenSlots() = " + Long.toString(timer.delta()));
	timer.reset();
	allocObjectWithOneIntSlot(loopCount);
	timer.mark();
	logger.info(Integer.toString(loopCount) + " ObjectAllocation.allocObjectWithOneIntSlot() = " + Long.toString(timer.delta()));
	timer.reset();
	allocObjectWithTwoIntSlots(loopCount);
	timer.mark();
	logger.info(Integer.toString(loopCount) + " ObjectAllocation.allocObjectWithTwoIntSlots() = " + Long.toString(timer.delta()));
	timer.reset();
	allocObjectWithTenIntSlots(loopCount);
	timer.mark();
	logger.info(Integer.toString(loopCount) + " ObjectAllocation.allocObjectWithTenIntSlots() = " + Long.toString(timer.delta()));
	return;
}
}