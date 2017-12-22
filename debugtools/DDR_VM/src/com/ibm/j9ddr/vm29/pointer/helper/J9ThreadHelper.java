/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.pointer.helper;

import java.lang.ref.WeakReference;
import java.util.Iterator;
import java.util.Map;
import java.util.TreeMap;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OMR_VMThreadPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class J9ThreadHelper {
	
	private static WeakReference<Map<Long, IOSThread>> cachedThreads = null;
	
	public static VoidPointer getTLS(J9ThreadPointer threadPointer, UDATA key) throws CorruptDataException
	{
		return VoidPointer.cast(threadPointer.tlsEA().at(key.sub(1)));
	}
	
	public static J9VMThreadPointer getVMThread(J9ThreadPointer threadPointer) throws CorruptDataException
	{
		J9VMThreadPointer vmThread = null;
		OMR_VMThreadPointer omrVmThread = OMR_VMThreadPointer.cast(getTLS(threadPointer, J9RASHelper.getVM(DataType.getJ9RASPointer()).omrVM()._vmThreadKey()));
		if (omrVmThread.isNull()) {
			vmThread = J9VMThreadPointer.NULL;
		} else {
			vmThread = J9VMThreadPointer.cast(omrVmThread._language_vmthread());
		}
		return vmThread;
	}
	
	public static IOSThread getOSThread(J9ThreadPointer threadPointer) throws CorruptDataException {
		return getOSThread(threadPointer.tid().longValue());
	}

	public static IOSThread getOSThread(long tid) throws CorruptDataException {
		Map<Long, IOSThread> threadMap = getThreadMap();
		return threadMap.get(tid);
	}

	public static Iterator<IOSThread> getOSThreads() throws CorruptDataException {
		Map<Long, IOSThread> threadMap = getThreadMap();
		return threadMap.values().iterator();
	}

	private static Map<Long, IOSThread> getThreadMap() throws CorruptDataException {
		Map<Long, IOSThread> thrMap = null;
		if (cachedThreads != null) {
			thrMap = cachedThreads.get();
		}
		if (thrMap != null) {
			return thrMap;
		}

		/*
		 * There was no cache of threads, populate a new one while we find the
		 * thread the caller wanted.
		 */
		thrMap = new TreeMap<Long, IOSThread>();
		for (IOSThread thread : DataType.getProcess().getThreads()) {
			thrMap.put(thread.getThreadId(), thread);
		}
		cachedThreads = new WeakReference<Map<Long, IOSThread>>(thrMap);
		return thrMap;
	}

}
