/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
package com.ibm.jpp.om;

import java.io.File;
import java.io.IOException;

/**
 * JIT Attributes Extension
 */
public class JitAttributesExtension extends BuilderExtension {

	private final JitAttributes jitAttributes = new JitAttributes();
	protected String jitAttributesFilePath = "java" + File.separator + "lang" + File.separator + "jit.txt";

	/**
	 * Constructor for JitAttributesExtension.
	 */
	public JitAttributesExtension() {
		super("jit");
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#notifyConfigurePreprocessor(JavaPreprocessor)
	 */
	@Override
	public void notifyConfigurePreprocessor(JavaPreprocessor preprocessor) {
		preprocessor.setJitAttributes(this.jitAttributes);
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#notifyBuildEnd()
	 */
	@Override
	public void notifyBuildEnd() {
		try {
			jitAttributes.write(builder.getOutputDir().getAbsolutePath() + File.separator + jitAttributesFilePath);
		} catch (IOException e) {
			builder.getLogger().log("An exception occured while writing the jit attributes file", Logger.SEVERITY_ERROR, e);
		}
	}

}
