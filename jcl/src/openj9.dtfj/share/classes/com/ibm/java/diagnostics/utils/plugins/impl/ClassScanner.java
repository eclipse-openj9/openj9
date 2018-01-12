/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.java.diagnostics.utils.plugins.impl;

import java.net.URL;
import java.util.Set;

import jdk.internal.org.objectweb.asm.AnnotationVisitor;
import jdk.internal.org.objectweb.asm.Attribute;
import jdk.internal.org.objectweb.asm.ClassVisitor;
import jdk.internal.org.objectweb.asm.FieldVisitor;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;

import com.ibm.java.diagnostics.utils.plugins.Annotation;
import com.ibm.java.diagnostics.utils.plugins.ClassInfo;
import com.ibm.java.diagnostics.utils.plugins.ClassListener;

public class ClassScanner extends  ClassVisitor {
	private ClassInfo info;
	private Annotation currentAnnotation = null;
	private final URL url;			//where the class is being scanned from
	private final Set<ClassListener> listeners;
	
	public ClassScanner(URL url, Set<ClassListener> listeners) {
		super(Opcodes.ASM4, null);
		this.url = url;
		this.listeners = listeners;
	}

	public AnnotationVisitor visitAnnotation(String classname, boolean visible) {
		currentAnnotation = info.addAnnotation(classname);
		for(ClassListener listener : listeners) {
			listener.visitAnnotation(classname, visible);
		}
		return new ClassScannerAnnotation(Opcodes.ASM4);
	}
	
	public void visit(int version, int access, String name, String signature, String superName, String[] interfaces) {
		String dotName = name.replace('/', '.');
		String dotSuperName = superName.replace('/', '.');
		info = new ClassInfo(dotName, url);
		//record all interfaces supplied by this class
		for(String iface : interfaces) {
			info.addInterface(iface);
		}
		for(ClassListener listener : listeners) {
			listener.visit(version, access, dotName, signature, dotSuperName, interfaces);
		}
	}

	public void visitAttribute(Attribute attr) {}
	public void visitEnd() {}
	public FieldVisitor visitField(int arg0, String arg1, String arg2, String arg3, Object arg4) { return null; }
	public void visitInnerClass(String arg0, String arg1, String arg2, int arg3) {}
	public MethodVisitor visitMethod(int arg0, String arg1,	String arg2, String arg3, String[] arg4) { return null;	}
	public void visitOuterClass(String arg0, String arg1, String arg2) {}
	public void visitSource(String arg0, String arg1) {}

	public ClassInfo getClassInfo() {
		return info;
	}
	
	class ClassScannerAnnotation extends AnnotationVisitor {

		public ClassScannerAnnotation(int arg0) {
			super(arg0);
		}

		public AnnotationVisitor visitAnnotation(String name, String desc) {
			return null;			//not interested in nested annotations
		}

		public AnnotationVisitor visitArray(String name) {
			return null;		//not interested in arrays
		}
		
		public void visitEnum(String name, String desc, String value) {
		}

		public void visit(String name, Object value) {
			currentAnnotation.addEntry(name, value);
			for(ClassListener listener : listeners) {
				listener.visitAnnotationValue(name, value);
			}
			
		}
	}
}
