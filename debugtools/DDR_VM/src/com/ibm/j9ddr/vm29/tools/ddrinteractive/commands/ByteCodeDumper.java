/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokedynamic;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBaload;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBaloadw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBanewarray;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBastore;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBastorew;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBbipush;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBcheckcast;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdload;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdloadw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdstore;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdstorew;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBfload;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBfloadw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBfstore;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBfstorew;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBwithfield;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBgetfield;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBgetstatic;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBgoto;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBgotow;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBifacmpeq;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBifacmpne;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBifeq;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBifge;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBifgt;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBificmpeq;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBificmpge;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBificmpgt;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBificmple;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBificmplt;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBificmpne;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBifle;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBiflt;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBifne;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBifnonnull;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBifnull;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBiinc;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBiincw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBiload;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBiloadw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinstanceof;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokehandle;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokehandlegeneric;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokeinterface;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokeinterface2;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokespecial;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokespecialsplit;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokestatic;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokestaticsplit;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokevirtual;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBistore;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBistorew;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBldc;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBldc2dw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBldc2lw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBldcw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBlload;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBlloadw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBlookupswitch;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBlstore;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBlstorew;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBmultianewarray;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdefaultvalue;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBnew;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBnewdup;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBnewarray;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBputfield;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBputstatic;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBsipush;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBtableswitch;
import static com.ibm.j9ddr.vm29.structure.J9BCTranslationData.BCT_BigEndianOutput;
import static com.ibm.j9ddr.vm29.structure.J9BCTranslationData.BCT_DumpMaps;
import static com.ibm.j9ddr.vm29.structure.J9BCTranslationData.BCT_J9DescriptionCpTypeClass;
import static com.ibm.j9ddr.vm29.structure.J9BCTranslationData.BCT_J9DescriptionCpTypeScalar;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.j9.BCNames;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.j9.stackmap.DebugLocalMap;
import com.ibm.j9ddr.vm29.j9.stackmap.LocalMap;
import com.ibm.j9ddr.vm29.j9.stackmap.StackMap;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMNameAndSignaturePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMSingleSlotConstantRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMStringRefPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9ROMMethod;
import com.ibm.j9ddr.vm29.types.I16;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.I8;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;

public class ByteCodeDumper {
	private static final String nl = System.getProperty("line.separator");
	public static int BCT_ERR_NO_ERROR = 0;
	private static final int OPCODE_RELATIVE_BRANCHES = 1;

	private static final int LOCAL_MAP = 0;
	private static final int DEBUG_MAP = 1;
	private static final int STACK_MAP = 2;
	private static final int MAP_COUNT = 3;
	private static boolean bigEndian;
	private static U8Pointer bcIndex;

	public static IDATA dumpBytecodes(PrintStream out, J9ROMClassPointer romClass, J9ROMMethodPointer romMethod, U32 flags) throws Exception {
		U16 temp;
		UDATA length;

		out.append(String.format("  Argument Count: %d", romMethod.argCount().intValue()));
		out.append(nl);

		temp = romMethod.tempCount();
		out.append(String.format("  Temp Count: %d", temp.intValue()));
		out.append(nl);
		out.append(nl);

		length = ROMHelp.J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
		if (length.eq(0)) {
			return new IDATA(BCT_ERR_NO_ERROR); /* catch abstract methods */
		}
		return j9bcutil_dumpBytecodes(out, romClass, ROMHelp.J9_BYTECODE_START_FROM_ROM_METHOD(romMethod), new UDATA(0), length.sub(1), flags, "");
	}

