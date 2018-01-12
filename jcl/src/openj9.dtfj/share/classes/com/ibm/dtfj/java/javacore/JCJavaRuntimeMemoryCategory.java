/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java.javacore;

import java.util.Collections;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.dtfj.java.JavaRuntimeMemoryCategory;

public class JCJavaRuntimeMemoryCategory implements JavaRuntimeMemoryCategory
{
	private final String name;
	private final long deepAllocations;
	private final long deepBytes;
	private boolean shallowValuesSet = false;
	private long shallowAllocations;
	private long shallowBytes;
	
	private final List children = new LinkedList();
	
	private static final String nl = System.getProperty("line.separator");
	
	public JCJavaRuntimeMemoryCategory(String name, long deepBytes, long deepAllocations)
	{
		this.name = name;
		this.deepAllocations = deepAllocations;
		this.deepBytes = deepBytes;
	}
	
	public Iterator getChildren()
	{
		return Collections.unmodifiableList(children).iterator();
	}
	
	public void addChild(JavaRuntimeMemoryCategory child)
	{
		children.add(child);
	}

	public long getDeepAllocations()
	{
		return deepAllocations;
	}

	public long getDeepBytes()
	{
		return deepBytes;
	}

	public Iterator getMemorySections(boolean includeFreed)
	{
		return Collections.EMPTY_LIST.iterator();
	}

	public String getName()
	{
		return name;
	}

	public void setShallowCounters(long shallowBytes, long shallowAllocations)
	{
		this.shallowBytes = shallowBytes;
		this.shallowAllocations = shallowAllocations;
		this.shallowValuesSet = true;
	}
	
	public long getShallowAllocations()
	{
		if (shallowValuesSet) {
			return shallowAllocations;
		} else if (children.size() == 0) {
			return deepAllocations;
		} else {
			return 0;
		}
	}

	public long getShallowBytes()
	{
		if (shallowValuesSet) {
			return shallowBytes;
		} else if (children.size() == 0) {
			return deepBytes;
		} else {
			return 0;
		}
	}

	public String toString()
	{
		StringBuffer buffer = new StringBuffer();
		
		buildPrintTree(buffer, 1);
		
		return buffer.toString();
	}
	
	private void buildPrintTree(StringBuffer buffer, int depth)
	{
		if (depth > 1) {
			for (int i = 0; i < (depth - 1); i++) {
				if (i > 0) {
					buffer.append("    ");
				}
				buffer.append("|");
			}
			buffer.append(nl);
		}
		
		for (int i = 0; i < (depth - 2); i++) {
			buffer.append("|    ");
		}
		
		if (depth > 1) {
			buffer.append("+--");
		}
		
		buffer.append(name);
		buffer.append(": ");
		buffer.append(Long.toString(deepBytes));
		buffer.append(" / ");
		buffer.append(Long.toString(deepAllocations));
		buffer.append(" allocation");
		if (deepAllocations != 1) {
			buffer.append("s");
		}
		
		buffer.append(nl);
		
		Iterator childIt = children.iterator();
		while (childIt.hasNext()) {
			JCJavaRuntimeMemoryCategory child = (JCJavaRuntimeMemoryCategory) childIt.next();
			
			child.buildPrintTree(buffer, depth + 1);
		}
		
		if (children.size() > 0 && getShallowBytes() > 0) {
			//Print the magic "other" category for shallow values
			JCJavaRuntimeMemoryCategory otherPseudoCategory = new JCJavaRuntimeMemoryCategory("Other", getShallowBytes(), getShallowAllocations());
			otherPseudoCategory.buildPrintTree(buffer, depth + 1);
		}
	}

	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result
				+ ((children == null) ? 0 : children.hashCode());
		result = prime * result
				+ (int) (deepAllocations ^ (deepAllocations >>> 32));
		result = prime * result + (int) (deepBytes ^ (deepBytes >>> 32));
		result = prime * result + ((name == null) ? 0 : name.hashCode());
		return result;
	}

	public boolean equals(Object obj)
	{
		if (this == obj) {
			return true;
		}
		if (obj == null) {
			return false;
		}
		if (!(obj instanceof JCJavaRuntimeMemoryCategory)) {
			return false;
		}
		JCJavaRuntimeMemoryCategory other = (JCJavaRuntimeMemoryCategory) obj;
		if (children == null) {
			if (other.children != null) {
				return false;
			}
		} else if (!children.equals(other.children)) {
			return false;
		}
		if (deepAllocations != other.deepAllocations) {
			return false;
		}
		if (deepBytes != other.deepBytes) {
			return false;
		}
		if (name == null) {
			if (other.name != null) {
				return false;
			}
		} else if (!name.equals(other.name)) {
			return false;
		}
		return true;
	}

}
