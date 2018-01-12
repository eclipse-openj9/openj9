/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders;

/**
 * Simple paging cache, to speedup getMemoryBytes
 * 
 */
public class PageCache
{
	// Cached byte buffer
	protected byte[] bytes;

//==== Tuning statistics =================================================
//	private double cacheUse = 0;
//	private double loadCount = 0;
//========================================================================

	protected static final int NO_KEY = -1;

	/*
	 * Abstract view of a 'page' of memory
	 */
	public class Page
	{
		protected int  slot = -1;
		protected long base = 0;
		protected int  span = 0;
		protected int  key  = NO_KEY;

		// Auto-tuning statistics
		protected int  used = 1;

//==== Tuning statistics =================================================
//		public void evict() {
//			double pageUse = ((double)used)/(span+1);
//			cacheUse = (cacheUse*loadCount + pageUse) / (loadCount+1);
//			System.out.println("EVICT ["+
//				(int)loadCount+"|"+
//				slot/pageSize+"|"+
//				(int)(pageUse*100)+"%|"+
//				(int)(cacheUse*100)+"%|"+
//				evictPeriod+"|"+
//				evictGrowth+"]");
//		}
//========================================================================

		/*
		 * Query page status
		 */
		public boolean isEmpty()
		{
			return (key == NO_KEY);
		}

		/*
		 * Push data into the page
		 */
		public void push(int forKey, long vaddr, byte[] buf)
		{
			// Fresh page update
			System.arraycopy(buf, 0, bytes, slot, buf.length);

			// May be truncated
			base = vaddr;
			span = buf.length;
			key  = forKey;

//==== Tuning statistics =================================================
//			loadCount++;
//========================================================================
		}

		/*
		 * Pull data from the page
		 */
		public byte[] pull(int forKey, long vaddr, int size)
		{
			if (key != forKey || key == NO_KEY) {
				return null;
			}

			int delta = (int)(vaddr - base);

			// Safety check in case of truncation
			if (delta > span-size) {
				size = span-delta;
			}
			if (size < 0) {
				return null;
			}

			byte[] buf = new byte[size];

			// Fill byte buffer using cached data
			System.arraycopy(bytes, slot + delta, buf, 0, size);
			used += size;

			// When we have enough, begin evicting again
			if (used >= (span/hiUsage) && evictPeriod > 1) {

				// Halve eviction delay
				evictPeriod >>>= 1;
				evictTick = 0;

				// Drop rate of delay
				evictGrowth = 1;
			}

			return buf;
		}
	}

	// Default cache parameters
	public int numPages = 16;
	public int pageSize = 256*1024;

	// Eviction throttles
	public int hiUsage = 5;
	public int loUsage = 20;

	// Page details
	protected Page[] pages;

	// Pseudo-LRU tree
	protected long lruBits = 0;

	// Auto-tuning variables
	protected int evictTick = 0;
	protected int evictPeriod = 1;
	protected int evictGrowth = 1;

	// ... and their limits
	protected int maxPeriod = 64*1024;
	protected int maxGrowth = 32;

	/*
	 * Constructor utility
	 */
	protected void initialize()
	{
		pages = new Page[numPages];
		bytes = new byte[numPages*pageSize];

		for (int i = 0; i < numPages; i++) {
			pages[i] = new Page();
			pages[i].slot = i*pageSize;
		}
	}


	/*
	 * Construct a cache with default heuristics
	 */
	PageCache()
	{
		initialize();
	}


	/*
	 * Construct a cache with specific heuristics
	 */
	PageCache(int _numPages, int _pageSize, double _hiDensity, double _loDensity)
	{
		numPages = _numPages;
		pageSize = _pageSize;

		hiUsage  = (int)(1.0/_hiDensity);
		loUsage  = (int)(1.0/_loDensity);

		initialize();
	}


	/*
	 * Have we cached something matching this request?
	 */
	public Page find(int key, long vaddr, int size)
	{
		// Wrong size to cache
		if (size < 0 || size > pageSize) {
			return null;
		}

		// Match request to cached page
		for (int i = 0; i < numPages; i++) {
			Page p = pages[i];

			if (p.key == key) {
				int delta = (int)(vaddr - p.base);

				// We're close to an existing page
				if (0 <= delta && delta <= p.span-size) {
					return p;
				}
			}
		}

		// Auto-tuning to limit bad evictions
		if (evictTick++ % evictPeriod > 0) {
			return null;
		}

		int index = 0;

		// Find LRU page and update tree...
		while (index < numPages) {

			long lruSite = 1 << index;
			long branch = (~lruBits) & lruSite;

			// Flip LRU branch
			lruBits &= ~lruSite;
			lruBits |= branch;

			if (branch == 0) {
				index += index;		// left
			} else {
				index += (index+1);	// right
			}
		}

		Page lruPage = pages[index-numPages];

		// Peg evict rate otherwise we risk starving the cache
		if (lruPage.used < (lruPage.span/loUsage) && evictPeriod < maxPeriod) {

			// Wait longer before evicting
			evictPeriod += evictGrowth;
			evictTick = 1;

			// Increase rate of delay
			if (evictGrowth < maxGrowth) {
				evictGrowth <<= 1;
			}
		}

//==== Tuning statistics =================================================
//		lruPage.evict();
//========================================================================

		// Reset page (span will be set when it's pushed)
		lruPage.base = vaddr;
		lruPage.span = 0;
		lruPage.used = 0;
		lruPage.key  = NO_KEY;

		return lruPage;
	}
}
