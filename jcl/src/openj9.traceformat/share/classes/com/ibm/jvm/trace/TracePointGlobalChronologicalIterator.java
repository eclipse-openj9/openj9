/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2016 IBM Corp. and others
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
package com.ibm.jvm.trace;

import java.util.Iterator;

/**
 * This is a Cursor that thinks it's an Iterator really.
 * 
 * @author Simon Rowland
 *	
 */
public class TracePointGlobalChronologicalIterator<Type extends com.ibm.jvm.trace.TracePoint> implements Iterator {
	private com.ibm.jvm.format.TraceFormat formatter = null;
	
	protected TracePointGlobalChronologicalIterator(com.ibm.jvm.format.TraceFormat formatter){
		this.formatter = formatter;
		updateNext();
		/* prime */
	}
	
	private com.ibm.jvm.trace.TracePoint next = null;
	private synchronized void updateNext(){
		next = formatter.getNextTracePoint();
	}
	
	/* (non-Javadoc)
	 * @see java.util.Iterator#hasNext()
	 */
	public boolean hasNext() {
		if (next == null){
			return false;
		} else {
			return true;
		}
	}

	/* (non-Javadoc)
	 * @see java.util.Iterator#next()
	 */
	public com.ibm.jvm.trace.TracePoint next() {
		com.ibm.jvm.trace.TracePoint toReturn = next;
		updateNext();
		return toReturn;
	}

	/* (non-Javadoc)
	 * @see java.util.Iterator#remove()
	 */
	public void remove() {
		updateNext();
	}

}
