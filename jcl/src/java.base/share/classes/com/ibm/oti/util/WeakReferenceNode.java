/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
package com.ibm.oti.util;

import java.lang.ref.*;

/**
 * WeakReference that can be used as an element in a doubly linked list.
 * 
 * @param <T> The type of the referent 
 */
public class WeakReferenceNode<T> extends WeakReference<T> {
	private WeakReferenceNode<T> previous;
	private WeakReferenceNode<T> next;
	
	/**
	 * Constructs a new WeakReferenceNode
	 *
	 * @param r referent to track.
	 * @param q queue to register to the reference object with
	 */
	public WeakReferenceNode(T r, ReferenceQueue<? super T> q) {
		super(r, q);
	}

	/**
	 * Adds the current node before the specified node.
	 * 
	 * @param after The node that will be after this node in the list (may be null)
	 */
	public void addBefore(WeakReferenceNode<T> after) {
		if (null != after) {
			if (null != after.previous) {
				after.previous.next = this;
				this.previous = after.previous;
			}
			this.next = after;
			after.previous = this;
		}
	}
	
	/**
	 * Removes the current node from any list it may be part of. 
	 */
	public void remove() {
		if (null != this.previous) {
			this.previous.next = this.next;
		}
		if (null != this.next) {
			this.next.previous = this.previous;
		}
	}
	
	/**
	 * @return The node before this node in the list
	 */
	public WeakReferenceNode<T> previous() {
		return previous;
	}
	
	/**
	 * @return The node after this node in the list
	 */
	public WeakReferenceNode<T> next() {
		return next;
	}
}
