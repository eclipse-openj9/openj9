/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2007
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
package com.ibm.dtfj.javacore.parser.j9.section.memory;

import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;

public class MemorySectionParser extends SectionParser implements IMemoryTypes {

	private IImageAddressSpaceBuilder fImageAddressSpaceBuilder;
	private IImageProcessBuilder fImageProcessBuilder;
	private IJavaRuntimeBuilder fJavaRuntimeBuilder;

	public MemorySectionParser() {
		super(MEMORY_SECTION);
	}

	/**
	 * Controls parsing for memory section in the javacore
	 * @throws ParserException
	 */
	protected void topLevelRule() throws ParserException {

		// get access to DTFJ AddressSpace and ImageProcess objects
		fImageAddressSpaceBuilder = fImageBuilder.getCurrentAddressSpaceBuilder();
		if (fImageAddressSpaceBuilder != null) {
			fImageProcessBuilder = fImageAddressSpaceBuilder.getCurrentImageProcessBuilder();
			if (fImageProcessBuilder != null) {
				fJavaRuntimeBuilder = fImageProcessBuilder.getCurrentJavaRuntimeBuilder();
			}
		}

		memInfo();
	}

	/**
	 * Parse the memory section and segment information
	 * @throws ParserException
	 */
	private void memInfo() throws ParserException {

		IAttributeValueMap results;

		while ((results = processTagLineOptional(T_1STHEAPTYPE)) != null) {
			// The heap type
			String segName = results.getTokenValue(MEMORY_SEGMENT_NAME);
			IAttributeValueMap resultsSpace;
			while ((resultsSpace = processTagLineOptional(T_1STHEAPSPACE)) != null) {
				long spaceid = resultsSpace.getLongValue(MEMORY_SEGMENT_ID);
				String spacetype = resultsSpace.getTokenValue(MEMORY_SEGMENT_TYPE).trim();
				String spaceName;
				if (spaceid != IBuilderData.NOT_AVAILABLE) {
					spaceName = segName + " " + spacetype + " 0x" + Long.toHexString(spaceid);
				} else {
					spaceName = segName + " " + spacetype;
				}
				JavaHeap heap = fJavaRuntimeBuilder != null ? fJavaRuntimeBuilder.addJavaHeap(spaceName) : null;
				// Each segment
				IAttributeValueMap resultsRegion;
				while ((resultsRegion = processTagLineOptional(T_1STHEAPREGION)) != null) {
					long id = resultsRegion.getLongValue(MEMORY_SEGMENT_ID);
					long head = resultsRegion.getLongValue(MEMORY_SEGMENT_HEAD);
					long size = resultsRegion.getLongValue(MEMORY_SEGMENT_SIZE);
					long tail = resultsRegion.getLongValue(MEMORY_SEGMENT_TAIL);
					String type = resultsRegion.getTokenValue(MEMORY_SEGMENT_TYPE).trim();
					String name;
					if (id != IBuilderData.NOT_AVAILABLE) {
						name = type + " 0x" + Long.toHexString(id);
					} else {
						name = type;
					}
					if (fImageAddressSpaceBuilder != null) {
						ImageSection section = fImageAddressSpaceBuilder.addImageSection(name, head, size);
						if (heap != null) {
							fJavaRuntimeBuilder.addJavaHeapSection(heap, section);
						}
					}
				}
			}
		}

		// Heap information
		results = processTagLineOptional(T_1STHEAPALLOC);
		results = processTagLineOptional(T_1STHEAPFREE);

		while ((results = processTagLineOptional(T_1STSEGTYPE)) != null) {
			// The segment type
			String segName = results.getTokenValue(MEMORY_SEGMENT_NAME);

			// Each segment
			while ((results = processTagLineOptional(T_1STSEGMENT)) != null) {

				long id = results.getLongValue(MEMORY_SEGMENT_ID);
				String name;
				if (id != IBuilderData.NOT_AVAILABLE) {
					name = segName + " segment 0x" + Long.toHexString(id);
				} else {
					name = segName;
				}
				long head = results.getLongValue(MEMORY_SEGMENT_HEAD);
				long size = results.getLongValue(MEMORY_SEGMENT_SIZE);
				long free = results.getLongValue(MEMORY_SEGMENT_FREE);
				long tail = results.getLongValue(MEMORY_SEGMENT_TAIL);

				if (head != IBuilderData.NOT_AVAILABLE) {
					if (free != IBuilderData.NOT_AVAILABLE) {
						long headSize = free - head;
						if ((headSize != 0) && (fImageAddressSpaceBuilder != null)) {
							fImageAddressSpaceBuilder.addImageSection(name
									+ " head", head, headSize);
						}
					}
				}

				if (free != IBuilderData.NOT_AVAILABLE) {
					if (tail != IBuilderData.NOT_AVAILABLE) {
						long freeSize = tail - free;
						if ((freeSize != 0) && (fImageAddressSpaceBuilder != null)) {
							fImageAddressSpaceBuilder.addImageSection(name
									+ " free", free, freeSize);
						}
					}
				}

				if (head != IBuilderData.NOT_AVAILABLE) {
					if (tail != IBuilderData.NOT_AVAILABLE) {
						if (size != IBuilderData.NOT_AVAILABLE) {
							long tailSize = head + size - tail;
							if ((tailSize != 0) && (fImageAddressSpaceBuilder != null)) {
								fImageAddressSpaceBuilder.addImageSection(name
										+ " tail", tail, tailSize);
							}
						}
					}
				}

			}
		}
	}

	/**
	 * Empty hook for now.
	 */
	protected void sovOnlyRules(String startingTag) throws ParserException {

	}

}
