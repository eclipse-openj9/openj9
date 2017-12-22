/*[INCLUDE-IF Sidecar16]*/

package com.ibm.oti.util;

/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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

import java.lang.reflect.AccessibleObject;
import java.security.Policy;
import java.security.PrivilegedAction;
import java.security.Security;
import java.util.*;
import java.io.IOException;
import com.ibm.oti.vm.MsgHelp;

/**
 * Helper class to avoid multiple anonymous inner class for
 * <code>{@link java.security.AccessController#doPrivileged(PrivilegedAction)}</code>
 * calls.
 * 
 * @author OTI
 * @version initial
 */
public class PriviAction implements PrivilegedAction {

    // unique id for each possible action; used in switch statement in run()
    private int action;

    private static final int GET_SYSTEM_PROPERTY = 1;
    private static final int GET_SECURITY_POLICY = 2;
    private static final int SET_ACCESSIBLE = 3;
    private static final int GET_SECURITY_PROPERTY = 4;
    private static final int LOAD_MESSAGES = 5;
    private static final int GET_BUNDLE = 6;

    // the possible argument types for all calls
    //(using generic Object args and cast appropriately in run() is more expensive) 
    private String stringArg1, stringArg2;
    private AccessibleObject accessible;
    private Locale locale;

    /**
     * Creates a PrivilegedAction to get the security
     * property with the given name. 
     * 
     * @param property	the name of the property
     * 
     * @see Security#getProperty
     */
    public static PrivilegedAction getSecurityProperty(String property) {
        return new PriviAction(GET_SECURITY_PROPERTY, property);
    }

   /**
     * Creates a PrivilegedAction to load messages
     * from the given name. 
     * 
     * @param resourceName	the name of the properties file
     * 
     * @see MsgHelp#loadMessages(String)
     */
    public static PrivilegedAction loadMessages(String resourceName) {
    	return new PriviAction(LOAD_MESSAGES, resourceName);
    }

    private PriviAction(int action, String arg) {
        this.action = action;
        stringArg1 = arg;
    }

    /**
     * Creates a PrivilegedAction to get the current security
     * policy object. 
     * 
     * @see Policy#getPolicy
     */
    public PriviAction() {
        action = GET_SECURITY_POLICY;
    }

    /**
     * Creates a PrivilegedAction to disable the access
     * checks to the given object. 
     * 
     * @param object	the object whose accessible flag
     * 					will be set to <code>true</code>
     * 
     * @see AccessibleObject#setAccessible(boolean)
     */
    public PriviAction(AccessibleObject object) {
        action = SET_ACCESSIBLE;
        accessible = object;
    }
    
    /**
     * Creates a PrivilegedAction to return the value of
     * the system property with the given key. 
     * 
     * @param property	the key of the system property
     * 
     * @see System#getProperty(String)
     */
    public PriviAction(String property) {
        action = GET_SYSTEM_PROPERTY;
        stringArg1 = property;
    }

    /**
     * Creates a PrivilegedAction to return the value of
     * the system property with the given key. 
     * 
     * @param property		the key of the system property
     * @param defaultAnswer	the return value if the system property
     * 						does not exist
     * 
     * @see System#getProperty(String, String)
     */
    public PriviAction(String property, String defaultAnswer) {
        action = GET_SYSTEM_PROPERTY;
        stringArg1 = property;
        stringArg2 = defaultAnswer;
    }

   /**
     * Creates a PrivilegedAction to load the resource bundle. 
     * 
     * @param bundleName	the name of the resource file
     * @param locale		the locale
     * 
     * @see ResourceBundle#getBundle(String, Locale)
     */
    public PriviAction(String bundleName, Locale locale) {
    	action = GET_BUNDLE;
    	stringArg1 = bundleName;
    	this.locale = locale;
    }

    /**
     * Performs the actual privileged computation as defined
     * by the constructor.
     * 
     * @see java.security.PrivilegedAction#run()
     */
    public Object run() {
        switch (action) {
        case GET_SYSTEM_PROPERTY:
            return System.getProperty(stringArg1, stringArg2);
        case GET_SECURITY_PROPERTY:
            return Security.getProperty(stringArg1);
        case GET_SECURITY_POLICY:
            return Policy.getPolicy();
        case LOAD_MESSAGES:
            try {
                return MsgHelp.loadMessages(stringArg1);
            } catch (IOException e) {
        		    e.printStackTrace();
        	   }
       		return null;
        case GET_BUNDLE:
        	   return ResourceBundle.getBundle(stringArg1, locale);
        case SET_ACCESSIBLE:
        	   accessible.setAccessible(true);
        	   // fall through
        }
        return null;
    }
}
