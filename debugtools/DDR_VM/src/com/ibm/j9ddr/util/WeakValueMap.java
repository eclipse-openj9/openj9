/*******************************************************************************
 * Copyright (c) 2009, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.util;

import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;
import java.util.HashMap;
import java.util.Map;

/**
 * Map that weakly references values.
 * 
 * @author andhall
 *
 */
public class WeakValueMap<K,V>
{
	private final Map<K,ReferenceType<K,V>> map = new HashMap<K,ReferenceType<K,V>>();
	
	private final ReferenceQueue<V> refQueue = new ReferenceQueue<V>();
	
	public void put(K key,V value)
	{
		cleanupTax();
		
		map.put(key, new ReferenceType<K,V>(key,value,refQueue));
	}
	
	public V get(K key)
	{
		cleanupTax();
		
		ReferenceType<K,V> entry = map.get(key);
		
		if (entry != null) {
			return entry.get();
		} else {
			return null;
		}
	}
	
	public void clear()
	{
		map.clear();
	}
	
	/* Every operation is taxed to clean-up the refQueue. Otherwise we'll leak reference objects */
	@SuppressWarnings("unchecked")
	private void cleanupTax()
	{
		ReferenceType<K,V> queued = null;
		
		while ( (queued = (ReferenceType<K, V>) refQueue.poll()) != null) {
			map.remove(queued.key);
		}
	}

	private static class ReferenceType<K,V> extends SoftReference<V>
	{
		public final K key;
		
		public ReferenceType(K key, V value, ReferenceQueue<V> refQueue)
		{
			super(value,refQueue);
			this.key = key;
		}
	}
}
