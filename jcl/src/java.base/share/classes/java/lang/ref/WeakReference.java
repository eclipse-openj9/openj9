/*[INCLUDE-IF Sidecar16]*/
package java.lang.ref;

/*******************************************************************************
 * Copyright (c) 1998, 2010 IBM Corp. and others
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
 
/**
 * WeakReference objects are used to detect referents which
 * are no longer visible.
 *
 * @author		OTI
 * @version		initial
 * @since		1.2
 */	
public class WeakReference<T> extends java.lang.ref.Reference<T> {

/**
 * Constructs a new instance of this class.
 *
 * @param		r	referent to track.
 * @param		q	queue to register to the reference object with.
 */
public WeakReference(T r, ReferenceQueue<? super T> q) {
	initReference(r, q);
}

/**
 * Constructs a new instance of this class.
 *
 * @param	r	referent to track.
 */
public WeakReference(T r) {
	initReference(r);
}
}
