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
package com.oti.j9.exclude;

public class DateUtil {

	private static byte[] DaysInMonth = new byte[] {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	private static int[] DaysInYear = new int[] {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
	private static String[] MONTHS = new String[] {"january", "february", "march", "april",
					"may", "june", "july", "august", "september", "october",
					"november", "december"};
	private static int		 _fCreationYear;

	static {
		long millis = System.currentTimeMillis();
		int dayCount = (int) (millis / 86400000);

		int dayOfYear = dayCount;
		_fCreationYear = 1970;
		int approxYears;

		while ((approxYears = dayOfYear / 365) != 0) {
			_fCreationYear = _fCreationYear + approxYears;
			dayOfYear = dayCount - daysFromBaseYear(_fCreationYear);
		}
		if (dayOfYear < 0) _fCreationYear--;
		_fCreationYear -= 1900;
	}

/**
 * Answers the millisecond value of the date and time parsed from
 * the specified String. Many date/time formats are recognized, including
 * modified IETF standard syntax, i.e. 22 Jun 1999
 */
public static long parseExpiry(String expiry) {
	int offset = 0, length = expiry.length(), state = 0;
	int year = -1, month = -1, date = -1;
	final int PAD=0, LETTERS=1, NUMBERS=2;
	StringBuffer buffer = new StringBuffer();

	while (offset <= length) {
		char next = offset < length ? expiry.charAt(offset) : '\r';
		offset++;

		int nextState = PAD;
		if ('a' <= next && next <= 'z' || 'A' <= next && next <= 'Z') nextState = LETTERS;
		else if ('0' <= next && next <= '9') nextState = NUMBERS;
		else if (!isSpace(next) && ",-/".indexOf(next, 0) == -1)
			throw new IllegalArgumentException();

		if (state == NUMBERS && nextState != NUMBERS) {
			int digit = Integer.parseInt(buffer.toString());
			buffer = new StringBuffer();
			if (digit >= 70) {
				if (year == -1 && (isSpace(next) || next == ',' || next == '/' || next == '\r'))
					year = digit;
				else throw new IllegalArgumentException();
			} else if (next == '/') {
				if (month == -1) month = digit - 1;
				else if (date == -1) date = digit;
				else throw new IllegalArgumentException();
			} else if (isSpace(next) || next == ',' || next == '-' || next == '\r') {
				if (date == -1) date = digit;
				else if (year == -1) year = digit;
				else throw new IllegalArgumentException();
			} else if (year == -1 && month != -1 && date != -1)
				year = digit;
			else throw new IllegalArgumentException();
		} else if (state == LETTERS && nextState != LETTERS) {
			String text = buffer.toString().toUpperCase();
			buffer = new StringBuffer();
			if (text.length() == 1) throw new IllegalArgumentException();
			String[] months = MONTHS;
			int value;
			if (month == -1 && (month = parse(text, months)) != -1) ;
			else throw new IllegalArgumentException();
		}

		if (nextState == LETTERS || nextState == NUMBERS) buffer.append(next);
		state = nextState;
	}

	if (year != -1 && month != -1 && date != -1) {
		if (year < (_fCreationYear - 80)) year += 2000;
		else if (year < 100) year += 1900;
		return computeDate(year, month, date);
	}
	throw new IllegalArgumentException();
}

private static boolean isSpace(char ch) {
	return ch == ' ' || ch == '\t' || ch == '\r';
}

private static int parse(String string, String[] array) {
	string = string.toLowerCase();
	for (int i = 0, alength = array.length, slength = string.length(); i < alength; i++)
		if (array[i].startsWith(string, 0)) return i;
	return -1;
}

private static long computeDate(int year, int month, int date) {
	year += month / 12;
	month %= 12;
	if (month < 0) {
		year--;
		month += 12;
	}
	long days = daysFromBaseYear(year) + daysInYear(isLeapYear(year), month);
	days += date - 1;
	return days * 86400000;
}

private static int daysFromBaseYear(int year) {
	if (year >= 1970) {
		return (year - 1970) * 365 + ((year - 1969) / 4) -
			((year - 1901) / 100) + ((year - 1601) / 400);
	}
	return (year - 1970) * 365 + ((year - 1972) / 4) -
		((year - 2000) / 100) + ((year - 2000) / 400);
}

private static boolean isLeapYear(int year) {
	return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

private static int daysInYear(boolean leapYear, int month) {
	if (leapYear && month > 1) // FEBRUARY
		return DaysInYear[month] + 1;
	else return DaysInYear[month];
}

private static int daysInMonth(boolean leapYear, int month) {
	if (leapYear && month == 1) // FEBRUARY
		return DaysInMonth[month] + 1;
	else return DaysInMonth[month];
}

private static int mod7(long num1) {
	int rem = (int)(num1 % 7);
	if (num1 < 0 && rem < 0) return rem + 7;
	return rem;
}

private static int[] getFields(long millis) {
	int dayCount = (int) (millis / 86400000);
	if (millis < 0 && (int)(millis % 86400000) != 0)
		dayCount--;

	int dayOfYear = dayCount;
	int year = 1970;
	int approxYears;

	while ((approxYears = dayOfYear / 365) != 0) {
		year = year + approxYears;
		dayOfYear = dayCount - daysFromBaseYear(year);
	}
	if (dayOfYear < 0) {
		year--;
		dayOfYear = dayOfYear + 365 + (isLeapYear(year) ? 1 : 0);
	}
	dayOfYear++;

	int month = dayOfYear / 32;
	boolean leapYear = isLeapYear(year);
	int day = dayOfYear - daysInYear(leapYear, month);
	if (day > daysInMonth(leapYear, month)) {
		day -= daysInMonth(leapYear, month);
		month++;
	}
	int dayOfWeek = mod7(dayCount - 3) + 1;
	return new int[] {year, month, day, dayOfWeek};
}

public static String printDate(long millis) {
	if (millis == 0) return "none";
	int[] fields = getFields(millis);
	StringBuffer buffer = new StringBuffer(25);
	int offset = (fields[3] - 1) * 3;
	buffer.append("SunMonTueWedThuFriSat".substring(offset, offset + 3));
	buffer.append(' ');
	offset = fields[1] * 3;
	buffer.append("JanFebMarAprMayJunJulAugSepOctNovDec".substring(offset, offset + 3));
	buffer.append(' ');
	format(buffer, fields[2], 2);
	buffer.append(' ');
	format(buffer, fields[0], 4);
	return buffer.toString();
}

private static void format(StringBuffer buffer, int value, int digits) {
	String out = String.valueOf(value);
	for (int i=0; i<digits - out.length(); i++)
		buffer.append('0');
	buffer.append(out);
}

}
