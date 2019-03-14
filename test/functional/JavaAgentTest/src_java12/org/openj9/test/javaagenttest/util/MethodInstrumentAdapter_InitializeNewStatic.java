/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package org.openj9.test.javaagenttest.util;
import jdk.internal.org.objectweb.asm.Label;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.commons.AdviceAdapter;
import org.openj9.test.javaagenttest.AgentMain;

public class MethodInstrumentAdapter_InitializeNewStatic extends AdviceAdapter
{
	private String _methodName = null;
	private String _className = null;

	public MethodInstrumentAdapter_InitializeNewStatic(int access, String name, String desc, MethodVisitor mv, String className)
	{
		super(ASM7, mv, access, name, desc);
		_methodName = name;
		_className = className;
	}

	@Override
	protected void onMethodEnter()
	{
		AgentMain.logger.debug("Class: MethodInstrumentAdapter_InitializeNewStatic is attempting to initialize new static field : " + _methodName);
		mv.visitLabel(new Label());

		/* initialize new integer array */
		mv.visitInsn(ICONST_5);
		mv.visitIntInsn(NEWARRAY, T_INT);
		mv.visitFieldInsn(PUTSTATIC, _className, "new_static_int_array", "[I");
	}
}
