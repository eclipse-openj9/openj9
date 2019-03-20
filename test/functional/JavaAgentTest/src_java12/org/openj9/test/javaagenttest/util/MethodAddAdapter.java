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

import static jdk.internal.org.objectweb.asm.Opcodes.GETSTATIC;
import static jdk.internal.org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static jdk.internal.org.objectweb.asm.Opcodes.RETURN;
import static jdk.internal.org.objectweb.asm.Opcodes.ASM7;

import jdk.internal.org.objectweb.asm.Label;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;
import jdk.internal.org.objectweb.asm.util.CheckClassAdapter;

public class MethodAddAdapter extends CheckClassAdapter {

	public MethodAddAdapter(CheckClassAdapter cv) {
		super(ASM7, cv, true);
	}

	/* Used for instrumenting an existing method in java/lang/ClassLoader
	 * (non-Javadoc)
	 * @see jdk.internal.org.objectweb.asm.util.CheckClassAdapter#visitMethod(int, java.lang.String, java.lang.String, java.lang.String, java.lang.String[])
	 */
	@Override
	public MethodVisitor visitMethod(int access, String name, String desc, String signature, String[] exceptions ) {
	   if ( name.contains( "getResource" )) {
		   return new MethodInstrumentAdapter_CallNewMethod( access,
				   					name,
			   						desc,
			   						super.visitMethod(access, name, desc, signature, exceptions)
               			) ;
	   } else {
		   return super.visitMethod(access, name, desc, signature, exceptions);
	   }
	}

	/* Used for adding a new method in java/lang/ClassLoader class
	 * (non-Javadoc)
	 * @see jdk.internal.org.objectweb.asm.util.CheckClassAdapter#visitEnd()
	 */
	public void visitEnd() {
		Label L0 = new Label();
		Label L1 = new Label();

		MethodVisitor mv = cv.visitMethod( Opcodes.ACC_PUBLIC | Opcodes.ACC_STATIC, "myAddedMethod", "()V", null, null );
		mv.visitCode();
		mv.visitLabel(L0);
		mv.visitFieldInsn( GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;" );
		mv.visitLdcInsn( "**IN ADDED METHOD**");
		mv.visitMethodInsn( INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V" );

		mv.visitLabel(L1);
		mv.visitInsn( RETURN );

		mv.visitLocalVariable("this", "Ljava/lang/ClassLoader;", null, L0, L1, 0);
		/*mv.visitInsn( ICONST_1 );
		mv.visitFieldInsn( PUTSTATIC, "com/ibm/j9/refreshgccache/test/RefreshGCCacheTestMain", "addedSwitchHit", "Z" );
		mv.visitInsn( RETURN );
		*/
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		super.visitEnd();
	}
}
