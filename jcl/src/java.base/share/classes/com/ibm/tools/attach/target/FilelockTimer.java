/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;

/*******************************************************************************
 * Copyright (c) 2009, 2010 IBM Corp. and others
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

import java.lang.reflect.Field;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Timer;


/**
 * Allows us to ensure the timer thread is dead,
 *
 */
/*[PR CMVC 169171  java.util.Timer does not guarantee thread shutdown ]*/
final class FilelockTimer extends Timer {

	volatile Field timerThreadField;

	/**
	 * @param name Thread name
	 */
	FilelockTimer(String name) {
		super(name, true);

		timerThreadField = AccessController.doPrivileged(new PrivilegedAction<Field>() {
			@Override
			public Field run() {
				try {
					/*[PR CMVC 194420, 195081 Java 7&8 use the RI Timer implementation which calls the thread object "thread"]*/
					String timerThreadFieldName = "thread"; //$NON-NLS-1$
					final Field impl = Timer.class.getDeclaredField(timerThreadFieldName);
					impl.setAccessible(true);
					return impl;
				} catch (Exception e) {
					IPC.logMessage("FilelockTimer: get field exception "+e.getClass().getName()+":"+e.getMessage()); //$NON-NLS-1$ //$NON-NLS-2$
				}
				return null;
			}
		});
	}

	@Override
	public void cancel() {
		super.cancel();
		if (null != timerThreadField) {
			try {
				Thread timerThread = (Thread) timerThreadField.get(this);
				if (null != timerThread) {
					timerThread.join(10000);	/* timeout in ms*/	
				}
			} catch (Exception e) {
				IPC.logMessage("Exception in FilelockTimer.cancel: ", e.getClass().getName()+":"+e.getMessage()); //$NON-NLS-1$ //$NON-NLS-2$
			}
		}
	}
}

