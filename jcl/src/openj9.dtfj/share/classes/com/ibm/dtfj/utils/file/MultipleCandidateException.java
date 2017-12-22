/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.dtfj.utils.file;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Thrown when there is more than one core file present in a zip
 * file and the calling application has not specified which entry
 * should be used.
 * 
 * @author adam
 *
 */
public class MultipleCandidateException extends IOException {
	private static final long serialVersionUID = 4233613937969520082L;
	private List<ManagedImageSource> candidates = new ArrayList<ManagedImageSource>();
	private File file = null;			//file which contained multiple candidates

	public MultipleCandidateException(List<ManagedImageSource> candidates, File file) {
		super("More than one core file was detected in " + file.getAbsolutePath());
		this.candidates = candidates;
	}

	public MultipleCandidateException(String arg0) {
		super(arg0);
	}

	public MultipleCandidateException(Throwable arg0) {
		super();
		initCause(arg0);
	}

	public MultipleCandidateException(String arg0, Throwable arg1) {
		super(arg0);
		initCause(arg1);
	}

	public List<ManagedImageSource> getCandidates() {
		return candidates;
	}

	public File getFile() {
		return file;
	}
	
	
}

