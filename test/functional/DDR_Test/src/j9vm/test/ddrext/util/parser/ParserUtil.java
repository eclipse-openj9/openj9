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
package j9vm.test.ddrext.util.parser;

import j9vm.test.ddrext.Constants;

import java.math.BigInteger;
import java.util.StringTokenizer;
import java.util.regex.Pattern;

import org.testng.log4testng.Logger;

/**
 * This class offers utilities for parsing DDR extension outputs to extract info.
 *
 * @author fkaraman
 */
public class ParserUtil {

	private static Logger log = Logger.getLogger(FindVMOutputParser.class);

	/**
	 * This method is being used to convert the given string into a BigInteger instance.
	 * This method does not do check for value being null,
	 * it is callers responsibility to make it sure that it is not null.
	 *
	 * @param value String representation of a number
	 * @throws NumberFormatException If the given string value is not a decimal or hexadecimal number
	 */
	public static BigInteger toBigInteger(String value) throws NumberFormatException {
		BigInteger bigInt;

		/* get rid of the spaces at the beginning and end if there are any */
		value = value.trim();

		/* Check whether the value is hex or not */
		if (value.startsWith(Constants.HEXADDRESS_HEADER)) {
			bigInt = new BigInteger(value.substring(Constants.HEXADDRESS_HEADER.length()), Constants.HEXADECIMAL_RADIX);
		} else {
			bigInt = new BigInteger(value, Constants.DECIMAL_RADIX);
		}

		return bigInt;
	}

	/**
	 *	This method finds the address of the field with specified signature and structureType.
	 *	struct J9VMThread* mainThread = !j9vmthread 0x7cc8900
	 *
	 *	In the case where structureType is null, then it finds the value for the field with specified signature.
	 *  UDATA parm->j2seVersion = 0x111700
	 *
	 * @param signature
	 * @param structureType
	 * @param extensionOutput !j9javavm output.
	 * @return address or value for the field with specified signature
	 */
	public static String getFieldAddressOrValue(String signature, String structureType, String extensionOutput) {
		return getFieldAddressOrValue(Pattern.compile(signature, Pattern.LITERAL), structureType, extensionOutput);
	}

	/**
	 * This method finds the address of the field with a signature matching the given pattern
	 * and the specified structureType.
	 * struct J9VMThread* mainThread = !j9vmthread 0x7cc8900
	 *
	 * In the case where structureType is null, then it finds the value for the field with specified signature.
	 * UDATA parm->j2seVersion = 0x111700
	 *
	 * @param pattern
	 * @param structureType
	 * @param extensionOutput !j9javavm output
	 * @return address or value for the field with specified signature
	 */
	public static String getFieldAddressOrValue(Pattern pattern, String structureType, String extensionOutput) {
		if (null == extensionOutput) {
			log.error("!j9javavm output is null");
			return null;
		}

		String prefixToken = (null == structureType) ? "=" : ("!" + structureType);
		String[] lines = extensionOutput.split("\n");
		for (String currentLine : lines) {
			if (!pattern.matcher(currentLine.trim()).find()) {
				continue;
			}

			/* The pattern has been found. */
			StringTokenizer tokenizer = new StringTokenizer(currentLine);
			while (tokenizer.hasMoreElements()) {
				String token = tokenizer.nextToken();
				/*
				 * The value after the prefixToken is the value for the field with the specified signature.
				 */
				if (token.equals(prefixToken)) {
					if (tokenizer.hasMoreElements()) {
						return tokenizer.nextToken();
					} else {
						/* Should never happen */
						log.error("Value is missing after " + prefixToken + " in debug extension output: " + currentLine);
						return null;
					}
				}
			}
			log.error("Unexpected info line in debug extension output. Missing string " + prefixToken + " in: " + currentLine);
			return null;
		}

		log.error("ParserUtil: missing line " + pattern + ", structure type " + structureType
				+ ", in debug extension output:\n" + extensionOutput);
		return null;
	}

	/**
	 * This method is being used to extract the first method name from !shrc
	 * jitpstats output
	 */
	public static String getMethodNameFromJITPStats(String jitpStatsOutput) {
		String[] lines = jitpStatsOutput.split(Constants.NL);

		if (lines.length == 1) {
			lines = jitpStatsOutput.split("\n");
		}

		for (int i = 0; i < lines.length; i++) {
			int index = lines[i].indexOf("(");
			if ((index != -1) && (lines[i].indexOf("!j9rommethod") != -1)) {
				return lines[i].substring(0, index).trim();
			}
		}

		return null;
	}

	/**
	 * This method is being used to extract the first method name from !shrc
	 * jitpstats output
	 */
	public static String getMethodAddrForName(String jitpStatsOutput,
			String methodFindInjitpStats) {
		String[] lines = jitpStatsOutput.split(Constants.NL);

		if (lines.length == 1) {
			lines = jitpStatsOutput.split("\n");
		}

		for (int i = 0; i < lines.length; i++) {
			if ((lines[i].indexOf(methodFindInjitpStats) != -1)
					&& (lines[i].indexOf("!j9rommethod") != -1)) {
				return lines[i].split("j9rommethod")[1].trim();
			}
		}

		return null;
	}

	/**
	 * This method is used to remove leading zeroes in an address.
	 * Eg input = 0x00007FECADCF23A0
	 *    output = 0x7FECADCF23A0
	 */
	public static String removeExtraZero(String address) {
		int nonZeroIndex = 2;
		for (int j = 2; j < address.length(); j++) {
			if (address.charAt(j) != '0') {
				nonZeroIndex = j;
				break;
			}
		}
		return "0x" + address.substring(nonZeroIndex);
	}

}
