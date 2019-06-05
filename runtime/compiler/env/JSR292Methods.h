/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifndef JSR292Methods_h
#define JSR292Methods_h

#define JSR292_ILGenMacros    "java/lang/invoke/ILGenMacros"
#define JSR292_placeholder    "placeholder"
#define JSR292_placeholderSig "(I)I"

#define JSR292_MethodHandle   "java/lang/invoke/MethodHandle"
#define JSR292_invokeExactTargetAddress    "invokeExactTargetAddress"
#define JSR292_invokeExactTargetAddressSig "()J"
#define JSR292_getType                     "type"
#define JSR292_getTypeSig                  "()Ljava/lang/invoke/MethodType;"

#define JSR292_invokeExact    "invokeExact"
#define JSR292_invokeExactSig "([Ljava/lang/Object;)Ljava/lang/Object;"

#define JSR292_ComputedCalls  "java/lang/invoke/ComputedCalls"
#define JSR292_dispatchDirectPrefix "dispatchDirect_"
#define JSR292_dispatchDirectArgSig "(JI)"

#define JSR292_asType              "asType"
#define JSR292_asTypeSig           "(Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;"
#define JSR292_forGenericInvoke    "forGenericInvoke"
#define JSR292_forGenericInvokeSig "(Ljava/lang/invoke/MethodType;Z)Ljava/lang/invoke/MethodHandle;"

#define JSR292_ArgumentMoverHandle  "java/lang/invoke/ArgumentMoverHandle"

#define JSR292_StaticFieldGetterHandle "java/lang/invoke/StaticFieldGetterHandle"
#define JSR292_StaticFieldSetterHandle "java/lang/invoke/StaticFieldSetterHandle"

#define JSR292_FieldGetterHandle "java/lang/invoke/FieldGetterHandle"
#define JSR292_FieldSetterHandle "java/lang/invoke/FieldSetterHandle"
#endif