	private static IDATA j9bcutil_dumpBytecodes(PrintStream out, J9ROMClassPointer romClass, U8Pointer bytecodes, UDATA walkStartPC, UDATA walkEndPC, U32 flags, String indent) throws Exception {
		J9ROMConstantPoolItemPointer constantPool = J9ROMClassHelper.constantPool(romClass);
		J9ROMConstantPoolItemPointer info;
		J9ROMNameAndSignaturePointer nameAndSig;
		bigEndian = flags.anyBitsIn(BCT_BigEndianOutput);

		UDATA index = new UDATA(0);
		UDATA target = new UDATA(0);
		UDATA start;
		UDATA pc;
		UDATA bc;
		I32 low, high;
		U32 npairs;
		J9ROMMethodPointer romMethod = (J9ROMMethodPointer.cast(bytecodes.sub(J9ROMMethod.SIZEOF)));
		U32 localsCount = new U32(ROMHelp.J9_ARG_COUNT_FROM_ROM_METHOD(romMethod).add(ROMHelp.J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod)));
		int resultArray[] = new int[8192];
		String environment = "0";
		boolean envVarDefined = false;
		int result;

		if (System.getenv().containsKey("j9bcutil_dumpBytecodes")) {
			envVarDefined = true;
			environment = System.getenv().get("j9bcutil_dumpBytecodes");
		}
		pc = new UDATA(walkStartPC);
		bcIndex = bytecodes.add(pc); // cell address
		while (pc.lte(walkEndPC)) {
			if (flags.anyBitsIn(BCT_DumpMaps)) {
				for (int j = LOCAL_MAP; j < MAP_COUNT; j++) {
					if (envVarDefined && (!pc.eq(Integer.parseInt(environment)))) {
						continue;
					}

					boolean wrapLine = false;
					UDATA outputCount = new UDATA(0);
					U8 mapChar = new U8('0');

					switch (j) {
					case LOCAL_MAP:
						result = LocalMap.j9localmap_LocalBitsForPC(romMethod, pc, resultArray);
						mapChar = new U8('l');
						outputCount = new UDATA(localsCount);
						break;
					case DEBUG_MAP:
						result = DebugLocalMap.j9localmap_DebugLocalBitsForPC(romMethod, pc, resultArray);
						mapChar = new U8('d');
						outputCount = new UDATA(localsCount);
						break;
					case STACK_MAP:
						/* First call is to get the stack depth */
						result = StackMap.j9stackmap_StackBitsForPC(pc, romClass, romMethod, null, 0);
						mapChar = new U8('s');
						outputCount = new UDATA(result);
						break;
					}

					if (outputCount.eq(0)) {
						out.append(String.format("               %cmap [empty]", (char) mapChar.intValue()));
					} else {
						out.append(String.format("               %cmap [%5d] =", (char) mapChar.intValue(), outputCount.intValue()));
					}

					for (int i = 0; outputCount.gt(i); i++) {
						int x = i / 32;
						if ((i % 8) == 0) {
							out.append(" ");
							if (wrapLine) {
								out.append(nl);
							}
						}
						out.append(String.format("%d ", resultArray[x] & 1));
						resultArray[x] >>= 1;
						wrapLine = (((i + 1) % 32) == 0);
					}
					out.append(nl);
				}
			}
			// _GETNEXT_U8(bc, bcIndex);
			// #define _GETNEXT_U8(value, index) (bc = *(bcIndex++))
			bc = new UDATA(_GETNEXT_U8()); // error
			out.append(String.format("%s%5d %s ", indent, pc.intValue(), BCNames.getName(bc.intValue())));
			start = new UDATA(pc);
			pc = pc.add(1);

			int bcIntVal = bc.intValue();
			if (bcIntVal == JBbipush) {
				index = new UDATA(_GETNEXT_U8());
				out.append(String.format("%d\n", new I8(index).intValue()));
				pc = pc.add(1);
			} else if (bcIntVal == JBsipush) {
				index = new UDATA(_GETNEXT_U16());
				out.append(String.format("%d\n", new I16(index).intValue()));
				pc = pc.add(2);
			} else if ((bcIntVal == JBldc) || (bcIntVal == JBldcw)) {
				if (bc.eq(JBldc)) {
					index = new UDATA(_GETNEXT_U8());
					pc = pc.add(1);
				} else {
					index = new UDATA(_GETNEXT_U16());
					pc = pc.add(2);
				}
				out.append(String.format("%d ", index.intValue()));
				info = constantPool.add(index);

				J9ROMSingleSlotConstantRefPointer romSingleSlotConstantRef = J9ROMSingleSlotConstantRefPointer.cast(info);
				if (!romSingleSlotConstantRef.cpType().eq(BCT_J9DescriptionCpTypeScalar)) {
					/* this is a string or class constant */
					if (romSingleSlotConstantRef.cpType().eq(BCT_J9DescriptionCpTypeClass)) {
						/* ClassRef */
						out.append("(java.lang.Class) ");
					} else {
						/* StringRef */
						out.append("(java.lang.String) ");
					}
					out.append(J9UTF8Helper.stringValue(J9ROMStringRefPointer.cast(info).utf8Data()));
					out.append(nl);
				} else {
					/* this is a float/int constant */
					out.append(String.format("(int/float) 0x%08X", romSingleSlotConstantRef.data().longValue()));
					out.append(nl);
				}
			} else if (bcIntVal == JBldc2lw) {
				index = new UDATA(_GETNEXT_U16());
				out.append(String.format("%d ", index.intValue()));
				info = constantPool.add(index);
				out.append(String.format("(long) 0x%08X%08X\n", bigEndian ? info.slot1().longValue() : info.slot2().longValue(), bigEndian ? info.slot2().longValue() : info.slot1().longValue()));
				pc = pc.add(2);
			} else if (bcIntVal == JBldc2dw) {
				index = new UDATA(_GETNEXT_U16());
				out.append(String.format("%d ", index.intValue()));
				info = constantPool.add(index);
				/* this will print incorrectly on Linux ARM! FIX ME! */
				out.append(String.format("(double) 0x%08X%08X\n", bigEndian ? info.slot1().longValue() : info.slot2().longValue(), bigEndian ? info.slot2().longValue() : info.slot1().longValue()));
				pc = pc.add(2);
			} else if ((bcIntVal == JBiload)
				|| (bcIntVal == JBlload)
				|| (bcIntVal == JBfload)
				|| (bcIntVal == JBdload)
				|| (bcIntVal == JBaload)
				|| (bcIntVal == JBistore)
				|| (bcIntVal == JBlstore)
				|| (bcIntVal == JBfstore)
				|| (bcIntVal == JBdstore)
				|| (bcIntVal == JBastore)
			) {
				index = new UDATA(_GETNEXT_U8());
				pc = pc.add(1);
				out.append(String.format("%d\n", index.intValue()));
			} else if ((bcIntVal == JBiloadw)
				|| (bcIntVal == JBlloadw)
				|| (bcIntVal == JBfloadw)
				|| (bcIntVal == JBdloadw)
				|| (bcIntVal == JBaloadw)
				|| (bcIntVal == JBistorew)
				|| (bcIntVal == JBlstorew)
				|| (bcIntVal == JBfstorew)
				|| (bcIntVal == JBdstorew)
				|| (bcIntVal == JBastorew)
			) {
				index = new UDATA(_GETNEXT_U16());
				incIndex();
				pc = pc.add(3);
				out.append(String.format("%d\n", index.intValue()));
			} else if (bcIntVal == JBiinc) {
				index = new UDATA(_GETNEXT_U8());
				out.append(String.format("%d ", index.intValue()));
				target = new UDATA(_GETNEXT_U8());
				out.append(String.format("%d\n", new I8(target).intValue()));
				pc = pc.add(2);
			} else if (bcIntVal == JBiincw) {
				index = new UDATA(_GETNEXT_U16());
				out.append(String.format("%d ", index.intValue()));
				index = new UDATA(_GETNEXT_U16());
				out.append(String.format("%d\n", new I16(index).intValue()));
				incIndex();
				pc = pc.add(5);
			} else if ((bcIntVal == JBifeq)
				|| (bcIntVal == JBifne)
				|| (bcIntVal == JBiflt)
				|| (bcIntVal == JBifge)
				|| (bcIntVal == JBifgt)
				|| (bcIntVal == JBifle)
				|| (bcIntVal == JBificmpeq)
				|| (bcIntVal == JBificmpne)
				|| (bcIntVal == JBificmplt)
				|| (bcIntVal == JBificmpge)
				|| (bcIntVal == JBificmpgt)
				|| (bcIntVal == JBificmple)
				|| (bcIntVal == JBifacmpeq)
				|| (bcIntVal == JBifacmpne)
				|| (bcIntVal == JBgoto)
				|| (bcIntVal == JBifnull)
				|| (bcIntVal == JBifnonnull)
			) {
				index = new UDATA(_GETNEXT_U16());
				pc = pc.add(2);
				if (OPCODE_RELATIVE_BRANCHES != 0) {
					target = start.add(new I16(index));
				} else {
					target = pc.add(new I16(index));
				}
				out.append(String.format("%d\n", target.intValue()));
			} else if (bcIntVal == JBtableswitch) {
				switch (start.intValue() % 4) {
				case 0:
					incIndex();
					pc = pc.add(1);  // fall through
				case 1:
					incIndex();
					pc = pc.add(1);  // fall through
				case 2:
					incIndex();
					pc = pc.add(1);  // fall through
				case 3:
					break;
				}
				index = new UDATA(_GETNEXT_U32());
				target = start.add(index);
				index = new UDATA(_GETNEXT_U32());
				low = new I32(index);
				index = new UDATA(_GETNEXT_U32());
				high = new I32(index);
				pc = pc.add(12);
				out.append(String.format("low %d high %d\n", low.intValue(), high.intValue()));
				out.append(String.format("        default %10d\n", target.intValue()));
				npairs = new U32(high.sub(low).add(1));
				for (int i = 0; npairs.gt(i); i++) {
					index = new UDATA(_GETNEXT_U32());
					target = start.add(index);
					out.append(String.format("     %10d %10d\n", low.add(i).intValue(), target.intValue()));
					pc = pc.add(4);
				}
			} else if (bcIntVal == JBlookupswitch) {
				switch (start.intValue() % 4) {
				case 0:
					incIndex();
					pc = pc.add(1);
					break;
				case 1:
					incIndex();
					pc = pc.add(1);
					break;
				case 2:
					incIndex();
					pc = pc.add(1);
					break;
				case 3:
					break;
				}
				index = new UDATA(_GETNEXT_U32());
				target = start.add(index);
				npairs = new U32(_GETNEXT_U32());
				out.append(String.format("pairs %d\n", npairs.intValue()));
				out.append(String.format("        default %10d\n", target.intValue()));
				pc = pc.add(8);
				for (int i = 0; npairs.gt(i); i++) {
					index = new UDATA(_GETNEXT_U32());
					out.append(String.format("     %10d", index.intValue()));
					index = new UDATA(_GETNEXT_U32());
					target = start.add(index);
					out.append(String.format(" %10d\n", target.intValue()));
					pc = pc.add(8);
				}
			} else if ((bcIntVal == JBgetstatic)
				|| (bcIntVal == JBputstatic)
				|| (bcIntVal == JBgetfield)
				|| (bcIntVal == JBwithfield)
				|| (bcIntVal == JBputfield)
			) {
				index = new UDATA(_GETNEXT_U16());
				info = constantPool.add(index);
				out.append(String.format("%d ", index.intValue()));
				// dump declaringClassName
				J9ROMFieldRefPointer romFieldRef = J9ROMFieldRefPointer.cast(info);
				out.append(J9UTF8Helper.stringValue(J9ROMClassRefPointer.cast(constantPool.add(romFieldRef.classRefCPIndex())).name()));

				nameAndSig = romFieldRef.nameAndSignature();
				out.append(".");
				/* dump name */
				out.append(J9UTF8Helper.stringValue(nameAndSig.name()));
				out.append(" ");
				/* dump signature */
				out.append(J9UTF8Helper.stringValue(nameAndSig.signature()));
				out.append(nl);

				pc = pc.add(2);
			} else if (bcIntVal == JBinvokedynamic) {
				index = new UDATA(_GETNEXT_U16());
				out.append(String.format("%d ", index.intValue()));
				
				long callSiteCount = romClass.callSiteCount().longValue();
				SelfRelativePointer callSiteData = SelfRelativePointer.cast(romClass.callSiteData());
				U16Pointer bsmIndices = U16Pointer.cast(callSiteData.addOffset(4*callSiteCount));
				
				nameAndSig = J9ROMNameAndSignaturePointer.cast(callSiteData.add(index).get());
				
				out.append("bsm #" + String.valueOf(bsmIndices.at(index).longValue()));	/* Bootstrap method index */
				out.append(":");
				out.append(J9UTF8Helper.stringValue(nameAndSig.name())); /* dump name */
				out.append(J9UTF8Helper.stringValue(nameAndSig.signature())); /* dump signature */
				out.append(nl);

				pc = pc.add(4);
			} else if (bcIntVal == JBinvokeinterface2) {
				incIndex();
				pc = pc.add(1);
				out.append(nl);
			} else if ((bcIntVal == JBinvokehandle)
				|| (bcIntVal == JBinvokehandlegeneric)
				|| (bcIntVal == JBinvokevirtual)
				|| (bcIntVal == JBinvokespecial)
				|| (bcIntVal == JBinvokestatic)
				|| (bcIntVal == JBinvokeinterface)
				|| (bcIntVal == JBinvokespecialsplit)
				|| (bcIntVal == JBinvokestaticsplit)
			) {
				if (bcIndex.longValue() == 0) {
					bcIndex = bcIndex.sub(1);
				}
				index = new UDATA(_GETNEXT_U16());
				if (bc.intValue() == JBinvokestaticsplit) {
					index = new UDATA(romClass.staticSplitMethodRefIndexes().at(index));
				} else if (bc.intValue() == JBinvokespecialsplit) {
					index = new UDATA(romClass.specialSplitMethodRefIndexes().at(index));
				}
				info = constantPool.add(index);

				out.append(String.format("%d ", index.intValue()));

				/* dump declaringClassName and signature */
				J9ROMMethodRefPointer romMethodRef = J9ROMMethodRefPointer.cast(info);
				UDATA classRefCPIndex = romMethodRef.classRefCPIndex();
				J9ROMConstantPoolItemPointer cpItem = constantPool.add(classRefCPIndex);
				J9ROMClassRefPointer romClassRef = J9ROMClassRefPointer.cast(cpItem);

				String name = J9UTF8Helper.stringValue(romClassRef.name());
				out.append(name);

				nameAndSig = romMethodRef.nameAndSignature();
				out.append(".");
				out.append(J9UTF8Helper.stringValue(nameAndSig.name())); /* dump name */
				out.append(J9UTF8Helper.stringValue(nameAndSig.signature())); /*
																 * dump
																 * signature
																 */
				out.append(nl);
				pc = pc.add(2);
			} else if ((bcIntVal == JBnew)
				|| (bcIntVal == JBnewdup)
				|| (bcIntVal == JBdefaultvalue)
				|| (bcIntVal == JBanewarray)
				|| (bcIntVal == JBcheckcast)
				|| (bcIntVal == JBinstanceof)
			) {
				index = new UDATA(_GETNEXT_U16());
				info = constantPool.add(index);
				out.append(String.format("%d ", index.intValue()));
				out.append(J9UTF8Helper.stringValue(J9ROMStringRefPointer.cast(info).utf8Data()));
				out.append(nl);
				pc = pc.add(2);
			} else if (bcIntVal == JBnewarray) {
				index = new UDATA(_GETNEXT_U8());
				switch (index.intValue()) {
				case /* T_BOOLEAN */4:
					out.append("boolean\n");
					break;
				case /* T_CHAR */5:
					out.append("char\n");
					break;
				case /* T_FLOAT */6:
					out.append("float\n");
					break;
				case /* T_DOUBLE */7:
					out.append("double\n");
					break;
				case /* T_BYTE */8:
					out.append("byte\n");
					break;
				case /* T_SHORT */9:
					out.append("short\n");
					break;
				case /* T_INT */10:
					out.append("int\n");
					break;
				case /* T_LONG */11:
					out.append("long\n");
					break;
				default:
					out.append(String.format("(unknown type %d)\n", index.intValue()));
					break;
				}
				pc = pc.add(1);
			} else if (bcIntVal == JBmultianewarray) {
				index = new UDATA(_GETNEXT_U16());
				info = constantPool.add(index);
				out.append(String.format("%d ", index.intValue()));
				index = new UDATA(_GETNEXT_U8());
				out.append(String.format("dims %d ", index.intValue()));
				/* dump name */
				out.append(J9UTF8Helper.stringValue(J9ROMStringRefPointer.cast(info).utf8Data()));
				out.append(nl);
				pc = pc.add(3);
			} else if (bcIntVal == JBgotow) {
				index = new UDATA(_GETNEXT_U32());
				pc = pc.add(4);
				if (OPCODE_RELATIVE_BRANCHES != 0) {
					target = start.add(index);
				} else {
					target = pc.add(index);
				}
				out.append(String.format("%d\n", target.intValue()));
			} else {
				out.append(nl);
			}
		}
		return new IDATA(BCT_ERR_NO_ERROR);
	}

	private static void incIndex() {
		bcIndex = bcIndex.add(1);
	}

	private static U32 _GETNEXT_U32() throws CorruptDataException {
		if (bigEndian) {
			return _NEXT_BE_U32();
		} else {
			return _NEXT_LE_U32();
		}
	}

	private static U32 _NEXT_BE_U32() throws CorruptDataException {
		U32 a = new U32(bcIndex.at(0)).leftShift(24);
		U32 b = new U32(bcIndex.at(1)).leftShift(16);
		U32 c = new U32(bcIndex.at(2)).leftShift(8);
		U32 d = new U32(bcIndex.at(3));
		U32 value = a.bitOr(b).bitOr(c).bitOr(d);
		bcIndex = bcIndex.add(4);
		return value;
	}

	private static U32 _NEXT_LE_U32() throws CorruptDataException {
		U32 a = new U32(bcIndex.at(0));
		U32 b = new U32(bcIndex.at(1)).leftShift(8);
		U32 c = new U32(bcIndex.at(2)).leftShift(16);
		U32 d = new U32(bcIndex.at(3)).leftShift(24);
		U32 value = a.bitOr(b).bitOr(c).bitOr(d);
		bcIndex = bcIndex.add(4);
		return value;
	}

	private static U16 _GETNEXT_U16() throws CorruptDataException {
		if (bigEndian) {
			return _NEXT_BE_U16();
		} else {
			return _NEXT_LE_U16();
		}
	}

	private static U16 _NEXT_LE_U16() throws CorruptDataException {
		U16 a = new U16(bcIndex.at(0));
		U16 b = new U16(bcIndex.at(1)).leftShift(8);
		U16 value = a.bitOr(b);
		bcIndex = bcIndex.add(2);
		return value;
	}

	private static U16 _NEXT_BE_U16() throws CorruptDataException {
		U16 a = new U16(bcIndex.at(0)).leftShift(8);
		U16 b = new U16(bcIndex.at(1));
		U16 value = a.bitOr(b);
		bcIndex = bcIndex.add(2);
		return value;
	}

	private static U8 _GETNEXT_U8() throws CorruptDataException {
		U8 result = new U8(bcIndex.at(0));
		incIndex();
		return result;
	}
}
