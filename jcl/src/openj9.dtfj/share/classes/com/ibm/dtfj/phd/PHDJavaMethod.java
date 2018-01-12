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

import java.util.Collections;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaMethod;

/** 
 * @author ajohnson
 */
public class PHDJavaMethod implements JavaMethod {

	private JavaClass cls;
	private CorruptData cls_cd;
	private String name;
	private CorruptData name_cd;
	private String sig;
	private CorruptData sig_cd;
	private int mods;
	private CorruptData mods_cd;
	
	/**
	 * Build Java method information from a JavaMethod from another dump type.
	 * Extract all the information on object construction.
	 * @param space
	 * @param cls1
	 * @param meta
	 */
	PHDJavaMethod(ImageAddressSpace space, JavaClass cls1, JavaMethod meta) {
		cls = cls1;
		try {
			name = meta.getName();
		} catch (CorruptDataException e) {
			name_cd = new PHDCorruptData(space, e);
		}
		try {
			sig = meta.getSignature();
		} catch (CorruptDataException e) {
			sig_cd = new PHDCorruptData(space, e);
		}		
		try {
			mods = meta.getModifiers();
		} catch (CorruptDataException e) {
			mods_cd = new PHDCorruptData(space, e);
		}
	}
	
	PHDJavaMethod(ImageAddressSpace space, PHDJavaRuntime runtime, JavaMethod meta) {
		this(space, (JavaClass)null, meta);
		try {
			JavaClass metacls = meta.getDeclaringClass();
			ImagePointer ip = metacls.getID();
			if (ip != null) {
				cls = runtime.findClass(ip.getAddress());
			}
			if (cls == null) {
				cls = runtime.findClass(metacls.getName());
			}
		} catch (CorruptDataException e) {
			cls_cd = new PHDCorruptData(space, e);
		} catch (DataUnavailable e) {
		}
	}

	public Iterator<ImageSection> getBytecodeSections() {
		return Collections.<ImageSection>emptyList().iterator();
	}

	public Iterator getCompiledSections() {
		return Collections.<ImageSection>emptyList().iterator();
	}

	public JavaClass getDeclaringClass() throws CorruptDataException,
			DataUnavailable {
		checkCD(cls_cd);
		if (cls == null) throw new DataUnavailable("No class");
		return cls;
	}

	public int getModifiers() throws CorruptDataException {
		checkCD(mods_cd);
		return mods;
	}

	public String getName() throws CorruptDataException {
		checkCD(name_cd);
		return name;
	}

	public String getSignature() throws CorruptDataException {
		checkCD(sig_cd);
		return sig;
	}

	public boolean equals(Object o) {
		if (o == null) return false;
		if (o.getClass() != getClass()) return false;
		PHDJavaMethod m = (PHDJavaMethod)o;
		return equals(cls, m.cls) && equals(name, m.name) && equals(sig, m.sig);
	}

	public int hashCode() {
		return hashCode(cls) ^ hashCode(name) ^ hashCode(sig);
	}
	
	private boolean equals(Object o1, Object o2) {
		return (o1 == null ? o2 == null : o1.equals(o2));
	}
	
	private int hashCode(Object o) {
		return o == null ? 0 : o.hashCode();
	}

	private void checkCD(CorruptData cd) throws CorruptDataException {
		if (cd != null) throw new CorruptDataException(cd);
	}
}
