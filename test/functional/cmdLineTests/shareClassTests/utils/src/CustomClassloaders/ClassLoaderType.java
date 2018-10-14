/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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
package CustomClassloaders;

/**
 * @author Matthew Kilner
 */
import java.util.*;

public final class ClassLoaderType {
	
	private String id;
	public final int ord;
	private ClassLoaderType prev;
	private ClassLoaderType next;

	private static int upperBound = 0;
	private static ClassLoaderType first = null;
	private static ClassLoaderType last = null;
	    
	private ClassLoaderType(String anID) {
		this.id = anID;
		this.ord = upperBound++;
		if (first == null) first = this;
		if (last != null) {
			this.prev = last;
			last.next = this;
		}
		last = this;
	}
	
	public static Enumeration elements() {
		return new Enumeration() {
			private ClassLoaderType curr = first;
			public boolean hasMoreElements() {
				return curr != null;
			}
			public Object nextElement() {
				ClassLoaderType c = curr;
				curr = curr.next();
				return c;
			}
		};
	}
	
	public String toString() {return this.id; }
	public static int size() { return upperBound; }
	public static ClassLoaderType first() { return first; }
	public static ClassLoaderType last()  { return last;  }
	public ClassLoaderType prev()  { return this.prev; }
	public ClassLoaderType next()  { return this.next; }

	public static final ClassLoaderType TOKEN = new
	ClassLoaderType("TokenLoader");
	public static final ClassLoaderType CACHEDURL = new
	ClassLoaderType("CachedURLLoader");
	public static final ClassLoaderType URL = new
	ClassLoaderType("URLLoader");
}
