/*
 * Copyright IBM Corp. and others 2026
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assem bly-exception-1.0
 */
package org.openj9.test;

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.EnumSet;

enum Week {
	MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY
}

public class ConcurrentClassLoading extends Thread {
	static EnumSet<Week> weekend = EnumSet.of(Week.SATURDAY, Week.SUNDAY);
	static String weekendStr;

	public static void main(String[] args) {
		for (int i=0; i < 32 ; i++) {
			new ConcurrentClassLoading().start();
		}
	}
	@Override
	public void run() {
		weekendStr = weekend.toString();

		String regex = "\\bJava\\w*\\b";
		String text = "Java and JavaScript are popular, but playing with Javanese is fun.";
		Pattern pattern = Pattern.compile(regex, Pattern.CASE_INSENSITIVE);
		Matcher matcher = pattern.matcher(text);

		while (matcher.find()) {
			String word = matcher.group();
			if (word.equals("Java")) {
				if (matcher.start() != 0 || matcher.end() != 4) {
					throw new RuntimeException("Java is not match");
				}
			}
			if (word.equals("JavaScript")) {
				if (matcher.start() != 9 || matcher.end() != 19) {
					throw new RuntimeException("JavaScript is not match");
				}
			}
			if (word.equals("Javanese")) {
				if (matcher.start() != 50 || matcher.end() != 58) {
					throw new RuntimeException("Javanese is not match");
				}
			}
		}
		if (!weekendStr.equals("[SATURDAY, SUNDAY]")) {
			throw new RuntimeException("Weekend is not match");
		}
		System.out.println("Success");
	}
}
