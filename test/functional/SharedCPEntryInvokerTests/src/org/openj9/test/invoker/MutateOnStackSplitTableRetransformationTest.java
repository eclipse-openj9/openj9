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
import java.lang.reflect.Method;
import java.util.Hashtable;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Type;

import org.openj9.test.invoker.util.ClassGenerator;
import org.openj9.test.invoker.util.DummyClassGenerator;
import org.openj9.test.invoker.util.GeneratedClassLoader;
import com.ibm.jvmti.tests.redefineClasses.rc001;

@Test(groups = { "level.sanity" })
public class MutateOnStackSplitTableRetransformationTest {
	public static Logger logger = Logger.getLogger(MutateOnStackSplitTableRetransformationTest.class);

	private static final String PKG_NAME = "com/ibm/j9/invoker/junit";

	String dummyClassName = PKG_NAME + "/" + ClassGenerator.DUMMY_CLASS_NAME;
	String intfClassName = PKG_NAME + "/" + ClassGenerator.INTERFACE_NAME;

	public void testStaticSplitMutateOnStack() throws Exception {
		String expected = null;
		String rc = null;
		String testName = "testStaticSplitMutateOnStack";
		Class<?> dummyClass = null;
		Method method = null;
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		InterfaceGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		DummyGenerator dummyClassGenerator = new DummyGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.INTF_HAS_STATIC_METHOD, true);
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.SHARED_INVOKERS, new String[] { DummyClassGenerator.INVOKE_STATIC });
		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		Retransformer retransformer = new Retransformer(testName, dummyClass, dummyClass, true);
		retransformer.start();

		/* Wait for retransformer thread to go to wait state */
		while (retransformer.getState() != Thread.State.WAITING) {
			try {
				Thread.sleep(5);
			} catch (InterruptedException e) {
				logger.warn("Thread interrupted: ", e);;
			}
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + DummyClassGenerator.INVOKE_STATIC;
		method = dummyClass.getMethod("invokeStatic", new Class[] { boolean.class });
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), new Object[] { new Boolean(true) });
			logger.debug("Returned " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V2." + DummyClassGenerator.INVOKE_STATIC;
		method = dummyClass.getMethod("invokeStatic", new Class[] { boolean.class });
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), new Object[] { new Boolean(false) });
			logger.debug("Returned " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}

		/* Wait for retransformer thread to go back to wait state */
		while (retransformer.getState() != Thread.State.WAITING) {
			try {
				Thread.sleep(5);
			} catch (InterruptedException e) {
				logger.warn("Thread interrupted: ", e);;
			}
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V2." + DummyClassGenerator.INVOKE_STATIC;
		method = dummyClass.getMethod("invokeStatic", new Class[] { boolean.class });
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), new Object[] { new Boolean(true) });
			logger.debug("Returned " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + DummyClassGenerator.INVOKE_STATIC;
		method = dummyClass.getMethod("invokeStatic", new Class[] { boolean.class });
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), new Object[] { new Boolean(false) });
			logger.debug("Returned " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	public void testSpecialSplitMutateOnStack() throws Exception {
		String expected = null;
		String rc = null;
		String testName = "testSpecialSplitMutateOnStack";
		Class<?> dummyClass = null;
		Method method = null;
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		InterfaceGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		DummyGenerator dummyClassGenerator = new DummyGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.INTF_HAS_DEFAULT_METHOD, true);
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.SHARED_INVOKERS, new String[] { DummyClassGenerator.INVOKE_SPECIAL });
		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		Retransformer retransformer = new Retransformer(testName, dummyClass, dummyClass, false);
		retransformer.start();

		/* Wait for retransformer thread to go to wait state */
		while (retransformer.getState() != Thread.State.WAITING) {
			try {
				Thread.sleep(5);
			} catch (InterruptedException e) {
				logger.warn("Thread interrupted: ", e);
			}
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + DummyClassGenerator.INVOKE_SPECIAL;
		method = dummyClass.getMethod("invokeSpecial", new Class[] { boolean.class });
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), new Object[] { new Boolean(true) });
			logger.debug("Returned " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V2." + DummyClassGenerator.INVOKE_SPECIAL;
		method = dummyClass.getMethod("invokeSpecial", new Class[] { boolean.class });
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), new Object[] { new Boolean(false) });
			logger.debug("Returned " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}

		/* Wait for retransformer thread to go back to wait state */
		while (retransformer.getState() != Thread.State.WAITING) {
			try {
				Thread.sleep(5);
			} catch (InterruptedException e) {
				logger.warn("Thread interrupted: ", e);;
			}
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V2." + DummyClassGenerator.INVOKE_SPECIAL;
		method = dummyClass.getMethod("invokeSpecial", new Class[] { boolean.class });
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), new Object[] { new Boolean(true) });
			logger.debug("Returned " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + DummyClassGenerator.INVOKE_SPECIAL;
		method = dummyClass.getMethod("invokeSpecial", new Class[] { boolean.class });
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), new Object[] { new Boolean(false) });
			logger.debug("Returned " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	class Retransformer extends Thread {
		String testName;
		Class<?> dummyClass;
		Object lock;
		boolean isInvokeStatic;

		Retransformer(String testName, Class<?> dummyClass, Object lock, boolean isInvokeStatic) {
			this.testName = testName;
			this.dummyClass = dummyClass;
			this.lock = lock;
			this.isInvokeStatic = isInvokeStatic;
		}

		private boolean redefineDummyClass(Hashtable<String, Object> characteristics) throws Exception {
			DummyGenerator dummyClassGenerator = new DummyGenerator(testName, PKG_NAME);
			dummyClassGenerator.setCharacteristics(characteristics);
			byte[] classData = dummyClassGenerator.getClassData();
			return rc001.redefineClass(dummyClass, classData.length, classData);
		}

		public void run() {
			Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
			/*
			 * Loop twice; During first loop, Dummy class V2 is used. During
			 * second loop, Dummy class V1 is used.
			 */
			for (int i = 0; i < 2; i++) {
				synchronized (lock) {
					try {
						lock.wait();
						if (i == 0) {
							String invokers[] = new String[2];
							characteristics.put(ClassGenerator.VERSION, "V2");
							/*
							 * Dummy_V2 has invokeInterface() and
							 * invoke[Static|Special]()
							 */
							invokers[0] = DummyClassGenerator.INVOKE_INTERFACE;
							if (isInvokeStatic) {
								invokers[1] = DummyClassGenerator.INVOKE_STATIC;
							} else {
								invokers[1] = DummyClassGenerator.INVOKE_SPECIAL;
							}

							characteristics.put(ClassGenerator.SHARED_INVOKERS, invokers);
						} else {
							String invokers[] = new String[1];
							/* Dummy_V1 has only invoke[Static|Special]() */
							characteristics.put(ClassGenerator.VERSION, "V1");
							if (isInvokeStatic) {
								invokers[0] = DummyClassGenerator.INVOKE_STATIC;
							} else {
								invokers[0] = DummyClassGenerator.INVOKE_SPECIAL;
							}
							characteristics.put(ClassGenerator.SHARED_INVOKERS, invokers);
						}
						redefineDummyClass(characteristics);
						lock.notify();
					} catch (Exception e) {
						logger.warn("Exception: ", e);;
					}
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
			MethodVisitor mv;
			String intfName = packageName + "/" + INTERFACE_NAME;
			String[] interfaces = new String[] { intfName };
			boolean modifyOtherInvokerOperands = false;
			String version = (String)characteristics.get(VERSION);

			className = packageName + "/" + DUMMY_CLASS_NAME;

			cw.visit(V1_8, ACC_PUBLIC, className, null, "java/lang/Object", interfaces);
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
				mv = cw.visitMethod(ACC_PUBLIC, invoker, "(Z)Ljava/lang/String;", null, null);
				mv.visitCode();
				switch (invoker) {
				case INVOKE_INTERFACE: {
					String retValue = DUMMY_CLASS_NAME + "_" + version + "." + INVOKE_INTERFACE;
					mv.visitVarInsn(ALOAD, 0);
					mv.visitVarInsn(ILOAD, 1);
					mv.visitMethodInsn(INVOKEINTERFACE, intfName, INTF_METHOD_NAME, "(Z)V", true);
					mv.visitLdcInsn(retValue);
					mv.visitInsn(ARETURN);
					mv.visitMaxs(2, 2);
					modifyOtherInvokerOperands = true;
					break;
				}

				case INVOKE_STATIC: {
					String retValue = DUMMY_CLASS_NAME + "_" + version + "." + INVOKE_STATIC;
					mv.visitVarInsn(ILOAD, 1);
					mv.visitMethodInsn(INVOKESTATIC, intfName, INTF_METHOD_NAME, "(Z)V", true);
					mv.visitLdcInsn(retValue);
					mv.visitInsn(ARETURN);
					mv.visitMaxs(1, 2);
					mv.visitEnd();
					break;
				}
				case INVOKE_SPECIAL: {
					String retValue = DUMMY_CLASS_NAME + "_" + version + "." + INVOKE_SPECIAL;
					mv.visitVarInsn(ALOAD, 0);
					mv.visitVarInsn(ILOAD, 1);
					mv.visitMethodInsn(INVOKESPECIAL, intfName, INTF_METHOD_NAME, "(Z)V", true);
					mv.visitLdcInsn(retValue);
					mv.visitInsn(ARETURN);
					mv.visitMaxs(2, 2);
					mv.visitEnd();
					break;
				}
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
			boolean hasStaticMethod = false;

			if (characteristics.get(INTF_HAS_STATIC_METHOD) != null) {
				hasStaticMethod = (Boolean)characteristics.get(INTF_HAS_STATIC_METHOD);
			}

			className = packageName + "/" + INTERFACE_NAME;

			cw.visit(V1_8, ACC_ABSTRACT + ACC_INTERFACE, className, null, "java/lang/Object", null);
			{
				if (hasStaticMethod) {
					mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, INTF_METHOD_NAME, "(Z)V", null,
							new String[] { "java/lang/Exception" });
				} else {
					mv = cw.visitMethod(ACC_PUBLIC, INTF_METHOD_NAME, "(Z)V", null,
							new String[] { "java/lang/Exception" });
				}
				mv.visitCode();
				Label l0 = new Label();
				Label l1 = new Label();
				Label l2 = new Label();
				mv.visitTryCatchBlock(l0, l1, l2, null);
				Label l3 = new Label();
				mv.visitTryCatchBlock(l2, l3, l2, null);
				if (hasStaticMethod) {
					mv.visitVarInsn(ILOAD, 0);
				} else {
					mv.visitVarInsn(ILOAD, 1);
				}
				Label l4 = new Label();
				mv.visitJumpInsn(IFEQ, l4);
				mv.visitLdcInsn(Type.getType("L" + dummyClassName + ";"));
				mv.visitInsn(DUP);
				if (hasStaticMethod) {
					mv.visitVarInsn(ASTORE, 1);
				} else {
					mv.visitVarInsn(ASTORE, 2);
				}
				mv.visitInsn(MONITORENTER);
				mv.visitLabel(l0);
				mv.visitLdcInsn(Type.getType("L" + dummyClassName + ";"));
				mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Object", "notify", "()V");
				mv.visitLdcInsn(Type.getType("L" + dummyClassName + ";"));
				mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Object", "wait", "()V");
				if (hasStaticMethod) {
					mv.visitVarInsn(ALOAD, 1);
				} else {
					mv.visitVarInsn(ALOAD, 2);
				}
				mv.visitInsn(MONITOREXIT);
				mv.visitLabel(l1);
				mv.visitJumpInsn(GOTO, l4);
				mv.visitLabel(l2);
				if (hasStaticMethod) {
					mv.visitFrame(F_FULL, 2, new Object[] { INTEGER, "java/lang/Object" }, 1,
							new Object[] { "java/lang/Throwable" });
					mv.visitVarInsn(ASTORE, 2);
					mv.visitVarInsn(ALOAD, 1);
				} else {
					mv.visitFrame(F_FULL, 3, new Object[] { intfClassName, INTEGER, "java/lang/Object" }, 1,
							new Object[] { "java/lang/Throwable" });
					mv.visitVarInsn(ASTORE, 3);
					mv.visitVarInsn(ALOAD, 2);
				}
				mv.visitInsn(MONITOREXIT);
				mv.visitLabel(l3);
				if (hasStaticMethod) {
					mv.visitVarInsn(ALOAD, 2);
				} else {
					mv.visitVarInsn(ALOAD, 3);
				}
				mv.visitInsn(ATHROW);
				mv.visitLabel(l4);
				mv.visitFrame(F_CHOP, 1, null, 0, null);
				mv.visitInsn(RETURN);
				if (hasStaticMethod) {
					mv.visitMaxs(2, 3);
				} else {
					mv.visitMaxs(2, 4);
				}
				mv.visitEnd();
			}
			cw.visitEnd();
			data = cw.toByteArray();
			writeToFile();

			return data;
		}
	}
}
