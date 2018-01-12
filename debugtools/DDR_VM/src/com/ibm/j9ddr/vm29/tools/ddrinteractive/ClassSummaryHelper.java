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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;

import com.ibm.j9ddr.vm29.tools.ddrinteractive.IClassWalkCallbacks.SlotType;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper.J9ClassRegion;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper.J9ClassRegionNode;

public class ClassSummaryHelper {
	private final Map<String, Long> sizeStat;
	private int numberOfClasses = 0;
	private String[] preferredOrder;

	public ClassSummaryHelper(String[] preferredOrder) {
		sizeStat = new HashMap<String, Long>();
		this.preferredOrder = preferredOrder;
	}

	public void addRegionsForClass(J9ClassRegionNode allRegionsNode) {
		numberOfClasses++;
		appendSizeStat(sizeStat, allRegionsNode, "");
	}

	public void printStatistics(PrintStream out) {
		/*
		 * This will sort the data, sizeStat should not be a TreeMap, because it
		 * will sort the key on every access to the map
		 */
		Map<String, Long> data = new TreeMap<String, Long>(new StringCompare(
				preferredOrder));
		data.putAll(sizeStat);

		long totalSize = 0;
		for (Map.Entry<String, Long> entry : data.entrySet()) {
			if (getLevel(entry.getKey()) == 1) {
				totalSize += entry.getValue().longValue();
			}
		}

		// Print the global stats
		out.println(String.format("%d classes, using: %d bytes or %d kB",
				numberOfClasses, totalSize, totalSize / 1024));

		// Print the sections
		out.println(String.format("%-40s%-8s %-8s %-6s", "Section", "Byte",
				"kB", "%"));
		for (Map.Entry<String, Long> entry : data.entrySet()) {
			out.println(printSize(entry.getKey(), entry.getValue(), totalSize));
		}
	}

	private void appendSizeStat(Map<String, Long> sizeStat,
			J9ClassRegionNode regionNode, String parentName) {
		String section = parentName;
		final J9ClassRegion nodeValue = regionNode.getNodeValue();
		if (nodeValue != null) {
			if (nodeValue.getType() == SlotType.J9_SECTION_START
					|| nodeValue.getType() == SlotType.J9_Padding) {
				section = section + "/" + nodeValue.getName();
				if (sizeStat.containsKey(section)) {
					sizeStat.put(section, new Long(sizeStat.get(section)
							+ nodeValue.getLength()));
				} else {
					sizeStat.put(section, new Long(nodeValue.getLength()));
				}
			}
		}
		for (J9ClassRegionNode classRegionNode : regionNode.getChildren()) {
			appendSizeStat(sizeStat, classRegionNode, section);
		}
	}

	private String printSize(String key, Long size, Long totalSize) {
		long sizeByte = size.longValue();
		double sizekB = sizeByte / 1024.0;

		/*
		 * Get the tab indent, the sections are separated by "/" like
		 * /methods/method/stackMap
		 */
		String[] parts = key.split("/");
		String section = parts[parts.length - 1];
		for (int i = 0; i < getLevel(key); i++) {
			section = "  " + section;
		}

		double percent = ((double) sizeByte / totalSize) * 100;

		return String.format("%-40s%-8d %-8.2f %-6.2f", section, sizeByte,
				sizekB, percent);
	}

	private int getLevel(String key) {
		return key.split("/").length - 1;
	}

	/**
	 * This class sorts the sections using a preferredOrder The root level of
	 * the tree is sorting using that order and the levels under are sorted
	 * alphabetically.
	 * 
	 * @author jeanpb
	 * 
	 */
	private class StringCompare implements Comparator<String> {
		private final Map<String, Integer> preferredOrderMap;

		public StringCompare(String[] preferredOrder) {
			preferredOrderMap = new HashMap<String, Integer>();

			for (int i = 0; i < preferredOrder.length; i++) {
				preferredOrderMap.put(preferredOrder[i], i);
			}
		}

		private int find(String key) {
			if (preferredOrderMap.containsKey(key.trim())) {
				return preferredOrderMap.get(key.trim());
			}
			return Integer.MAX_VALUE;
		}

		public int compare(String o1, String o2) {
			String[] partso1 = o1.split("/");
			String[] partso2 = o2.split("/");
			int ordero1 = find(partso1[1]);
			int ordero2 = find(partso2[1]);
			if (ordero1 != ordero2) {
				return ordero1 - ordero2;
			} else {
				return o1.compareTo(o2);
			}
		}
	}
}
