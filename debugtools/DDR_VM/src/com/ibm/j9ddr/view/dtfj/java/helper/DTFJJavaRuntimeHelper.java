/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.java.helper;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;

public class DTFJJavaRuntimeHelper {

	private static long totalSizeOfAllHeaps = -1;
	
	@SuppressWarnings("rawtypes")
	public static long getTotalHeapSize(JavaRuntime runtime, IProcess process) throws CorruptDataException {

		if (totalSizeOfAllHeaps > 0) {// already calculated, never changes
			return totalSizeOfAllHeaps;
		}

		if (runtime == null) {
			throw new com.ibm.dtfj.image.CorruptDataException(
					J9DDRDTFJUtils.newCorruptData(process, "No Java Runtime present"));
		}

		Iterator heaps = runtime.getHeaps();

		while (heaps.hasNext()) {
			Object nextHeap = heaps.next();
			Iterator sections;

			if (nextHeap instanceof JavaHeap) {
				sections = ((JavaHeap)nextHeap).getSections();
			} else { // CorruptData?
				throw new com.ibm.dtfj.image.CorruptDataException(
						J9DDRDTFJUtils.newCorruptData(process, "Corrupt heap encountered whilst calculating total heap size"));
			}

			while (sections.hasNext()) {
				Object nextSection = sections.next();

				if (nextSection instanceof ImageSection) {
					long sectionSize = ((ImageSection)nextSection).getSize();
					if (Long.MAX_VALUE - sectionSize > totalSizeOfAllHeaps) {
						totalSizeOfAllHeaps += sectionSize;
					} else { // no point adding further, the cap is on Long.MAX_VALUE
						return Long.MAX_VALUE;
					}
				} else { // CorruptData?
					throw new com.ibm.dtfj.image.CorruptDataException(
							J9DDRDTFJUtils.newCorruptData(process, "Corrupt section encountered whilst calculating total heap size"));
				}
			}
		}

		return totalSizeOfAllHeaps;
	}
}
