/*
 * Copyright IBM Corp. and others 2001
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import static com.ibm.j9ddr.vm29.j9.ROMHelp.getMethodDebugInfoFromROMMethod;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_CLASS;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_DOUBLE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_FIELD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_FLOAT;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_HANDLE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INSTANCE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INT;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INTERFACE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_LONG;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_METHODHANDLE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_METHOD_TYPE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_STATIC_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_STRING;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_UNUSED8;
import static com.ibm.j9ddr.vm29.structure.J9DescriptionBits.J9DescriptionCpTypeShift;
import static com.ibm.j9ddr.vm29.structure.J9ROMMethodHandleRef.MH_REF_GETFIELD;
import static com.ibm.j9ddr.vm29.structure.J9ROMMethodHandleRef.MH_REF_GETSTATIC;
import static com.ibm.j9ddr.vm29.structure.J9ROMMethodHandleRef.MH_REF_PUTFIELD;
import static com.ibm.j9ddr.vm29.structure.J9ROMMethodHandleRef.MH_REF_PUTSTATIC;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.j9.J9ROMFieldShapeIterator;
import com.ibm.j9ddr.vm29.j9.OptInfo;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.j9.walkers.LineNumber;
import com.ibm.j9ddr.vm29.j9.walkers.LineNumberIterator;
import com.ibm.j9ddr.vm29.j9.walkers.LocalVariableTable;
import com.ibm.j9ddr.vm29.j9.walkers.LocalVariableTableIterator;
import com.ibm.j9ddr.vm29.pointer.FloatPointer;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9EnclosingObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionHandlerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodDebugInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodParameterPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodParametersDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodHandleRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodTypeRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMNameAndSignaturePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMSingleSlotConstantRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMStringRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SourceDebugExtensionPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodDebugInfoHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMFieldShapeHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMMethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.pointer.helper.ValueTypeHelper;
import com.ibm.j9ddr.vm29.structure.J9BCTranslationData;
import com.ibm.j9ddr.vm29.structure.J9CfrClassFile;
import com.ibm.j9ddr.vm29.structure.J9ConstantPool;
import com.ibm.j9ddr.vm29.structure.J9FieldFlags;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;

public class J9BCUtil {
	private static final String nl = System.getProperty("line.separator");

	private static final int MODIFIERSOURCE_CLASS = 1;
	private static final int MODIFIERSOURCE_METHOD = 2;
	private static final int MODIFIERSOURCE_FIELD = 3;
	private static final int MODIFIERSOURCE_METHODPARAMETER = 4;

	private static final int ONLY_SPEC_MODIFIERS = 1;
	private static final int INCLUDE_INTERNAL_MODIFIERS = 2;

	public static int BCUtil_DumpAnnotations = 1;

	/*
	 * Dump a printed representation of the specified @romMethod.
	 */
	/* note that methodIndex was not used and was removed */
	public static void j9bcutil_dumpRomMethod(PrintStream out, J9ROMMethodPointer romMethod, J9ROMClassPointer romClass, long flags, int options) throws CorruptDataException {
		J9ROMNameAndSignaturePointer nameAndSignature = romMethod.nameAndSignature();
		J9UTF8Pointer name = nameAndSignature.name();
		J9UTF8Pointer signature = nameAndSignature.signature();
		J9ROMConstantPoolItemPointer constantPool = J9ROMClassHelper.constantPool(romClass);

		out.append("  Name: " + J9UTF8Helper.stringValue(name) + nl);
		out.append("  Signature: " + J9UTF8Helper.stringValue(signature) + nl);
		out.append(String.format("  Access Flags (%s): ", Long.toHexString(romMethod.modifiers().longValue())));
		dumpModifiers(out, romMethod.modifiers().longValue(), MODIFIERSOURCE_METHOD, INCLUDE_INTERNAL_MODIFIERS, false);
		out.append(nl);
		out.append("  Internal Attribute Flags:");
		dumpMethodJ9Modifiers(out, romMethod.modifiers(), J9ROMMethodHelper.getExtendedModifiersDataFromROMMethod(romMethod));
		out.append(nl);
		out.append("  Max Stack: " + romMethod.maxStack().longValue() + nl);

		if (J9ROMMethodHelper.hasExceptionInfo(romMethod)) {

			J9ExceptionInfoPointer exceptionData = ROMHelp.J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
			long catchCount = exceptionData.catchCount().longValue();
			if (catchCount > 0) {
				J9ExceptionHandlerPointer handler = ROMHelp.J9EXCEPTIONINFO_HANDLERS(exceptionData);
				out.append(String.format("  Caught Exceptions (%d):", catchCount));
				out.append(nl);
				out.append("     start   end   handler   catch type" + nl);
				out.append("     -----   ---   -------   ----------" + nl);

				for (int i = 0; i < catchCount; i++) {
					out.append(String.format("     %d  %d  %d   ", handler.startPC().longValue(), handler.endPC().longValue(), handler.handlerPC().longValue()));

					long exceptionClassIndex = handler.exceptionClassIndex().longValue();
					if (exceptionClassIndex != 0) {
						J9ROMConstantPoolItemPointer addOffset = constantPool.add(exceptionClassIndex);
						J9ROMStringRefPointer romStringRefPointer = J9ROMStringRefPointer.cast(addOffset.longValue());
						out.append(J9UTF8Helper.stringValue(romStringRefPointer.utf8Data()));
					} else {
						out.append("(any)");
					}
					out.append(nl);
					handler = handler.add(1);
				}
			}

			long throwCount = exceptionData.throwCount().longValue();
			if (throwCount > 0) {
				out.append(String.format("  Thrown Exceptions (%d):", throwCount));
				out.append(nl);
				SelfRelativePointer currentThrowName = ROMHelp.J9EXCEPTIONINFO_THROWNAMES(exceptionData);
				for (long i = 0; i < throwCount; i++) {
					J9UTF8Pointer currentName = J9UTF8Pointer.cast(currentThrowName.get());
					out.append("    ");
					out.append(J9UTF8Helper.stringValue(currentName));
					out.append(nl);
				}
			}
		}

		if (romMethod.modifiers().anyBitsIn(J9CfrClassFile.CFR_ACC_NATIVE)) {
			dumpNative(out, romMethod, flags);
		} else {
			dumpBytecodes(out, romClass, romMethod, flags);
		}
		dumpMethodDebugInfo(out, romClass, romMethod, flags);
		dumpStackMapTable(out, romClass, romMethod, flags);
		dumpMethodParameters(out, romClass, romMethod,flags);

		if (0 != (BCUtil_DumpAnnotations & options)) {
			dumpMethodAnnotations(out, romMethod);
		}
		out.append(nl);
	}

	/**
	 * This method prints the modifiers for class, method, field, nested class or methodParameters.
	 * Following is the list of valid modifiers for each element:
	 *
	 * 		:: CLASS ::
	 * 			-ACC_PUBLIC
	 *			-ACC_FINAL
	 *			-ACC_SUPER | ACC_IDENTITY
	 *			-ACC_INTERFACE
	 *			-ACC_ABSTRACT
	 *			-ACC_SYNTHETIC
	 *			-ACC_ANNOTATION
	 *			-ACC_ENUM
	 *
	 *		:: NESTED CLASS ::
	 *			-ACC_PUBLIC
	 *			-ACC_PRIVATE
	 *			-ACC_PROTECTED
	 *			-ACC_STATIC
	 *			-ACC_FINAL
	 *			-ACC_INTERFACE
	 *			-ACC_ABSTRACT
	 *			-ACC_SYNTHETIC
	 *			-ACC_ANNOTATION
	 *			-ACC_ENUM
	 *
	 *		:: METHOD ::
	 *			-ACC_PUBLIC
	 *			-ACC_PRIVATE
	 *			-ACC_PROTECTED
	 *			-ACC_STATIC
	 * 			-ACC_FINAL
	 * 			-ACC_SYNCHRONIZED
	 *			-ACC_BRIDGE
	 *			-ACC_VARARGS
	 *			-ACC_NATIVE
	 *			-ACC_ABSTRACT
	 *			-ACC_STRICT
	 *			-ACC_SYNTHETIC
	 *
	 *		:: FIELD ::
	 *			-ACC_PUBLIC
	 *			-ACC_PRIVATE
	 *			-ACC_PROTECTED
	 *			-ACC_STATIC
	 *			-ACC_FINAL
	 *			-ACC_VOLATILE
	 *			-ACC_TRANSIENT
	 *			-ACC_SYNTHETIC
	 *			-ACC_ENUM
	 *			-ACC_STRICT
	 *			-J9FieldFlagConstant
	 *			-J9FieldFlagIsNullRestricted
	 *
	 *		:: METHODPARAMETERS ::
	 *			-ACC_FINAL
	 *			-ACC_SYNTHETIC
	 *			-ACC_MANDATED
	 *
	 *
	 */
	private static void dumpModifiers(PrintStream out, long modifiers, int modifierSrc, int modScope, boolean isValueClass)
	{
		switch (modifierSrc)
		{
		case MODIFIERSOURCE_CLASS :
			modifiers &= J9CfrClassFile.CFR_CLASS_ACCESS_MASK;
			break;

		case MODIFIERSOURCE_METHOD :
			if (ONLY_SPEC_MODIFIERS == modScope) {
				modifiers &= J9CfrClassFile.CFR_METHOD_ACCESS_MASK;
			} else {
				modifiers &= J9CfrClassFile.CFR_METHOD_ACCESS_MASK | J9CfrClassFile.CFR_ACC_EMPTY_METHOD | J9CfrClassFile.CFR_ACC_FORWARDER_METHOD;
			}
			break;

		case MODIFIERSOURCE_FIELD :
			if (ONLY_SPEC_MODIFIERS == modScope) {
				modifiers &= J9CfrClassFile.CFR_FIELD_ACCESS_MASK;
			} else {
				modifiers &= J9CfrClassFile.CFR_FIELD_ACCESS_MASK | J9FieldFlags.J9FieldFlagConstant | J9FieldFlags.J9FieldFlagIsNullRestricted;
			}
			break;

		case MODIFIERSOURCE_METHODPARAMETER :
			if (ONLY_SPEC_MODIFIERS == modScope) {
				modifiers &= J9CfrClassFile.CFR_ATTRIBUTE_METHOD_PARAMETERS_MASK;
			} else {
				/* We dont have any internal modifiers now, once we have them, they need to be or'ed with the mask below. */
				modifiers &= J9CfrClassFile.CFR_ATTRIBUTE_METHOD_PARAMETERS_MASK;
			}
			break;

		default :
			out.append("TYPE OF MODIFIER IS INVALID");
			return;
		}

		/* Parse internal flags first.
		 * Since we might be using same bit for internal purposes if we know that bit can not be used.
		 * For instance, Class (not nested class) can not be protected or private, so these bits can be used internally.
		 */
		if (INCLUDE_INTERNAL_MODIFIERS == modScope) {
			if (MODIFIERSOURCE_METHOD == modifierSrc) {
				if ((modifiers & J9CfrClassFile.CFR_ACC_EMPTY_METHOD) != 0) {
					out.append("(empty) ");
					modifiers &= ~J9CfrClassFile.CFR_ACC_EMPTY_METHOD;
				}

				if ((modifiers & J9CfrClassFile.CFR_ACC_FORWARDER_METHOD) != 0) {
					out.append("(forwarder) ");
					modifiers &= ~J9CfrClassFile.CFR_ACC_FORWARDER_METHOD;
				}

				if ((modifiers & J9CfrClassFile.CFR_ACC_VTABLE) != 0) {
					out.append("(vtable) ");
					modifiers &= ~J9CfrClassFile.CFR_ACC_VTABLE;
				}

				if ((modifiers & J9CfrClassFile.CFR_ACC_HAS_EXCEPTION_INFO) != 0) {
					out.append("(hasExceptionInfo) ");
					modifiers &= ~J9CfrClassFile.CFR_ACC_HAS_EXCEPTION_INFO;
				}
			}

			if (MODIFIERSOURCE_FIELD == modifierSrc) {
				if ((modifiers & J9FieldFlags.J9FieldFlagConstant) != 0) {
					out.append("(constant) ");
					modifiers &= ~J9FieldFlags.J9FieldFlagConstant;
				}
				if ((modifiers & J9FieldFlags.J9FieldFlagIsNullRestricted) != 0) {
					out.append("(NullRestricted) ");
					modifiers &= ~J9FieldFlags.J9FieldFlagIsNullRestricted;
				}
			}
		}

		/* Method params don't use the scope modifiers. Scope is always within a method */
		if (MODIFIERSOURCE_METHODPARAMETER != modifierSrc) {
			if ((modifiers & J9CfrClassFile.CFR_PUBLIC_PRIVATE_PROTECTED_MASK) == 0) {
				out.append("default ");
			} else {
				if ((modifiers & J9CfrClassFile.CFR_ACC_PUBLIC) != 0) {
					out.append("public ");
					modifiers &= ~J9CfrClassFile.CFR_ACC_PUBLIC;
				}

				if ((modifiers & J9CfrClassFile.CFR_ACC_PROTECTED) != 0) {
					out.append("protected ");
					modifiers &= ~J9CfrClassFile.CFR_ACC_PROTECTED;
				}

				if ((modifiers & J9CfrClassFile.CFR_ACC_PRIVATE) != 0) {
					out.append("private ");
					modifiers &= ~J9CfrClassFile.CFR_ACC_PRIVATE;
				}
			}
		}
		if ((modifiers & J9CfrClassFile.CFR_ACC_STATIC) != 0) {
			out.append("static ");
			modifiers &= ~J9CfrClassFile.CFR_ACC_STATIC;
		}

		if ((modifiers & J9CfrClassFile.CFR_ACC_FINAL) != 0) {
			out.append("final ");
			modifiers &= ~J9CfrClassFile.CFR_ACC_FINAL;
		}

		if ((modifiers & J9CfrClassFile.CFR_ACC_SYNTHETIC) != 0) {
			out.append("synthetic ");
			modifiers &= ~J9CfrClassFile.CFR_ACC_SYNTHETIC;
		}

		if (MODIFIERSOURCE_METHODPARAMETER == modifierSrc) {
			if (0 != (modifiers & J9CfrClassFile.CFR_ACC_MANDATED)) {
				out.append("mandated");
				modifiers &= ~J9CfrClassFile.CFR_ACC_MANDATED;
			}
		}

		if (modifierSrc != MODIFIERSOURCE_METHOD) {
			if ((modifiers & J9CfrClassFile.CFR_ACC_ABSTRACT) != 0) {
				out.append("abstract ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_ABSTRACT;
			}

			if ((modifiers & J9CfrClassFile.CFR_ACC_ENUM) != 0) {
				out.append("enum ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_ENUM;
			}
		}

		if (modifierSrc == MODIFIERSOURCE_CLASS) {
			if ((modifiers & J9CfrClassFile.CFR_ACC_INTERFACE) != 0) {
				out.append("interface ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_INTERFACE;
			}

			if ((modifiers & J9CfrClassFile.CFR_ACC_SUPER) != 0) {
				out.append("super ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_SUPER;
			}

			if ((modifiers & J9CfrClassFile.CFR_ACC_ANNOTATION) != 0) {
				out.append("annotation ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_ANNOTATION;
			}
			if (isValueClass) {
				out.append("value ");
			}
		}

		if (modifierSrc == MODIFIERSOURCE_METHOD) {
			if ((modifiers & J9CfrClassFile.CFR_ACC_SYNCHRONIZED) != 0) {
				out.append("synchronized ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_SYNCHRONIZED;
			}

			if ((modifiers & J9CfrClassFile.CFR_ACC_BRIDGE) != 0) {
				out.append("bridge ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_BRIDGE;
			}

			if ((modifiers & J9CfrClassFile.CFR_ACC_VARARGS) != 0) {
				out.append("varargs ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_VARARGS;
			}

			if ((modifiers & J9CfrClassFile.CFR_ACC_NATIVE) != 0) {
				out.append("native ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_NATIVE;
			}

			if ((modifiers & J9CfrClassFile.CFR_ACC_STRICT) != 0) {
				out.append("strict ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_STRICT;
			}
		}

		if (modifierSrc == MODIFIERSOURCE_FIELD) {
			if ((modifiers & J9CfrClassFile.CFR_ACC_VOLATILE) != 0) {
				out.append("volatile ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_VOLATILE;
			}

			if ((modifiers & J9CfrClassFile.CFR_ACC_TRANSIENT) != 0) {
				out.append("transient ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_TRANSIENT;
			}

			if ((modifiers & J9CfrClassFile.CFR_ACC_STRICT) != 0) {
				out.append("strict ");
				modifiers &= ~J9CfrClassFile.CFR_ACC_STRICT;
			}
		}

		if (modifiers != 0) {
			out.append(String.format("unknown_flags = 0x%X", modifiers));
		}
	}

	private static void dumpNative(PrintStream out, J9ROMMethodPointer romMethod, long flags) throws CorruptDataException {
		U8Pointer bytecodes = J9ROMMethodHelper.bytecodes(romMethod);
		long argCount = bytecodes.at(0).longValue();
		U8 returnType = bytecodes.at(1);
		U8Pointer currentDescription = bytecodes.add(2);

		String descriptions[] = new String[] { "void", "boolean", "byte", "char", "short", "float", "int", "double", "long", "object" };
		out.append(String.format("  Argument Count: %d", romMethod.argCount().longValue()));
		out.append(nl);
		out.append(String.format("  Temp Count: %d", romMethod.tempCount().longValue()));
		out.append(nl);
		out.append(String.format("  Native Argument Count: %d, types: (", argCount));
		for (long i = argCount; i > 0; i--) {
			out.append(descriptions[currentDescription.at(0).intValue()]);
			currentDescription = currentDescription.add(1);
			if (i != 1) {
				out.append(",");
			}
		}
		out.append(String.format(") %s ", descriptions[returnType.intValue()]));
		out.append(nl);
	}

	private static void dumpBytecodes(PrintStream out, J9ROMClassPointer romClass, J9ROMMethodPointer romMethod, long flags) throws CorruptDataException {
		try {
			ByteCodeDumper.dumpBytecodes(out, romClass, romMethod, new U32(flags));
		} catch (Exception e) {
			throw new CorruptDataException(e);
		}
	}

	private static void dumpMethodDebugInfo(PrintStream out, J9ROMClassPointer romClass, J9ROMMethodPointer romMethod, long flags) throws CorruptDataException {
		J9MethodDebugInfoPointer methodInfo;

		if ((flags & J9BCTranslationData.BCT_StripDebugAttributes) == 0) {
			methodInfo = getMethodDebugInfoFromROMMethod(romMethod);
			if (!methodInfo.isNull()) {
				out.append(nl);
				out.append("  Debug Info:");
				out.append(nl);
				out.append(String.format("    Line Number Table (%d):", J9MethodDebugInfoHelper.getLineNumberCount(methodInfo).intValue()));
				out.append(nl);
				LineNumberIterator lineNumberIterator = LineNumberIterator.lineNumberIteratorFor(methodInfo);
				while (lineNumberIterator.hasNext()) {
					LineNumber lineNumber = lineNumberIterator.next();
					if (null == lineNumber) {
						out.append("      Bad compressed data \n");
						U8Pointer currentLineNumberPtr = lineNumberIterator.getLineNumberTablePtr();
						int sizeLeft = J9MethodDebugInfoHelper.getLineNumberCompressedSize(methodInfo).sub((currentLineNumberPtr.sub(J9MethodDebugInfoHelper.getLineNumberTableForROMClass(methodInfo)))).intValue();
						while (0 < sizeLeft--) {
							out.append("      " + currentLineNumberPtr.at(0));
							out.append(nl);
							currentLineNumberPtr = currentLineNumberPtr.add(1);
						}
						break;
					} else {
						out.append(String.format("      Line: %5d PC: %5d", lineNumber.getLineNumber().longValue(), lineNumber.getLocation().longValue()));
						out.append(nl);
					}
				}
				out.append(nl);

				out.append(String.format("    Variables (%d):", methodInfo.varInfoCount().longValue()));
				out.append(nl);
				LocalVariableTableIterator variableInfoValuesIterator = LocalVariableTableIterator.localVariableTableIteratorFor(methodInfo);
				while (variableInfoValuesIterator.hasNext()) {
					LocalVariableTable values = variableInfoValuesIterator.next();

					out.append(String.format("      Slot: %d", values.getSlotNumber().intValue()));
					out.append(nl);
					out.append(String.format("      Visibility Start: %d", values.getStartVisibility().intValue()));
					out.append(nl);
					out.append(String.format("      Visibility End: %d", values.getStartVisibility().intValue() + values.getVisibilityLength().intValue()));
					out.append(nl);
					out.append(String.format("      Visibility Length: %d", values.getVisibilityLength().intValue()));
					out.append(nl);
					out.append("      Name: ");
					if ((null != values.getName()) && (!values.getName().isNull())) {
						out.append(String.format("%s", J9UTF8Helper.stringValue(values.getName())));
					} else {
						out.append("None");
					}
					out.append(nl);
					out.append("      Signature: ");
					if ((null != values.getSignature()) && (!values.getSignature().isNull())) {
						out.append(String.format("%s", J9UTF8Helper.stringValue(values.getSignature())));
					} else {
						out.append("None");
					}
					out.append(nl);
					out.append("      Generic Signature: ");
					if ((null != values.getGenericSignature()) && (!values.getGenericSignature().isNull())) {
						out.append(String.format("%s", J9UTF8Helper.stringValue(values.getGenericSignature())));

					} else {
						out.append("None");
					}
					out.append(nl);
				}
			}
		}
	}

	public static void j9bcutil_dumpRomClass(PrintStream out, J9ROMClassPointer romClass, long flags) throws CorruptDataException {
		out.append((String.format("ROM Size: 0x%s (%d)", Long.toHexString(romClass.romSize().longValue()), romClass.romSize().longValue())));
		out.append(nl);
		out.append(String.format("Class Name: %s", J9UTF8Helper.stringValue(romClass.className())));
		out.append(nl);
		if (romClass.superclassName().isNull()) {
			out.append("Superclass Name: <none>");
		} else {
			out.append(String.format("Superclass Name: %s", J9UTF8Helper.stringValue(romClass.superclassName())));
		}
		out.append(nl);

		/* dump the source file name */
		dumpSourceFileName(out, romClass, flags);

		/* dump the simple name */
		dumpSimpleName(out, romClass, flags);

		/* dump the class generic signature */
		dumpGenericSignature(out, romClass, flags);

		/* dump the enclosing method */
		dumpEnclosingMethod(out, romClass, flags);

		out.append(String.format("Basic Access Flags (0x%s): ", Long.toHexString(romClass.modifiers().longValue())));
		dumpModifiers(out, romClass.modifiers().longValue(), MODIFIERSOURCE_CLASS, ONLY_SPEC_MODIFIERS,
			ValueTypeHelper.getValueTypeHelper().isRomClassAValueType(romClass));
		out.append(nl);
		out.append(String.format("J9 Access Flags (0x%s): ", Long.toHexString(romClass.extraModifiers().longValue())));
		dumpClassJ9ExtraModifiers(out, romClass.extraModifiers().longValue());
		out.append(nl);

		out.append(String.format("Class file version: %d.%d", romClass.majorVersion().longValue(), romClass.minorVersion().longValue()));
		out.append(nl);

		out.append(String.format("Instance Shape: 0x%s", Long.toHexString(romClass.instanceShape().longValue())));
		out.append(nl);
		out.append(String.format("Intermediate Class Data (%d bytes): %s", romClass.intermediateClassDataLength().longValue(), Long.toHexString(romClass.intermediateClassData().longValue())));
		out.append(nl);
		out.append(String.format("Maximum Branch Count: %d", romClass.maxBranchCount().longValue()));
		out.append(nl);

		out.append(String.format("Interfaces (%d):" + nl, romClass.interfaceCount().longValue()));
		if (!romClass.interfaceCount().eq(0)) {
			SelfRelativePointer interfaces = romClass.interfaces();

			long interfaceCount = romClass.interfaceCount().longValue();
			for (int i = 0; i < interfaceCount; i++) {
				out.append("  ");
				J9UTF8Pointer interfaceName = J9UTF8Pointer.cast(interfaces.get());
				out.append(J9UTF8Helper.stringValue(interfaceName));
				out.append(nl);
				interfaces = interfaces.add(1);
			}
		}

		J9UTF8Pointer outerClassName = romClass.outerClassName();
		if (!outerClassName.isNull()) {
			out.append("Declaring Class: " + J9UTF8Helper.stringValue(romClass.outerClassName()));
			out.append(nl);
			out.append(String.format("Member Access Flags (0x%s): ", Long.toHexString(romClass.memberAccessFlags().longValue())));
			dumpModifiers(out, romClass.memberAccessFlags().longValue(), MODIFIERSOURCE_CLASS, ONLY_SPEC_MODIFIERS,
				ValueTypeHelper.getValueTypeHelper().isRomClassAValueType(romClass));
			out.append(nl);

			outerClassName = outerClassName.add(1);
		}
		long innerClassCount = romClass.innerClassCount().longValue();
		if (innerClassCount != 0) {
			SelfRelativePointer innerClasses = romClass.innerClasses();
			out.format("Declared Classes (%d):%n", innerClassCount);

			for (int i = 0; i < innerClassCount; i++) {
				J9UTF8Pointer innerClassName = J9UTF8Pointer.cast(innerClasses.get());
				out.format("   %s%n", J9UTF8Helper.stringValue(innerClassName));
				innerClasses = innerClasses.add(1);
			}
		}

		/* Permitted subclasses for a sealed class */
		if (J9ROMClassHelper.isSealed(romClass)) {
			int permittedSubclassCount = OptInfo.getPermittedSubclassCount(romClass);
			out.format("Permitted subclasses (%d):%n", permittedSubclassCount);

			for (int i = 0; i < permittedSubclassCount; i++) {
				J9UTF8Pointer permittedSubclassName = OptInfo.getPermittedSubclassNameAtIndex(romClass, i);
				out.format("   %s%n", J9UTF8Helper.stringValue(permittedSubclassName));
			}
		}

		if (J9ROMClassHelper.hasLoadableDescriptorsAttribute(romClass)) {
			int loadableDescriptorsCount = OptInfo.getLoadableDescriptorsCount(romClass);
			out.format("Loadable Descriptors (%d):%n", loadableDescriptorsCount);

			for (int i = 0; i < loadableDescriptorsCount; i++) {
				J9UTF8Pointer loadableDescriptor = OptInfo.getLoadableDescriptorAtIndex(romClass, i);
				out.format("   %s%n", J9UTF8Helper.stringValue(loadableDescriptor));
			}
		}

		if (J9ROMClassHelper.hasImplicitCreationAttribute(romClass)) {
			U32 implicitCreationFlags = OptInfo.getImplicitCreationFlags(romClass);
			out.format("ImplicitCreation flags: %s%n", implicitCreationFlags.getHexValue());
		}

		UDATA romFieldCount = romClass.romFieldCount();
		out.append(String.format("Fields (%d):" + nl, romFieldCount.longValue()));

		J9ROMFieldShapeIterator iterator = new J9ROMFieldShapeIterator(romClass.romFields(), romFieldCount);

		while (iterator.hasNext()) {
			J9ROMFieldShapePointer currentField = (J9ROMFieldShapePointer) iterator.next();
			if (!currentField.modifiers().bitAnd(J9JavaAccessFlags.J9AccStatic).eq(0)) {
				dumpRomStaticField(out, currentField, flags);
			} else {
				dumpRomField(out, currentField, flags);
			}
			out.append(nl);
		}

		dumpCPShapeDescription(out, romClass, flags);

		long romMethodsCount = romClass.romMethodCount().longValue();
		out.append(String.format("Methods (%d):" + nl, romMethodsCount));

		J9ROMMethodPointer romMethod = romClass.romMethods();
		for (int i = 0; i < romClass.romMethodCount().intValue(); i++) {
			J9BCUtil.j9bcutil_dumpRomMethod(out, romMethod, romClass, flags, 0);
			romMethod = ROMHelp.nextROMMethod(romMethod);
		}

		/* dump source debug extension */
		dumpSourceDebugExtension(out, romClass, flags);

		/* dump annotation info */
		dumpAnnotationInfo(out, romClass, flags);

		/* dump callsite data */
		dumpCallSiteData(out, romClass);

		/* dump split side tables */
		dumpStaticSplitSideTable(out, romClass);
		dumpSpecialSplitSideTable(out, romClass);
	}

	private static void dumpCPShapeDescription(PrintStream out, J9ROMClassPointer romClass, long flags) throws CorruptDataException {
		U32Pointer cpDescription = J9ROMClassHelper.cpShapeDescription(romClass);
		long descriptionLong;
		long i, j, k, numberOfLongs;
		char symbols[] = new char[] { '.', 'C', 'S', 'I', 'F', 'J', 'D', 'i', 's', 'v', 'x', 'y', 'z', 'T', 'H', 'A', '.', 'c', 'x', 'v' };

		symbols[(int)J9CPTYPE_UNUSED8] = '.';

		numberOfLongs = (romClass.romConstantPoolCount().longValue() + J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32 - 1) / J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32;

		out.append("CP Shape Description:" + nl);
		k = romClass.romConstantPoolCount().longValue();
		for (i = 0; i < numberOfLongs; i++) {
			out.append("  ");
			descriptionLong = cpDescription.at(i).longValue();
			for (j = 0; j < J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32; j++, k--) {
				if (k == 0)
					break;
				out.append(String.format("%c ", symbols[(int) (descriptionLong & J9ConstantPool.J9_CP_DESCRIPTION_MASK)]));
				descriptionLong >>= J9ConstantPool.J9_CP_BITS_PER_DESCRIPTION;
			}
			out.append(nl);
		}
		out.append(nl);

	}

	private static void dumpRomField(PrintStream out, J9ROMFieldShapePointer romField, long flags) throws CorruptDataException {
		out.append("  Name: " + J9UTF8Helper.stringValue(romField.nameAndSignature().name()) + nl);
		out.append("  Signature: " + J9UTF8Helper.stringValue(romField.nameAndSignature().signature()) + nl);
		out.append(String.format("  Access Flags (%s): ", Long.toHexString(romField.modifiers().longValue())));
		dumpModifiers(out, romField.modifiers().longValue(), MODIFIERSOURCE_FIELD, INCLUDE_INTERNAL_MODIFIERS, false);
		out.append(nl);
	}

	private static void dumpRomStaticField(PrintStream out, J9ROMFieldShapePointer romStatic, long flags) throws CorruptDataException {
		out.append("  Name: " + J9UTF8Helper.stringValue(romStatic.nameAndSignature().name()) + nl);
		out.append("  Signature: " + J9UTF8Helper.stringValue(romStatic.nameAndSignature().signature()) + nl);
		out.append(String.format("  Access Flags (%s): ", Long.toHexString(romStatic.modifiers().longValue())));
		dumpModifiers(out, romStatic.modifiers().longValue(), MODIFIERSOURCE_FIELD, INCLUDE_INTERNAL_MODIFIERS, false);
		out.append(nl);
	}

	/*
	 * Dump a printed representation of the specified @accessFlags to @out.
	 */

	private static void dumpClassJ9ExtraModifiers(PrintStream out, long accessFlags) {
		if ((accessFlags & J9CfrClassFile.CFR_ACC_REFERENCE_WEAK) != 0)
			out.append("(weak) ");
		if ((accessFlags & J9CfrClassFile.CFR_ACC_REFERENCE_SOFT) != 0)
			out.append("(soft) ");
		if ((accessFlags & J9CfrClassFile.CFR_ACC_REFERENCE_PHANTOM) != 0)
			out.append("(phantom) ");
		if ((accessFlags & J9CfrClassFile.CFR_ACC_HAS_FINAL_FIELDS) != 0)
			out.append("(final fields) ");
		if ((accessFlags & J9CfrClassFile.CFR_ACC_HAS_VERIFY_DATA) != 0)
			out.append("(preverified) ");
		if ((accessFlags & J9JavaAccessFlags.J9AccClassAnonClass) != 0)
			out.append("(anonClass) ");
		if ((accessFlags & J9JavaAccessFlags.J9AccClassIsUnmodifiable) != 0)
			out.append("(unmodifiable) ");
		if ((accessFlags & J9JavaAccessFlags.J9AccRecord) != 0)
			out.append("(record) ");
		if ((accessFlags & J9JavaAccessFlags.J9AccSealed) != 0)
			out.append("(sealed) ");
	}

	private static void dumpMethodJ9Modifiers(PrintStream out, UDATA modifiers, UDATA extraModifiers) {
		if (modifiers.allBitsIn(J9JavaAccessFlags.J9AccMethodCallerSensitive)) {
			out.append(" @CallerSensitive");
		}
		if (modifiers.allBitsIn(J9JavaAccessFlags.J9AccMethodFrameIteratorSkip)) {
			out.append(" @FrameIteratorSkip");
		}
		if (extraModifiers.allBitsIn(J9CfrClassFile.CFR_METHOD_EXT_NOT_CHECKPOINT_SAFE_ANNOTATION)) {
			out.append(" @NotCheckpointSafe");
		}
		if (extraModifiers.allBitsIn(J9CfrClassFile.CFR_METHOD_EXT_HAS_SCOPED_ANNOTATION)) {
			out.append(" @Scoped");
		}
	}

	private static void dumpEnclosingMethod(PrintStream out, J9ROMClassPointer romClass, long flags) throws CorruptDataException {
		J9EnclosingObjectPointer enclosingMethodForROMClass = OptInfo.getEnclosingMethodForROMClass(romClass);
		if (!enclosingMethodForROMClass.isNull()) {
			J9ROMConstantPoolItemPointer constantPool = ConstantPoolHelpers.J9_ROM_CP_FROM_ROM_CLASS(romClass);

			J9ROMClassRefPointer romClassRefPointer = J9ROMClassRefPointer.cast(constantPool);
			String className = romClassRefPointer.name().toString();

			J9ROMNameAndSignaturePointer nameAndSignature  = enclosingMethodForROMClass.nameAndSignature();

			if (!nameAndSignature.isNull()) {
				out.append(String.format("Enclosing Method: %s%s%s", className, J9UTF8Helper.stringValue(nameAndSignature.name()), J9UTF8Helper.stringValue(nameAndSignature.signature())));
			} else {
				out.append(String.format("Enclosing Class: %s", className));
			}
		}
	}

	private static void dumpGenericSignature(PrintStream out, J9ROMClassPointer romClass, long flags) throws CorruptDataException {
		String simpleNameForROMClass = OptInfo.getGenericSignatureForROMClass(romClass);
		if (simpleNameForROMClass != null) {
			out.append(String.format("Generic Signature: %s", simpleNameForROMClass));
			out.append(nl);
		}
	}

	private static void dumpSimpleName(PrintStream out, J9ROMClassPointer romClass, long flags) throws CorruptDataException {
		String simpleNameForROMClass = OptInfo.getSimpleNameForROMClass(romClass);
		if (simpleNameForROMClass != null) {
			out.append(String.format("Simple Name: %s", simpleNameForROMClass));
			out.append(nl);
		}
	}

	private static void dumpSourceFileName(PrintStream out, J9ROMClassPointer romClass, long flags) throws CorruptDataException {
		if (((flags & J9BCTranslationData.BCT_StripDebugAttributes) == 0)) {
			String sourceFileNameForROMClass = OptInfo.getSourceFileNameForROMClass(romClass);
			if (sourceFileNameForROMClass != null) {
				out.append(String.format("Source File Name: %s", sourceFileNameForROMClass));
				out.append(nl);
			}
		}
	}

	private static void dumpSourceDebugExtension(PrintStream out, J9ROMClassPointer romClass, long flags) throws CorruptDataException {
		if (J9BuildFlags.J9VM_OPT_DEBUG_INFO_SERVER) {
			U8Pointer current;
			UDATA temp;

			if ((flags & J9BCTranslationData.BCT_StripDebugAttributes) == 0) {
				J9SourceDebugExtensionPointer sde = OptInfo.getSourceDebugExtensionForROMClass(romClass);
				if (!sde.isNull()) {
					temp = sde.size();
					if (!temp.eq(0)) {
						current = U8Pointer.cast(sde.add(1));

						out.append(String.format("  Source debug extension (%d bytes):    ", temp.longValue()));
						out.append(nl);

						while (!temp.eq(0)) {
							temp = temp.sub(1);
							U8 c = current.at(0);
							current = current.add(1);

							if (c.eq('\015')) {
								if (!temp.eq(0)) {
									if (current.at(0).eq('\012')) {
										current = current.add(1);
									}
									out.append(nl + "    ");
								}
							} else if (c.eq('\012')) {
								out.append(nl + "    ");
							} else {
								out.append((char) c.intValue());
							}
						}
					}
				}
			}
		}
	}

	private static void dumpAnnotationInfo(PrintStream out, J9ROMClassPointer romClass, long flags) throws CorruptDataException {
		U32Pointer classAnnotationData = OptInfo.getClassAnnotationsDataForROMClass(romClass);
		U32Pointer classTypeAnnotationData = OptInfo.getClassTypeAnnotationsDataForROMClass(romClass);
		out.append("  Annotation Info:" + nl);
		if (!classAnnotationData.isNull()) {
			U32 length = classAnnotationData.at(0);
			U32Pointer data = classAnnotationData.add(1);
			out.append(String.format("    Class Annotations (%d bytes): %s" + nl, length.intValue(), data.getHexAddress()));
		}

		if (!classTypeAnnotationData.isNull()) {
			U32 length = classTypeAnnotationData.at(0);
			U32Pointer data = classTypeAnnotationData.add(1);
			out.append(String.format("    Class Type Annotations (%d bytes): %s" + nl, length.intValue(), data.getHexAddress()));
		}

		/* print field annotations */
		J9ROMFieldShapeIterator iterator = new J9ROMFieldShapeIterator(romClass.romFields(), romClass.romFieldCount());
		out.append("    Field Annotations:" + nl);
		while (iterator.hasNext()) {
			J9ROMFieldShapePointer currentField = (J9ROMFieldShapePointer) iterator.next();
			U32Pointer fieldAnnotationData = J9ROMFieldShapeHelper.getFieldAnnotationsDataFromROMField(currentField);
			U32Pointer fieldTypeAnnotationData = J9ROMFieldShapeHelper.getFieldTypeAnnotationsDataFromROMField(currentField);
			if (!fieldAnnotationData.isNull()) {
				U32 length = fieldAnnotationData.at(0);
				U32Pointer data = fieldAnnotationData.add(1);

				out.append("     Name: " + J9UTF8Helper.stringValue(currentField.nameAndSignature().name()) + nl);
				out.append("     Signature: " + J9UTF8Helper.stringValue(currentField.nameAndSignature().signature()) + nl);
				out.append(String.format("      Annotations (%d bytes): %s" + nl, length.intValue(), data.getHexAddress()));
			}
			if (!fieldTypeAnnotationData.isNull()) {
				U32 length = fieldTypeAnnotationData.at(0);
				U32Pointer data = fieldTypeAnnotationData.add(1);
				out.append(String.format("      Type Annotations (%d bytes): %s" + nl, length.intValue(), data.getHexAddress()));
			}
		}
		out.append(nl);

		/* print method, parameter and default annotations */
		J9ROMMethodPointer romMethod = romClass.romMethods();
		out.append("    Method Annotations:" + nl);
		for (int i = 0; i < romClass.romMethodCount().intValue(); i++) {
			dumpMethodAnnotations(out, romMethod);
			romMethod = ROMHelp.nextROMMethod(romMethod);
		}
		out.append(nl);
	}

	private static void dumpMethodAnnotations(PrintStream out,
			J9ROMMethodPointer romMethod) throws CorruptDataException {
		U32Pointer methodAnnotationData = ROMHelp.getMethodAnnotationsDataFromROMMethod(romMethod);
		U32Pointer parametersAnnotationData = ROMHelp.getParameterAnnotationsDataFromROMMethod(romMethod);
		U32Pointer defaultAnnotationData = ROMHelp.getDefaultAnnotationDataFromROMMethod(romMethod);
		U32Pointer methodTypeAnnotations = ROMHelp.getMethodTypeAnnotationDataFromROMMethod(romMethod);
		U32Pointer codeTypeAnnotations = ROMHelp.getCodeTypeAnnotationDataFromROMMethod(romMethod);
		if ((!methodAnnotationData.isNull()) || (!parametersAnnotationData.isNull()) || (!defaultAnnotationData.isNull())) {
			J9ROMNameAndSignaturePointer nameAndSignature = romMethod.nameAndSignature();
			J9UTF8Pointer name = nameAndSignature.name();
			J9UTF8Pointer signature = nameAndSignature.signature();
			out.append("      Name: " + J9UTF8Helper.stringValue(name) + nl);
			out.append("      Signature: " + J9UTF8Helper.stringValue(signature) + nl);
		}
		if (!methodAnnotationData.isNull()) {
			U32 length = methodAnnotationData.at(0);
			U32Pointer data = methodAnnotationData.add(1);
			out.append(String.format("      Annotations (%d bytes): %s" + nl, length.intValue(), data.getHexAddress()));
		}
		if (!parametersAnnotationData.isNull()) {
			U32 length = parametersAnnotationData.at(0);
			U32Pointer data = parametersAnnotationData.add(1);
			out.append(String.format("      Parameters Annotations (%d bytes): %s" + nl, length.intValue(), data.getHexAddress()));
		}
		if (!defaultAnnotationData.isNull()) {
			U32 length = defaultAnnotationData.at(0);
			U32Pointer data = defaultAnnotationData.add(1);
			out.append(String.format("      Default Annotations (%d bytes): %s" + nl, length.intValue(), data.getHexAddress()));
		}
		if (!methodTypeAnnotations.isNull()) {
			U32 length = methodTypeAnnotations.at(0);
			U32Pointer data = methodTypeAnnotations.add(1);
			out.append(String.format("      Method Type Annotations (%d bytes): %s" + nl, length.intValue(), data.getHexAddress()));
		}
		if (!codeTypeAnnotations.isNull()) {
			U32 length = codeTypeAnnotations.at(0);
			U32Pointer data = codeTypeAnnotations.add(1);
			out.append(String.format("      Code Type Annotations (%d bytes): %s" + nl, length.intValue(), data.getHexAddress()));
		}
	}

	/**
	 * This method is Java implementation of rdump.c#dumpCallSiteData function.
	 * This method is called when dumping a ROMClass.
	 * This method has only affect for invokedDynamic stuff,
	 * for other ROMClasses, there wont be anything to print since their callsite and bsm count are zero.
	 *
	 * @param out PrintStream to print the user info to the console
	 * @param romClass ROMClass address in the dump file.
	 *
	 * @throws CorruptDataException
	 */
	private static void dumpCallSiteData(PrintStream out, J9ROMClassPointer romClass) throws CorruptDataException
	{
		int HEX_RADIX = 16;
		long callSiteCount = romClass.callSiteCount().longValue();
		long bsmCount = romClass.bsmCount().longValue();
		SelfRelativePointer callSiteData = SelfRelativePointer.cast(romClass.callSiteData());
		U16Pointer bsmIndices = U16Pointer.cast(callSiteData.addOffset(4*callSiteCount));

		if (0 != callSiteCount) {
			out.println(String.format("  Call Sites (%d):\n", callSiteCount));
			for(int i = 0; i < callSiteCount; i++){
				J9ROMNameAndSignaturePointer nameAndSig = J9ROMNameAndSignaturePointer.cast(callSiteData.add(i).get());
				out.println("    Name: " + J9UTF8Helper.stringValue(nameAndSig.name()));
				out.println("    Signature: " + J9UTF8Helper.stringValue(nameAndSig.signature()));
				out.println("    Bootstrap Method Index: " + bsmIndices.at(i).longValue());
				out.println();
			}
		}

		if (0 != bsmCount) {
			J9ROMConstantPoolItemPointer constantPool = ConstantPoolHelpers.J9_ROM_CP_FROM_ROM_CLASS(romClass);
			U32Pointer cpShapeDescription = J9ROMClassHelper.cpShapeDescription(romClass);
			U16Pointer bsmDataCursor = bsmIndices.add(callSiteCount);

			out.println(String.format("  Bootstrap Methods (%d):", bsmCount));
			for(int i = 0; i < bsmCount; i++){
				J9ROMMethodHandleRefPointer methodHandleRef = J9ROMMethodHandleRefPointer.cast(constantPool.add(bsmDataCursor.at(0).longValue()));
				bsmDataCursor = bsmDataCursor.add(1);
				/* methodRef will be either a field or a method ref - they both have the same shape so we can pretend it is always a methodref */
				J9ROMMethodRefPointer methodRef = J9ROMMethodRefPointer.cast(constantPool.add(methodHandleRef.methodOrFieldRefIndex().longValue()));
				J9ROMClassRefPointer classRef = J9ROMClassRefPointer.cast(constantPool.add(methodRef.classRefCPIndex().longValue()));
				J9ROMNameAndSignaturePointer nameAndSig = methodRef.nameAndSignature();
				long bsmArgumentCount = bsmDataCursor.at(0).longValue();
				bsmDataCursor = bsmDataCursor.add(1);

				out.println("    Name: " + J9UTF8Helper.stringValue(classRef.name())
						+ "." + J9UTF8Helper.stringValue(nameAndSig.name()));

				out.println("    Signature: " + J9UTF8Helper.stringValue(nameAndSig.signature()));

				out.println(String.format("    Bootstrap Method Arguments (%d):", bsmArgumentCount));

				for (; 0 != bsmArgumentCount; bsmArgumentCount--) {
					long argCPIndex = bsmDataCursor.at(0).longValue();
					bsmDataCursor = bsmDataCursor.add(1);
					J9ROMConstantPoolItemPointer item = constantPool.add(argCPIndex);
					long shapeDesc = ConstantPoolHelpers.J9_CP_TYPE(cpShapeDescription, (int)argCPIndex);

					if (shapeDesc == J9CPTYPE_CLASS) {
						J9ROMClassRefPointer romClassRef = J9ROMClassRefPointer.cast(item);
						out.println("      Class: " + J9UTF8Helper.stringValue(romClassRef.name()));
					} else if (shapeDesc == J9CPTYPE_STRING) {
						J9ROMStringRefPointer romStringRef = J9ROMStringRefPointer.cast(item);
						out.println("      String: " + J9UTF8Helper.stringValue(romStringRef.utf8Data()));
					} else if (shapeDesc == J9CPTYPE_INT) {
						J9ROMSingleSlotConstantRefPointer singleSlotConstantRef = J9ROMSingleSlotConstantRefPointer.cast(item);
						out.println("      Int: " + singleSlotConstantRef.data().getHexValue());
					} else if (shapeDesc == J9CPTYPE_FLOAT) {
						J9ROMSingleSlotConstantRefPointer singleSlotConstantRef = J9ROMSingleSlotConstantRefPointer.cast(item);
						FloatPointer floatPtr = FloatPointer.cast(singleSlotConstantRef.dataEA());
						out.println("      Float: " + floatPtr.getHexValue() + " (" + floatPtr.floatAt(0) + ")");
					} else if (shapeDesc == J9CPTYPE_LONG) {
						String hexValue = "";
						if (J9BuildFlags.J9VM_ENV_LITTLE_ENDIAN) {
							hexValue += item.slot2().getHexValue();
							hexValue += item.slot1().getHexValue().substring(2);
						} else {
							hexValue += item.slot1().getHexValue();
							hexValue += item.slot2().getHexValue().substring(2);
						}
						long longValue = Long.parseLong(hexValue.substring(2), HEX_RADIX);
						out.println("      Long: " + hexValue + "(" + longValue + ")");
					} else if (shapeDesc == J9CPTYPE_DOUBLE) {
						String hexValue = "";
						if (J9BuildFlags.J9VM_ENV_LITTLE_ENDIAN) {
							hexValue += item.slot2().getHexValue();
							hexValue += item.slot1().getHexValue().substring(2);
						} else {
							hexValue += item.slot1().getHexValue();
							hexValue += item.slot2().getHexValue().substring(2);
						}
						long longValue = Long.parseLong(hexValue.substring(2), HEX_RADIX);
						double doubleValue = Double.longBitsToDouble(longValue);
						out.println("      Double: " + hexValue + "(" + Double.toString(doubleValue) + ")");
					} else if (shapeDesc == J9CPTYPE_FIELD) {
						J9ROMFieldRefPointer romFieldRef = J9ROMFieldRefPointer.cast(item);
						classRef = J9ROMClassRefPointer.cast(constantPool.add(romFieldRef.classRefCPIndex()));
						nameAndSig = romFieldRef.nameAndSignature();
						out.println("      Field: " + J9UTF8Helper.stringValue(classRef.name())
								+ "." + J9UTF8Helper.stringValue(nameAndSig.name())
								+ " " + J9UTF8Helper.stringValue(nameAndSig.signature()));
					} else if ((shapeDesc == J9CPTYPE_INSTANCE_METHOD)
								|| (shapeDesc == J9CPTYPE_STATIC_METHOD)
								|| (shapeDesc == J9CPTYPE_HANDLE_METHOD)
								|| (shapeDesc == J9CPTYPE_INTERFACE_METHOD)) {
						J9ROMMethodRefPointer romMethodRef = J9ROMMethodRefPointer.cast(item);
						classRef = J9ROMClassRefPointer.cast(constantPool.add(romMethodRef.classRefCPIndex()));
						nameAndSig = romMethodRef.nameAndSignature();
						out.println("      Method: " + J9UTF8Helper.stringValue(classRef.name())
								+ "." + J9UTF8Helper.stringValue(nameAndSig.name())
								+ " " + J9UTF8Helper.stringValue(nameAndSig.signature()));
					} else if (shapeDesc == J9CPTYPE_METHOD_TYPE) {
						J9ROMMethodTypeRefPointer romMethodTypeRef = J9ROMMethodTypeRefPointer.cast(item);
						out.println("      Method Type: " + J9UTF8Helper.stringValue(J9UTF8Pointer.cast(romMethodTypeRef.signature())));
					} else if (shapeDesc == J9CPTYPE_METHODHANDLE) {
						methodHandleRef = J9ROMMethodHandleRefPointer.cast(item);
						methodRef = J9ROMMethodRefPointer.cast(constantPool.add(methodHandleRef.methodOrFieldRefIndex()));
						classRef = J9ROMClassRefPointer.cast(constantPool.add(methodRef.classRefCPIndex()));
						nameAndSig = methodRef.nameAndSignature();
						out.print("      Method Handle: " + J9UTF8Helper.stringValue(classRef.name())
								+ "." + J9UTF8Helper.stringValue(nameAndSig.name()));
						long methodType = methodHandleRef.handleTypeAndCpType().rightShift((int)J9DescriptionCpTypeShift).longValue();

						if ((methodType == MH_REF_GETFIELD)
								|| (methodType == MH_REF_PUTFIELD)
								|| (methodType == MH_REF_GETSTATIC)
								|| (methodType == MH_REF_PUTSTATIC)){
							out.print(" ");
						}
						out.println(J9UTF8Helper.stringValue(nameAndSig.signature()));
					} else {
						out.println("      <unknown type>");
					}
				}
			}
		}
	}

	/**
	 * This method is Java implementation of rdump.c#dumpStaticSplitSideTable function.
	 * This method is called when dumping a ROMClass.
	 *
	 * @param out PrintStream to print the user info to the console
	 * @param romClass ROMClass address in the dump file.
	 *
	 * @throws CorruptDataException
	 */
	private static void dumpStaticSplitSideTable(PrintStream out, J9ROMClassPointer romClass) throws CorruptDataException
	{
		int splitTableCount = romClass.staticSplitMethodRefCount().intValue();
		if (splitTableCount > 0) {
			out.println(String.format("Static Split Table (%d):\n", splitTableCount));
			out.println("    SplitTable Index -> CP Index");
			U16Pointer cursor = romClass.staticSplitMethodRefIndexes();
			for (int i = 0; i < splitTableCount; i++) {
				cursor.add(i);
				out.println(String.format("    %16d -> %d", i, cursor.at(i).intValue()));
			}
		}
	}

	/**
	 * This method is Java implementation of rdump.c#dumpSpecialSplitSideTable function.
	 * This method is called when dumping a ROMClass.
	 *
	 * @param out PrintStream to print the user info to the console
	 * @param romClass ROMClass address in the dump file.
	 *
	 * @throws CorruptDataException
	 */
	private static void dumpSpecialSplitSideTable(PrintStream out, J9ROMClassPointer romClass) throws CorruptDataException
	{
		int splitTableCount = romClass.specialSplitMethodRefCount().intValue();
		if (splitTableCount > 0) {
			out.println(String.format("Special Split Table (%d):\n", splitTableCount));
			out.println("    SplitTable Index -> CP Index");
			U16Pointer cursor = romClass.specialSplitMethodRefIndexes();
			for (int i = 0; i < splitTableCount; i++) {
				cursor.add(i);
				out.println(String.format("    %16d -> %d", i, cursor.at(i).intValue()));
			}
		}
	}

	private static void dumpStackMapTable(PrintStream out, J9ROMClassPointer romclass, J9ROMMethodPointer romMethod, long flags) throws CorruptDataException {
		U32Pointer stackMapMethod = ROMHelp.getStackMapFromROMMethod(romMethod);
		U16 stackMapCount;
		U8Pointer stackMapData;
		long mapPC = -1;
		long mapType;

		if (stackMapMethod.notNull()) {
			stackMapData = U8Pointer.cast(stackMapMethod.add(1));
			stackMapCount = new U16(stackMapData.at(0)).leftShift(8).bitOr(stackMapData.at(1));
			stackMapData = stackMapData.add(2);

			out.println("\n  StackMapTable\n    Stackmaps(" + stackMapCount.intValue() + "):");

			for (int i = 0; i < stackMapCount.intValue(); i++) {
				mapPC++;
				mapType = stackMapData.at(0).longValue();
				stackMapData = stackMapData.add(1);

				if (mapType < 64) {
					mapPC += mapType;
					out.println("      pc: " + mapPC +" same");
				} else if (mapType < 128) {
					mapPC += (mapType - 64);
					out.print("      pc: " + mapPC +" same_locals_1_stack_item: ");
					stackMapData = dumpStackMapSlots(out, romclass, stackMapData, 1);
					out.println();
				} else if (mapType < 247) {
					out.println("      UNKNOWN FRAME TAG: (" + mapType + ")\n");
				} else if (mapType == 247) {
					long offset = new U16(stackMapData.at(0)).leftShift(8).add(stackMapData.at(1)).longValue();
					stackMapData = stackMapData.add(2);
					mapPC += offset;
					out.print("      pc: "+ mapPC +" same_locals_1_stack_item_extended: ");
					stackMapData = dumpStackMapSlots(out, romclass, stackMapData, 1);
					out.println();
				} else if (mapType < 251) {
					long offset = new U16(stackMapData.at(0)).leftShift(8).add(stackMapData.at(1)).longValue();
					stackMapData = stackMapData.add(2);
					mapPC += offset;
					out.println("      pc: "+ mapPC +" chop " + (251 - mapType));

				} else if (mapType == 251) {
					long offset = new U16(stackMapData.at(0)).leftShift(8).add(stackMapData.at(1)).longValue();
					stackMapData = stackMapData.add(2);
					mapPC += offset;
					out.println("      pc: "+ mapPC +" same_extended\n");

				} else if (mapType < 255) {
					long offset = new U16(stackMapData.at(0)).leftShift(8).add(stackMapData.at(1)).longValue();
					stackMapData = stackMapData.add(2);
					mapPC += offset;
					out.print("      pc: "+ mapPC +" append: ");
					stackMapData = dumpStackMapSlots(out, romclass, stackMapData, (mapType - 251));
					out.println();
				} else if (mapType == 255) {
					long offset = new U16(stackMapData.at(0)).leftShift(8).add(stackMapData.at(1)).longValue();
					stackMapData = stackMapData.add(2);
					mapPC += offset;
					out.print("      pc: "+ mapPC +" full, local(s): ");
					offset = new U16(stackMapData.at(0)).leftShift(8).add(stackMapData.at(1)).longValue();
					stackMapData = stackMapData.add(2);
					stackMapData = dumpStackMapSlots(out, romclass, stackMapData, offset);
					out.print(", stack: ");
					offset = new U16(stackMapData.at(0)).leftShift(8).add(stackMapData.at(1)).longValue();
					stackMapData = stackMapData.add(2);
					stackMapData = dumpStackMapSlots(out, romclass, stackMapData, offset);
					out.println();
				}
			}
		}
	}

	private static void dumpMethodParameters(PrintStream out, J9ROMClassPointer romclass, J9ROMMethodPointer romMethod, long flags) throws CorruptDataException
	{
		J9MethodParametersDataPointer methodParameters = ROMHelp.getMethodParametersFromROMMethod(romMethod);
		if (methodParameters != J9MethodParametersDataPointer.NULL) {
			J9MethodParameterPointer parameters = methodParameters.parameters();
			out.println(String.format("  Method Parameters (%d):\n", methodParameters.parameterCount().longValue()));
			for (int i = 0 ; i < methodParameters.parameterCount().longValue(); i++) {
				J9UTF8Pointer name = J9UTF8Pointer.cast(parameters.nameEA().get());
				long parameterFlags = parameters.flags().longValue();
				if (name.isNull()) {
					out.print("    <no name>");
				} else {
					out.print("    " + J9UTF8Helper.stringValue(name));
				}

				out.print(String.format("    0x%x ( ", parameterFlags));
				dumpModifiers(out, parameterFlags, MODIFIERSOURCE_METHODPARAMETER, ONLY_SPEC_MODIFIERS, false);
				out.println(" )\n");
			}
			out.println("\n");
		}

	}

	static U8Pointer dumpStackMapSlots(PrintStream out, J9ROMClassPointer classfile, U8Pointer slotData, long slotCount) throws CorruptDataException
	{
		int slotType;

		String slotTypes [] = {
			"top",
			"int",
			"float",
			"double",
			"long",
			"null",
			"uninitialized_this" };

		String[] primitiveArrayTypes = {
				"I",
				"F",
				"D",
				"J",
				"S",
				"B",
				"C",
				"Z" };

		out.print("(");

		for (int i = 0; i < slotCount; i++) {
			slotType = slotData.at(0).intValue();
			slotData = slotData.add(1);

			if (slotType <= 0x06 /*FR_STACKMAP_TYPE_INIT_OBJECT*/) {
				out.print(slotTypes[slotType]);

			} else if (slotType == 0x07 /* CFR_STACKMAP_TYPE_OBJECT */) {
				long index = new U16(slotData.at(0)).leftShift(8).add(slotData.at(1)).longValue();
				J9ROMConstantPoolItemPointer constantPool = ConstantPoolHelpers.J9_ROM_CP_FROM_ROM_CLASS(classfile);
				J9ROMStringRefPointer item = J9ROMStringRefPointer.cast(constantPool.add(index));
				J9UTF8Pointer data = item.utf8Data();
				String s = J9UTF8Helper.stringValue(data);
				if (s.charAt(0) != '[') {
					out.print("L");
				}
				out.print(s);
				slotData = slotData.add(2);

			} else if (slotType == 0x08 /* CFR_STACKMAP_TYPE_NEW_OBJECT */) {
				long index = new U16(slotData.at(0)).leftShift(8).add(slotData.at(1)).longValue();
				out.print("this pc:" + index);
				slotData = slotData.add(2);

			} else {
				/* J9-ism: primitive array types start at slotType 9 and arity is really NEXT_U16()+1*/
				StringBuffer primitiveType = new StringBuffer("[");
				long index = new U16(slotData.at(0)).leftShift(8).add(slotData.at(1)).longValue();
				while (index-- > 0) {
					primitiveType.append("[");
				}
				primitiveType.append(primitiveArrayTypes[slotType - 9]);
				out.print(primitiveType.toString());
				slotData = slotData.add(2);
			}

			if (i != (slotCount - 1)) {
				out.print(", ");
			}
		}
		out.print(")");

		return slotData;
	}

}
