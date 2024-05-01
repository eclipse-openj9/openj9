/*
 * Copyright IBM Corp. and others 2022
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

import java.io.PrintStream;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.util.HashMap;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9InitializerMethodsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMClassRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMConstantDynamicRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMInterfaceMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMMethodHandleRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMMethodTypeRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMStaticFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMStringRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantDynamicRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodHandleRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodTypeRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMNameAndSignaturePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMSingleSlotConstantRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMStringRefPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9Class;
import com.ibm.j9ddr.vm29.structure.J9ConstantPool;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.structure.J9DescriptionBits;
import com.ibm.j9ddr.vm29.structure.J9VTableHeader;
import com.ibm.j9ddr.vm29.structure.J9VmconstantpoolConstants;
import com.ibm.j9ddr.vm29.types.UDATA;

public class J9ConstantPoolCommand extends Command {
	private static final HashMap<Long, String> cpTypeToString = new HashMap<>();
	private static final int J9_METHOD_INDEX_SHIFT = Long.numberOfTrailingZeros(J9Consts.J9_ITABLE_INDEX_METHOD_INDEX);
	private static final int J9_METHOD_ARG_COUNT_MASK = (1 << J9_METHOD_INDEX_SHIFT) - 1;
	private static final long J9VTABLE_INITIAL_VIRTUAL_OFFSET;

	static {
		long offset;

		try {
			offset = J9Class.SIZEOF + J9VTableHeader._initialVirtualMethodOffset_;
		} catch (NoClassDefFoundError e) {
			/* For core files predating the introduction of J9VTableHeader. */
			offset = 0;
		}
		J9VTABLE_INITIAL_VIRTUAL_OFFSET = offset;

		cpTypeToString.put(J9ConstantPool.J9CPTYPE_CLASS, "Class");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_STRING, "String");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_INT, "int");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_FLOAT, "float");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_LONG, "long");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_DOUBLE, "double");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_FIELD, "Fieldref");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_INSTANCE_METHOD, "Methodref");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_STATIC_METHOD, "Methodref");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_INTERFACE_STATIC_METHOD, "Methodref");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_INTERFACE_INSTANCE_METHOD, "Methodref");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_HANDLE_METHOD, "HandleMethodref");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_INTERFACE_METHOD, "InterfaceMethodref");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_METHOD_TYPE, "MethodType");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_METHODHANDLE, "MethodHandle");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_ANNOTATION_UTF8, "UTF8");
		cpTypeToString.put(J9ConstantPool.J9CPTYPE_CONSTANT_DYNAMIC, "ConstantDynamic");
	}

	public J9ConstantPoolCommand() {
		addCommand("constantpool", "<ramclass>", "dump constant pool for the given RAM class.");
	}

	public static boolean isSupported() {
		return (0 != J9VTABLE_INITIAL_VIRTUAL_OFFSET)
			&& (0 != J9Consts.J9_ITABLE_INDEX_METHOD_INDEX)
			&& (0 != J9Consts.J9_ITABLE_INDEX_OBJECT)
			&& (0 != J9Consts.J9_ITABLE_INDEX_SHIFT)
			&& (0 != J9Consts.J9_ITABLE_INDEX_TAG_BITS)
			&& (0 != J9Consts.J9_ITABLE_INDEX_UNRESOLVED)
			&& (0 != J9DescriptionBits.J9DescriptionCpBsmIndexMask)
			&& (0 != J9DescriptionBits.J9DescriptionCpTypeShift);
	}

	private static J9ClassPointer findClassByName(J9JavaVMPointer vm, String name) throws CorruptDataException {
		ClassSegmentIterator iterator = new ClassSegmentIterator(vm.classMemorySegments());
		while (iterator.hasNext()) {
			J9ClassPointer classRef = (J9ClassPointer) iterator.next();
			String className;
			if (J9ClassHelper.isArrayClass(classRef)) {
				className = J9ClassHelper.getArrayName(classRef);
			} else {
				className = J9ClassHelper.getName(classRef);
			}
			if (className.equals(name)) {
				return classRef;
			}
		}
		return null;
	}

	private static boolean sameNameAndSignature(J9ROMNameAndSignaturePointer a, J9ROMNameAndSignaturePointer b) throws CorruptDataException {
		return J9UTF8Helper.stringValue(a.name()).equals(J9UTF8Helper.stringValue(b.name())) &&
			J9UTF8Helper.stringValue(a.signature()).equals(J9UTF8Helper.stringValue(b.signature()));
	}

	private static J9RAMClassRefPointer J9VMConstantPoolClassRefAt(J9JavaVMPointer vm, long index) throws CorruptDataException {
		J9RAMConstantPoolItemPointer jclConstantPool = vm.jclConstantPoolEA();
		J9RAMClassRefPointer ref = J9RAMClassRefPointer.cast(jclConstantPool.add(index));
		return ref;
	}

	private static PointerPointer getSpecialTable(J9ClassPointer ramClass) throws CorruptDataException {
		try {
			Lookup lookup = MethodHandles.lookup();
			String methodName = J9BuildFlags.J9VM_OPT_OPENJDK_METHODHANDLE ? "invokeCache" : "methodTypes";
			MethodHandle mhTable = lookup.findVirtual(J9ClassPointer.class, methodName, MethodType.methodType(PointerPointer.class));
			return (PointerPointer)mhTable.invoke(ramClass);
		} catch (CorruptDataException e) {
			throw e;
		} catch (Throwable e) {
			throw new CorruptDataException(e);
		}
	}

	@Override
	public void run(String command, String[] args, Context context, final PrintStream out) throws DDRInteractiveCommandException {
		try {
			final J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			final J9InitializerMethodsPointer initializerMethods = vm.initialMethods();

			long address = CommandUtils.parsePointer(args[0], J9BuildFlags.J9VM_ENV_DATA64);
			final J9ClassPointer ramClass = J9ClassPointer.cast(address);
			final J9ROMClassPointer romClass = ramClass.romClass();

			final int romConstantPoolCount = romClass.romConstantPoolCount().intValue();
			final int ramCPCount = romClass.ramConstantPoolCount().intValue();

			final J9ConstantPoolPointer constantPool = J9ConstantPoolPointer.cast(ramClass.ramConstantPool());
			final J9RAMConstantPoolItemPointer ramConstantPool = J9RAMConstantPoolItemPointer.cast(constantPool);
			final J9ROMConstantPoolItemPointer romConstantPool = constantPool.romConstantPool();
			J9RAMConstantPoolItemPointer ramCPP = J9RAMConstantPoolItemPointer.cast(constantPool);
			J9ROMConstantPoolItemPointer romCPP = constantPool.romConstantPool();

			U32Pointer cpShapeDescription = J9ROMClassHelper.cpShapeDescription(romClass);

			CommandUtils.dbgPrint(out, String.format("%-8s%-50s%-50s%-20s%s%n", "Index", "ROM", "RAM", "Type", "Value"));

			for (int i = 1; i < romConstantPoolCount; i++) {
				ramCPP = ramCPP.add(1);
				romCPP = romCPP.add(1);
				long cpType = ConstantPoolHelpers.J9_CP_TYPE(cpShapeDescription, i);
				String ramInfo = "";
				String romInfo = "";
				String value;
				if (cpType == J9ConstantPool.J9CPTYPE_CLASS) {
					J9ClassPointer clazz = J9RAMClassRefPointer.cast(ramCPP).value();
					String className = J9UTF8Helper.stringValue(J9ROMClassRefPointer.cast(romCPP).name());
					if (clazz.notNull() && (i < ramCPCount)) {
						if (J9ClassHelper.isArrayClass(clazz)) {
							ramInfo = "!j9arrayclass " + clazz.getHexAddress();
							romInfo = "!j9romarrayclass " + clazz.romClass().getHexAddress();
						} else {
							ramInfo = "!j9class " + clazz.getHexAddress();
							romInfo = "!j9romclass " + clazz.romClass().getHexAddress();
						}
					} else {
						if ((i < ramCPCount) && clazz.isNull()) {
							ramInfo = "(unresolved)";
						}
						J9ClassPointer foundClass = findClassByName(vm, className);
						if (foundClass == null) {
							romInfo = "(unresolved)";
						} else if (J9ClassHelper.isArrayClass(foundClass)) {
							romInfo = "!j9romarrayclass " + foundClass.romClass().getHexAddress();
						} else {
							romInfo = "!j9romclass " + foundClass.romClass().getHexAddress();
						}
					}
					value = className;
				} else if (cpType == J9ConstantPool.J9CPTYPE_STRING) {
					romInfo = "!j9utf8 " + J9ROMStringRefPointer.cast(romCPP).utf8Data().getHexAddress();
					if (i < ramCPCount) {
						J9ObjectPointer stringObject = J9RAMStringRefPointer.cast(ramCPP).stringObject();
						if (stringObject.notNull()) {
							ramInfo = "!j9object " + stringObject.getHexAddress();
						} else {
							ramInfo = "(unresolved)";
						}
					}
					value = J9UTF8Helper.stringValue(J9ROMStringRefPointer.cast(romCPP).utf8Data());
				} else if (cpType == J9ConstantPool.J9CPTYPE_INT) {
					J9ROMSingleSlotConstantRefPointer constantRef = J9ROMSingleSlotConstantRefPointer.cast(romCPP);
					romInfo = "!J9ROMSingleSlotConstantRef " + constantRef.dataEA().getHexAddress();
					if (i < ramCPCount) {
						ramInfo = "!J9RAMSingleSlotConstantRef " + ramCPP.getHexAddress();
					}
					value = Long.toString(constantRef.data().longValue());
				} else if (cpType == J9ConstantPool.J9CPTYPE_FLOAT) {
					J9ROMSingleSlotConstantRefPointer constantRef = J9ROMSingleSlotConstantRefPointer.cast(romCPP);
					romInfo = "!J9ROMSingleSlotConstantRef " + constantRef.getHexAddress();
					if (i < ramCPCount) {
						ramInfo = "!J9RAMSingleSlotConstantRef " + ramCPP.getHexAddress();
					}
					value = Float.toString(Float.intBitsToFloat((int)constantRef.data().longValue()));
				} else if (cpType == J9ConstantPool.J9CPTYPE_LONG) {
					// long and double don't have an entry in RAM CP
					romInfo = "!j9romconstantref " + romCPP.getHexAddress();
					value = Long.toString(I64Pointer.cast(romCPP).at(0).longValue());
				} else if (cpType == J9ConstantPool.J9CPTYPE_DOUBLE) {
					romInfo = "!j9romconstantref " + romCPP.getHexAddress();
					value = Double.toString(Double.longBitsToDouble(I64Pointer.cast(romCPP).at(0).longValue()));
				} else if (cpType == J9ConstantPool.J9CPTYPE_FIELD) {
					if (i < ramCPCount) {
						J9RAMStaticFieldRefPointer staticRef = J9RAMStaticFieldRefPointer.cast(ramCPP);
						J9RAMFieldRefPointer ref = J9RAMFieldRefPointer.cast(ramCPP);
						if ((staticRef.flagsAndClass().longValue() > 0) && !staticRef.valueOffset().eq(UDATA.MAX)) {
							ramInfo = staticRef.valueOffset().getHexValue() + " (static offset)"; // resolved static
						} else if (ref.flags().gt(ref.valueOffset())) {
							ramInfo = ref.valueOffset().getHexValue() + " (instance offset)"; // resolved instance
						} else {
							ramInfo = "(unresolved)"; // unresolved
						}
					}

					J9ROMFieldRefPointer romFieldRef = J9ROMFieldRefPointer.cast(romCPP);
					romInfo = "#" + Long.toString(romFieldRef.classRefCPIndex().longValue());

					J9ROMClassRefPointer classRef = J9ROMClassRefPointer.cast(romConstantPool.add(romFieldRef.classRefCPIndex()));
					J9ROMNameAndSignaturePointer nameAndSig = romFieldRef.nameAndSignature();
					value = String.format("%s.%s:%s #%d",
							J9UTF8Helper.stringValue(classRef.name()),
							J9UTF8Helper.stringValue(nameAndSig.name()),
							J9UTF8Helper.stringValue(nameAndSig.signature()),
							romFieldRef.classRefCPIndex().intValue());
				} else if ((cpType == J9ConstantPool.J9CPTYPE_STATIC_METHOD)
				|| (cpType == J9ConstantPool.J9CPTYPE_INTERFACE_STATIC_METHOD)
				|| (cpType == J9ConstantPool.J9CPTYPE_INTERFACE_INSTANCE_METHOD)
				|| (cpType == J9ConstantPool.J9CPTYPE_INSTANCE_METHOD)
				) {
					J9ROMMethodRefPointer romMethodRef = J9ROMMethodRefPointer.cast(romCPP);
					J9ROMClassRefPointer romClassRef = J9ROMClassRefPointer.cast(romConstantPool.add(romMethodRef.classRefCPIndex()));
					J9ROMNameAndSignaturePointer nameAndSignature = romMethodRef.nameAndSignature();
					String className = J9UTF8Helper.stringValue(romClassRef.name());
					boolean inRamAndResolved = false;

					if (i < ramCPCount) {
						J9RAMMethodRefPointer ramMethodRef = J9RAMMethodRefPointer.cast(ramCPP);
						J9MethodPointer ramMethod = J9RAMMethodRefPointer.cast(ramCPP).method();
						long methodIndex = ramMethodRef.methodIndexAndArgCount().rightShift(J9_METHOD_INDEX_SHIFT).longValue();

						boolean isInitialMethod = true;
						if ((cpType == J9ConstantPool.J9CPTYPE_STATIC_METHOD)
						|| (cpType == J9ConstantPool.J9CPTYPE_INTERFACE_STATIC_METHOD)
						) {
							if ((methodIndex != J9VTABLE_INITIAL_VIRTUAL_OFFSET)
							|| (ramMethod.getAddress() != initializerMethods.initialStaticMethod().getAddress())
							) {
								isInitialMethod = false;
							}
						} else {
							if (ramMethod.getAddress() != initializerMethods.initialSpecialMethod().getAddress()) {
								isInitialMethod = false;
							} else {
								if (methodIndex != J9VTABLE_INITIAL_VIRTUAL_OFFSET) { // virtual method
									J9ClassPointer receiverClass = J9RAMClassRefPointer.cast(ramConstantPool.add(romMethodRef.classRefCPIndex())).value();
									if (receiverClass.notNull()) {
										long methodAddress = PointerPointer.cast(receiverClass).addOffset(methodIndex).at(0).getAddress();
										J9MethodPointer sendMethod = J9MethodPointer.cast(methodAddress);
										ramInfo = "!j9method " + sendMethod.getHexAddress();
										romInfo = "!j9rommethod " + ROMHelp.J9_ROM_METHOD_FROM_RAM_METHOD(sendMethod).getHexAddress();
										inRamAndResolved = true;
									}
								}
							}
						}
						if (!isInitialMethod) {
							ramInfo = "!j9method " + ramMethod.getHexAddress();
							romInfo = "!j9rommethod " + ROMHelp.J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod).getHexAddress();
							inRamAndResolved = true;
						}
					}

					if (!inRamAndResolved) {
						if (i < ramCPCount) {
							ramInfo = "(unresolved)";
						}

						J9ClassPointer foundClass = findClassByName(vm, className);
						if (foundClass != null) {
							J9ROMClassPointer foundRomClass = foundClass.romClass();
							J9ROMMethodPointer romMethod = foundRomClass.romMethods();
							int romMethodCount = foundRomClass.romMethodCount().intValue();
							while (romMethodCount > 0) {
								if (sameNameAndSignature(romMethod.nameAndSignature(), nameAndSignature)) {
									romInfo = "!j9rommethod " + romMethod.getHexAddress();
									break;
								}
								romMethod = ROMHelp.nextROMMethod(romMethod);
								romMethodCount -= 1;
							}
						}
					}

					value = String.format("%s.%s:%s #%d",
							className,
							J9UTF8Helper.stringValue(nameAndSignature.name()),
							J9UTF8Helper.stringValue(nameAndSignature.signature()),
							romMethodRef.classRefCPIndex().intValue());
				} else if (cpType == J9ConstantPool.J9CPTYPE_HANDLE_METHOD) {
					J9ROMMethodRefPointer romMethodRef = J9ROMMethodRefPointer.cast(romCPP);
					if (i < ramCPCount) {
						J9RAMMethodRefPointer ramMethodRef = J9RAMMethodRefPointer.cast(ramCPP);
						long methodIndex = ramMethodRef.methodIndexAndArgCount().rightShift(J9_METHOD_INDEX_SHIFT).longValue();

						PointerPointer mhObject = getSpecialTable(ramClass).add(methodIndex);

						if (mhObject.isNull()) {
							ramInfo = "(unresolved)";
						} else {
							ramInfo = "!j9object " + mhObject.getHexValue();
						}
					}
					J9ROMNameAndSignaturePointer nameAndSignature = romMethodRef.nameAndSignature();
					romInfo = "!j9romnameandsignature " + nameAndSignature.getHexAddress();
					value = J9UTF8Helper.stringValue(nameAndSignature.name()) + ":" + J9UTF8Helper.stringValue(nameAndSignature.signature());
				} else if (cpType == J9ConstantPool.J9CPTYPE_INTERFACE_METHOD) {
					J9ROMMethodRefPointer romMethodRef = J9ROMMethodRefPointer.cast(romCPP);
					J9ROMClassRefPointer romClassRef = J9ROMClassRefPointer.cast(romConstantPool.add(romMethodRef.classRefCPIndex()));
					J9ROMNameAndSignaturePointer nameAndSignature = romMethodRef.nameAndSignature();
					String className = J9UTF8Helper.stringValue(romClassRef.name());

					if (i < ramCPCount) {
						J9RAMInterfaceMethodRefPointer ramMethodRef = J9RAMInterfaceMethodRefPointer.cast(ramCPP);
						J9ClassPointer interfaceClass = J9ClassPointer.cast(ramMethodRef.interfaceClass());
						UDATA methodIndexAndArgCount = ramMethodRef.methodIndexAndArgCount();
						if (interfaceClass.notNull()
						&& (J9Consts.J9_ITABLE_INDEX_UNRESOLVED != methodIndexAndArgCount.bitAnd(J9_METHOD_ARG_COUNT_MASK).longValue())
						) {
							UDATA methodIndex = methodIndexAndArgCount.rightShift((int)J9Consts.J9_ITABLE_INDEX_SHIFT);
							if (methodIndexAndArgCount.anyBitsIn(J9Consts.J9_ITABLE_INDEX_TAG_BITS)) { // object or private interface method
								if (methodIndexAndArgCount.anyBitsIn(J9Consts.J9_ITABLE_INDEX_METHOD_INDEX)) {
									J9ClassPointer receiverClass;
									if (methodIndexAndArgCount.anyBitsIn(J9Consts.J9_ITABLE_INDEX_OBJECT)) { // object method not in the vTable
										receiverClass = J9VMConstantPoolClassRefAt(vm, J9VmconstantpoolConstants.J9VMCONSTANTPOOL_JAVALANGOBJECT).value();
									} else { // private interface method
										receiverClass = interfaceClass;
									}
									J9MethodPointer sendMethod = receiverClass.ramMethods().add(methodIndex);
									ramInfo = "!j9method " + sendMethod.getHexAddress();
									romInfo = "!j9rommethod " + ROMHelp.J9_ROM_METHOD_FROM_RAM_METHOD(sendMethod).getHexAddress();
								} else {
									ramInfo = "VTableOffset: " + methodIndex.longValue();
								}
							} else { // standard interface method
								ramInfo = "ITable Index: " + methodIndex.longValue();
							}
						} else {
							ramInfo = "(unresolved)";
						}
					}

					value = String.format("%s.%s:%s #%d",
							className,
							J9UTF8Helper.stringValue(nameAndSignature.name()),
							J9UTF8Helper.stringValue(nameAndSignature.signature()),
							romMethodRef.classRefCPIndex().intValue());
				} else if (cpType == J9ConstantPool.J9CPTYPE_METHOD_TYPE) {
					if (i < ramCPCount) {
						ramInfo = "!j9object " + J9RAMMethodTypeRefPointer.cast(ramCPP).type().getHexAddress();
					}
					J9ROMMethodTypeRefPointer methodTypeRef = J9ROMMethodTypeRefPointer.cast(romCPP);
					romInfo = "!j9utf8 " + methodTypeRef.signature().getHexAddress();
					value = J9UTF8Helper.stringValue(methodTypeRef.signature());
				} else if (cpType == J9ConstantPool.J9CPTYPE_METHODHANDLE) {
					if (i < ramCPCount) {
						J9ObjectPointer methodHandle = J9RAMMethodHandleRefPointer.cast(ramCPP).methodHandle();
						if (methodHandle.notNull()) {
							ramInfo = "!j9object " + methodHandle.getHexAddress();
						} else {
							ramInfo = "(unresolved)";
						}
					}
					J9ROMMethodHandleRefPointer romMHRef = J9ROMMethodHandleRefPointer.cast(romCPP);
					long refIndex = romMHRef.methodOrFieldRefIndex().longValue();
					J9ROMMethodRefPointer romMethodRef = J9ROMMethodRefPointer.cast(romConstantPool.add(refIndex));
					J9ROMNameAndSignaturePointer nameAndSignature = romMethodRef.nameAndSignature();
					romInfo = romMethodRef.getHexValue();
					J9ROMClassRefPointer romClassRef = J9ROMClassRefPointer.cast(romConstantPool.add(romMethodRef.classRefCPIndex()));
					value = String.format("%s.%s:%s #%s",
							J9UTF8Helper.stringValue(romClassRef.name()),
							J9UTF8Helper.stringValue(nameAndSignature.name()),
							J9UTF8Helper.stringValue(nameAndSignature.signature()),
							refIndex);
				} else if (cpType == J9ConstantPool.J9CPTYPE_ANNOTATION_UTF8) {
					romInfo = "!j9utf8 " + J9ROMStringRefPointer.cast(romCPP).utf8Data().getHexAddress();
					if (i < ramCPCount) {
						J9ObjectPointer stringObject = J9RAMStringRefPointer.cast(ramCPP).stringObject();
						if (stringObject.notNull()) {
							ramInfo = "!j9object " + stringObject.getHexAddress();
						} else {
							ramInfo = "(unresolved)";
						}
					}
					value = J9UTF8Helper.stringValue(J9ROMStringRefPointer.cast(romCPP).utf8Data());
				} else if (cpType == J9ConstantPool.J9CPTYPE_CONSTANT_DYNAMIC) {
					try {
						if (i < ramCPCount) {
							J9RAMConstantDynamicRefPointer ref = J9RAMConstantDynamicRefPointer.cast(ramCPP);
							if (ref.value().notNull()) { // resolved
								ramInfo = "!j9object " + ref.value().getHexAddress() + " (resolved)";
							} else {
								J9ObjectPointer exception = ref.exception();
								if (exception.notNull()) {
									J9ClassPointer clazz = J9ObjectHelper.clazz(exception);
									J9ClassPointer throwable = J9VMConstantPoolClassRefAt(vm, J9VmconstantpoolConstants.J9VMCONSTANTPOOL_JAVALANGTHROWABLE).value();
									if (exception.getAddress() == vm.voidReflectClass().classObject().getAddress()) {
										ramInfo = "NULL (resolved)";
									} else if (J9ClassHelper.isSameOrSuperClassOf(throwable, clazz)) { // resolved exception
										ramInfo = "!j9object " + exception.getHexAddress() + " (exception)";
									} else { // resolving by a thread object
										ramInfo = "!j9object " + exception.getHexAddress() + " (resolving Thread)";
									}
								} else {
									ramInfo = "(unresolved)";
								}
							}
						}

						J9ROMConstantDynamicRefPointer romRef = J9ROMConstantDynamicRefPointer.cast(romCPP);
						SelfRelativePointer callSiteData = SelfRelativePointer.cast(romClass.callSiteData());
						long callSiteCount = romClass.callSiteCount().longValue();
						U16Pointer bsmIndicies = U16Pointer.cast(callSiteData.addOffset(4 * callSiteCount));
						U16Pointer bsmData = bsmIndicies.add(callSiteCount);
						long bsmIndex = romRef.bsmIndexAndCpType().rightShift((int)J9DescriptionBits.J9DescriptionCpTypeShift).bitAnd(J9DescriptionBits.J9DescriptionCpBsmIndexMask).longValue();

						long argCount;
						for (long j = 0; j < bsmIndex; j++) {
							argCount = bsmData.at(1).longValue();
							bsmData = bsmData.add(argCount + 2);
						}
						argCount = bsmData.at(1).longValue();
						long bsmMethodRefIndex = bsmData.at(0).longValue();
						romInfo = "#" + bsmMethodRefIndex;

						J9ROMNameAndSignaturePointer nameAndSig = romRef.nameAndSignature();
						value = String.format("%s:%s #%d(",
								J9UTF8Helper.stringValue(nameAndSig.name()),
								J9UTF8Helper.stringValue(nameAndSig.signature()),
								bsmMethodRefIndex);
						for (long j = 1; j <= argCount; j++) {
							value += "#" + bsmData.add(1 + j).at(0).longValue();
							if (j < argCount) {
								value += ", ";
							}
						}
						value += ")";
					} catch (NoClassDefFoundError | NoSuchFieldException e) {
						// The type J9RAMConstantDynamicRefPointer and its fields may be unknown to the core file being examined.
						throw new CorruptDataException(e);
					}
				} else {
					throw new CorruptDataException("Incorrect or unsupported constant pool entry type");
				}

				CommandUtils.dbgPrint(out, String.format("%-8d%-50s%-50s%-20s%s%n",
						i, romInfo, ramInfo,
						cpTypeToString.getOrDefault(cpType, "Incorrect or unsupported type"),
						value));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
