/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.heapdump;

import java.util.Iterator;
import java.util.List;


/**
 * Reference iterator based on a list of Longs.
 * 
 * @author andhall
 */
public class LongListReferenceIterator implements ReferenceIterator
{
	private final List _data;
	private Iterator _currentIterator;
	
	public LongListReferenceIterator(List data) 
	{
		_data = data;
		reset();
	}
	
	public boolean hasNext()
	{
		return _currentIterator.hasNext();
	}

	public Long next()
	{
		return (Long) _currentIterator.next();
	}

	public void reset()
	{
		_currentIterator = _data.iterator();
	}
	
}
