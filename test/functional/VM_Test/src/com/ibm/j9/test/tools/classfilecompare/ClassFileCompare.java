/*******************************************************************************
 * Copyright (c) 2013, 2013 IBM Corp. and others
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

package com.ibm.j9.test.tools.classfilecompare;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.logging.Logger;
import java.util.logging.ConsoleHandler;
import java.util.Objects;

import org.objectweb.asm.Handle;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.Type;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.AnnotationNode;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.FieldInsnNode;
import org.objectweb.asm.tree.FieldNode;
import org.objectweb.asm.tree.FrameNode;
import org.objectweb.asm.tree.IincInsnNode;
import org.objectweb.asm.tree.InnerClassNode;
import org.objectweb.asm.tree.IntInsnNode;
import org.objectweb.asm.tree.InvokeDynamicInsnNode;
import org.objectweb.asm.tree.JumpInsnNode;
import org.objectweb.asm.tree.LabelNode;
import org.objectweb.asm.tree.LdcInsnNode;
import org.objectweb.asm.tree.LineNumberNode;
import org.objectweb.asm.tree.LocalVariableNode;
import org.objectweb.asm.tree.LookupSwitchInsnNode;
import org.objectweb.asm.tree.MethodInsnNode;
import org.objectweb.asm.tree.MethodNode;
import org.objectweb.asm.tree.MultiANewArrayInsnNode;
import org.objectweb.asm.tree.TableSwitchInsnNode;
import org.objectweb.asm.tree.TryCatchBlockNode;
import org.objectweb.asm.tree.TypeInsnNode;
import org.objectweb.asm.tree.VarInsnNode;
import org.objectweb.asm.util.Printer;
import org.objectweb.asm.Opcodes;

import static org.objectweb.asm.tree.AbstractInsnNode.*;

public class ClassFileCompare {
		
	/* Count of mismatches in .class files */
	private int diffCount = 0;
	
	/* Flag of skipping debug information */
	private boolean shouldCompareDebugInfo;
	
	/* Set logger's output to console */
	private static final Logger logger;
	
	static {
		logger = Logger.getAnonymousLogger();
		logger.setUseParentHandlers(false);
		logger.addHandler(new ConsoleHandler());
	}
	
	/* Hashset intended for J9 opcode mapping */
	private static final Set<String> specialObjectMethods = 
			Collections.unmodifiableSet(new HashSet<String>(Arrays.asList("wait()V",
																		"wait(J)V",
	 		 		 													"wait(JI)V",
	 		 		 													"notify()V",
	 		 															"notifyAll()V",
		 																"getClass()Ljava/lang/Class;")));

	private ClassFileCompare (boolean shouldCompareDebugInfo) {
		this.shouldCompareDebugInfo = shouldCompareDebugInfo;
	}
	
	public static int compareClassFiles(byte[] classFile1, byte[] classFile2, boolean shouldCompareDebugInfo) {
		return new ClassFileCompare(shouldCompareDebugInfo).compare(classFile1, classFile2);
	}
	
	private void reportDifference(String message) {
		diffCount++;
		logger.warning(message);
	}

	private int compare(byte[] classFile1, byte[] classFile2) {
		ClassNode classNode1 = new ClassNode();
		ClassNode classNode2 = new ClassNode();

		/* Read in all data from byte arrays into ClassNodes 
		 * Specify SKIP_DEBUG mode to filter out information related to LineNumber and LocalVariable
		 */
		int debugMode = shouldCompareDebugInfo ? 0 : ClassReader.SKIP_DEBUG;
		new ClassReader(classFile1).accept(classNode1, debugMode);
		new ClassReader(classFile2).accept(classNode2, debugMode);
		
		/*
		 * Non-standard attributes and runtime invisible annotations are stripped by the JVM - don't compare them.
		 * Constant pools are reordered by the JVM - don't compare them.
		 * BootstrapMethods attribute is referenced by invokedynamic instructions 
		 * - BSM entries are compared in the case of INVOKE_DYNAMIC_INSN in the method "compareInstructions"
		 */
		compareMagic(classFile1, classFile2);
		compare(classNode1.version, classNode2.version, "Version numbers", false);
		
		/* ACC_SYNTHETIC is placed in access_flag of item as part of composition value */
		compareAccessFlag(classNode1.access, classNode2.access, "Class access flags", true);
		compare(classNode1.name, classNode2.name, "Class name");
		compare(classNode1.superName,classNode2.superName, "Superclass name");
		compare(classNode1.signature, classNode2.signature, "Generic signature");
		compare(classNode1.sourceFile, classNode2.sourceFile, "Source file name");
		compare(classNode1.sourceDebug, classNode2.sourceDebug, "Source debug extension");
		compare(classNode1.outerClass, classNode2.outerClass, "Outer class name");
		compare(classNode1.outerMethod, classNode2.outerMethod, "Enclosing method name");
		compare(classNode1.outerMethodDesc, classNode2.outerMethodDesc, "Enclosing method descriptor");

		compare(classNode1.interfaces, classNode2.interfaces, "Interfaces");
		compareInnerClasses(classNode1, classNode2);
		compareFields(classNode1, classNode2);
		compareAnnotations(classNode1.visibleAnnotations, classNode2.visibleAnnotations, "class " + classNode1.name);
		compareMethods(classNode1, classNode2);

		return diffCount;
	}
	
	private void compareMagic(byte[] classFile1, byte[] classFile2) {
		int magic1 = ByteBuffer.wrap(classFile1).getInt();
		int magic2 = ByteBuffer.wrap(classFile2).getInt();
		compare(magic1, magic2, "Magic numbers", true);
	}

	private void compare(MethodNode method1, LabelNode l1, MethodNode method2, LabelNode l2, String description) {
		compare(method1.instructions.indexOf(l1), method2.instructions.indexOf(l2), description, false);
	}
	
	private void compare(String str1, String str2, String description) {
		
		if (nullCheck(str1, str2, description)) {
			return;
		}
		
		if (str1 != str2 && !str1.equals(str2)) {
			reportDifference(description + " differs: " + str1 + ", " + str2);
		}
	}
	
	private void compare(int i1, int i2, String description, boolean reportHexadecimal) {
		if (i1 != i2) {
			String format = reportHexadecimal ? " differ: %#X, %#X" : " differ: %d, %d";
			reportDifference(String.format(description + format, i1, i2));
		}
	}

	/*
	 * It turns out that ASM ends up with a successful conversion of Synthetic attribute (0x41000) but 0x40000
	 * still remains visible after conversion, which leads to a mismatch of access flag in comparison of two class files. 
	 * This method is to address this issue simply by masking out 0x40000 so as to ensure that class files with 0x40000
	 * in the access flag should be considered identical in comparison if no other differences are detected.
	 * See Task 45969 for more details.
	 */
	private void compareAccessFlag(int i1, int i2, String description, boolean reportHexadecimal) {
		if ((i1 & 0xFFFF) != (i2 & 0xFFFF)) {
			String format = reportHexadecimal ? " differ: %#X, %#X" : " differ: %d, %d";
			reportDifference(String.format(description + format, i1, i2));
		}
	}
	
	private void compare(List<Object> list1, List<Object> list2, String description) {

		if (nullCheck(list1, list2, description)) {
			return;
		}
		
		Object[] array1 = list1.toArray(new Object[]{});
		Object[] array2 = list2.toArray(new Object[]{});
		
		Arrays.sort(array1);
		Arrays.sort(array2);
		
		if (!Arrays.equals(array1, array2)) {
			reportDifference(description + " differ: " + Arrays.deepToString(array1) + ", " + Arrays.deepToString(array2));
		}
	}

	private void compareFrames(List<Object> list1, List<Object> list2, String description) {

		if (nullCheck(list1, list2, description)) {
			return;
		}
		
		Object[] array1 = list1.toArray(new Object[]{});
		Object[] array2 = list2.toArray(new Object[]{});
				
		if (!Arrays.equals(array1, array2)) {
			reportDifference(description + " differ: " + Arrays.deepToString(array1) + ", " + Arrays.deepToString(array2));
		}
	}
	
	private void compareFields(ClassNode class1, ClassNode class2) {
		
		if (nullCheck(class1.fields, class2.fields, "Class fields")) {
			return;
		}
		
		/*
		 * Copy and sort fields by name
		 */
		@SuppressWarnings("unchecked")
		List<FieldNode> fields1 = new ArrayList<>(class1.fields);
		@SuppressWarnings("unchecked")
		List<FieldNode> fields2 = new ArrayList<>(class2.fields);

		Comparator<FieldNode> comparator = new Comparator<FieldNode>() {
			@Override
			public int compare(FieldNode field1, FieldNode field2) {
				return field1.name.compareTo(field2.name);
			}};

		Collections.sort(fields1, comparator);
		Collections.sort(fields2, comparator);
		
		/*
		 * Compare fields
		 */
		if (fields1.size() != fields2.size()) {
			reportDifference("Fields size differ: " + fields1.size() + ", " + fields2.size());
		} else {
			Iterator<FieldNode> iter = fields1.iterator();
			for (FieldNode field2 : fields2) {
				FieldNode field1 = iter.next();

				/*
				 * Non-standard attributes and runtime invisible annotations are stripped by the JVM - don't compare them.
				 * ACC_SYNTHETIC or Synthetic attribute is part of access flag computed as a composition value.
				 */
				compareAccessFlag(field1.access, field2.access, "Field (" + field1.name + ") access flags", true);
				compare(field1.name, field2.name, "Field name");
				compare(field1.desc, field2.desc, "Field (" + field1.name + ") descriptor");
				compare(field1.signature, field2.signature, "Field (" + field1.name + ") generic signature");

				if (!Objects.equals(field1.value, field2.value)) {
					reportDifference("Field (" + field1.name + ") values differ: " + field1.value + ", " + field2.value);
				}
				
				compareAnnotations(field1.visibleAnnotations, field2.visibleAnnotations, "field " + field1.name);
			}
		}
	}
	
	private void compareInnerClasses(ClassNode class1, ClassNode class2) {

		if (nullCheck(class1.innerClasses, class2.innerClasses, "Inner classes")) {
			return;
		}
		
		/*
		 * Copy and sort inner classes by name, innerName, outerName
		 */
		@SuppressWarnings("unchecked")
		List<InnerClassNode> innerClasses1 = new ArrayList<>(class1.innerClasses);
		@SuppressWarnings("unchecked")
		List<InnerClassNode> innerClasses2 = new ArrayList<>(class2.innerClasses);

		Comparator<InnerClassNode> comparator = new Comparator<InnerClassNode>() {
			@Override
			public int compare(InnerClassNode innerClass1, InnerClassNode innerClass2) {
				int name = innerClass1.name.compareTo(innerClass2.name);
				if (0 != name) {
					return name;
				}
				int innerName = innerClass1.innerName.compareTo(innerClass2.innerName);
				if (0 != innerName) {
					return innerName;
				}
				return innerClass1.outerName.compareTo(innerClass2.outerName);
			}};

		Collections.sort(innerClasses1, comparator);
		Collections.sort(innerClasses2, comparator);
		
		/*
		 * Compare inner classes
		 */
		if (innerClasses1.size() != innerClasses2.size()) {
			reportDifference("Inner classes differ: " + innerClasses1 + ", " + innerClasses2);
		} else {
			Iterator<InnerClassNode> iter = innerClasses1.iterator();
			for (InnerClassNode innerClass2 : innerClasses2) {
				InnerClassNode innerClass1 = iter.next();
				
				/*
				 * ASM would not add any extra flag like ACC_SYNTHETIC_ATTRIBUTE to the inner_class_access_flags 
				 * since it may not have access to the class file of inner class when inspecting its attributes.
				 * Thus, there is no need to use compareAccessFlag() in inner class.
				 */
				compare(innerClass1.access, innerClass2.access, "Inner class access flags", true);
				compare(innerClass1.name, innerClass2.name, "Inner class name");
				compare(innerClass1.innerName, innerClass2.innerName, "Inner class inner name");
				compare(innerClass1.outerName, innerClass2.outerName, "Inner class outer name");
			}
		}
	}

	private void compareAnnotation(AnnotationNode annotation1, AnnotationNode annotation2, String owner) {

		if (nullCheck(annotation1.values, annotation2.values, owner + ": annotations")) {
			return;
		}
		
		compare(annotation1.desc, annotation2.desc, owner + ": annotation type");
		
		if (annotation1.values.size() == annotation2.values.size()) {
			/*
			 * Assume name-value pairs will be in the same order. This is correct for J9.
			 */
			boolean even = true;
			@SuppressWarnings("unchecked")
			Iterator<Object> valueIter = annotation1.values.iterator();

			for (Object value2 : annotation2.values) {
				Object value1 = valueIter.next();
				if (even) {
					if (!value1.equals(value2)) {
						reportDifference(owner + ": annotation element names differ: " + value1 + ", " + value2);
						break;
					}
				} else {
					compareAnnotationElementValue(value1, value2, owner);
				}
				even = !even;
			}

			/*
			 * Any differences are already noted
			 */
			return;
		}
		reportDifference(owner + ": annotation values differ: " + annotation1.values + ", " + annotation2.values);
	}

	private void compareAnnotationElementValue(Object value1, Object value2, String owner) {
		if (value1 instanceof Byte ||
			value1 instanceof Boolean ||
			value1 instanceof Character ||
			value1 instanceof Short ||
			value1 instanceof Integer ||
			value1 instanceof Long ||
			value1 instanceof Float ||
			value1 instanceof Double ||
			value1 instanceof String ||
			value1 instanceof Type)
		{
			if (!value1.equals(value2)) {
				reportDifference(owner + ": annotation element values differ: " + value1 + ", " + value2);
			}
		} else if (value1.getClass().isArray() && value1.getClass().getComponentType().isPrimitive()) {
			if (!Objects.deepEquals(value1, value2)) {
				reportDifference(owner + ": annotation element array values differ: " + Arrays.deepToString(new Object[]{value1, value2}));
			}
		} else if (value1 instanceof String[]) {
			if (!Objects.deepEquals(value1, value2)) {
				reportDifference(owner + ": annotation element enum values differ: " + Arrays.deepToString(new Object[]{value1, value2}));
			}
		} else if (value1 instanceof AnnotationNode) {
			if (!(value2 instanceof AnnotationNode)) {
				reportDifference(owner + ": annotation element annotation values differ: " + value1 + ", " + value2);
			} else {
				compareAnnotation((AnnotationNode) value1, (AnnotationNode) value2, owner);
			}
		} else {
			if (value1.getClass() != value2.getClass()) {
				reportDifference(owner + ": annotation element class values differ: " + value1 + ", " + value2);
			} else {
				List<?> values1 = (List<?>) value1;
				List<?> values2 = (List<?>) value2;
				if (values1.size() != values2.size()) {
					reportDifference(owner + ": annotation element array sizes differ: " + values1.size() + ", " + values2.size());
				} else {
					Iterator<?> iter = values1.iterator();
					for (Object v2 : values2) {
						Object v1 = iter.next();

						compareAnnotationElementValue(v1, v2, owner);
					}
				}
			}
		}
	}
	
	private void compareAnnotations(List<AnnotationNode> annotations1, List<AnnotationNode> annotations2, String owner) {
		
		if (nullCheck(annotations1, annotations2, owner + ": annotations")) {
			return;
		}
		
		/*
		 * Copy and sort before comparing size to allow the printed annotations to be more easily compared.
		 */
		annotations1 = new ArrayList<>(annotations1);
		annotations2 = new ArrayList<>(annotations2);

		Comparator<AnnotationNode> comparator = new Comparator<AnnotationNode>() {
			@Override
			public int compare(AnnotationNode annotation1, AnnotationNode annotation2) {
				return annotation1.desc.compareTo(annotation2.desc);
			}};

		Collections.sort(annotations1, comparator);
		Collections.sort(annotations2, comparator);

		if (annotations1.size() != annotations2.size()) {
			reportDifference(owner + ": annotations differ: " + annotations1.size() + ", " + annotations2.size());
		} else {
			Iterator<AnnotationNode> iter = annotations1.iterator();
			for (AnnotationNode annotation2 : annotations2) {
				AnnotationNode annotation1 = iter.next();

				compareAnnotation(annotation1, annotation2, owner);
			}
		}
	}

	private void compareMethods(ClassNode class1, ClassNode class2) {
		
		if (nullCheck(class1.methods, class2.methods, class1.name + ": methods")) {
			return;
		}

		/*
		 * Copy and sort methods by name and descriptor
		 */
		@SuppressWarnings("unchecked")
		List<MethodNode> methods1 = new ArrayList<>(class1.methods);
		@SuppressWarnings("unchecked")
		List<MethodNode> methods2 = new ArrayList<>(class2.methods);
		
		Comparator<MethodNode> comparator = new Comparator<MethodNode>() {
			@Override
			public int compare(MethodNode method1, MethodNode method2) {
				int name = method1.name.compareTo(method2.name);
				return name != 0 ? name : method1.desc.compareTo(method2.desc);
			}};
		
		Collections.sort(methods1, comparator);
		Collections.sort(methods2, comparator);
		
		/*
		 * Compare methods
		 */
		if (methods1.size() != methods2.size()) {
			reportDifference("Methods differ: " + methods1.size() + ", " + methods2.size());
		} else {
			Iterator<MethodNode> iter = methods1.iterator();
			for (MethodNode method2 : methods2) {
				MethodNode method1 = iter.next();

				/*
				 * Non-standard attributes and runtime invisible annotations are stripped by the JVM - don't compare them.
				 * LocalVariableTypeTable is implicitly checked by local variable comparison.
				 * LineNumberTable attribute is stripped out with SKIP_DEBUG specified
				 * ACC_SYNTHETIC or Synthetic attribute is part of access flag computed as a composition value 
				 */
				compareAccessFlag(method1.access, method2.access, "Method (" + method1.name + ") access flags", true);
				compare(method1.name, method2.name, "Method (" + method1.name + ") name");
				compare(method1.desc, method2.desc, "Method (" + method1.name + ") descriptor");
				compare(method1.signature, method2.signature, "Method (" + method1.name + ") generic signature");
				
				compare(method1.exceptions, method2.exceptions, "Method (" + method1.name + ") thrown exceptions");
				compare(method1.maxLocals, method2.maxLocals, "Method (" + method1.name + ") max locals", false);
				compare(method1.maxStack, method2.maxStack, "Method (" + method1.name + ") max stack", false);

				compareAnnotations(method1.visibleAnnotations, method2.visibleAnnotations, "Method " + method1.name);
				compareLocalVariables(method1, method2);
				compareTryCatchBlocks(method1, method2);
				compareParameterAnnotations(method1, method2);
				compareAnnotationDefault(method1, method2);
				compareInstructions(class1, method1, class2, method2);
			}
		}
	}

	private void compareLabels(MethodNode method1, List<LabelNode> list1, MethodNode method2, List<LabelNode> list2, String description) {
		
		if (nullCheck(list1, list2, description)) {
			return;
		}
		
		/*
		 * Copy and sort label nodes by index
		 */
		Collections.sort(list1, labelNodeComparatorFor(method1));
		Collections.sort(list2, labelNodeComparatorFor(method2));

		/*
		 * Compare label nodes
		 */
		if (list1.size() != list2.size()) {
			reportDifference(description + ": label nodes differ: " + list1.size() + ", " + list2.size());
		} else {
			Iterator<LabelNode> iter = list1.iterator();
			int index = 0;
			for (LabelNode labelNode2 : list2) {
				LabelNode labelNode1 = iter.next();
				compare(method1, labelNode1, method2, labelNode2,  description + "[" + index + "]");
				index++;
			}
		}
	}
	
	private Comparator<LabelNode> labelNodeComparatorFor(final MethodNode method) {
		
		Comparator<LabelNode> comparator = new Comparator<LabelNode>() {
			@Override
			public int compare(LabelNode ln1, LabelNode ln2) {
				/*
				 * Label nodes in the same scope must have different indexes
				 */
				int start = method.instructions.indexOf(ln1) - method.instructions.indexOf(ln2);
				return start;
			}};
			
			return comparator;
	}
	
	private void compareLocalVariables(final MethodNode method1, final MethodNode method2) {
		
		/*
		 * J9 may strip LocalVariableTable and LocalVariableTypeTable attributes.
		 */
		if (!shouldCompareDebugInfo) {
			return;
		}
		
		if (nullCheck(method1.localVariables, method2.localVariables, "Local variables (" + method1.name + ")")) {
			return;
		}
		
		/*
		 * Copy and sort local variables by scope and index
		 */
		@SuppressWarnings("unchecked")
		List<LocalVariableNode> localVars1 = new ArrayList<>(method1.localVariables);
		@SuppressWarnings("unchecked")
		List<LocalVariableNode> localVars2 = new ArrayList<>(method2.localVariables);
		
		Collections.sort(localVars1, localVariableComparatorFor(method1));
		Collections.sort(localVars2, localVariableComparatorFor(method2));
		
		/*
		 * Compare local variables
		 */
		if (localVars1.size() != localVars2.size()) {
			reportDifference("Local variables (" + method1.name + ") differ: " + localVars1.size() + ", " + localVars2.size());
		} else {
			Iterator<LocalVariableNode> iter = localVars1.iterator();
			for (LocalVariableNode localVar2 : localVars2) {
				LocalVariableNode localVar1 = iter.next();
				
				compare(localVar1.name, localVar2.name, "Local variables (" + method1.name + ") name");
				compare(localVar1.desc, localVar2.desc, "Local variables (" + method1.name + ") descriptor");
				compare(localVar1.signature, localVar2.signature, "Local variables (" + method1.name + ") generic signature");
				compare(localVar1.index, localVar2.index, "Local variables (" + method1.name + ") indexes", false);
				compare(method1, localVar1.start, method2, localVar2.start, "Local variables scope (" + method1.name + ") starts");
				compare(method1, localVar1.end, method2, localVar2.end, "Local variables scope (" + method1.name + ") ends");
			}
		}
	}

	private Comparator<LocalVariableNode> localVariableComparatorFor(final MethodNode method) {
		
		Comparator<LocalVariableNode> comparator = new Comparator<LocalVariableNode>() {
			@Override
			public int compare(LocalVariableNode lvn1, LocalVariableNode lvn2) {
				/*
				 * Variables in the same scope must have different indexes
				 */
				int index = lvn1.index - lvn2.index;
				int start = method.instructions.indexOf(lvn1.start) - method.instructions.indexOf(lvn2.start);
				return 0 != start ? start : index;
			}};
			
			return comparator;
	}
	
	private void compareTryCatchBlocks(MethodNode method1, MethodNode method2) {
		
		if (nullCheck(method1.tryCatchBlocks, method2.tryCatchBlocks, "Try-catch blocks (" + method1.name + ")")) {
			return;
		}
		
		/*
		 * Copy and sort try-catch blocks by scope and type
		 */
		@SuppressWarnings("unchecked")
		List<TryCatchBlockNode> tryCatchBlocks1 = new ArrayList<>(method1.tryCatchBlocks);
		@SuppressWarnings("unchecked")
		List<TryCatchBlockNode> tryCatchBlocks2 = new ArrayList<>(method2.tryCatchBlocks);
		
		Collections.sort(tryCatchBlocks1, tryCatchBlockComparatorFor(method1));
		Collections.sort(tryCatchBlocks2, tryCatchBlockComparatorFor(method2));
		
		/*
		 * Compare try-catch blocks
		 */
		if (tryCatchBlocks1.size() != tryCatchBlocks2.size()) {
			reportDifference("Try-catch blocks (" + method1.name + ") differ: " + tryCatchBlocks1.size() + ", " + tryCatchBlocks2.size());
		} else {
			Iterator<TryCatchBlockNode> iter = tryCatchBlocks1.iterator();
			for (TryCatchBlockNode tryCatchBlock2 : tryCatchBlocks2) {
				TryCatchBlockNode tryCatchBlock1 = iter.next();
				
				compare(tryCatchBlock1.type, tryCatchBlock2.type, "Try-catch block (" + method1.name + ") type");
				compare(method1, tryCatchBlock1.start, method2, tryCatchBlock2.start, "Try-catch block scope (" + method1.name + ") starts");
				compare(method1, tryCatchBlock1.end, method2, tryCatchBlock2.end, "Try-catch block scope (" + method1.name + ") ends");
				compare(method1, tryCatchBlock1.handler, method2, tryCatchBlock2.handler, "Try-catch block scope (" + method1.name + ") handlers");
			}
		}
	}

	private Comparator<TryCatchBlockNode> tryCatchBlockComparatorFor(final MethodNode method) {
		
		Comparator<TryCatchBlockNode> comparator = new Comparator<TryCatchBlockNode>() {
			@Override			
			public int compare(TryCatchBlockNode tbn1, TryCatchBlockNode tbn2) {
				/*
				 * Try-catch blocks in the same scope must have different types
				 */
				int start = method.instructions.indexOf(tbn1.start) - method.instructions.indexOf(tbn2.start);
				int end = method.instructions.indexOf(tbn1.end) - method.instructions.indexOf(tbn2.end);
				
				if (0 == start && 0 == end) {
					return tbn1.type.compareTo(tbn2.type);
				}
				return 0 != start ? start : end;
			}};
			
			return comparator;
	}
	
	private void compareAnnotationDefault(MethodNode method1, MethodNode method2) {
		
		if (nullCheck(method1.annotationDefault, method2.annotationDefault, "Annotation defaults (" + method1.name + ")")) {
			return;
		}
		
		compareAnnotationElementValue(method1.annotationDefault, method2.annotationDefault, "Annotation defaults (" + method1.name + ")");
	}

	private void compareParameterAnnotations(MethodNode method1, MethodNode method2) {
		
		if (nullCheck(method1.visibleParameterAnnotations, method2.visibleParameterAnnotations, "Parameter annotations (" + method1.name + ")")) {
			return;
		}
		
		if (method1.visibleParameterAnnotations.length == method2.visibleParameterAnnotations.length) {
			for (int i = 0; i < method1.visibleParameterAnnotations.length; i++) {
				compareAnnotations(method1.visibleParameterAnnotations[i], method2.visibleParameterAnnotations[i], method1.name + ".parameter_annotations[" + i + "]");
			}

			/*
			 * Any differences are already noted
			 */
			return;
		}
		reportDifference("Parameter annotations (" + method1.name + ") differ: " + method1.visibleParameterAnnotations + ", " + method2.visibleParameterAnnotations);
	}

	private void compareInstructions(ClassNode clazz1, MethodNode method1, ClassNode clazz2, MethodNode method2) {
		
		if (nullCheck(method1.instructions, method2.instructions, "Instructions (" + method1.name + ")")) {
			return;
		}
		
		if (method1.instructions.size() != method2.instructions.size()) {
			reportDifference("Method " + method1.name + ": instructions differ: " + method1.instructions.size() + ", " + method2.instructions.size()); 
		} else {			
			@SuppressWarnings("unchecked")
			Iterator<AbstractInsnNode> iter1 = method1.instructions.iterator();
			@SuppressWarnings("unchecked")
			Iterator<AbstractInsnNode> iter2 = method2.instructions.iterator();
			int index = 0;
			while (iter1.hasNext()) {
				AbstractInsnNode instruction1 = iter1.next();
				AbstractInsnNode instruction2 = iter2.next();
				
				if (instruction1.getOpcode() != instruction2.getOpcode()) {
					/* Check for J9 opcode mapping */
					compareJ9OpcodeMapping(clazz2, method2, instruction1, instruction2, index);
				} else {
					switch (instruction1.getType()) {
					case INSN:
						/* Do nothing */
						break;
					case INT_INSN:
						compare(((IntInsnNode) instruction1).operand, ((IntInsnNode) instruction2).operand, 
								"Operands of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")", false);
						break;
					case VAR_INSN:
						compare(((VarInsnNode) instruction1).var, ((VarInsnNode) instruction2).var, 
								"Operands of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")", false);
						break;
					case TYPE_INSN:
						compare(((TypeInsnNode) instruction1).desc, ((TypeInsnNode) instruction2).desc, 
								"Operands of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")");
						break;
					case FIELD_INSN: {
						FieldInsnNode fieldInsn1 = (FieldInsnNode)instruction1;
						FieldInsnNode fieldInsn2 = (FieldInsnNode)instruction2;
						compare(fieldInsn1.owner, fieldInsn2.owner, 
								"Owning class name of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")");
						compare(fieldInsn1.name,fieldInsn2.name, 
								"Field name of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")");
						compare(fieldInsn1.desc, fieldInsn2.desc, 
								"Descriptor of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")");
						break;
					}
					case METHOD_INSN: {
						MethodInsnNode methodInsn1 = (MethodInsnNode)instruction1;
						MethodInsnNode methodInsn2 = (MethodInsnNode)instruction2;
						compare(methodInsn1.owner, methodInsn2.owner, 
								"Owning class name of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")");
						compare(methodInsn1.name, methodInsn2.name, 
								"Method name of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")");
						compare(methodInsn1.desc, methodInsn2.desc, 
								"Descriptor of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")");
						break;
					}
					case INVOKE_DYNAMIC_INSN:
						compareInvokeDynamic((InvokeDynamicInsnNode)instruction1, (InvokeDynamicInsnNode)instruction2, method1, index);
						break;
					case JUMP_INSN:
						compare(method1, ((JumpInsnNode) instruction1).label, method2, ((JumpInsnNode) instruction2).label, 
								"Operands of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")");
						break;
					case LABEL:
						/* Do nothing */
						break;
					case LDC_INSN:
						if (!((LdcInsnNode) instruction1).cst.equals(((LdcInsnNode) instruction2).cst)) {
							reportDifference("Operands of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ") differ: " + ((LdcInsnNode) instruction1).cst + ", " + ((LdcInsnNode) instruction2).cst);
						}
						break;
					case IINC_INSN: {
						IincInsnNode iincInsn1 = (IincInsnNode)instruction1;
						IincInsnNode iincInsn2 = (IincInsnNode)instruction2;
						compare(iincInsn1.var, iincInsn2.var, 
								"Variable operands of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")", false);
						compare(iincInsn1.incr, iincInsn2.incr, 
								"Increment operands of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")", false);
						break;
					}
					case TABLESWITCH_INSN: {
						TableSwitchInsnNode tableSwitchInsn1 = (TableSwitchInsnNode)instruction1;
						TableSwitchInsnNode tableSwitchInsn2 = (TableSwitchInsnNode)instruction2;
						compare(tableSwitchInsn1.min, tableSwitchInsn2.min, 
								Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ") minimum key value", false);
						compare(tableSwitchInsn1.max, tableSwitchInsn2.max, 
								Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ") maximum key value", false);
						compare(method1, tableSwitchInsn1.dflt, method2, tableSwitchInsn2.dflt, 
								Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ") default_handler_block");
						compareLabels(method1, tableSwitchInsn1.labels, method2, tableSwitchInsn2.labels, 
								Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ") handler_blocks");
						break;
					}
					case LOOKUPSWITCH_INSN: {
						LookupSwitchInsnNode lookupSwitchInsn1 = (LookupSwitchInsnNode)instruction1;
						LookupSwitchInsnNode lookupSwitchInsn2 = (LookupSwitchInsnNode)instruction2;
						compare(method1, lookupSwitchInsn1.dflt, method2, lookupSwitchInsn2.dflt, 
								Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ") default_handler_block");
						compare(lookupSwitchInsn1.keys, lookupSwitchInsn2.keys, 
								Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ") key values");
						compareLabels(method1, lookupSwitchInsn1.labels, method2, lookupSwitchInsn2.labels, 
								Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ") handler_blocks");
						break;
					}
					case MULTIANEWARRAY_INSN: {
						MultiANewArrayInsnNode arrayInsn1 = (MultiANewArrayInsnNode)instruction1;
						MultiANewArrayInsnNode arrayInsn2 = (MultiANewArrayInsnNode)instruction2;
						compare(arrayInsn1.desc, arrayInsn2.desc, 
								"Desc operand of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")");
						compare(arrayInsn1.dims, arrayInsn2.dims, 
								"Dimension operand of " + Printer.OPCODES[instruction1.getOpcode()] + " (" + method1.name + ":" + index + ")", false);
						break;
					}
					case FRAME: {
						FrameNode frameInsn1 = (FrameNode)instruction1;
						FrameNode frameInsn2 = (FrameNode)instruction2;
						compare(frameInsn1.type, frameInsn2.type, 
								"Stack map frame (" + method1.name + ":" + index + ") frame type", false);
						compareFrames(frameInsn1.local, frameInsn2.local, 
								"Stack map frame (" + method1.name + ":" + index + ") types of the local variables");
						compareFrames(frameInsn1.stack, frameInsn2.stack, 
								"Stack map frame (" + method1.name + ":" + index + ") types of the operand stack elements");
					}
						break;
					case LINE: {
						assert shouldCompareDebugInfo;
						LineNumberNode lineInsn1 = (LineNumberNode)instruction1;
						LineNumberNode lineInsn2 = (LineNumberNode)instruction2;
						compare(lineInsn1.line, lineInsn2.line, 
								"Line number (" + method1.name + ":" + index + ") line No.", false);
						compare(method1, lineInsn1.start, method2, lineInsn2.start, 
								"Line number (" + method1.name + ":" + index + ") offset of the 1st instruction");
						break;
					}
					default:
						assert false;
					}
				}
				index++;
			}
		}
	}

	/* J9 Opcode mapping comparison */
	private void compareJ9OpcodeMapping(ClassNode clazz, MethodNode method, AbstractInsnNode insn1, AbstractInsnNode insn2, int index) {

		int opCode1 = insn1.getOpcode();
		int opCode2 = insn2.getOpcode();

		/* There is no assumption as to which file input is the transformed .class file */
		switch(opCode1) {
		case Opcodes.LDC: /* LDC or LDC_W is distinguished from the operand */			
			if (Opcodes.ICONST_0 == opCode2 && ((LdcInsnNode)insn1).cst.equals(0)) {
			    return;
			} else if (Opcodes.FCONST_0 == opCode2 && ((LdcInsnNode)insn1).cst.equals(0.0f)) {
			    return;
			}
			break;
		case Opcodes.ICONST_0:
			if (Opcodes.LDC == opCode2 && ((LdcInsnNode)insn2).cst.equals(0)) {
			    return;
			}
			break;
		case Opcodes.FCONST_0:
			 if (Opcodes.LDC == opCode2 && ((LdcInsnNode)insn2).cst.equals(0.0f)) {
				    return;
				}
				break;
		case Opcodes.INVOKEVIRTUAL:
			if (isValidInvokeVirtualConversion(clazz, method, insn1, insn2)) {
			    return;
			}
			break;
		case Opcodes.INVOKESPECIAL:
			if (isValidInvokeVirtualConversion(clazz, method, insn2, insn1)) {
			    return;
			}
			break;
		default:
			break;
		}
		 reportDifference("Instructions (" + method.name + ":" + index + ") differ: " + Printer.OPCODES[opCode1] + ", " + Printer.OPCODES[opCode2]);
	}

	private boolean isValidInvokeVirtualConversion(ClassNode clazz, MethodNode method, AbstractInsnNode insn1, AbstractInsnNode insn2) {

		if (Opcodes.INVOKEVIRTUAL == insn1.getOpcode()
				&& Opcodes.INVOKESPECIAL == insn2.getOpcode()) {
			
			MethodInsnNode methodInsn1 = (MethodInsnNode)insn1;
			MethodInsnNode methodInsn2 = (MethodInsnNode)insn2;
			
			/*
			 * The class name as well as the method name involved are checked by
			 * comparing their method owner name(the internal name of the
			 * method's class) and name (the method's name)
			 */
			if (methodInsn1.owner.equals(methodInsn2.owner)
					&& methodInsn1.name.equals(methodInsn2.name)
					&& methodInsn1.desc.equals(methodInsn2.desc)) {
				/*
				 * Both methods should be compared further in terms of their
				 * access flag(has to be private) if they are not the 6 final
				 * methods of Object listed as follows: 
				 * name     descriptor 
				 * wait      ()V
				 * wait      (J)V 
				 * wait      (JI)V 
				 * notify    ()V 
				 * notifyAll ()V 
				 * getClass  ()Ljava/lang/Class;
				 */
				if (methodInsn1.owner.equals("java/lang/Object") 
						&& specialObjectMethods.contains(methodInsn1.name + methodInsn1.desc)) {
					return true;
				}

				@SuppressWarnings("unchecked")
				List<MethodNode> methodNodes1 = new ArrayList<>(clazz.methods);

				for (MethodNode mn : methodNodes1) {
					if (mn.name.equals(methodInsn1.name) && mn.desc.equals(methodInsn1.desc)) {
						return Opcodes.ACC_PRIVATE == mn.access;
					}
				}
			}
		}

		return false;

	}
		
	/* Compare invokedynamic instruction */
	private void compareInvokeDynamic(InvokeDynamicInsnNode dynInsn1, InvokeDynamicInsnNode dynInsn2, MethodNode method, int index) {
		Handle bsm1 = dynInsn1.bsm;
		Handle bsm2 = dynInsn2.bsm;
		
		 if (!dynInsn1.desc.equals(dynInsn2.desc) || !dynInsn1.name.equals(dynInsn2.name) || !bsm1.equals(bsm2)) {
	            reportDifference("Operands of " + Printer.OPCODES[dynInsn1.getOpcode()] + " (" + method.name + ":" + index + ") differ: " + dynInsn1.name + dynInsn1.desc + " " + bsm1.toString() + ", " + dynInsn2.name + dynInsn2.desc + " " + bsm2.toString());
	        }
		
		/* The bootstrap method constant arguments must be identical, including order */
		if (!Arrays.equals(dynInsn1.bsmArgs, dynInsn2.bsmArgs)) {
			reportDifference("Operands of " + Printer.OPCODES[dynInsn1.getOpcode()] + " (" + method.name + ":" + index + ") differ: " + Arrays.deepToString(dynInsn1.bsmArgs) + ", " + Arrays.deepToString(dynInsn2.bsmArgs));
		}
	}
	
	/* Returns true if either obj1 or obj2 is null */
	private boolean nullCheck(Object obj1, Object obj2, String description) {
	    if (null == obj1 || null == obj2) {
	        if (obj1 != obj2) {
	            reportDifference(description + " differ: " + obj1 + ", " + obj2);
	        }
	        return true;
	    }
	    return false;
	}
	
    public static void main(String args[]) {
    	
		byte[] classFile1 = null;
		byte[] classFile2 = null;
		String classFileName1 = null;
		String classFileName2 = null;
		boolean shouldCompareDebugInfo = true;
    	
		if (args.length < 2) {
			System.err.println("Usage: java " + ClassFileCompare.class.getName() + " <class-file> <class-file>");
			return;
		}
		else if (3 == args.length && !args[0].equals("-skipDebug") )
		{
			System.err.println("Usage: java " + ClassFileCompare.class.getName() + " <-skipDebug> <class-file> <class-file>");
			return;
		}
		
		try {
			if (args[0].equals("-skipDebug")) {
				shouldCompareDebugInfo = false;
				classFileName1 = args[1];
				classFileName2 = args[2];
			} 
			else 
			{
				classFileName1 = args[0];
				classFileName2 = args[1];
			}
			classFile1 = Files.readAllBytes(Paths.get(classFileName1));
			classFile2 = Files.readAllBytes(Paths.get(classFileName2));
		} catch (IOException e) {
			System.err.println("Error reading .class file: " + e);
			return;
		}

		if ((classFile1.length == 0) || (classFile2.length == 0)) {
			System.err.println("Unable to compare zero-length .class file");
			return;
		}
		
		int diffCount = compareClassFiles(classFile1, classFile2, shouldCompareDebugInfo);

		System.out.println("Class files to be compared:");
		System.out.println("1) " + classFileName1);
		System.out.println("2) " + classFileName2);
		
		if (0 == diffCount) {
			System.out.println("The two .class files are identical.");
		} else if (1 == diffCount) {
			System.out.println("The two .class files are different: 1 mismatch spotted!");
		} else {
			System.out.println("The two .class files are different: " + diffCount + " mismatches spotted!");
		}
	}
}
