/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.title;

import java.text.ParseException;
import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.Date;

import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;

public class TitleSectionParser extends SectionParser implements ITitleTypes {
	
	public TitleSectionParser() {
		super(TITLE_SECTION);
	}

	/**
	 * Controls parsing for title stuff
	 * @throws ParserException
	 */
	protected void topLevelRule() throws ParserException {
		
		IAttributeValueMap results = null;
		Date dumpDate = null;
		
		processTagLineRequired(T_1TISIGINFO);
		
		results = processTagLineRequired(T_1TIDATETIME);
		if (results != null) {
			String tm = results.getTokenValue(TI_DATE);
			if (tm != null) {
				// Java 8 SR2 or later, with millisecs added:
				// 1TIDATETIME    Date:    2015/07/17 at 09:46:27:261
				SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd 'at' HH:mm:ss:S");
				dumpDate = sdf.parse(tm, new ParsePosition(0));
				if (dumpDate == null) {
					// Earlier javacores:
					// 1TIDATETIME    Date:    2009/02/09 at 15:48:30
					sdf = new SimpleDateFormat("yyyy/MM/dd 'at' HH:mm:ss");
					dumpDate = sdf.parse(tm, new ParsePosition(0));
				}
				if (dumpDate != null) {
					long time = dumpDate.getTime();
					fImageBuilder.setCreationTime(time);
				}
			}
		}
		
		results = processTagLineOptional(T_1TINANOTIME);
		if (results != null) {
			String nanoTimeString = results.getTokenValue(TI_NANO);
			if (nanoTimeString != null) {
				fImageBuilder.setCreationTimeNanos(Long.parseLong(nanoTimeString));
			}
		}
		
		results = processTagLineRequired(T_1TIFILENAME);
		if (results != null) {
			String fn = results.getTokenValue(TI_FILENAME);
			if (fn != null && dumpDate != null) {
				// Strip off the path
				fn = fn.replaceAll(".*[\\\\/]", "");
				Date d1 = dumpDate;
				Date d2 = new Date(dumpDate.getTime() + 5*60*1000);
				String pid = getPID(fn, d1, d2);
				if (pid == null) {
					// The AIX file date is in UTC - the javacore date is local time for the dump system,
					// converted as though it was local time on the running system.
					// E.g. dump at 23:00 UTC+14, current TZ = UTC-12
					// converted to 23:00 UTC-12 = 11:00 +1 UTC
					// AIX date 09:00 UTC
					//
					// E.g. dump at 23:00 UTC-12, current TZ = UTC+14
					// converted to 23:00 UTC+14 = 09:00 UTC
					// AIX date 11:00 UTC + 1
					// We don't know the timezone, so try a generous range
					d1 = new Date(dumpDate.getTime() - 26*60*60*1000);
					d2 = new Date(dumpDate.getTime() + 26*60*60*1000);
					pid = getPIDAIX(fn ,d1, d2);
				}
				if (pid != null) {
					fImageBuilder.getCurrentAddressSpaceBuilder().getCurrentImageProcessBuilder().setID(pid);
				}
			}
		}
	}

	private String getPID(String fn, Date d1, Date d2) {
		SimpleDateFormat sdf = new SimpleDateFormat("yyyyMMdd.HHmmss");
		ParsePosition pp = new ParsePosition(0);
		for (int i = 0; i < fn.length(); ++i) {
			pp.setIndex(i);
			Date dF = sdf.parse(fn, pp);
			if (dF != null && !d1.before(dF) && d2.after(dF)) {
				// Match
				String rest[] = fn.substring(pp.getIndex()).split("\\.");
				for (int j = 0; j < rest.length; ++j) {
					if (rest[j].equals("")) continue;
					try {
						int pid = Integer.parseInt(rest[j]);
						return rest[j];
					} catch (NumberFormatException e) {
					}
					break;
				}
			}
		}
		return null;
	}

	private String getPIDAIX(String fn, Date d1, Date d2) {
		// or for AIX 1.4.2 heapdumpPID.EPOCHTIME.phd
		// heapdump454808.1244656860.phd
		String s[] = fn.split("\\.");
		String prefix = "javacore";
		if ((s.length == 3  || s.length == 4 && s[3].equals("gz")) && s[0].startsWith(prefix)) {
			try {
				// Check the first part is also a number (PID)
				int i1 = Integer.parseInt(s[0].substring(prefix.length()));
				// Check the second part is also a number
				long l2 = Long.parseLong(s[1]);
				// The second number is the number of seconds since the epoch
				// Simple validation - since circa 2000 ?
				Date dF = new Date(l2 * 1000L);
				if (!dF.before(d1) && dF.before(d2)) {
					return Integer.toString(i1);
				}
			} catch (NumberFormatException e) {
			}
		}	
		return null;
	}
	
	/**
	 * Empty hook for now.
	 */
	protected void sovOnlyRules(String startingTag) throws ParserException {

	}
}
