/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.J9VMThreadPointerUtil;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.structure.J9Consts;

public class J9VMThreadHelper
{
	
	public static int getDTFJState(J9VMThreadPointer thread) throws CorruptDataException
	{
		return J9VMThreadPointerUtil.getDTFJState(thread);
	}
	
	public static String getName(J9VMThreadPointer thread)
	{
		String threadName = "<unnamed thread>";
		
		try {
			if (thread.omrVMThread().threadName().notNull()) {
				threadName = String.format("\"%s\"", thread.omrVMThread().threadName().getCStringAtOffset(0));
			}
		} catch (CorruptDataException e) {
			threadName = "<FAULT>";
		}
		
		return threadName;
	}

	public static boolean waiting(J9VMThreadPointer thread) throws CorruptDataException {
		switch ((int)J9Consts.J9_PUBLIC_FLAGS_VERSION) {
		case 0:
			return thread.publicFlags().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS_THREAD_WAITING);
		case 1:
			try {
				return thread.publicFlags2().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS2_THREAD_WAITING);
			} catch (NoSuchFieldException e) {
				throw new CorruptDataException(e);
			}
		default:
			throw new CorruptDataException("Unexpected J9Consts.J9_PUBLIC_FLAGS_VERSION");
		}
	}

	public static boolean blocked(J9VMThreadPointer thread) throws CorruptDataException {
		switch ((int)J9Consts.J9_PUBLIC_FLAGS_VERSION) {
		case 0:
			return thread.publicFlags().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS_THREAD_BLOCKED);
		case 1:
			try {
				return thread.publicFlags2().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS2_THREAD_BLOCKED);
			} catch (NoSuchFieldException e) {
				throw new CorruptDataException(e);
			}
		default:
			throw new CorruptDataException("Unexpected J9Consts.J9_PUBLIC_FLAGS_VERSION");
		}
	}

	public static boolean sleeping(J9VMThreadPointer thread) throws CorruptDataException {
		switch ((int)J9Consts.J9_PUBLIC_FLAGS_VERSION) {
		case 0:
			return thread.publicFlags().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS_THREAD_SLEEPING);
		case 1:
			try {
				return thread.publicFlags2().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS2_THREAD_SLEEPING);
			} catch (NoSuchFieldException e) {
				throw new CorruptDataException(e);
			}
		default:
			throw new CorruptDataException("Unexpected J9Consts.J9_PUBLIC_FLAGS_VERSION");
		}
	}

	public static boolean parked(J9VMThreadPointer thread) throws CorruptDataException {
		switch ((int)J9Consts.J9_PUBLIC_FLAGS_VERSION) {
		case 0:
			return thread.publicFlags().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS_THREAD_PARKED);
		case 1:
			try {
				return thread.publicFlags2().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS2_THREAD_PARKED);
			} catch (NoSuchFieldException e) {
				throw new CorruptDataException(e);
			}
		default:
			throw new CorruptDataException("Unexpected J9Consts.J9_PUBLIC_FLAGS_VERSION");
		}
	}

	public static boolean timed(J9VMThreadPointer thread) throws CorruptDataException {
		switch ((int)J9Consts.J9_PUBLIC_FLAGS_VERSION) {
		case 0:
			return thread.publicFlags().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS_THREAD_TIMED);
		case 1:
			try {
				return thread.publicFlags2().anyBitsIn(J9Consts.J9_PUBLIC_FLAGS2_THREAD_TIMED);
			} catch (NoSuchFieldException e) {
				throw new CorruptDataException(e);
			}
		default:
			throw new CorruptDataException("Unexpected J9Consts.J9_PUBLIC_FLAGS_VERSION");
		}
	}
}
