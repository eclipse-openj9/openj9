/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
package org.openj9.test.invoker.util;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.Hashtable;

import javassist.bytecode.ClassFile;
import javassist.bytecode.CodeIterator;
import javassist.bytecode.MethodInfo;
import javassist.bytecode.Opcode;

import org.objectweb.asm.*;

public class DummyClassGenerator extends ClassGenerator {

	public static final String INVOKE_STATIC = "invokeStatic";
	public static final String INVOKE_SPECIAL = "invokeSpecial";
	public static final String INVOKE_VIRTUAL = "invokeVirtual";
	public static final String INVOKE_INTERFACE = "invokeInterface";

	/*
	 * This method is to workaround a limitation of ASM which prevents
	 * generating class file where invokeinterface and invoke[static|special]
	 * bytecodes share same constant pool entry.
	 * 
	 * To overcome this limitation, after the ASM has generated class data, the
	 * operand of invoke[static|special] bytecodes is modified to be same as the
	 * operand of invokeinterface bytecode. It uses javassist APIs to make this
	 * modification.
	 */
	protected byte[] modifyInvokerOperand() throws Exception {
		int cpIndex = 0;
		ByteArrayInputStream bais = new ByteArrayInputStream(data);
		ClassFile classFile = new ClassFile(new DataInputStream(bais));
		String sharedInvokers[] = (String[])characteristics.get(SHARED_INVOKERS);

		/* Record operand of invokeinterface in invokeInterface() */
		for (int i = 0; i < sharedInvokers.length; i++) {
			String methodName = sharedInvokers[i];
			if (methodName.equals(INVOKE_INTERFACE)) {
				MethodInfo methodInfo = classFile.getMethod(methodName);
				CodeIterator ci = methodInfo.getCodeAttribute().iterator();
				while (ci.hasNext()) {
					int bcIndex = ci.next();
					int bc = ci.byteAt(bcIndex);
					if (bc == Opcode.INVOKEINTERFACE) {
						/* found invokeinterface bytecode */
						cpIndex = ci.s16bitAt(bcIndex + 1);
						break;
					}
				}
			}
		}

		/*
		 * Modify operand of invokestatic/invokespecial to cpIndex recorded
		 * above
		 */
		for (int i = 0; i < sharedInvokers.length; i++) {
			String methodName = sharedInvokers[i];
			if (methodName.equals(INVOKE_SPECIAL) || (methodName.equals(INVOKE_STATIC))) {
				MethodInfo methodInfo = classFile.getMethod(methodName);
				CodeIterator ci = methodInfo.getCodeAttribute().iterator();
				while (ci.hasNext()) {
					int bcIndex = ci.next();
					int bc = ci.byteAt(bcIndex);
					if ((bc == Opcode.INVOKESPECIAL) || (bc == Opcode.INVOKESTATIC)) {
						/*
						 * found invokespecial or invokestatic bytecode; update
						 * the operand constant pool index
						 */
						ci.write16bit(cpIndex, bcIndex + 1);
					}
				}
			}
		}

		ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
		DataOutputStream dataStream = new DataOutputStream(byteStream);
		classFile.write(dataStream);
		return byteStream.toByteArray();
	}

	public DummyClassGenerator(String directory, String pkgName) {
		super(directory, pkgName);
	}

	public DummyClassGenerator(String directory, String pkgName, Hashtable<String, Object> characteristics) {
		super(directory, pkgName, characteristics);
	}

	public byte[] getClassData() throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		String helperName = null;
		String superName = null;
		String[] interfaces = null;
		String intfName = null;
		boolean useHelper = false;
		boolean extendsHelper = false;
		boolean implementsInterface = false;
		int opcode;
		boolean modifyOtherInvokerOperands = false;

		className = packageName + "/" + DUMMY_CLASS_NAME;

		if (characteristics.get(USE_HELPER_CLASS) != null) {
			useHelper = (Boolean)characteristics.get(USE_HELPER_CLASS);
		}
		if (useHelper) {
			helperName = packageName + "/" + HELPER_CLASS_NAME;
		}

		if (characteristics.get(EXTENDS_HELPER_CLASS) != null) {
			extendsHelper = (Boolean)characteristics.get(EXTENDS_HELPER_CLASS);
		}
		if (extendsHelper) {
			superName = packageName + "/" + HELPER_CLASS_NAME;
		} else {
			superName = "java/lang/Object";
		}

