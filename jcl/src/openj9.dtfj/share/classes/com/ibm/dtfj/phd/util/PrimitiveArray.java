/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd.util;

import java.util.Random;
import java.io.Serializable;
import javax.swing.ProgressMonitor;

/**
 *  Superclass of IntegerArray and LongArray
 */

public abstract class PrimitiveArray implements Serializable, SortListener {
	static final int shift = 12;
	static final int CHUNK_SIZE = 1 << shift;
	static final int mask = CHUNK_SIZE - 1;
	int chunkOffset;
	SortListener listener;
	int size;
	int progress;
	int maxProgress;
	ProgressMonitor monitor;

	public PrimitiveArray() {}

	public int size() {
		return size;
	}

	public void setSize(int size) {
		this.size = size;
	}

	abstract long lget(int index);

	abstract void lput(int index, long value);

	public int indexOf(long value) {
		int low = 0;
		int high = size - 1;

		while (low <= high) {
			int mid = (low + high) >> 1;
		long midVal = lget(mid);

		if (midVal < value)
			low = mid + 1;
		else if (midVal > value)
			high = mid - 1;
		else
			return mid;
		}

		return -1;
	}

	public int nearestIndexOf(long value) {
		int low = 0;
		int high = size - 1;
		int mid = 0;
		long midVal = 0;

		while (low <= high) {
			mid = (low + high) >> 1;
		midVal = lget(mid);

		if (midVal < value)
			low = mid + 1;
		else if (midVal > value)
			high = mid - 1;
		else
			return mid;
		}

		if (value > midVal)
			return mid;
		else if (mid > 0)
			return mid - 1;
		else
			return -1;
	}

	public int indexOf(int value) {
		return indexOf((long)value);
	}

	public void sort(ProgressMonitor monitor) {
		this.monitor = monitor;
		sort();
		this.monitor = null;
	}

	public void sort(SortListener listener, ProgressMonitor monitor) {
		this.monitor = monitor;
		sort(listener);
		this.monitor = null;
	}

	public void sort() {
		sort((SortListener)null);
	}

	public void reverseSort() {
		reverseSort(null);
	}

	public void sort(SortListener listener) {
		this.listener = listener;
		if (monitor != null) {
			maxProgress = size / 2;
			monitor.setMaximum(maxProgress);
		}
		quickSort(0, size - 1);
		this.listener = null;
	}

	public void reverseSort(SortListener listener) {
		this.listener = listener;
		quickSort(0, size - 1);
		for (int i = 0, j = size - 1; i < j; i++, j--)
			swap(i, j);
		this.listener = null;
	}

	public void swap(int a, int b) {
		long t = lget(a);

		lput(a, lget(b));
		lput(b, t);
		if (listener != null)
			listener.swap(a, b);
	}

	void quickSort(int p, int r) {
		if (monitor != null) {
			if ((progress % 0x100) == 0) {
				monitor.setProgress(progress);
			}
			progress++;
		}
		if (r - p < 7) {
			for (int i = p; i <= r; i++) {
				for (int j = i; j > p && lget(j - 1) > lget(j); j--) {
					swap(j, j - 1);
				}
			}
			return;
		}
		if (p < r) {
			int q = randomPartition(p, r);

			quickSort(p, q);
			quickSort(q + 1, r);
		}
	}

	// Use a seed to guarantee the same order every run
	Random random = new Random(0xc001d00dcafebabeL);

	int randomPartition(int p, int r) {
		int i = random.nextInt(r - p + 1) + p;

		swap(p, i);
		return partition(p, r);
	}

	int partition(int p, int r) {
		long x = lget(p);
		int i = p - 1;
		int j = r + 1;

		while (true) {
			while (lget(--j) > x);
			while (lget(++i) < x);
			if (i < j)
				swap(i, j);
			else
				return j;
		}
	}

	static final int log2bytes[] = {
		-1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
	};

	static int log2(int n) {
		int bit = 0;
		while ((n & 0xffffff00 ) != 0) {
			bit += 8;
			n >>>= 8;
		}
		return bit + log2bytes[n];
	}
}
