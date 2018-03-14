/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

public class J9ThreadMonitorHelper {

	public static J9ThreadPointer getBlockingField(J9ThreadMonitorPointer monitor) throws CorruptDataException {
		Exception exception;
		try {
			Class<?> monitorClazz = J9ThreadMonitorPointer.class;
			Method getBlocking = monitorClazz.getDeclaredMethod("blocking");
			J9ThreadPointer blocking = (J9ThreadPointer) getBlocking.invoke(monitor);
			return blocking;
		} catch (NoSuchMethodException e) {
			exception = e;
		} catch (IllegalAccessException e) {
			exception = e;
		} catch (InvocationTargetException e) {
			Throwable cause = e.getCause();

			if (cause instanceof CorruptDataException) {
				throw (CorruptDataException) cause;
			}

			exception = e;
		}

		// unexpected exception using reflection
		CorruptDataException cd = new CorruptDataException(exception.toString(), exception);
		raiseCorruptDataEvent("Error accessing J9ThreadMonitorPointer", cd, true);

		return null;
	}

	public static J9ThreadPointer getBlockingField(J9ThreadAbstractMonitorPointer monitor) throws CorruptDataException {
		Exception exception;
		try {
			Class<?> monitorClazz = J9ThreadAbstractMonitorPointer.class;
			Method getBlocking = monitorClazz.getDeclaredMethod("blocking");
			J9ThreadPointer blocking = (J9ThreadPointer) getBlocking.invoke(monitor);
			return blocking;
		} catch (NoSuchMethodException e) {
			exception = e;
		} catch (IllegalAccessException e) {
			exception = e;
		} catch (InvocationTargetException e) {
			Throwable cause = e.getCause();

			if (cause instanceof CorruptDataException) {
				throw (CorruptDataException) cause;
			}

			exception = e;
		}

		// unexpected exception using reflection
		CorruptDataException cd = new CorruptDataException(exception.toString(), exception);
		raiseCorruptDataEvent("Error accessing J9ThreadAbstractMonitorPointer", cd, true);

		return null;
	}
}
