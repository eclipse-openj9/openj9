/*[INCLUDE-IF Sidecar16]*/

package com.ibm.oti.util;

/*******************************************************************************
 * Copyright (c) 1998, 2014 IBM Corp. and others
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

import java.security.AccessController;
import java.util.*;
import com.ibm.oti.vm.*;

/**
 * This class retrieves strings from a resource bundle
 * and returns them, formatting them with MessageFormat
 * when required.
 * <p>
 * It is used by the system classes to provide national
 * language support, by looking up messages in the
 * <code>
 *    com.ibm.oti.util.ExternalMessages
 * </code>
 * resource bundle. Note that if this file is not available,
 * or an invalid key is looked up, or resource bundle support
 * is not available, the key itself will be returned as the 
 * associated message. This means that the <em>KEY</em> should
 * a reasonable human-readable (english) string.
 *
 * @author		OTI
 * @version		initial
 */

// Declaration of external messages to support the j9zip.jar file for the -Xzero option
/*[PR VMDESIGN 1705] Force the external messages for Zero into Java6 JCLs */ 
/*[MSG "K0059", "Stream is closed"]*/
/*[MSG "K00b7", "File is closed"]*/
/*[MSG "K01c3", "Unable to open: {0}"]*/
/*[MSG "K01c4", "Invalid zip file: {0}"]*/

public class Msg {

	// Properties holding the system messages.
	static private Hashtable messages;
	
	static {
		// Attempt to load the messages.
		messages = (Hashtable) AccessController.doPrivileged(
				PriviAction.loadMessages("com/ibm/oti/util/ExternalMessages")); //$NON-NLS-1$
	}


	/**
	 * Retrieves a message which has no arguments.
	 *
	 * @author		OTI
	 * @version		initial
	 *
	 * @param		msg String	
	 *					the key to look up.
	 * @return		String
	 *					the message for that key in the system
	 *					message bundle.
	 */
	static public String getString (String msg) {
		if (messages == null)
			return msg;
		String resource = (String)messages.get(msg);
		if (resource == null)
			return msg;
		return resource;
	}
	
	/**
     * Retrieves a message which takes 1 argument.
     *
     * @author      OTI
     * @version     initial
     *
     * @param       msg String  
     *                  the key to look up.
     * @param       arg Object
     *                  the object to insert in the formatted output.
     * @return      String
     *                  the message for that key in the system
     *                  message bundle.
     */
    static public String getString (String msg, Object arg) {
        String format = msg;
    
        if (messages != null) {
            format = (String) messages.get(msg);
            if (format == null) format = msg;
        }
    
        return MsgHelp.format(format, arg);
    }
    
	/**
	 * Retrieves a message which takes 1 integer argument.
	 *
	 * @author		OTI
	 * @version		initial
	 *
	 * @param		msg String	
	 *					the key to look up.
	 * @param		arg int
	 *					the integer to insert in the formatted output.
	 * @return		String
	 *					the message for that key in the system
	 *					message bundle.
	 */
	static public String getString (String msg, int arg) {
		return getString(msg, Integer.toString(arg));
	}
	
	/**
	 * Retrieves a message which takes 1 character argument.
	 *
	 * @author		OTI
	 * @version		initial
	 *
	 * @param		msg String	
	 *					the key to look up.
	 * @param		arg char
	 *					the character to insert in the formatted output.
	 * @return		String
	 *					the message for that key in the system
	 *					message bundle.
	 */
	static public String getString (String msg, char arg) {
		return getString(msg, String.valueOf(arg));
	}
	
	/**
	 * Retrieves a message which takes 2 arguments.
	 *
	 * @author		OTI
	 * @version		initial
	 *
	 * @param		msg String	
	 *					the key to look up.
	 * @param		arg1 Object
	 *					an object to insert in the formatted output.
	 * @param		arg2 Object
	 *					another object to insert in the formatted output.
	  * @return		String
	 *					the message for that key in the system
	 *					message bundle.
	 */
	static public String getString (String msg, Object arg1, Object arg2) {
		return getString(msg, new Object[] {arg1, arg2});
	}
	
	/**
	 * Retrieves a message which takes several arguments.
	 *
	 * @author		OTI
	 * @version		initial
	 *
	 * @param		msg String	
	 *					the key to look up.
	 * @param		args Object[]
	 *					the objects to insert in the formatted output.
	 * @return		String
	 *					the message for that key in the system
	 *					message bundle.
	 */
	static public String getString (String msg, Object[] args) {
		String format = msg;
	
		if (messages != null) {
			format = (String) messages.get(msg);
			if (format == null) {
				format = msg;
			}
		}
	
		return MsgHelp.format(format, args);
	}

	/**
	 * Retrieves a message which takes several arguments.
	 *
	 * @param		msg String	
	 *					the key to look up.
	 * @param		defaultMsg String	
	 *					the default format string if null is returned from key look up or messages hashtable is null.
	 * @param		args Object[]
	 *					the objects to insert in the formatted output.
	 * @return		String
	 *					the message for that key in the system
	 *					message bundle.
	 */
	static public String getString (String msg, String defaultMsg, Object[] args) {
		String format = null;
		if (messages != null) {
			format = (String) messages.get(msg);
		}
		if (null == format) {
			format = defaultMsg;
		}
		return MsgHelp.format(format, args);
	}
	
	/**
	 * Retrieves a message which takes 3 arguments.
	 *
	 * @author		OTI
	 * @version		initial
	 *
	 * @param		msg String	
	 *					the key to look up.
	 * @param		arg1 Object
	 *					an object to insert in the formatted output.
	 * @param		arg2 Object
	 *					another object to insert in the formatted output.
	 * @param		arg3 Object
	 *					another object to insert in the formatted output.
	  * @return		String
	 *					the message for that key in the system
	 *					message bundle.
	 */

	static public String getString(String msg, Object arg1, Object arg2, String arg3) {
		return getString(msg, new Object[] {arg1, arg2, arg3});
	}
}
