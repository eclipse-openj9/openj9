/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.dtfj.java.JavaThread;

/** 
 * @author ajohnson
 */
public class PHDJavaThread implements JavaThread {

	private ImagePointer env;
	private CorruptData env_cd;
	private String name;
	private CorruptData name_cd;
	private int priority;
	private CorruptData priority_cd;
	private int state;
	private CorruptData state_cd;
	private List<JavaStackFrame>frames = new ArrayList<JavaStackFrame>();
	private JavaObject object;
	private CorruptData object_cd;
	private ImageThread imageThread;
	private CorruptData imageThread_cd;
	
	/**
	 * Build Java thread information from a JavaThread from another dump type.
	 * Extract all the information on object construction.
	 * @param space
	 * @param process The PHD process
	 * @param runtime The PHD runtime
	 * @param meta
	 */
	PHDJavaThread(ImageAddressSpace space, PHDImageProcess process, PHDJavaRuntime runtime, JavaThread meta) {
		try {
			env = space.getPointer(meta.getJNIEnv().getAddress());
		} catch (CorruptDataException e) {
			env_cd = new PHDCorruptData(space, e);
		}
		try {
			name = meta.getName();
		} catch (CorruptDataException e) {
			name_cd = new PHDCorruptData(space, e);
		}
		try {
			priority = meta.getPriority();
		} catch (CorruptDataException e) {
			priority_cd = new PHDCorruptData(space, e);
		}
		try {
			state = meta.getState();
		} catch (CorruptDataException e) {
			state_cd = new PHDCorruptData(space, e);
		}
		for (Iterator it = meta.getStackFrames(); it.hasNext(); ) {
			Object next = it.next();
			if (next instanceof CorruptData) {
				frames.add(new PHDCorruptJavaStackFrame(space, (CorruptData)next));
			} else {
				frames.add(new PHDJavaStackFrame(space, runtime, (JavaStackFrame)next));
			}
		}
		try {
			JavaObject obj = meta.getObject();
			if (obj != null) {
				try {
					object = runtime.getObjectAtAddress(space.getPointer(obj
							.getID().getAddress()));
				} catch (CorruptDataException e) {
					object_cd = e.getCorruptData();
				} catch (DataUnavailable e) {
				} catch (MemoryAccessException e) {
				}
			}
		} catch (CorruptDataException e) {
			object_cd = new PHDCorruptData(space, e);
		}
		try {
			ImageThread metaThread = meta.getImageThread();
			imageThread = process.getThread(space, metaThread);
		} catch (DataUnavailable e) {
		} catch (CorruptDataException e) {
			imageThread_cd = new PHDCorruptData(space, e);
		}
	}
	
	public ImageThread getImageThread() throws CorruptDataException,
			DataUnavailable {
		if (imageThread_cd != null) throw new CorruptDataException(imageThread_cd);
		if (imageThread == null) throw new DataUnavailable();
		return imageThread;
	}

	public ImagePointer getJNIEnv() throws CorruptDataException {
		checkCD(env_cd);
		return env;
	}

	public String getName() throws CorruptDataException {
		checkCD(name_cd);
		return name;
	}

	public JavaObject getObject() throws CorruptDataException {
		checkCD(object_cd);
		return object;
	}

	public int getPriority() throws CorruptDataException {
		checkCD(priority_cd);
		return priority;
	}

	public Iterator<JavaStackFrame> getStackFrames() {
		return frames.iterator();
	}

	public Iterator<ImageSection> getStackSections() {
		return Collections.<ImageSection>emptyList().iterator();
	}

	public int getState() throws CorruptDataException {
		checkCD(state_cd);
		return state;
	}

	private void checkCD(CorruptData cd) throws CorruptDataException {
		if (cd != null) throw new CorruptDataException(cd);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getBlockingObject()
	 */
	public JavaObject getBlockingObject() throws CorruptDataException, DataUnavailable {
		throw new DataUnavailable("Not Available.");
	}
}
