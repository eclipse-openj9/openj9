package org.openj9.test.typeAnnotation;
/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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


import java.io.ByteArrayInputStream;
import java.lang.annotation.Annotation;
import java.util.HashMap;

import org.testng.log4testng.Logger;

class TypeAnnotationUtils {
	private static final Logger logger = Logger.getLogger(TypeAnnotationUtils.class);

	static void dumpTypeAnnotation(String annotationType, String targetName, @SuppressWarnings("rawtypes") Class clazz, byte[] attr) {
		logger.debug('\n'+annotationType+" type annotations for "+targetName);
		HashMap<Integer, String> nameMap = new HashMap<>();
		nameMap.put(0x00, "TypeParameterGenericClass");
		nameMap.put(0x01, "TypeParameterGenericMethod");
		nameMap.put(0x10, "TypeInExtends");
		nameMap.put(0x11, "TypeInBoundOfGenericClass");
		nameMap.put(0x12, "TypeInBoundOfGenericMethod");
		nameMap.put(0x13, "TypeInFieldDecl");
		nameMap.put(0x14, "ReturnType");
		nameMap.put(0x15, "ReceiverType");
		nameMap.put(0x16, "TypeInFormalParam");
		nameMap.put(0x17, "TypeInThrows");
		nameMap.put(0x40, "TypeInLocalVar");
		nameMap.put(0x41, "TypeInResourceVar");
		nameMap.put(0x42, "TypeInExceptionParam");
		nameMap.put(0x43, "TypeInInstanceof");
		nameMap.put(0x44, "TypeInNew");
		nameMap.put(0x45, "TypeInMethodrefNew");
		nameMap.put(0x46, "TypeInMethodrefIdentifier");
		nameMap.put(0x47, "TypeInCast");
		nameMap.put(0x48, "TypeForGenericConstructorInNew");
		nameMap.put(0x49, "TypeForGenericMethodInvocation");
		nameMap.put(0x4A, "TypeForGenericConstructorInMethodRef");
		nameMap.put(0x4B, "TypeForGenericMethodInvocationInMethodRef");

		if (null == attr) {
			logger.debug("NULL");
		} else {
			for (byte b: attr) {
				logger.debug(b);
				logger.debug(' ');
			}
			logger.debug('\n');
			ByteArrayInputStream attrReader = new ByteArrayInputStream(attr);

			short numAnnotations = TypeAnnotationUtils.get16(attrReader);
			logger.debug(numAnnotations+ " annotations");

			for (short i = 0; i < numAnnotations; i++) {

				int targetType = attrReader.read();
				logger.debug("annotation "+i+" target type = "+targetType + " "+nameMap.get(targetType)+'\n');

				switch (targetType) {
				case CFR_TARGET_TYPE_TypeParameterGenericClass:
				case CFR_TARGET_TYPE_TypeParameterGenericMethod:
					logger.debug(" typeParameterIndex ="+attrReader.read());
					break;

				case CFR_TARGET_TYPE_TypeInExtends:
					logger.debug(" supertypeIndex ="+TypeAnnotationUtils.get16(attrReader));
					break;

				case CFR_TARGET_TYPE_TypeInBoundOfGenericClass:
				case CFR_TARGET_TYPE_TypeInBoundOfGenericMethod:
					logger.debug(" typeParameterIndex ="+attrReader.read());
					logger.debug(" boundIndex ="+attrReader.read());
					break;

				case CFR_TARGET_TYPE_TypeInFieldDecl:
				case CFR_TARGET_TYPE_ReturnType:
				case CFR_TARGET_TYPE_ReceiverType: /* empty_target */
					break;

				case CFR_TARGET_TYPE_TypeInFormalParam:
					logger.debug(" formalParameterIndex ="+attrReader.read());
					break;

				case CFR_TARGET_TYPE_TypeInThrows:
					logger.debug(" throwsTypeIndex ="+TypeAnnotationUtils.get16(attrReader));
					break;

				case CFR_TARGET_TYPE_TypeInLocalVar:
				case CFR_TARGET_TYPE_TypeInResourceVar: {
					short tableLength = TypeAnnotationUtils.get16(attrReader);
					for (short ti=0; ti < tableLength; ++ti) {
						logger.debug(" startPC ="+TypeAnnotationUtils.get16(attrReader));
						logger.debug(" length ="+TypeAnnotationUtils.get16(attrReader));
						logger.debug(" index ="+TypeAnnotationUtils.get16(attrReader));
						if (ti < tableLength - 1) {
							logger.debug('\n');
						}
					}
				}
				break;
				case CFR_TARGET_TYPE_TypeInExceptionParam:
					logger.debug(" startPC ="+TypeAnnotationUtils.get16(attrReader));
					break;

				case CFR_TARGET_TYPE_TypeInInstanceof:
				case CFR_TARGET_TYPE_TypeInNew:
				case CFR_TARGET_TYPE_TypeInMethodrefNew:
				case CFR_TARGET_TYPE_TypeInMethodrefIdentifier:
					logger.debug(" offset ="+TypeAnnotationUtils.get16(attrReader));
					break;

				case CFR_TARGET_TYPE_TypeInCast:
				case CFR_TARGET_TYPE_TypeForGenericConstructorInNew:
				case CFR_TARGET_TYPE_TypeForGenericMethodInvocation:
				case CFR_TARGET_TYPE_TypeForGenericConstructorInMethodRef:
				case CFR_TARGET_TYPE_TypeForGenericMethodInvocationInMethodRef:
					logger.debug(" startPC ="+TypeAnnotationUtils.get16(attrReader));
					logger.debug(" typeArgumentIndex ="+attrReader.read());
					break;

				default:
					break;
				}
				logger.debug("\nType path:\n");
				int pathLength = attrReader.read();
				for (int pathIndex = 0; pathIndex < pathLength; ++pathIndex) {
					logger.debug(" typePathKind ="+attrReader.read());
					logger.debug(" typeArgumentIndex ="+attrReader.read());
					logger.debug('\n');
				}
				dumpAnnotation(attrReader);
			}
		}
	}

