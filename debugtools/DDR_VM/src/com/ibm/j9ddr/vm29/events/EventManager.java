/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

package com.ibm.j9ddr.vm29.events;

import java.util.LinkedList;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.events.IEventListener;

public class EventManager {
	private static Logger log = Logger.getLogger(EventManager.class.getPackage().getName());
	private static LinkedList<IEventListener> listeners = null;		//listeners that this manager will call
	private static DefaultEventListener defaultListener = null;
	
	static {
		listeners = new LinkedList<IEventListener>();
		defaultListener = new DefaultEventListener();		//create a default listener
	}
	
	public static void register(IEventListener listener) {
		if(!listeners.isEmpty()) {
			IEventListener top = listeners.peek();
			if(top == listener) {
				log.warning(String.format("The specified event listener was already registered : %s", listener.getClass().getName()));
				return;			//skip the registration
			}
		}
		listeners.addFirst(listener);
	}
	
	public static void unregister(IEventListener listener) {
		if(listeners.isEmpty()) {			//check that there are some entries on the stack
			log.warning("There are no listeners left on the stack, skipping unregistration");
			return;
		}
		IEventListener top = listeners.peek();
		if(top != listener) {			//check that the top of the stack is the same as being unregistered
			String msg = String.format("The top listener on the stack does not match : expected [%s] actual [%s]", 
					listener.getClass().getName(), top.getClass().getName());
			log.warning(msg);
			return;
		}
		listeners.removeFirst();
	}

	/**
	 * Signal that corrupt data was encountered
	 * @param message
	 * @param e
	 * @param isfatal
	 */
	public static void raiseCorruptDataEvent(String message, CorruptDataException e, boolean fatal) {
		if(listeners.isEmpty()) {							//no listeners, so use the default
			defaultListener.corruptData(message, e, fatal);
		} else {
			IEventListener listener = listeners.peek();
			listener.corruptData(message, e, fatal);			//send the event to the listener at the top of the stack
		}
	}
}
