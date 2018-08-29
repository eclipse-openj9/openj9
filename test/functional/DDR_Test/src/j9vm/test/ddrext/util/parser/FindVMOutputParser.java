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

import java.util.StringTokenizer;
import org.testng.log4testng.Logger;


/**
 * @author fkaraman
 *
 * This class parses the !findVM DDR extension output and finds/returns the specific value from the output. 
 * 
 * Sample output of !findVM
 * 
 * !j9javavm 0x000BAE08
 * 
 */
public class FindVMOutputParser {
	
	private static Logger log = Logger.getLogger(FindVMOutputParser.class);
	
	/**
	 * This method is used to extract the j9javavm address from !findvm output. 
	 * 
	 * @param findVMOutput Output of !findvm extension run.  
	 * @return String representation of extracted java vm address from !findvm extension. 
	 *         return null, if any error occurs or address can not be found in given !findVM output. 	
	 */
	public static String getJ9JavaVMAddress(String findVMOutput) {
		if (null == findVMOutput) {
			log.error("!findvm output is null");
			return null;
		}
		StringTokenizer tokenizer = new StringTokenizer(findVMOutput);
		String token; 
		/**
		 * Check whether !findvm output starts with !j9javavm 
		 */
		if (tokenizer.hasMoreTokens()) {
			token = tokenizer.nextToken();
			/*Do sanity check for the first token. It should be !j9javavm */
			if (!token.equals("!" + Constants.J9JAVAVM_CMD)) {
				log.error("Unexpected start of !findVM output. Expected : !" + Constants.J9JAVAVM_CMD + "Found: " + token + "\nOutput: \n" + findVMOutput);
				return null;
			}
		} else {
			log.error("!" + Constants.FINDVM_CMD + " output is empty.");
			return null;
		}
		
		/* Next token after !j9javavm should be the vm address */
		if (tokenizer.hasMoreTokens()) {
			token = tokenizer.nextToken();
			return token;
		} else {
			log.error("j9javavm address is missing in the output : " + findVMOutput);
			return null;	
		}
	}

}
