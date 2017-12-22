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

import java.util.Hashtable;

/**
 *		The state of this class (static fields) includes a table which allows the class
 *	to answer whether or not any particular (named) instance of this class has yet been 
 *	finalized.
 *
 *		This class and its instances can be used to determine whether instances of other
 *	classes have been finalized, or to determine whether classes have been unloaded.
 *
 *		Instances of this class hold String names which identify them.
 *
 *		If an instance of another class holds the only existing reference to an instance
 *	of this class, then garbage collection of the reference-holder should, with sufficient
 *	garbage collection and finalization activity, result in finalization of the instance
 * 	of this class, which will be reported to this class.  Thus, a query to determine whether
 *	that instance of this class has been finalized will, in effect, determined whether
 *	the object that held it has been garbage collected.  The name of the instance can
 *	be used to represent the reference-holding object.
 *		Similarly, if the only reference to an instance of this class is by a static field,
 *	then unloading of the class containing the static field will result in finalization of
 *	that instance of this class.  Thus, a query to determine whether that instance has
 *	been finalized will indicate whether that class has been unloaded.	The name of the
 *	FinalizationIndicator instance can represent the class whose static refered to it.
 *		When intending to use an instance of this class to indicate class unloading, we must
 *	ensure that that instance is actually created.  If we intend to have the instance
 *	created during class initialization, we must ensure that this class initialization
 *	does occur (a VM may defer class initialization indefinitely).  One can force
 *	initialization of a class by creating an instance of it. 
 *
 **/
public class FinalizationIndicator {

	public static Hashtable isInstanceFinalized = new Hashtable();

	static Counter counterLock;

	String name;

public FinalizationIndicator(String name) {
		//System.out.println("FinalizationIndicator instantiated for " + name);
		isInstanceFinalized.put(name, new Boolean(false));
		this.name = name;
}
public static void setLock(Counter counterLock) {
	FinalizationIndicator.counterLock = counterLock;
}
protected void finalize() {
	isInstanceFinalized.put(name, new Boolean(true));
	System.out.println(name + " unloaded");
	counterLock.increment();
	if(counterLock.atDesiredValue())
		synchronized(counterLock) {
			counterLock.notifyAll();
		}
}
public static boolean isFinalized(String name) {
	Boolean wrapper = (Boolean) isInstanceFinalized.get(name);
	if(wrapper == null)
		throw new Error("FinalizationIndicator instance for " + name + " does not exist");
	return wrapper.booleanValue();
}
}
