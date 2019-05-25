/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
package com.ibm.dtfj.corereaders;

import java.io.IOException;

import javax.imageio.stream.ImageInputStream;


public class DumpFactory {
	public static ICoreFileReader createDumpForCore(ImageInputStream stream) throws IOException {
		return createDumpForCore(stream, false);
	}
	
	public static ICoreFileReader createDumpForCore(ImageInputStream stream, boolean verbose) throws IOException {
		// TODO try next format if there is an IOException thrown from the test
		ICoreFileReader corefile = null;
		try {
			if (NewWinDump.isSupportedDump(stream)) {
				corefile = NewWinDump.dumpFromFile(stream);
			} else if (NewElfDump.isSupportedDump(stream)) {
				corefile = NewElfDump.dumpFromFile(stream, verbose);
			} else if (NewAixDump.isSupportedDump(stream)) {
				corefile = NewAixDump.dumpFromFile(stream);
			} else if (NewZosDump.isSupportedDump(stream)) {
				corefile = NewZosDump.dumpFromFile(stream);
			}
		} catch (CorruptCoreException e) {
			//CMVC 161800 : only the z/OS core reader throws this exception and so propagate it back to the caller to retain the original error message
			throw new IOException("Core file was identified but found to be corrupt:  " + e.getMessage());
		}

		return corefile;
	}
}