	static void dumpAnnotations(ByteArrayInputStream attrReader) {
		int numAnnotations = TypeAnnotationUtils.get16(attrReader);
		for (int i = 0; i < numAnnotations; ++i) {
			dumpAnnotation(attrReader);
		}
	}

	static void dumpAnnotation(ByteArrayInputStream attrReader) {
		logger.debug("Annotation: type_index ="+TypeAnnotationUtils.get16(attrReader)+'\n');
		int numPairs = TypeAnnotationUtils.get16(attrReader);
		for (int i = 0; i < numPairs; ++i) {
			logger.debug("Element value: name_index ="+TypeAnnotationUtils.get16(attrReader));
			dumpElementValue(attrReader);
			logger.debug('\n');
		}
	}

	private static void dumpElementValue(ByteArrayInputStream attrReader) {
		int tag = attrReader.read();
		logger.debug(" tag ="+tag);
		switch (tag) {
		case 'B':
		case 'C':
		case 'D':
		case 'F':
		case 'I':
		case 'J':
		case 'S':
		case 'Z':
		case 's':
			logger.debug(" value ="+TypeAnnotationUtils.get16(attrReader));
			break;
		case 'e':
			logger.debug(" type name ="+TypeAnnotationUtils.get16(attrReader));
			logger.debug(" const name ="+TypeAnnotationUtils.get16(attrReader));
			break;
		case 'c':
			logger.debug(" class index ="+TypeAnnotationUtils.get16(attrReader));
			break;
		case '@':
			dumpAnnotation(attrReader);
			break;
		case '[':
			int numValues = TypeAnnotationUtils.get16(attrReader);
			for (int i = 0; i < numValues; ++i) {
				dumpElementValue(attrReader);
			}
			break;
		}
	}

