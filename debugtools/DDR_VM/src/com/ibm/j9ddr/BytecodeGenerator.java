/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
package com.ibm.j9ddr;

import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.FieldVisitor;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;

import com.ibm.j9ddr.StructureReader.ConstantDescriptor;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;

/**
 * Generates the class bytecodes needed by DDR to represent the structures found in 
 * the blob as Java classes.
 * 
 * This method generates bytes codes using the ASM 3.1 framework from http://asm.ow2.org/. This has been
 * approved by the OSSC for shipping in the SDK, do not use any other version of ASM library except the one
 * DDR_VM/lib
 * 
 * @author adam
 */
public class BytecodeGenerator implements Opcodes {

	public static byte[] getClassBytes(StructureDescriptor structure, String fullClassName) throws ClassNotFoundException {
		// The className we are trying to load is FooOffsets.
		// The structure name stored in the reader is Foo

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;

		// Class definition
		cw.visit(V1_5, ACC_PUBLIC + ACC_FINAL + ACC_SUPER, fullClassName, null, "java/lang/Object", null);

		// Constants

		// SIZEOF
		fv = cw.visitField(ACC_PUBLIC + ACC_FINAL + ACC_STATIC, "SIZEOF", "J", null, Long.valueOf(structure.getSizeOf()));
		fv.visitEnd();
		
		// Declared constants
		for (ConstantDescriptor constantDescriptor : structure.getConstants()) {
			fv = cw.visitField(ACC_PUBLIC + ACC_FINAL + ACC_STATIC, constantDescriptor.getName(), "J", null, Long.valueOf(constantDescriptor.getValue()));
			fv.visitEnd();
		}

		// Offset fields
		int bitFieldBitCount = 0;
		for (FieldDescriptor field : structure.getFields()) {
			String fieldName = field.getName();
			String type = field.getType();
			int fieldOffset = field.getOffset();
			int colonIndex = type.replace("::", "__").indexOf(':'); // make sure match a bit-field, not a C++ namespace
			if (colonIndex != -1) {
				// Bitfield
				int bitSize = Integer.parseInt(type.substring(colonIndex + 1).trim());

				/*
				 * Newer blobs have accurate offsets of bitfields; adjust bitFieldBitCount
				 * to account for any fields preceding this field. In older blobs the byte
				 * offset of a bitfield is always zero, so this has no effect.
				 */
				bitFieldBitCount = Math.max(bitFieldBitCount, fieldOffset * Byte.SIZE);

				if (bitSize > (StructureReader.BIT_FIELD_CELL_SIZE - (bitFieldBitCount % StructureReader.BIT_FIELD_CELL_SIZE))) {
					throw new InternalError(String.format("Bitfield %s->%s must not span cells", structure.getName(), fieldName));
				}

				// s field
				String name = String.format("_%s_s_", fieldName);
				fv = cw.visitField(ACC_PUBLIC + ACC_FINAL + ACC_STATIC, name, "I", null, Integer.valueOf(bitFieldBitCount));
				fv.visitEnd();

				// b field
				name = String.format("_%s_b_", fieldName);
				fv = cw.visitField(ACC_PUBLIC + ACC_FINAL + ACC_STATIC, name, "I", null, Integer.valueOf(bitSize));
				fv.visitEnd();
				
				bitFieldBitCount += bitSize;
			} else {
				// Regular field
				String name = String.format("_%sOffset_", fieldName);
				
				// Offset fields
				fv = cw.visitField(ACC_PUBLIC + ACC_FINAL + ACC_STATIC, name, "I", null, Integer.valueOf(fieldOffset));
				fv.visitEnd();
			}
		}

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);

		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		mv.visitInsn(RETURN);
		mv.visitMaxs(1, 1);
		mv.visitEnd();

		cw.visitEnd();
		return cw.toByteArray();
	}

}
