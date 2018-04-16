package org.openj9.test.vm;

/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.util.Hashtable;
import java.io.IOException;
import com.ibm.oti.vm.MsgHelp;

@Test(groups = { "level.sanity" })
public class Test_MsgHelp {

	// Properties holding the system messages.
	static private Hashtable messages = null;

	static {
		// Attempt to load the messages.
		try {
			messages = MsgHelp.loadMessages("org/openj9/resources/openj9tr_ExternalTestMessages");
		} catch (IOException e) {
			Assert.fail("Cannot load message org/openj9/resources/openj9tr_ExternalTestMessages: " + e.toString());
		}
	}

	/**
	 * @tests com.ibm.oti.vm.MsgHelp#loadMessages(java.lang.String)
	 */
	@Test
	public void test_loadMessages_EN() {
		String msg;

		msg = Test_MsgHelp.getString("K0001", 1);
		AssertJUnit.assertFalse("Message returned cannot be null, should return 'K0001' if msg not found.",
				msg == null);
		AssertJUnit.assertTrue("Expected 'Error message 1: 1', found " + msg, msg.compareTo("Error message 1: 1") == 0);

		msg = Test_MsgHelp.getString("K0002");
		AssertJUnit.assertFalse("Message returned cannot be null, should return 'K0002' if msg not found.",
				msg == null);
		AssertJUnit.assertTrue("Expected 'Error message 2', found " + msg, msg.compareTo("Error message 2") == 0);

	}

	/**
	 * Retrieves a message which has no arguments.
	 *
	 * @author OTI
	 * @version initial
	 *
	 * @param msg
	 *            String the key to look up.
	 * @return String the message for that key in the system message bundle.
	 */
	static String getString(String msg) {
		// added to allow for re-loading new messages once default locale is changed
		if (messages != null) {
			if (messages.size() == 0)
				loadMessages();
		}

		if (messages == null)
			return msg;
		String resource = (String)messages.get(msg);
		if (resource == null)
			return msg;
		return resource;
	}

	/**
	 * Retrieves a message which takes 1 integer argument.
	 *
	 * @author OTI
	 * @version initial
	 *
	 * @param msg
	 *            String the key to look up.
	 * @param arg
	 *            int the integer to insert in the formatted output.
	 * @return String the message for that key in the system message bundle.
	 */
	static String getString(String msg, int arg) {
		return getString(msg, new Object[] { Integer.toString(arg) });
	}

	/**
	 * Retrieves a message which takes several arguments.
	 *
	 * @author OTI
	 * @version initial
	 *
	 * @param msg
	 *            String the key to look up.
	 * @param args
	 *            Object[] the objects to insert in the formatted output.
	 * @return String the message for that key in the system message bundle.
	 */
	static String getString(String msg, Object[] args) {
		String format = msg;

		// added to allow for re-loading new messages once default locale is changed
		if (messages != null) {
			if (messages.size() == 0)
				loadMessages();
		}

		if (messages != null) {
			format = (String)messages.get(msg);
			if (format == null)
				format = msg;
		}

		return MsgHelp.format(format, args);
	}

	/**
	 * Re-load messages (can be used with clearMessages() to load messages for a different locale)
	 */
	private static void loadMessages() {
		// Attempt to load the messages.
		try {
			messages = MsgHelp.loadMessages("org/openj9/resources/openj9tr_ExternalTestMessages");
		} catch (IOException e) {
			Assert.fail("Cannot load message org/openj9/resources/openj9tr_ExternalTestMessages: " + e.toString());
		}
	}

}
