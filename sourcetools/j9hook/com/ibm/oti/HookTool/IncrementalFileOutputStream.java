/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.oti.HookTool;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FilterOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public final class IncrementalFileOutputStream extends FilterOutputStream {

	private final File outputFile;

	public IncrementalFileOutputStream(File outputFile) {
		super(new ByteArrayOutputStream());
		this.outputFile = outputFile;
	}

	@Override
	public void close() throws IOException {
		if (out != null) {
			try {
				super.close();

				byte[] desired = ((ByteArrayOutputStream) out).toByteArray();

				if (!fileMatches(desired)) {
					try (OutputStream newOut = new FileOutputStream(outputFile)) {
						newOut.write(desired);
					}
				}
			} finally {
				out = null;
			}
		}
	}

	private boolean fileMatches(byte[] desired) throws IOException {
		try (InputStream existing = new FileInputStream(outputFile)) {
			byte[] buffer = new byte[4096];
			int offset = 0;
			int remaining = desired.length;

			for (;;) {
				int count = existing.read(buffer);

				if (count < 0) {
					// EOF
					return remaining == 0;
				}

				if (count > remaining) {
					// the existing file is too long
					return false;
				}

				for (int i = 0; i < count; ++i, ++offset) {
					if (buffer[i] != desired[offset]) {
						// we found a difference
						return false;
					}
				}

				remaining -= count;
			}
		} catch (FileNotFoundException e) {
			// the file does not exist
			return false;
		}
	}

	@Override
	public void write(byte[] buffer, int offset, int length) throws IOException {
		out.write(buffer, offset, length);
	}

}