		if (characteristics.get(IMPLEMENTS_INTERFACE) != null) {
			implementsInterface = (Boolean)characteristics.get(IMPLEMENTS_INTERFACE);
		}
		if (implementsInterface) {
			intfName = packageName + "/" + INTERFACE_NAME;
			interfaces = new String[] { intfName };
		}

		cw.visit(V1_8, ACC_PUBLIC, className, null, superName, interfaces);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, superName, "<init>", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}

		if (extendsHelper || implementsInterface) {
			String version = (String)characteristics.get(VERSION);
			String methodName = null;

			if (extendsHelper) {
				methodName = HELPER_METHOD_NAME;
				mv = cw.visitMethod(ACC_PUBLIC, HELPER_METHOD_NAME, HELPER_METHOD_SIG, null, null);
			} else {
				methodName = INTF_METHOD_NAME;
				mv = cw.visitMethod(ACC_PUBLIC, INTF_METHOD_NAME, INTF_METHOD_SIG, null, null);
			}
			mv.visitCode();
			if (version == null) {
				mv.visitLdcInsn(DUMMY_CLASS_NAME + "." + methodName);
			} else {
				mv.visitLdcInsn(DUMMY_CLASS_NAME + "_" + version + "." + methodName);
			}
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}

		String sharedInvokers[] = (String[])characteristics.get(SHARED_INVOKERS);
		for (int i = 0; i < sharedInvokers.length; i++) {
			String target = null;
			String invoker = sharedInvokers[i];
			mv = cw.visitMethod(ACC_PUBLIC, invoker, SHARED_INVOKER_METHOD_SIG, null, null);
			mv.visitCode();
			switch (invoker) {
			case INVOKE_STATIC:
			case INVOKE_SPECIAL:
			case INVOKE_INTERFACE:
				if (invoker.equals(INVOKE_STATIC)) {
					opcode = INVOKESTATIC;
				} else if (invoker.equals(INVOKE_SPECIAL)) {
					opcode = INVOKESPECIAL;
				} else {
					opcode = INVOKEINTERFACE;
					modifyOtherInvokerOperands = true;
				}
				if ((opcode == INVOKESPECIAL) || (opcode == INVOKEINTERFACE)) {
					mv.visitVarInsn(ALOAD, 0);
				}
				if ((characteristics.get(SHARED_INVOKERS_TARGET)) != null) {
					target = (String)characteristics.get(SHARED_INVOKERS_TARGET);
					if (target.equals("SUPER")) {
						mv.visitMethodInsn(opcode, superName, HELPER_METHOD_NAME, HELPER_METHOD_SIG, false);
					} else if (target.equals("INTERFACE")) {
						mv.visitMethodInsn(opcode, intfName, INTF_METHOD_NAME, INTF_METHOD_SIG, true);
					}
				} else {
					if (useHelper) {
						mv.visitMethodInsn(opcode, helperName, HELPER_METHOD_NAME, HELPER_METHOD_SIG, false);
					} else if (extendsHelper) {
						mv.visitMethodInsn(opcode, superName, HELPER_METHOD_NAME, HELPER_METHOD_SIG, false);
					} else if (implementsInterface) {
						mv.visitMethodInsn(opcode, intfName, INTF_METHOD_NAME, INTF_METHOD_SIG, true);
					}
				}
				mv.visitInsn(ARETURN);
				mv.visitMaxs(1, 1);
				break;
			case INVOKE_VIRTUAL:
				if (useHelper) {
					mv.visitTypeInsn(NEW, helperName);
				} else {
					mv.visitTypeInsn(NEW, superName);
				}
				mv.visitInsn(DUP);
				if (useHelper) {
					mv.visitMethodInsn(INVOKESPECIAL, helperName, "<init>", "()V");
				} else {
					mv.visitMethodInsn(INVOKESPECIAL, superName, "<init>", "()V");
				}
				mv.visitVarInsn(ASTORE, 1);
				mv.visitVarInsn(ALOAD, 1);
				if (useHelper) {
					mv.visitMethodInsn(INVOKEVIRTUAL, helperName, HELPER_METHOD_NAME, HELPER_METHOD_SIG);
				} else {
					mv.visitMethodInsn(INVOKEVIRTUAL, superName, HELPER_METHOD_NAME, HELPER_METHOD_SIG);
				}
				mv.visitInsn(ARETURN);
				mv.visitMaxs(2, 2);
				break;
			}
			mv.visitEnd();
		}
		cw.visitEnd();
		data = cw.toByteArray();

		if (modifyOtherInvokerOperands) {
			data = modifyInvokerOperand();
		}

		writeToFile();

		return data;
	}
}
