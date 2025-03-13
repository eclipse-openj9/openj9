/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2012
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.java.diagnostics.utils.plugins.impl;

import java.io.InputStream;
import java.io.IOException;
import java.net.URL;
import java.util.Set;
/*[IF JAVA_SPEC_VERSION >= 24]*/
import java.util.Optional;
/*[ENDIF] JAVA_SPEC_VERSION >= 24 */

import com.ibm.java.diagnostics.utils.plugins.Annotation;
import com.ibm.java.diagnostics.utils.plugins.ClassInfo;
import com.ibm.java.diagnostics.utils.plugins.ClassListener;

/*[IF JAVA_SPEC_VERSION < 24]*/
import jdk.internal.org.objectweb.asm.AnnotationVisitor;
import jdk.internal.org.objectweb.asm.ClassReader;
import jdk.internal.org.objectweb.asm.ClassVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;
/*[ELSE] JAVA_SPEC_VERSION < 24 */
import java.lang.classfile.AnnotationElement;
import java.lang.classfile.AnnotationValue;
import java.lang.classfile.Attributes;
import java.lang.classfile.ClassFile;
import java.lang.classfile.ClassModel;
import java.lang.classfile.attribute.RuntimeVisibleAnnotationsAttribute;
import java.lang.classfile.constantpool.ClassEntry;
import java.util.function.Function;
/*[ENDIF] JAVA_SPEC_VERSION < 24 */

public final class ClassScanner
		/*[IF JAVA_SPEC_VERSION < 24]*/
		extends ClassVisitor
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
{

	public static ClassInfo getClassInfo(InputStream file, URL url, Set<ClassListener> listeners) throws IOException {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		ClassScanner scanner = new ClassScanner(url, listeners);
		ClassReader reader = new ClassReader(file);

		reader.accept(scanner, ClassReader.SKIP_CODE | ClassReader.SKIP_DEBUG | ClassReader.SKIP_FRAMES);

		return scanner.info;
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		Function<ClassEntry, String> toDotted = entry -> entry.asInternalName().replace('/', '.');
		ClassModel model = ClassFile.of().parse(file.readAllBytes());
		int version = model.majorVersion() + (model.minorVersion() << 16);
		int access = model.flags().flagsMask();
		String dotName = toDotted.apply(model.thisClass());
		String signature = null; // not currently used
		String dotSuperName = model.superclass().map(toDotted).orElse(null);
		String[] interfaces = model.interfaces().stream().map(toDotted).toArray(String[]::new);

		ClassInfo info = new ClassInfo(dotName, url);

		for (String iface : interfaces) {
			info.addInterface(iface);
		}

		for (ClassListener listener : listeners) {
			listener.visit(version, access, dotName, signature, dotSuperName, interfaces);
		}

		Optional<RuntimeVisibleAnnotationsAttribute> attribute = model
				.findAttribute(Attributes.runtimeVisibleAnnotations());

		if (attribute.isPresent()) {
			for (java.lang.classfile.Annotation annotation : attribute.get().annotations()) {
				String classDescriptor = annotation.className().stringValue();
				Annotation current = info.addAnnotation(classDescriptor);

				for (ClassListener listener : listeners) {
					listener.visitAnnotation(classDescriptor, true /* visible */);
				}

				for (AnnotationElement element : annotation.elements()) {
					if (element.value() instanceof AnnotationValue.OfConstant constant) {
						current.addEntry(element.name().stringValue(), constant.resolvedValue());
					}
				}
			}
		}

		return info;
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	private ClassInfo info;
	private Annotation currentAnnotation;
	private final URL url; // where the class is being scanned from
	private final Set<ClassListener> listeners;

	private ClassScanner(URL url, Set<ClassListener> listeners) {
		/*[IF JAVA_SPEC_VERSION >= 19]*/
		super(Opcodes.ASM9, null);
		/*[ELSEIF JAVA_SPEC_VERSION >= 15]*/
		super(Opcodes.ASM8, null);
		/*[ELSEIF JAVA_SPEC_VERSION >= 11]*/
		super(Opcodes.ASM6, null);
		/*[ELSE] JAVA_SPEC_VERSION >= 11 */
		super(Opcodes.ASM5, null);
		/*[ENDIF] JAVA_SPEC_VERSION >= 19 */
		this.url = url;
		this.listeners = listeners;
	}

	@Override
	public AnnotationVisitor visitAnnotation(String className, boolean visible) {
		final class ClassScannerAnnotation extends AnnotationVisitor {

			ClassScannerAnnotation(int api) {
				super(api);
			}

			@Override
			public void visit(String name, Object value) {
				ClassScanner.this.visitAnnotationValue(name, value);
			}
		}

		currentAnnotation = info.addAnnotation(className);
		for (ClassListener listener : listeners) {
			listener.visitAnnotation(className, visible);
		}
		return new ClassScannerAnnotation(api);
	}

	final void visitAnnotationValue(String name, Object value) {
		currentAnnotation.addEntry(name, value);
		for (ClassListener listener : listeners) {
			listener.visitAnnotationValue(name, value);
		}
	}

	@Override
	public void visit(int version, int access, String name, String signature, String superName, String[] interfaces) {
		String dotName = name.replace('/', '.');
		String dotSuperName = superName.replace('/', '.');
		info = new ClassInfo(dotName, url);
		// record all interfaces supplied by this class
		for (String iface : interfaces) {
			info.addInterface(iface);
		}
		for (ClassListener listener : listeners) {
			listener.visit(version, access, dotName, signature, dotSuperName, interfaces);
		}
	}

	/*[IF JAVA_SPEC_VERSION == 17]*/
	@Deprecated
	@Override
	public void visitPermittedSubclassExperimental(String className) {
		/*
		 * Sealed classes (JEP 409) were introduced as a preview feature in Java 15
		 * and (supposedly) completed in Java 17. Unfortunately, the ClassReader and
		 * ClassVisitor classes in Java 17 still consider the "PermittedSubclasses"
		 * attribute experimental. On the other hand, the Java 17 compiler generates
		 * the attribute in some classes (e.g. com.ibm.dtfj.utils.file.ImageSourceType).
		 * To avoid throwing UnsupportedOperationException, we just ignore them.
		 */
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 17 */

	/*[ELSE] JAVA_SPEC_VERSION < 24 */

	private ClassScanner() {
		super();
	}

	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
}
