/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.oti.NLSTool;

/**
 * @author PBurka
 */
public interface NLSConstants {
	String SUFFIX_USER_RESPONSE = ".user_response";
	String SUFFIX_SYSTEM_ACTION = ".system_action";
	String SUFFIX_EXPLANATION = ".explanation";
	String SUFFIX_LINK = ".link";
	String DEFAULT_LOCALE = "";
	String MODULE_KEY = "J9NLS.MODULE";
	String HEADER_KEY = "J9NLS.HEADER";

	int DEFAULT_MODULE_LENGTH = 4;	
	int DEFAULT_MESSAGE_NUMBER_LENGTH = 3;
	int MAX_NUMBER_OF_MESSAGES_PER_MODULE = (int)Math.pow(10, DEFAULT_MESSAGE_NUMBER_LENGTH);
}
