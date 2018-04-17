/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.classunloading;

import java.util.Vector;
import java.io.File;

public abstract class ClassUnloadingTestParent {

protected static final String jarFileName = System.getProperty("j9vm.testing.tests.path") + File.separator + "classestoload.jar";

protected static final int DONT_WAIT = -1;
protected long waitTime = DONT_WAIT;
protected static final long waitPerIteration = 5000;

protected Counter counterLock;

protected abstract String[] unloadableItems();
protected abstract String[] itemsToBeUnloaded();

protected abstract void createScenario() throws Exception;

protected void afterFinalization() throws Exception {};

public void runTest() throws Exception {
	counterLock = Counter.toCountUpTo(itemsToBeUnloaded().length);
	FinalizationIndicator.setLock(counterLock);
	createScenario();
	thoroughGCandFinalization();
	long iteration = waitTime / waitPerIteration;
	if (waitTime != DONT_WAIT)
		synchronized (counterLock) {
			while (notAllUnloaded(unloadableItems()) && (0 != iteration)) {
				iteration--;
				thoroughGCandFinalization();
				/* The thread would be woken up by notify in finalize() of FinalizationIndicator when the last unloadableItem is unloaded */
				counterLock.wait(waitPerIteration);
			}
		}
	verifyUnloads();
	afterFinalization();
}

protected static void thoroughGCandFinalization() {
	System.gc();
	System.gc();
	System.runFinalization();
	System.runFinalization();
	System.gc();
	System.gc();
	System.runFinalization();
	System.runFinalization();
}

protected void verifyUnloads() {
	verifyUnloaded(itemsToBeUnloaded());
	verifyNotUnloaded(remainder(unloadableItems(), itemsToBeUnloaded()));
}
static Object[] remainder(Object[] superset, Object[] subset) {
	Object[] remainder = new Object[superset.length - subset.length];
	int remainderIndex = 0;
	for(int i = 0; i < superset.length; i++) {
		Object item = superset[i];
		if(!contains(subset, item))
			remainder[remainderIndex++] = item;
	}
	return remainder;
}
static boolean contains(Object[] set, Object item) {
	for(int i = 0; i < set.length; i++) {
		if(item.equals(set[i]))
			return true;
	}
	return false;
}
protected static void verifyNotUnloaded(Object[] names) {
	for(int i = 0; i < names.length; i++) {
		if(FinalizationIndicator.isFinalized((String) names[i])) {
			System.out.println(((String) names[i]) + " erroneously unloaded");
			System.exit(1);
		}
	}
}
protected static void verifyUnloaded(Object[] names) {
	for(int i = 0; i < names.length; i++) {
		if(!FinalizationIndicator.isFinalized((String) names[i])) {
			System.out.println(((String) names[i]) + " not unloaded");
			System.out.println("This test may fail intermittently due to object finalization delay.");
			System.exit(1);
		}
	}
}
protected static boolean notAllUnloaded(Object[] names) {
	for(int i = 0; i < names.length; i++) {
		if(!FinalizationIndicator.isFinalized((String) names[i]))
			return true;
	}
	return false;
}
}
