/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.invoker;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Hashtable;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.FieldVisitor;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Type;

import org.openj9.test.invoker.util.ClassGenerator;
import org.openj9.test.invoker.util.DummyClassGenerator;
import org.openj9.test.invoker.util.GeneratedClassLoader;
import com.ibm.jvmti.tests.redefineClasses.rc001;

@Test(groups = { "level.sanity" })
public class InvalidateOnStackSplitTableRetransformationTest {
	Logger logger = Logger.getLogger(InvalidateOnStackSplitTableRetransformationTest.class);
	private static final String PKG_NAME = "org/openj9/test/invoker";

	String dummyClassName = PKG_NAME + "/" + ClassGenerator.DUMMY_CLASS_NAME;
	String intfClassName = PKG_NAME + "/" + ClassGenerator.INTERFACE_NAME;

	public void testStaticSplitInvalidateOnStack() throws Exception {
		String expectedBefore = null;
		String expectedAfter = null;
		String testName = "testStaticSplitInvalidateOnStack";
		Class<?> dummyClass = null;
		Class<?> intfClass = null;
		Method method = null;
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		InterfaceGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		DummyGenerator dummyClassGenerator = new DummyGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.INTF_HAS_STATIC_METHOD, true);
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		intfClass = Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_STATIC });
		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		Retransformer retransformer = new Retransformer(testName, intfClass, dummyClass, true);
		retransformer.start();
		while (retransformer.getState() != Thread.State.WAITING) {
			try {
				Thread.sleep(5);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}

		expectedBefore = "Static " + ClassGenerator.INTERFACE_NAME + "_V1." + ClassGenerator.INTF_METHOD_NAME;
		expectedAfter = "Static " + ClassGenerator.INTERFACE_NAME + "_V2." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to set 'Dummy.before' to " + expectedBefore
				+ " and 'Dummy.after' to " + expectedAfter);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			Object dummyObj = dummyClass.newInstance();
			method.invoke(dummyObj, (Object[])null);
			Field field = dummyClass.getField("before");
			String beforeValue = (String)field.get(dummyObj);
			logger.debug("After return, 'Dummy.before' is " + beforeValue);
			AssertJUnit.assertEquals("Did not get expected value: ", expectedBefore, beforeValue);
			field = dummyClass.getField("after");
			String afterValue = (String)field.get(dummyObj);
			logger.debug("After return, 'Dummy.after' is " + afterValue);
			AssertJUnit.assertEquals("Did not get expected value: ", expectedAfter, afterValue);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}

		expectedBefore = "Static " + ClassGenerator.INTERFACE_NAME + "_V2." + ClassGenerator.INTF_METHOD_NAME;
		expectedAfter = "Static " + ClassGenerator.INTERFACE_NAME + "_V2." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to set 'Dummy.before' to " + expectedBefore
				+ " and 'Dummy.after' to " + expectedAfter);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			Object dummyObj = dummyClass.newInstance();
			method.invoke(dummyObj, (Object[])null);
			Field field = dummyClass.getField("before");
			String beforeValue = (String)field.get(dummyObj);
			logger.debug("After return, 'Dummy.before' is " + beforeValue);
			AssertJUnit.assertEquals("Did not get expected value: ", expectedBefore, beforeValue);
			field = dummyClass.getField("after");
			String afterValue = (String)field.get(dummyObj);
			logger.debug("After return, 'Dummy.after' is " + afterValue);
			AssertJUnit.assertEquals("Did not get expected value: ", expectedAfter, afterValue);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	public void testSpeciaSplitInvalidateOnStack() throws Exception {
		String expectedBefore = null;
		String expectedAfter = null;
		String testName = "testSpeciaSplitInvalidateOnStack";
		Class<?> dummyClass = null;
		Class<?> intfClass = null;
		Method method = null;
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		InterfaceGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		DummyGenerator dummyClassGenerator = new DummyGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.INTF_HAS_STATIC_METHOD, false);
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		intfClass = Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_SPECIAL });
		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		Retransformer retransformer = new Retransformer(testName, intfClass, dummyClass, false);
		retransformer.start();
		while (retransformer.getState() != Thread.State.WAITING) {
			try {
				Thread.sleep(5);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}

		expectedBefore = "Default " + ClassGenerator.INTERFACE_NAME + "_V1." + ClassGenerator.INTF_METHOD_NAME;
		expectedAfter = "Default " + ClassGenerator.INTERFACE_NAME + "_V2." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeSpecial(). Expected to set 'Dummy.before' to " + expectedBefore
				+ " and 'Dummy.after' to " + expectedAfter);
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			Object dummyObj = dummyClass.newInstance();
			method.invoke(dummyObj, (Object[])null);
			Field field = dummyClass.getField("before");
			String beforeValue = (String)field.get(dummyObj);
			logger.debug("After return, 'Dummy.before' is " + beforeValue);
			AssertJUnit.assertEquals("Did not get expected value: ", expectedBefore, beforeValue);
			field = dummyClass.getField("after");
			String afterValue = (String)field.get(dummyObj);
			logger.debug("After return, 'Dummy.after' is " + afterValue);
			AssertJUnit.assertEquals("Did not get expected value: ", expectedAfter, afterValue);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}

		expectedBefore = "Default " + ClassGenerator.INTERFACE_NAME + "_V2." + ClassGenerator.INTF_METHOD_NAME;
		expectedAfter = "Default " + ClassGenerator.INTERFACE_NAME + "_V2." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeSpecial(). Expected to set 'Dummy.before' to " + expectedBefore
				+ " and 'Dummy.after' to " + expectedAfter);
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			Object dummyObj = dummyClass.newInstance();
			method.invoke(dummyObj, (Object[])null);
			Field field = dummyClass.getField("before");
			String beforeValue = (String)field.get(dummyObj);
			logger.debug("After return, 'Dummy.before' is " + beforeValue);
			AssertJUnit.assertEquals("Did not get expected value: ", expectedBefore, beforeValue);
			field = dummyClass.getField("after");
			String afterValue = (String)field.get(dummyObj);
			logger.debug("After return, 'Dummy.after' is " + afterValue);
			AssertJUnit.assertEquals("Did not get expected value: ", expectedAfter, afterValue);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	class Retransformer extends Thread {
		String testName;
		Class<?> intfClass;
		Object lock;
		boolean isStatic;

		Retransformer(String testName, Class<?> intfClass, Object lock, boolean isStatic) {
			this.testName = testName;
			this.intfClass = intfClass;
			this.lock = lock;
			this.isStatic = isStatic;
		}

		private boolean redefineInterface() throws Exception {
			Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
			InterfaceGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
			characteristics.put(ClassGenerator.VERSION, "V2");
			if (isStatic) {
				characteristics.put(ClassGenerator.INTF_HAS_STATIC_METHOD, true);
			} else {
				characteristics.put(ClassGenerator.INTF_HAS_DEFAULT_METHOD, true);
			}
			intfGenerator.setCharacteristics(characteristics);
			byte[] classData = intfGenerator.getClassData();
			return rc001.redefineClass(intfClass, classData.length, classData);
		}

		public void run() {
			synchronized (lock) {
				try {
					lock.wait();
					redefineInterface();
					lock.notify();
				} catch (Exception e) {
					e.printStackTrace();
				}
			}
		}
	}

	class DummyGenerator extends DummyClassGenerator {
		public DummyGenerator(String directory, String pkgName) {
			super(directory, pkgName);
		}

		public byte[] getClassData() throws Exception {
			ClassWriter cw = new ClassWriter(0);
			FieldVisitor fv;
			MethodVisitor mv;
			String intfName = packageName + "/" + INTERFACE_NAME;
			String[] interfaces = new String[] { intfName };
			boolean modifyOtherInvokerOperands = false;

			className = packageName + "/" + DUMMY_CLASS_NAME;

			cw.visit(V1_8, ACC_PUBLIC, className, null, "java/lang/Object", interfaces);
			{
				fv = cw.visitField(ACC_PUBLIC, "before", "Ljava/lang/String;", null, null);
				fv.visitEnd();
			}
			{
				fv = cw.visitField(ACC_PUBLIC, "after", "Ljava/lang/String;", null, null);
				fv.visitEnd();
			}
			{
				mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
				mv.visitCode();
				mv.visitVarInsn(ALOAD, 0);
				mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
				mv.visitInsn(RETURN);
				mv.visitMaxs(1, 1);
				mv.visitEnd();
			}
			String sharedInvokers[] = (String[])characteristics.get(SHARED_INVOKERS);
			for (int i = 0; i < sharedInvokers.length; i++) {
				String invoker = sharedInvokers[i];
				mv = cw.visitMethod(ACC_PUBLIC, invoker, "()V", null, null);
				mv.visitCode();
				if (invoker.equals(INVOKE_INTERFACE)) {
					mv.visitVarInsn(ALOAD, 0);
					mv.visitMethodInsn(INVOKEINTERFACE, intfName, INTF_METHOD_NAME, INTF_METHOD_SIG, true);
					mv.visitInsn(POP);
					mv.visitInsn(RETURN);
					mv.visitMaxs(1, 1);
					modifyOtherInvokerOperands = true;
				} else {
					int opcode = INVOKESTATIC;
					if (invoker.equals(INVOKE_SPECIAL)) {
						opcode = INVOKESPECIAL;
						mv.visitVarInsn(ALOAD, 0);
					}
					mv.visitVarInsn(ALOAD, 0);
					mv.visitMethodInsn(opcode, intfName, INTF_METHOD_NAME, INTF_METHOD_SIG, true);
					mv.visitFieldInsn(PUTFIELD, className, "before", "Ljava/lang/String;");
					if (invoker.equals(INVOKE_SPECIAL)) {
						mv.visitVarInsn(ALOAD, 0);
					}
					mv.visitVarInsn(ALOAD, 0);
					mv.visitMethodInsn(opcode, intfName, INTF_METHOD_NAME, INTF_METHOD_SIG, true);
					mv.visitFieldInsn(PUTFIELD, className, "after", "Ljava/lang/String;");
					mv.visitInsn(RETURN);
					mv.visitMaxs(2, 1);
					mv.visitEnd();
				}

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

	class InterfaceGenerator extends ClassGenerator {
		public InterfaceGenerator(String directory, String pkgName) {
			super(directory, pkgName);
		}

		public byte[] getClassData() throws Exception {
			ClassWriter cw = new ClassWriter(0);
			MethodVisitor mv;
			String version = (String)characteristics.get(VERSION);
			boolean hasStaticMethod = false;

			if (characteristics.get(INTF_HAS_STATIC_METHOD) != null) {
				hasStaticMethod = (Boolean)characteristics.get(INTF_HAS_STATIC_METHOD);
			}

			className = packageName + "/" + INTERFACE_NAME;

			cw.visit(V1_8, ACC_ABSTRACT + ACC_INTERFACE, className, null, "java/lang/Object", null);
			if (version.equals("V1")) {
				String retValue = null;
				if (hasStaticMethod) {
					mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, INTF_METHOD_NAME, INTF_METHOD_SIG, null,
							new String[] { "java/lang/Exception" });

					retValue = "Static " + INTERFACE_NAME + "_" + version + "." + INTF_METHOD_NAME;
				} else {
					mv = cw.visitMethod(ACC_PUBLIC, INTF_METHOD_NAME, INTF_METHOD_SIG, null,
							new String[] { "java/lang/Exception" });

					retValue = "Default " + INTERFACE_NAME + "_" + version + "." + INTF_METHOD_NAME;
				}
				mv.visitCode();
				Label l0 = new Label();
				Label l1 = new Label();
				Label l2 = new Label();
				mv.visitTryCatchBlock(l0, l1, l2, null);
				Label l3 = new Label();
				mv.visitTryCatchBlock(l2, l3, l2, null);
				mv.visitLdcInsn(Type.getType("L" + dummyClassName + ";"));
				mv.visitInsn(DUP);
				mv.visitVarInsn(ASTORE, 0);
				mv.visitInsn(MONITORENTER);
				mv.visitLabel(l0);
				mv.visitLdcInsn(Type.getType("L" + dummyClassName + ";"));
				mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Object", "notify", "()V");
				mv.visitLdcInsn(Type.getType("L" + dummyClassName + ";"));
				mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Object", "wait", "()V");
				mv.visitVarInsn(ALOAD, 0);
				mv.visitInsn(MONITOREXIT);
				mv.visitLabel(l1);
				Label l4 = new Label();
				mv.visitJumpInsn(GOTO, l4);
				mv.visitLabel(l2);
				mv.visitFrame(F_FULL, 1, new Object[] { "java/lang/Object" }, 1,
						new Object[] { "java/lang/Throwable" });
				mv.visitVarInsn(ASTORE, 1);
				mv.visitVarInsn(ALOAD, 0);
				mv.visitInsn(MONITOREXIT);
				mv.visitLabel(l3);
				mv.visitVarInsn(ALOAD, 1);
				mv.visitInsn(ATHROW);
				mv.visitLabel(l4);
				mv.visitFrame(F_CHOP, 1, null, 0, null);
				mv.visitLdcInsn(retValue);
				mv.visitInsn(ARETURN);
				mv.visitMaxs(2, 2);
				mv.visitEnd();
			} else {
				String retValue = null;
				if (hasStaticMethod) {
					mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, INTF_METHOD_NAME, INTF_METHOD_SIG, null, null);
					retValue = "Static " + INTERFACE_NAME + "_" + version + "." + INTF_METHOD_NAME;
				} else {
					mv = cw.visitMethod(ACC_PUBLIC, INTF_METHOD_NAME, INTF_METHOD_SIG, null, null);
					retValue = "Default " + INTERFACE_NAME + "_" + version + "." + INTF_METHOD_NAME;
				}
				mv.visitCode();
				mv.visitLdcInsn(retValue);
				mv.visitInsn(ARETURN);
				mv.visitMaxs(1, 1);
			}
			cw.visitEnd();
			data = cw.toByteArray();
			writeToFile();

			return data;
		}
	}
}
