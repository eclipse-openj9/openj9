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
package com.ibm.dtfj.javacore.parser.j9.section.common;

import java.text.ParsePosition;
import java.time.Instant;
import java.time.ZoneOffset;
import java.time.ZonedDateTime;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeFormatterBuilder;
import java.time.format.DateTimeParseException;
import java.time.temporal.ChronoField;
import java.time.temporal.TemporalAccessor;

/**
 * Helper class to parse the time from GC and thread history traces,
 * and to merge the time with the javacore creation time which might
 * have been parsed the wrong timezone.
 */
public class TraceTimeParser {
	/**
	 * Merge the timestamp in UTC without date with the old time which should have a correct date
	 * to get a correct time/date at UTC. The old time might have timezone problems.
	 * @param timestamp the timestamp from the trace. E.g. 04:36:21:235528600 GMT
	 * @param oldCreationTime the Javacore time as a long milliseconds from the epoch, should be in GMT
	 * might have been converted using the wrong time zone from local time, but treated here as UTC.
	 * @param roundMinutes How many minutes to round the minutes/seconds from the timestamp
	 * and the old creation time when applying to the new.
	 * @return the corrected time in milliseconds since the epoch
	 */
	public static long parseTimeAdjust(String timestamp, long oldCreationTime, int roundMinutes) {
		Instant is2 = Instant.ofEpochMilli(oldCreationTime);
		ZonedDateTime ztd = is2.atZone(ZoneOffset.UTC);
		DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss:SSSSSSSSS zzz");
		long best = 0;
		/* find the day for time stamp which is closest to the oldCreationTime */
		long epochDay = ztd.getLong(ChronoField.EPOCH_DAY);
		for (int dayoffset = -1; dayoffset < 2; ++dayoffset) {
			DateTimeFormatterBuilder bld = new DateTimeFormatterBuilder();
			bld.append(dtf);
			bld.parseDefaulting(ChronoField.EPOCH_DAY, epochDay + dayoffset);
			DateTimeFormatter dtf2 = bld.toFormatter();
			ParsePosition pp = new ParsePosition(0);
			try {
				TemporalAccessor ta = dtf2.parse(timestamp, pp);
				Instant is3 = Instant.from(ta);
				long m2 = is3.toEpochMilli();
				long newdiff = Math.abs(m2 - oldCreationTime);
				if (newdiff < Math.abs(best - oldCreationTime)) {
					best = m2;
				}
			} catch (DateTimeParseException e) {
				return oldCreationTime;
			}
		}
		/*
		 * Use minutes and seconds from the old creation time to adjust the
		 * new time from the trace. The date and minutes/seconds for the old
		 * time should be accurate, but this could have been parsed as
		 * a local time but in the wrong timezone.
		 * The hours from the trace accounts is in UTC.
		 * Adjust to nearest 60/15 minutes to allow for timezone errors.
		 * E.g. IST (+5:30) or NPT (+5:45).
		 * GC times aren't necessarily close to javacore, so take to nearest hour.
		 * Thread times are more accurate so allow for 15 minute time zones
		 * The Javacore time should be after the trace time, so allow for that.
		 */
		long mins = roundMinutes * 60L * 1000;
		long remOld = oldCreationTime % mins;
		long remNew = best % mins;
		long periods = best / mins;
		if (remNew > remOld) {
			remOld += mins;
		}
		long newCreationTime = periods * mins + remOld;
		return newCreationTime;
	}
}
