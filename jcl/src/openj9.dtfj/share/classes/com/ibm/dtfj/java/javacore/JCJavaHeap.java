/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.dtfj.java.javacore;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaHeap;

/**
 * The representation of the JavaHeap from the javacore.
 * This representation will be incomplete as it cannot
 * have all the objects in the heap as they are not present in the javacore.
 * The image sections can be retrieved from the javacore though.
 */
public final class JCJavaHeap implements JavaHeap {

	private final String name;
	private final List<ImageSection> sections;

	/**
	 * Construct the heap.
	 * @param name for example 'Object Memory Generational 0x19eef4b3e40'
	 * from 1STHEAPTYPE and 1STHEAPSPACE tags.
	 */
	public JCJavaHeap(String name) {
		this.name = name;
		this.sections = new ArrayList<>();
	}

	@Override
	public Iterator<?> getSections() {
		return Collections.unmodifiableCollection(sections).iterator();
	}

	@Override
	public String getName() {
		return name;
	}

	@Override
	public Iterator<?> getObjects() {
		return Collections.emptyListIterator();
	}

	/*
	 * Not DTFJ API below here.
	 */

	/**
	 * Add an ImageSection to the heap.
	 * Used when constructing the JCJavaHeap during parsing.
	 * @param section the new section to add
	 */
	public void addSection(ImageSection section) {
		sections.add(section);
	}

}
