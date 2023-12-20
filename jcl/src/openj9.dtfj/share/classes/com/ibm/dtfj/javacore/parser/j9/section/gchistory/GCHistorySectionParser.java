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
package com.ibm.dtfj.javacore.parser.j9.section.gchistory;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.builder.javacore.ImageBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.TraceTimeParser;

/**
 * Provides parser for GC history section in the javacore.
 */
public class GCHistorySectionParser extends SectionParser implements IGCHistoryTypes {

	private IImageAddressSpaceBuilder fImageAddressSpaceBuilder;
	private IImageProcessBuilder fImageProcessBuilder;
	private IJavaRuntimeBuilder fJavaRuntimeBuilder;

	/**
	 * Construct a javacore parser for the GC history
	 * section of the javacore file.
	 */
	public GCHistorySectionParser() {
		super(GCHISTORY_SECTION);
	}

	/**
	 * Overall controls of parsing for the GC history section.
	 * @throws ParserException on a failure to parse the history line
	 * such as an IOException or excessively long line
	 */
	@Override
	protected void topLevelRule() throws ParserException {
		fImageAddressSpaceBuilder = fImageBuilder.getCurrentAddressSpaceBuilder();
		fImageProcessBuilder = fImageAddressSpaceBuilder.getCurrentImageProcessBuilder();
		fJavaRuntimeBuilder = fImageProcessBuilder.getCurrentJavaRuntimeBuilder();

		parseGCHistoryLine();
	}

	/**
	 * Parse the GC history information.
	 * @throws ParserException on a failure to parse the history line
	 */
	private void parseGCHistoryLine() throws ParserException {
		IAttributeValueMap results;

		// Process the history lines
		while ((results = processTagLineOptional(T_1STGCHTYPE)) != null) {
			String name = results.getTokenValue(GCHISTORY_NAME);
			List<String> lines = new ArrayList<>();
			boolean first = true;
			IAttributeValueMap results2;
			while ((results2 = processTagLineOptional(T_3STHSTTYPE)) != null) {
				// 3STHSTTYPE
				// 04:36:21:235528600 GMT j9mm.51 -   SystemGC end: newspace=1794016/2097152 oldspace=4050144/9961472 loa=498688/498688
				String hist = results2.getTokenValue(GCHISTORY_LINE);
				lines.add(hist);
				if (first) {
					first = false;
					ImageBuilder imageBuilder = (ImageBuilder)fImageBuilder;
					try {
						long oldtime = imageBuilder.getImage().getCreationTime();
						long newtime = TraceTimeParser.parseTimeAdjust(hist, oldtime, 15);
						if (oldtime != newtime) {
							fImageBuilder.setCreationTime(newtime);
						}
					} catch (DataUnavailable e) {
						continue;
					}
				}
			}
			// lines appear in Javacore as newest first, so reverse
			Collections.reverse(lines);
			fJavaRuntimeBuilder.addTraceBuffer(name, lines);
		}
	}

	/**
	 * Empty hook for now.
	 */
	@Override
	protected void sovOnlyRules(String startingTag) throws ParserException {

	}
}
