/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package openj9.tools.attach.diagnostics.base;

/**
 * Container for information about the objects of a given class
 *
 */
public class HeapClassInformation {

	private String heapClassName;
	private Class<?> klass;
	private long objectCount;
	private long objectSize;

	/**
	 * @param heapClassName name of class represented by this object
	 * @param objectCount   number of instances
	 * @param objectSize    size of each instance
	 */
	public HeapClassInformation(String heapClassName, long objectCount, long objectSize) {
		this();
		this.heapClassName = heapClassName;
		this.objectCount = objectCount;
		this.objectSize = objectSize;
	}

	HeapClassInformation() {
		klass = null;
		heapClassName = null;
	}

	/**
	 * @return The classname for the objects represented by this object
	 */
	public String getHeapClassName() {
		if ((null == heapClassName) && (null != klass)) {
			heapClassName = klass.getCanonicalName();
			if (null == heapClassName) {
				/* Anonymous class */
				heapClassName = klass.getName();
			}
		}
		return heapClassName;
	}

	/**
	 * @return Number of objects in this class
	 */
	public long getObjectCount() {
		return objectCount;
	}

	/**
	 * @return Size of each object in this class
	 */
	public long getObjectSize() {
		return objectSize;
	}

	/**
	 * @return Size of all objects in this class
	 */
	public long getAggregateObjectSize() {
		return objectSize * objectCount;
	}

	@Override
	public String toString() {

		return String.format(
				"Class: %s Object size: %d Object count: %d Aggregate size: %d", //$NON-NLS-1$
				getHeapClassName(),
				Long.valueOf(objectSize), 
				Long.valueOf(objectCount), 
				Long.valueOf(objectSize * objectCount));
	}
}
