/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
	private int numberOfClasses;
	private final String[] preferredOrder;

	public ClassSummaryHelper(String[] preferredOrder) {
		super();
		this.sizeStat = new HashMap<>();
		this.numberOfClasses = 0;
		this.preferredOrder = preferredOrder;
	}

	public void addRegionsForClass(J9ClassRegionNode allRegionsNode) {
		numberOfClasses += 1;
		appendSizeStat(sizeStat, allRegionsNode, "");
	}

	public void printStatistics(PrintStream out) {
		/*
		 * This will sort the data, sizeStat should not be a TreeMap,
		 * otherwise it would sort on every addition to the map.
		 */
		Map<String, Long> data = new TreeMap<>(new StringCompare(preferredOrder));
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
		out.println(String.format("%-39s %-8s %-8s %-6s",
				"Section", "Byte", "kB", "%"));
		for (Map.Entry<String, Long> entry : data.entrySet()) {
			out.println(printSize(entry.getKey(), entry.getValue(), totalSize));
		}
	}

	private static void appendSizeStat(Map<String, Long> sizeStat,
			J9ClassRegionNode regionNode, String parentName) {
		String section = parentName;
		final J9ClassRegion nodeValue = regionNode.getNodeValue();
		if (nodeValue != null) {
			if (nodeValue.getType() == SlotType.J9_SECTION_START
					|| nodeValue.getType() == SlotType.J9_Padding) {
				section = section + "/" + nodeValue.getName();
				Long length = sizeStat.get(section);
				if (length != null) {
					sizeStat.put(section, Long.valueOf(length.longValue() + nodeValue.getLength()));
				} else {
					sizeStat.put(section, Long.valueOf(nodeValue.getLength()));
				}
			}
		}
		for (J9ClassRegionNode classRegionNode : regionNode.getChildren()) {
			appendSizeStat(sizeStat, classRegionNode, section);
		}
	}

	private static String printSize(String key, Long size, long totalSize) {
		/*
		 * Get the tab indent, the sections are separated by "/" like
		 * /methods/method/stackMap
		 */
		StringBuilder section = new StringBuilder();
		for (int i = 0, level = getLevel(key); i < level; ++i) {
			section.append("  ");
		}
		section.append(key.substring(key.lastIndexOf('/') + 1));

		long sizeByte = size.longValue();
		double sizekB = sizeByte / 1024.0;
		double percent = sizeByte * 100.0 / totalSize;

		return String.format("%-39s %-8d %-8.2f %-6.2f",
				section, sizeByte, sizekB, percent);
	}

	/* count the '/' separators to determine the nesting level */
	private static int getLevel(String key) {
		for (int index = -1, level = 0;;) {
			index = key.indexOf('/', index + 1);

			if (index < 0) {
				return level;
			}

			level += 1;
		}
	}

	/**
	 * This class sorts the sections using a preferredOrder The root level of
	 * the tree is sorting using that order and the levels under are sorted
	 * alphabetically.
	 * 
	 * @author jeanpb
	 */
	private static class StringCompare implements Comparator<String> {

		private final Map<String, Integer> preferredOrderMap;

		public StringCompare(String[] preferredOrder) {
			preferredOrderMap = new HashMap<>();

			for (int i = 0; i < preferredOrder.length; i++) {
				preferredOrderMap.put(preferredOrder[i], Integer.valueOf(i));
			}
		}

		private int find(String key) {
			Integer order = preferredOrderMap.get(key.trim());
			if (order != null) {
				return order.intValue();
			}
			return Integer.MAX_VALUE;
		}

		private static String firstSegment(String key) {
			int start = key.indexOf('/') + 1;

			if (start <= 0) {
				/* all the keys used here should have at least one slash */
				throw new IllegalArgumentException();
			}

			int end = key.indexOf('/', start);
			String segment;

			if (end < 0) {
				segment = key.substring(start);
			} else {
				segment = key.substring(start, end);
			}

			return segment;
		}

		@Override
		public int compare(String o1, String o2) {
			String segment1 = firstSegment(o1);
			String segment2 = firstSegment(o2);
			int ordero1 = find(segment1);
			int ordero2 = find(segment2);
			if (ordero1 != ordero2) {
				return Integer.compare(ordero1, ordero2);
			} else {
				return o1.compareTo(o2);
			}
		}
	}

}