	final static short CFR_TARGET_TYPE_TypeParameterGenericClass = 0x00;
	final static short CFR_TARGET_TYPE_TypeParameterGenericMethod = 0x01;
	final static short CFR_TARGET_TYPE_TypeInExtends = 0x10;
	final static short CFR_TARGET_TYPE_TypeInBoundOfGenericClass = 0x11;
	final static short CFR_TARGET_TYPE_TypeInBoundOfGenericMethod = 0x12;
	final static short CFR_TARGET_TYPE_TypeInFieldDecl = 0x13;
	final static short CFR_TARGET_TYPE_ReturnType = 0x14;
	final static short CFR_TARGET_TYPE_ReceiverType = 0x15;
	final static short CFR_TARGET_TYPE_TypeInFormalParam = 0x16;
	final static short CFR_TARGET_TYPE_TypeInThrows = 0x17;
	final static short CFR_TARGET_TYPE_TypeInLocalVar = 0x40;
	final static short CFR_TARGET_TYPE_TypeInResourceVar = 0x41;
	final static short CFR_TARGET_TYPE_TypeInExceptionParam = 0x42;
	final static short CFR_TARGET_TYPE_TypeInInstanceof = 0x43;
	final static short CFR_TARGET_TYPE_TypeInNew = 0x44;
	final static short CFR_TARGET_TYPE_TypeInMethodrefNew = 0x45;
	final static short CFR_TARGET_TYPE_TypeInMethodrefIdentifier = 0x46;
	final static short CFR_TARGET_TYPE_TypeInCast = 0x47;
	final static short CFR_TARGET_TYPE_TypeForGenericConstructorInNew = 0x48;
	final static short CFR_TARGET_TYPE_TypeForGenericMethodInvocation = 0x49;
	final static short CFR_TARGET_TYPE_TypeForGenericConstructorInMethodRef = 0x4A;
	final static short CFR_TARGET_TYPE_TypeForGenericMethodInvocationInMethodRef = 0x4B;
	enum targetTypes {
		TypeParameterGenericClass(CFR_TARGET_TYPE_TypeParameterGenericClass),
		TypeParameterGenericMethod(CFR_TARGET_TYPE_TypeParameterGenericMethod),
		TypeInExtends(CFR_TARGET_TYPE_TypeInExtends),
		TypeInBoundOfGenericClass(CFR_TARGET_TYPE_TypeInBoundOfGenericClass),
		TypeInBoundOfGenericMethod(CFR_TARGET_TYPE_TypeInBoundOfGenericMethod),
		TypeInFieldDecl(CFR_TARGET_TYPE_TypeInFieldDecl),
		ReturnType(CFR_TARGET_TYPE_ReturnType),
		ReceiverType(CFR_TARGET_TYPE_ReceiverType),
		TypeInFormalParam(CFR_TARGET_TYPE_TypeInFormalParam),
		TypeInThrows(CFR_TARGET_TYPE_TypeInThrows),
		TypeInLocalVar(CFR_TARGET_TYPE_TypeInLocalVar),
		TypeInResourceVar(CFR_TARGET_TYPE_TypeInResourceVar),
		TypeInExceptionParam(CFR_TARGET_TYPE_TypeInExceptionParam),
		TypeInInstanceof(CFR_TARGET_TYPE_TypeInInstanceof),
		TypeInNew(CFR_TARGET_TYPE_TypeInNew),
		TypeInMethodrefNew(CFR_TARGET_TYPE_TypeInMethodrefNew),
		TypeInMethodrefIdentifier(CFR_TARGET_TYPE_TypeInMethodrefIdentifier),
		TypeInCast(CFR_TARGET_TYPE_TypeInCast),
		TypeForGenericConstructorInNew(CFR_TARGET_TYPE_TypeForGenericConstructorInNew),
		TypeForGenericMethodInvocation(CFR_TARGET_TYPE_TypeForGenericMethodInvocation),
		TypeForGenericConstructorInMethodRef(CFR_TARGET_TYPE_TypeForGenericConstructorInMethodRef),
		TypeForGenericMethodInvocationInMethodRef(CFR_TARGET_TYPE_TypeForGenericMethodInvocationInMethodRef);
		private targetTypes(int value) {
		}

	}
	static short get16(ByteArrayInputStream s) {
		short buffer = (short) (s.read() * 256 + (s.read() & 0x0ff));
		return buffer;
	}

	static void printAnnotationList(String source, Annotation[] annotations) {
		logger.debug("#"+source+":");

		for (Annotation an : annotations) {
			logger.debug(an.toString());
		}
	}


}
