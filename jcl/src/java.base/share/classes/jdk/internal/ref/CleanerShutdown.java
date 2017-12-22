/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
package jdk.internal.ref;

import jdk.internal.ref.CleanerFactory;

import java.lang.ref.Cleaner;
import java.lang.ref.Cleaner.Cleanable;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.security.AccessController;
import java.security.PrivilegedAction;

@SuppressWarnings("javadoc")
public class CleanerShutdown {
	
	public static void shutdownCleaner() {
		Cleaner commonCleaner = CleanerFactory.cleaner();
		CleanerImpl commonCleanerImpl = CleanerImpl.getCleanerImpl(commonCleaner);
		Cleanable ref = null;

		while ((ref = (Cleanable) commonCleanerImpl.queue.poll()) != null) {
			try {
				ref.clean();
			} catch (Throwable t) {
				/* do nothing */
			}
		}
		
		try {
			Method phantomRemove = PhantomCleanable.class.getDeclaredMethod("remove", (Class<?>[]) null); //$NON-NLS-1$
			Method weakRemove = WeakCleanable.class.getDeclaredMethod("remove", (Class<?>[]) null); //$NON-NLS-1$
			Method softRemove = SoftCleanable.class.getDeclaredMethod("remove", (Class<?>[]) null); //$NON-NLS-1$
			AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
				phantomRemove.setAccessible(true);
				weakRemove.setAccessible(true);
				softRemove.setAccessible(true);
				return null;
			});
			while(!commonCleanerImpl.phantomCleanableList.isListEmpty()) {
				phantomRemove.invoke(commonCleanerImpl.phantomCleanableList, (Object[]) null);
			}

			while(!commonCleanerImpl.weakCleanableList.isListEmpty()) {
				phantomRemove.invoke(commonCleanerImpl.weakCleanableList, (Object[]) null);
			}

			while(!commonCleanerImpl.softCleanableList.isListEmpty()) {
				phantomRemove.invoke(commonCleanerImpl.softCleanableList, (Object[]) null);
			}
		} catch (NoSuchMethodException
				| SecurityException
				| IllegalAccessException
				| IllegalArgumentException
				| InvocationTargetException e)
		{
			/* should not fail */
			e.printStackTrace();
		}
	}
}
