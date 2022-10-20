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
package com.ibm.dtfj.javacore.parser.j9.section.threadhistory;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.builder.javacore.ImageBuilder;
import com.ibm.dtfj.javacore.builder.javacore.JavaRuntimeBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.TraceTimeParser;

/**
 * Provides parser for Thread history section in the javacore.
 */
public class ThreadHistorySectionParser extends SectionParser implements IThreadHistoryTypes {

	private IImageAddressSpaceBuilder fImageAddressSpaceBuilder;
	private IImageProcessBuilder fImageProcessBuilder;
	private IJavaRuntimeBuilder fJavaRuntimeBuilder;

	/**
	 * Construct a javacore parser for the trace
	 * of the current thread.
	 */
	public ThreadHistorySectionParser() {
		super(THREADHISTORY_SECTION);
	}

	/**
	 * Overall controls of parsing for the thread history section.
	 * @throws ParserException on failure to parse the history due to IOException or excessive line length
	 */
	@Override
	protected void topLevelRule() throws ParserException {
		fImageAddressSpaceBuilder = fImageBuilder.getCurrentAddressSpaceBuilder();
		fImageProcessBuilder = fImageAddressSpaceBuilder.getCurrentImageProcessBuilder();
		fJavaRuntimeBuilder = fImageProcessBuilder.getCurrentJavaRuntimeBuilder();

		parseThreadHistoryLine();
	}

	/**
	 * Parse the thread history information.
	 * Also uses the times in the thread history to fix the JVM start time
	 * and the Javacore creation time which have been parsed elsewhere
	 * with an ambiguous time zone.
	 * The start time and creation times do not have a time zone, so have been parsed according
	 * to the parser's time zone, which might differ from the time zone when creating the Javacore.
	 * Once the javacore creation time has been fixed up, the offset between the
	 * JVM start time and nanos should be the same as the offset between
	 * the javacore creation time and nanos.
	 * @throws ParserException on failure to parse the history due to IOException or excessive line length
	 */
	private void parseThreadHistoryLine() throws ParserException {
		IAttributeValueMap results;

		// Process the history lines
		while ((results = processTagLineOptional(T_1XECTHTYPE)) != null) {
			String name = results.getTokenValue(THREADHISTORY_NAME);
			List<String> lines = new ArrayList<>();
			boolean first = true;
			IAttributeValueMap resultsTraceLine;
			while ((resultsTraceLine = processTagLineOptional(T_3XEHSTTYPE)) != null) {
				// 3XEHSTTYPE
				// 04:36:21:235675300 GMT j9dmp.9 -   Preparing for dump, filename=C:\Users\name1\javacore.20220930.053619.81568.0001.txt
				String hist = resultsTraceLine.getTokenValue(THREADHISTORY_LINE);
				lines.add(hist);
				if (first) {
					first = false;
					ImageBuilder imageBuilder = (ImageBuilder)fImageBuilder;
					try {
						// Fix up the Javacore creation time.
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

		/*
		 * Fix up JVM start time.
		 */
		ImageBuilder imageBuilder = (ImageBuilder)fImageBuilder;
		JavaRuntimeBuilder javaRuntimeBuilder = (JavaRuntimeBuilder)fJavaRuntimeBuilder;
		try {
			/*
			 * The seconds and milliseconds are presumed correct, but due to time zone problems
			 * the hours might be wrong, and minutes in steps of 15 minutes might be wrong.
			 */
			final long roundingMillis = 15 * 60 * 1000;
			long creationTime = imageBuilder.getImage().getCreationTime();
			long creationNanos = imageBuilder.getImage().getCreationTimeNanos();
			long startTime = javaRuntimeBuilder.getStartTime();
			long startNanos = javaRuntimeBuilder.getStartTimeNanos();
			long javacoreDelta = creationTime - (creationNanos / 1000000);
			long startDelta = startTime - (startNanos / 1000000);
			/*
			 * The delta between the millisecond times, and the nano times should be
			 * the same (to within 15 minutes) for the Javacore times and the start times.
			 */
			long adjust = javacoreDelta - startDelta;
			/*
			 * Fix the start time by a multiple of 15 minutes to cope with the time zone.
			 */
			long roundedAdjust = Math.floorDiv(adjust + (roundingMillis / 2), roundingMillis) * roundingMillis;
			fJavaRuntimeBuilder.setStartTime(startTime + roundedAdjust);
		} catch (CorruptDataException | DataUnavailable e) {
			// log, we are just trying to fix up when time zones differ
			handleError("Failed to correct JVM start time", e);
		}
	}

	/**
	 * Empty hook for now.
	 */
	@Override
	protected void sovOnlyRules(String startingTag) throws ParserException {

	}
}
