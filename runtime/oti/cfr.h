/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 *******************************************************************************/

#ifndef CFR_H
#define CFR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "j9.h"
#include "j9port.h"

typedef struct J9CfrBootstrapMethod {
    U_16 bootstrapMethodIndex;
    U_16 numberOfBootstrapArguments;
    U_16* bootstrapArguments;
} J9CfrBootstrapMethod;

typedef struct J9CfrClassesEntry {
    U_16 innerClassInfoIndex;
    U_16 outerClassInfoIndex;
    U_16 innerNameIndex;
    U_16 innerClassAccessFlags;
} J9CfrClassesEntry;

typedef struct J9CfrExceptionTableEntry {
    U_32 startPC;
    U_32 endPC;
    U_32 handlerPC;
    U_16 catchType;
} J9CfrExceptionTableEntry;

typedef struct J9CfrLineNumberTableEntry {
    U_32 startPC;
    U_16 lineNumber;
} J9CfrLineNumberTableEntry;

typedef struct J9CfrLocalVariableTableEntry {
    U_32 startPC;
    U_32 length;
    U_16 nameIndex;
    U_16 descriptorIndex;
    U_16 index;
} J9CfrLocalVariableTableEntry;

typedef struct J9CfrLocalVariableTypeTableEntry {
    U_32 startPC;
    U_32 length;
    U_16 nameIndex;
    U_16 signatureIndex;
    U_16 index;
} J9CfrLocalVariableTypeTableEntry;

typedef struct J9CfrRecordComponent {
    U_16 nameIndex;
    U_16 descriptorIndex;
    U_16 attributesCount;
    struct J9CfrAttribute** attributes;
} J9CfrRecordComponent;

typedef struct J9CfrAnnotationElement {
    U_8 tag;
} J9CfrAnnotationElement;

typedef struct J9CfrAnnotationElementPair {
    U_16 elementNameIndex;
    struct J9CfrAnnotationElement* value;
} J9CfrAnnotationElementPair;

typedef struct J9CfrAnnotation {
    U_16 typeIndex;
    U_16 numberOfElementValuePairs;
    struct J9CfrAnnotationElementPair* elementValuePairs;
    U_32 j9dataSize;
} J9CfrAnnotation;

typedef struct J9CfrAnnotationElementAnnotation {
    U_8 tag;
    struct J9CfrAnnotation annotationValue;
} J9CfrAnnotationElementAnnotation;

typedef struct J9CfrAnnotationElementArray {
    U_8 tag;
    U_16 numberOfValues;
    struct J9CfrAnnotationElement** values;
    U_32 j9dataSize;
} J9CfrAnnotationElementArray;

typedef struct J9CfrAnnotationElementClass {
    U_8 tag;
    U_16 classInfoIndex;
} J9CfrAnnotationElementClass;

typedef struct J9CfrAnnotationElementEnum {
    U_8 tag;
    U_16 typeNameIndex;
    U_16 constNameIndex;
} J9CfrAnnotationElementEnum;

typedef struct J9CfrAnnotationElementPrimitive {
    U_8 tag;
    U_16 constValueIndex;
} J9CfrAnnotationElementPrimitive;

typedef struct J9CfrParameterAnnotations {
    U_16 numberOfAnnotations;
    struct J9CfrAnnotation* annotations;
} J9CfrParameterAnnotations;

typedef struct J9CfrTypeParameterTarget {
	U_8 typeParameterIndex;
} J9CfrTypeParameterTarget;

typedef struct J9CfrSupertypeTarget {
	U_16 supertypeIndex;
} J9CfrSupertypeTarget;

typedef struct J9CfrTypeParameterBoundTarget {
	U_8 typeParameterIndex;
	U_8 boundIndex;
} J9CfrTypeParameterBoundTarget;

typedef struct J9CfrMethodFormalParameterTarget {
	U_8 formalParameterIndex;
} J9CfrMethodFormalParameterTarget;

typedef struct J9CfrThrowsTarget {
	U_16 throwsTypeIndex;
} J9CfrThrowsTarget;

typedef struct J9CfrLocalvarTargetEntry {
	U_16 startPC;
	U_16 length;
	U_16 index;
} J9CfrLocalvarTargetEntry;

typedef struct J9CfrLocalvarTarget {
	U_16 tableLength;
	J9CfrLocalvarTargetEntry *table;
} J9CfrLocalvarTarget;

typedef struct J9CfrCatchTarget {
	U_16 exceptiontableIndex;
} J9CfrCatchTarget;

typedef struct J9CfrOffsetTarget {
	U_16 offset;
} J9CfrOffsetTarget;

typedef struct J9CfrTypeArgumentTarget {
	U_16 offset;
	U_8 typeArgumentIndex;
} J9CfrTypeArgumentTarget;

typedef struct J9CfrTypePathEntry {
	U_8 typePathKind;
	U_8 typeArgumentIndex;
} J9CfrTypePathEntry;

typedef struct J9CfrTypePath {
	U_8 pathLength;
	J9CfrTypePathEntry* path;
} J9CfrTypePath;

typedef union J9CfrTypeAnnotationTargetInfo {
	J9CfrTypeParameterTarget typeParameterTarget;
	J9CfrSupertypeTarget supertypeTarget;
	J9CfrTypeParameterBoundTarget typeParameterBoundTarget;
	/* emptyTarget - no member required */
	J9CfrMethodFormalParameterTarget methodFormalParameterTarget;
	J9CfrThrowsTarget throwsTarget;
	J9CfrLocalvarTarget localvarTarget;
	J9CfrCatchTarget catchTarget;
	J9CfrOffsetTarget offsetTarget;
	J9CfrTypeArgumentTarget typeArgumentTarget;
} J9CfrTypeAnnotationTargetInfo;

typedef struct J9CfrTypeAnnotation {
	U_8 targetType;
	J9CfrTypeAnnotationTargetInfo targetInfo;
	J9CfrTypePath typePath;
	J9CfrAnnotation annotation;

} J9CfrTypeAnnotation;

/* @ddr_namespace: map_to_type=J9CfrAttribute */

typedef struct J9CfrAttribute {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
} J9CfrAttribute;

#define CFR_ATTRIBUTE_Unknown  0
#define CFR_ATTRIBUTE_SourceFile  1
#define CFR_ATTRIBUTE_ConstantValue  2
#define CFR_ATTRIBUTE_Code  3
#define CFR_ATTRIBUTE_Exceptions  4
#define CFR_ATTRIBUTE_LocalVariableTable  6
#define CFR_ATTRIBUTE_LineNumberTable  5
#define CFR_ATTRIBUTE_Synthetic  7
#define CFR_ATTRIBUTE_InnerClasses  8
#define CFR_ATTRIBUTE_Deprecated  9
#define CFR_ATTRIBUTE_SourceDebugExtension  10
#define CFR_ATTRIBUTE_EnclosingMethod  11
#define CFR_ATTRIBUTE_Signature  12
#define CFR_ATTRIBUTE_LocalVariableTypeTable  13
#define CFR_ATTRIBUTE_RuntimeInvisibleAnnotations  14
#define CFR_ATTRIBUTE_RuntimeInvisibleParameterAnnotations  15
#define CFR_ATTRIBUTE_RuntimeVisibleAnnotations  16
#define CFR_ATTRIBUTE_RuntimeVisibleParameterAnnotations  17
#define CFR_ATTRIBUTE_AnnotationDefault  18
#define CFR_ATTRIBUTE_StackMap  19
#define CFR_ATTRIBUTE_StackMapTable  20
#define CFR_ATTRIBUTE_BootstrapMethods  21
#define CFR_ATTRIBUTE_RuntimeVisibleTypeAnnotations  22
#define CFR_ATTRIBUTE_RuntimeInvisibleTypeAnnotations  23
#define CFR_ATTRIBUTE_MethodParameters 24
#define CFR_ATTRIBUTE_NestMembers 25
#define CFR_ATTRIBUTE_NestHost 26
#define CFR_ATTRIBUTE_Record 27
#define CFR_ATTRIBUTE_PermittedSubclasses 28
#define CFR_ATTRIBUTE_LoadableDescriptors 29
#define CFR_ATTRIBUTE_ImplicitCreation 30
#define CFR_ATTRIBUTE_NullRestricted 31
#define CFR_ATTRIBUTE_StrippedLocalVariableTypeTable  122
#define CFR_ATTRIBUTE_StrippedSourceDebugExtension  123
#define CFR_ATTRIBUTE_StrippedInnerClasses  124
#define CFR_ATTRIBUTE_StrippedLineNumberTable  125
#define CFR_ATTRIBUTE_StrippedLocalVariableTable  126
#define CFR_ATTRIBUTE_StrippedUnknown  127

typedef struct J9CfrAttributeAnnotationDefault {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    struct J9CfrAnnotationElement* defaultValue;
} J9CfrAttributeAnnotationDefault;

typedef struct J9CfrAttributeBootstrapMethods {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 numberOfBootstrapMethods;
    struct J9CfrBootstrapMethod* bootstrapMethods;
} J9CfrAttributeBootstrapMethods;

typedef struct J9CfrAttributeCode {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 maxStack;
    U_16 maxLocals;
    U_32 codeLength;
    U_8* code;
    U_8* originalCode;
    U_16 exceptionTableLength;
    struct J9CfrExceptionTableEntry* exceptionTable;
    U_16 attributesCount;
    struct J9CfrAttribute** attributes;
} J9CfrAttributeCode;

typedef struct J9CfrAttributeConstantValue {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 constantValueIndex;
} J9CfrAttributeConstantValue;

typedef struct J9CfrAttributeDeprecated {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
} J9CfrAttributeDeprecated;

typedef struct J9CfrAttributeEnclosingMethod {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 classIndex;
    U_16 methodIndex;
} J9CfrAttributeEnclosingMethod;

typedef struct J9CfrAttributeExceptions {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 numberOfExceptions;
    U_16* exceptionIndexTable;
} J9CfrAttributeExceptions;

typedef struct J9CfrAttributeInnerClasses {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 numberOfClasses;
    struct J9CfrClassesEntry* classes;
} J9CfrAttributeInnerClasses;

typedef struct J9CfrAttributeLineNumberTable {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 lineNumberTableLength;
    struct J9CfrLineNumberTableEntry* lineNumberTable;
    U_32 j9StartPC;
    U_32 j9EndPC;
} J9CfrAttributeLineNumberTable;

typedef struct J9CfrAttributeLocalVariableTable {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 localVariableTableLength;
    struct J9CfrLocalVariableTableEntry* localVariableTable;
} J9CfrAttributeLocalVariableTable;

typedef struct J9CfrAttributeLocalVariableTypeTable {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 localVariableTypeTableLength;
    struct J9CfrLocalVariableTypeTableEntry* localVariableTypeTable;
} J9CfrAttributeLocalVariableTypeTable;

typedef struct J9CfrAttributeRuntimeInvisibleAnnotations {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 numberOfAnnotations;
    struct J9CfrAnnotation* annotations;
	U_32 rawDataLength;
	U_8 *rawAttributeData; /* hold the original attribute data in case of error  */
} J9CfrAttributeRuntimeInvisibleAnnotations;

typedef struct J9CfrAttributeRuntimeInvisibleParameterAnnotations {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_8 numberOfParameters;
    struct J9CfrParameterAnnotations* parameterAnnotations;
	U_32 rawDataLength;
	U_8 *rawAttributeData; /* hold the original attribute data in case of error  */
} J9CfrAttributeRuntimeInvisibleParameterAnnotations;

#define CFR_TARGET_TYPE_TypeParameterGenericClass 0x00
#define CFR_TARGET_TYPE_TypeParameterGenericMethod 0x01
#define CFR_TARGET_TYPE_TypeInExtends 0x10
#define CFR_TARGET_TYPE_TypeInBoundOfGenericClass 0x11
#define CFR_TARGET_TYPE_TypeInBoundOfGenericMethod 0x12
#define CFR_TARGET_TYPE_TypeInFieldDecl 0x13
#define CFR_TARGET_TYPE_ReturnType 0x14
#define CFR_TARGET_TYPE_ReceiverType 0x15
#define CFR_TARGET_TYPE_TypeInFormalParam 0x16
#define CFR_TARGET_TYPE_TypeInThrows 0x17
#define CFR_TARGET_TYPE_TypeInLocalVar 0x40
#define CFR_TARGET_TYPE_TypeInResourceVar 0x41
#define CFR_TARGET_TYPE_TypeInExceptionParam 0x42
#define CFR_TARGET_TYPE_TypeInInstanceof 0x43
#define CFR_TARGET_TYPE_TypeInNew 0x44
#define CFR_TARGET_TYPE_TypeInMethodrefNew 0x45
#define CFR_TARGET_TYPE_TypeInMethodrefIdentifier 0x46
#define CFR_TARGET_TYPE_TypeInCast 0x47
#define CFR_TARGET_TYPE_TypeForGenericConstructorInNew 0x48
#define CFR_TARGET_TYPE_TypeForGenericMethodInvocation 0x49
#define CFR_TARGET_TYPE_TypeForGenericConstructorInMethodRef 0x4A
#define CFR_TARGET_TYPE_TypeForGenericMethodInvocationInMethodRef 0x4B
#define CFR_TARGET_TYPE_ErrorInAttribute 0xff


typedef struct J9CfrAttributeRuntimeInvisibleTypeAnnotations {
	U_8 tag;
	U_16 nameIndex;
	U_32 length;
	UDATA romAddress;
	U_16 numberOfAnnotations;
	struct J9CfrTypeAnnotation* typeAnnotations;
	U_32 rawDataLength;
	U_8 *rawAttributeData; /* hold the original attribute data in case of error  */
} J9CfrAttributeRuntimeInvisibleTypeAnnotations;

typedef struct J9CfrAttributeRuntimeVisibleAnnotations {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 numberOfAnnotations;
    struct J9CfrAnnotation* annotations;
	U_32 rawDataLength;
	U_8 *rawAttributeData; /* hold the original attribute data in case of error  */
} J9CfrAttributeRuntimeVisibleAnnotations;

typedef struct J9CfrAttributeRuntimeVisibleParameterAnnotations {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_8 numberOfParameters;
    struct J9CfrParameterAnnotations* parameterAnnotations;
	U_32 rawDataLength;
	U_8 *rawAttributeData; /* hold the original attribute data in case of error  */
} J9CfrAttributeRuntimeVisibleParameterAnnotations;

typedef struct J9CfrAttributeMethodParameters {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_8 numberOfMethodParameters;
    U_16 * methodParametersIndexTable;
    U_16 * flags;
} J9CfrAttributeMethodParameters;

typedef struct J9CfrAttributeRuntimeVisibleTypeAnnotations {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 numberOfAnnotations;
    struct J9CfrTypeAnnotation* typeAnnotations;
	U_32 rawDataLength;
	U_8 *rawAttributeData; /* hold the original attribute data in case of error  */
} J9CfrAttributeRuntimeVisibleTypeAnnotations;

typedef struct J9CfrAttributeSignature {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 signatureIndex;
} J9CfrAttributeSignature;

typedef struct J9CfrAttributeSourceFile {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 sourceFileIndex;
} J9CfrAttributeSourceFile;

typedef struct J9CfrAttributeStackMap {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_32 mapLength;
    U_32 numberOfEntries;
    U_8* entries;
} J9CfrAttributeStackMap;

typedef struct J9CfrAttributeSynthetic {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
} J9CfrAttributeSynthetic;

#if JAVA_SPEC_VERSION >= 11
typedef struct J9CfrAttributeNestHost {
	U_8 tag;
	U_16 nameIndex;
	U_32 length;
	UDATA romAddress;
	U_16 hostClassIndex;
} J9CfrAttributeNestHost;

typedef struct J9CfrAttributeNestMembers {
	U_8 tag;
	U_16 nameIndex;
	U_32 length;
	UDATA romAddress;
	U_16 numberOfClasses;
	U_16* classes;
} J9CfrAttributeNestMembers;
#endif /* JAVA_SPEC_VERSION >= 11 */

typedef struct J9CfrAttributeUnknown {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_8* value;
} J9CfrAttributeUnknown;

typedef struct J9CfrAttributeRecord {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 numberOfRecordComponents;
    struct J9CfrRecordComponent* recordComponents;
} J9CfrAttributeRecord;

typedef struct J9CfrAttributePermittedSubclasses {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 numberOfClasses;
    U_16* classes;
} J9CfrAttributePermittedSubclasses;

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
typedef struct J9CfrAttributeLoadableDescriptors {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 numberOfDescriptors;
    U_16 *descriptors;
} J9CfrAttributeLoadableDescriptors;
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
typedef struct J9CfrAttributeImplicitCreation {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
    U_16 implicitCreationFlags;
} J9CfrAttributeImplicitCreation;

typedef struct J9CfrAttributeNullRestricted {
    U_8 tag;
    U_16 nameIndex;
    U_32 length;
    UDATA romAddress;
} J9CfrAttributeNullRestricted;
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

/* @ddr_namespace: map_to_type=J9CfrConstantPoolInfo */

typedef struct J9CfrConstantPoolInfo {
    U_8 tag;
    U_8 flags1;
    U_16 nextCPIndex;
    U_32 slot1;
    U_32 slot2;
    U_8* bytes;
    UDATA romAddress;
} J9CfrConstantPoolInfo;

#define CFR_CONSTANT_Null  0
#define CFR_CONSTANT_Utf8  1
#define CFR_CONSTANT_Integer  3
#define CFR_CONSTANT_Float  4
#define CFR_CONSTANT_Long  5
#define CFR_CONSTANT_Double  6
#define CFR_CONSTANT_Class  7
#define CFR_CONSTANT_String  8
#define CFR_CONSTANT_Fieldref  9
#define CFR_CONSTANT_Methodref  10
#define CFR_CONSTANT_InterfaceMethodref  11
#define CFR_CONSTANT_NameAndType  12
#define CFR_CONSTANT_MethodHandle  15
#define CFR_CONSTANT_MethodType  16
#define CFR_CONSTANT_Dynamic  17
#define CFR_CONSTANT_InvokeDynamic  18
#define CFR_CONSTANT_Module 19
#define CFR_CONSTANT_Package 20
#define CFR_ReferencedBit   0x80
#define CFR_ReferencedMask  0x7F
#define CFR_SeenByConvertInvokevirtualToSpecial  1
#define CFR_FLAGS1_VerifiedMemberName  1
#define CFR_FLAGS1_VerifiedFieldSignature  2
#define CFR_ShouldConvertInvokevirtualToSpecial  2


typedef struct J9CfrMember {
    U_16 accessFlags;
    U_16 nameIndex;
    U_16 descriptorIndex;
    U_16 attributesCount;
    struct J9CfrAttribute** attributes;
    UDATA romAddress;
} J9CfrMember;

typedef struct J9CfrField {
    U_16 accessFlags;
    U_16 nameIndex;
    U_16 descriptorIndex;
    U_16 attributesCount;
    struct J9CfrAttribute** attributes;
    UDATA romAddress;
    struct J9CfrAttributeConstantValue* constantValueAttribute;
} J9CfrField;

/* @ddr_namespace: map_to_type=J9CfrMethod */

typedef struct J9CfrMethod {
    U_16 accessFlags;
    U_16 nameIndex;
    U_16 descriptorIndex;
    U_16 attributesCount;
    struct J9CfrAttribute** attributes;
    UDATA romAddress;
    struct J9CfrAttributeCode* codeAttribute;
    struct J9CfrAttributeExceptions* exceptionsAttribute;
    struct J9CfrAttributeMethodParameters* methodParametersAttribute;
    U_32 j9ArgumentCount;
    U_16 j9Flags;
} J9CfrMethod;

#define CFR_BC_nop 0
#define CFR_BC_aconst_null 1
#define CFR_BC_iconst_m1 2
#define CFR_BC_iconst_0 3
#define CFR_BC_iconst_1 4
#define CFR_BC_iconst_2 5
#define CFR_BC_iconst_3 6
#define CFR_BC_iconst_4 7
#define CFR_BC_iconst_5 8
#define CFR_BC_lconst_0 9
#define CFR_BC_lconst_1 10
#define CFR_BC_fconst_0 11
#define CFR_BC_fconst_1 12
#define CFR_BC_fconst_2 13
#define CFR_BC_dconst_0 14
#define CFR_BC_dconst_1 15
#define CFR_BC_bipush 16
#define CFR_BC_sipush 17
#define CFR_BC_ldc 18
#define CFR_BC_ldc_w 19
#define CFR_BC_ldc2_w 20
#define CFR_BC_iload 21
#define CFR_BC_lload 22
#define CFR_BC_fload 23
#define CFR_BC_dload 24
#define CFR_BC_aload 25
#define CFR_BC_iload_0 26
#define CFR_BC_iload_1 27
#define CFR_BC_iload_2 28
#define CFR_BC_iload_3 29
#define CFR_BC_lload_0 30
#define CFR_BC_lload_1 31
#define CFR_BC_lload_2 32
#define CFR_BC_lload_3 33
#define CFR_BC_fload_0 34
#define CFR_BC_fload_1 35
#define CFR_BC_fload_2 36
#define CFR_BC_fload_3 37
#define CFR_BC_dload_0 38
#define CFR_BC_dload_1 39
#define CFR_BC_dload_2 40
#define CFR_BC_dload_3 41
#define CFR_BC_aload_0 42
#define CFR_BC_aload_1 43
#define CFR_BC_aload_2 44
#define CFR_BC_aload_3 45
#define CFR_BC_iaload 46
#define CFR_BC_laload 47
#define CFR_BC_faload 48
#define CFR_BC_daload 49
#define CFR_BC_aaload 50
#define CFR_BC_baload 51
#define CFR_BC_caload 52
#define CFR_BC_saload 53
#define CFR_BC_istore 54
#define CFR_BC_lstore 55
#define CFR_BC_fstore 56
#define CFR_BC_dstore 57
#define CFR_BC_astore 58
#define CFR_BC_istore_0 59
#define CFR_BC_istore_1 60
#define CFR_BC_istore_2 61
#define CFR_BC_istore_3 62
#define CFR_BC_lstore_0 63
#define CFR_BC_lstore_1 64
#define CFR_BC_lstore_2 65
#define CFR_BC_lstore_3 66
#define CFR_BC_fstore_0 67
#define CFR_BC_fstore_1 68
#define CFR_BC_fstore_2 69
#define CFR_BC_fstore_3 70
#define CFR_BC_dstore_0 71
#define CFR_BC_dstore_1 72
#define CFR_BC_dstore_2 73
#define CFR_BC_dstore_3 74
#define CFR_BC_astore_0 75
#define CFR_BC_astore_1 76
#define CFR_BC_astore_2 77
#define CFR_BC_astore_3 78
#define CFR_BC_iastore 79
#define CFR_BC_lastore 80
#define CFR_BC_fastore 81
#define CFR_BC_dastore 82
#define CFR_BC_aastore 83
#define CFR_BC_bastore 84
#define CFR_BC_castore 85
#define CFR_BC_sastore 86
#define CFR_BC_pop 87
#define CFR_BC_pop2 88
#define CFR_BC_dup 89
#define CFR_BC_dup_x1 90
#define CFR_BC_dup_x2 91
#define CFR_BC_dup2 92
#define CFR_BC_dup2_x1 93
#define CFR_BC_dup2_x2 94
#define CFR_BC_swap 95
#define CFR_BC_iadd 96
#define CFR_BC_ladd 97
#define CFR_BC_fadd 98
#define CFR_BC_dadd 99
#define CFR_BC_isub 100
#define CFR_BC_lsub 101
#define CFR_BC_fsub 102
#define CFR_BC_dsub 103
#define CFR_BC_imul 104
#define CFR_BC_lmul 105
#define CFR_BC_fmul 106
#define CFR_BC_dmul 107
#define CFR_BC_idiv 108
#define CFR_BC_ldiv 109
#define CFR_BC_fdiv 110
#define CFR_BC_ddiv 111
#define CFR_BC_irem 112
#define CFR_BC_lrem 113
#define CFR_BC_frem 114
#define CFR_BC_drem 115
#define CFR_BC_ineg 116
#define CFR_BC_lneg 117
#define CFR_BC_fneg 118
#define CFR_BC_dneg 119
#define CFR_BC_ishl 120
#define CFR_BC_lshl 121
#define CFR_BC_ishr 122
#define CFR_BC_lshr 123
#define CFR_BC_iushr 124
#define CFR_BC_lushr 125
#define CFR_BC_iand 126
#define CFR_BC_land 127
#define CFR_BC_ior 128
#define CFR_BC_lor 129
#define CFR_BC_ixor 130
#define CFR_BC_lxor 131
#define CFR_BC_iinc 132
#define CFR_BC_i2l 133
#define CFR_BC_i2f 134
#define CFR_BC_i2d 135
#define CFR_BC_l2i 136
#define CFR_BC_l2f 137
#define CFR_BC_l2d 138
#define CFR_BC_f2i 139
#define CFR_BC_f2l 140
#define CFR_BC_f2d 141
#define CFR_BC_d2i 142
#define CFR_BC_d2l 143
#define CFR_BC_d2f 144
#define CFR_BC_i2b 145
#define CFR_BC_i2c 146
#define CFR_BC_i2s 147
#define CFR_BC_lcmp 148
#define CFR_BC_fcmpl 149
#define CFR_BC_fcmpg 150
#define CFR_BC_dcmpl 151
#define CFR_BC_dcmpg 152
#define CFR_BC_ifeq 153
#define CFR_BC_ifne 154
#define CFR_BC_iflt 155
#define CFR_BC_ifge 156
#define CFR_BC_ifgt 157
#define CFR_BC_ifle 158
#define CFR_BC_if_icmpeq 159
#define CFR_BC_if_icmpne 160
#define CFR_BC_if_icmplt 161
#define CFR_BC_if_icmpge 162
#define CFR_BC_if_icmpgt 163
#define CFR_BC_if_icmple 164
#define CFR_BC_if_acmpeq 165
#define CFR_BC_if_acmpne 166
#define CFR_BC_goto 167
#define CFR_BC_jsr 168
#define CFR_BC_ret 169
#define CFR_BC_tableswitch 170
#define CFR_BC_lookupswitch 171
#define CFR_BC_ireturn 172
#define CFR_BC_lreturn 173
#define CFR_BC_freturn 174
#define CFR_BC_dreturn 175
#define CFR_BC_areturn 176
#define CFR_BC_return 177
#define CFR_BC_getstatic 178
#define CFR_BC_putstatic 179
#define CFR_BC_getfield 180
#define CFR_BC_putfield 181
#define CFR_BC_invokevirtual 182
#define CFR_BC_invokespecial 183
#define CFR_BC_invokestatic 184
#define CFR_BC_invokeinterface 185
#define CFR_BC_invokedynamic 186
#define CFR_BC_new 187
#define CFR_BC_newarray 188
#define CFR_BC_anewarray 189
#define CFR_BC_arraylength 190
#define CFR_BC_athrow 191
#define CFR_BC_checkcast 192
#define CFR_BC_instanceof 193
#define CFR_BC_monitorenter 194
#define CFR_BC_monitorexit 195
#define CFR_BC_wide 196
#define CFR_BC_multianewarray 197
#define CFR_BC_ifnull 198
#define CFR_BC_ifnonnull 199
#define CFR_BC_goto_w 200
#define CFR_BC_jsr_w 201
#define CFR_BC_breakpoint 202  			/* Reserved opcodes */
#define CFR_BC_MaxDefined CFR_BC_jsr_w
#define CFR_BC_impdep1 254
#define CFR_BC_impdep2 255
#define CFR_BC_invokehandle 232 		/* JSR 292 internals */
#define CFR_BC_invokehandlegeneric 233
#define CFR_BC_invokestaticsplit 234 	/* Constant pool splitting / sidetable invokes */
#define CFR_BC_invokespecialsplit 235

/* @ddr_namespace: map_to_type=J9CfrClassFile */

typedef struct J9CfrClassFile {
    U_32 magic;
    U_16 minorVersion;
    U_16 majorVersion;
    U_16 accessFlags;
    U_16 j9Flags;
    U_16 thisClass;
    U_16 superClass;
    U_16 constantPoolCount;
    U_16 interfacesCount;
    U_16 fieldsCount;
    U_16 methodsCount;
    U_16 attributesCount;
    U_16 firstUTF8CPIndex;
    U_16 lastUTF8CPIndex;
    U_16 firstNATCPIndex;
    struct J9CfrConstantPoolInfo* constantPool;
    U_16* interfaces;
    struct J9CfrField* fields;
    struct J9CfrMethod* methods;
    struct J9CfrAttribute** attributes;
    U_32 classFileSize;
} J9CfrClassFile;

#define CFR_ACC_PUBLIC  					0x00000001
#define CFR_ACC_PRIVATE  					0x00000002
#define CFR_ACC_PROTECTED  					0x00000004
#define CFR_ACC_STATIC  					0x00000008
#define CFR_ACC_FINAL  						0x00000010
#define CFR_ACC_SUPER  						0x00000020
/* From the spec, CFR_ACC_SUPER semantics is mandatory since Java 8 and the bit takes no effect, so it is repurposed as CFR_ACC_IDENTITY in Valhalla */
#define CFR_ACC_IDENTITY  					0x00000020
#define CFR_ACC_SYNCHRONIZED  				0x00000020
#define CFR_ACC_BRIDGE  					0x00000040
#define CFR_ACC_VOLATILE  					0x00000040
#define CFR_ACC_TRANSIENT  					0x00000080
#define CFR_ACC_VARARGS  					0x00000080
#define CFR_ACC_NATIVE  					0x00000100
#define CFR_ACC_INTERFACE  					0x00000200
#define CFR_ACC_ABSTRACT  					0x00000400
#define CFR_ACC_STRICT  					0x00000800
#define CFR_ACC_SYNTHETIC  					0x00001000
#define CFR_ACC_ANNOTATION 					0x00002000
#define CFR_ACC_ENUM  						0x00004000
#define CFR_ACC_MANDATED  					0x00008000
#define CFR_ACC_MODULE						0x00008000
#define CFR_ACC_GETTER_METHOD  				0x00000200
#define CFR_ACC_FORWARDER_METHOD  			0x00002000
#define CFR_ACC_EMPTY_METHOD  				0x00004000
#define CFR_ACC_VTABLE  					0x00010000
#define CFR_ACC_HAS_EXCEPTION_INFO  		0x00020000
#define CFR_ACC_METHOD_HAS_DEBUG_INFO  		0x00040000
#define CFR_ACC_METHOD_FRAME_ITERATOR_SKIP  0x00080000
#define CFR_ACC_METHOD_CALLER_SENSITIVE  	0x00100000
#define CFR_ACC_METHOD_HAS_STACK_MAP  		0x10000000
#define CFR_ACC_HAS_EMPTY_FINALIZE  		0x00200000
#define CFR_ACC_HAS_VERIFY_DATA  			0x00800000
#define CFR_ACC_HAS_FINAL_FIELDS  			0x02000000
#define CFR_ACC_REFERENCE_WEAK  			0x10000000
#define CFR_ACC_REFERENCE_SOFT  			0x20000000
#define CFR_ACC_REFERENCE_PHANTOM     		0x30000000
#define CFR_ACC_FINALIZE_NEEDED  			0x40000000
#define CFR_ACC_CLONEABLE  					0x80000000
#define CFR_MAGIC  0xCAFEBABE
#define CFR_MAJOR_VERSION  45
#define CFR_MINOR_VERSION  3
#define CFR_PUBLIC_PRIVATE_PROTECTED_MASK	(CFR_ACC_PUBLIC | CFR_ACC_PRIVATE | CFR_ACC_PROTECTED)

#define J9_IS_CLASSFILE_VALUETYPE(classfile) (J9_IS_CLASSFILE_OR_ROMCLASS_VALUETYPE_VERSION(classfile) && J9_ARE_NO_BITS_SET((classfile)->accessFlags, CFR_ACC_IDENTITY | CFR_ACC_INTERFACE))

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
#define CFR_CLASS_ACCESS_MASK					(CFR_ACC_PUBLIC | CFR_ACC_FINAL | CFR_ACC_IDENTITY | CFR_ACC_INTERFACE | CFR_ACC_ABSTRACT | CFR_ACC_SYNTHETIC | CFR_ACC_ANNOTATION | CFR_ACC_ENUM)
#define CFR_FIELD_ACCESS_MASK  					(CFR_ACC_PUBLIC | CFR_ACC_PRIVATE | CFR_ACC_PROTECTED | CFR_ACC_STATIC | CFR_ACC_FINAL | CFR_ACC_VOLATILE | CFR_ACC_TRANSIENT | CFR_ACC_SYNTHETIC | CFR_ACC_ENUM | CFR_ACC_STRICT)
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
#define CFR_CLASS_ACCESS_MASK					(CFR_ACC_PUBLIC | CFR_ACC_FINAL | CFR_ACC_SUPER | CFR_ACC_INTERFACE | CFR_ACC_ABSTRACT | CFR_ACC_SYNTHETIC | CFR_ACC_ANNOTATION | CFR_ACC_ENUM)
#define CFR_FIELD_ACCESS_MASK  					(CFR_ACC_PUBLIC | CFR_ACC_PRIVATE | CFR_ACC_PROTECTED | CFR_ACC_STATIC | CFR_ACC_FINAL | CFR_ACC_VOLATILE | CFR_ACC_TRANSIENT | CFR_ACC_SYNTHETIC | CFR_ACC_ENUM)
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
#define CFR_INTERFACE_CLASS_ACCESS_MASK			(CFR_ACC_PUBLIC | CFR_ACC_INTERFACE | CFR_ACC_ABSTRACT | CFR_ACC_SYNTHETIC | CFR_ACC_ANNOTATION)
#define CFR_INTERFACE_FIELD_ACCESS_MASK  		(CFR_ACC_PUBLIC | CFR_ACC_STATIC | CFR_ACC_FINAL | CFR_ACC_SYNTHETIC)
#define CFR_INTERFACE_FIELD_ACCESS_REQUIRED  	(CFR_ACC_PUBLIC | CFR_ACC_STATIC | CFR_ACC_FINAL)
#define CFR_METHOD_ACCESS_MASK  				(CFR_ACC_PUBLIC | CFR_ACC_PRIVATE | CFR_ACC_PROTECTED | CFR_ACC_STATIC | CFR_ACC_FINAL | CFR_ACC_SYNCHRONIZED | CFR_ACC_BRIDGE | CFR_ACC_VARARGS | CFR_ACC_NATIVE | CFR_ACC_STRICT | CFR_ACC_ABSTRACT | CFR_ACC_SYNTHETIC)
#define CFR_ABSTRACT_METHOD_ACCESS_MASK  		(CFR_ACC_PUBLIC | CFR_ACC_PROTECTED | CFR_ACC_BRIDGE | CFR_ACC_VARARGS | CFR_ACC_ABSTRACT | CFR_ACC_SYNTHETIC)
#define CFR_INTERFACE_METHOD_ACCESS_MASK  		(CFR_ACC_PUBLIC | CFR_ACC_BRIDGE | CFR_ACC_VARARGS | CFR_ACC_ABSTRACT | CFR_ACC_SYNTHETIC)
#define CFR_INTERFACE_METHOD_ACCESS_REQUIRED	(CFR_ACC_PUBLIC | CFR_ACC_ABSTRACT)
#define CFR_INIT_METHOD_ACCESS_MASK		(CFR_ACC_PUBLIC | CFR_ACC_PRIVATE | CFR_ACC_PROTECTED | CFR_ACC_SYNTHETIC | CFR_ACC_VARARGS | CFR_ACC_STRICT)
#define CFR_CLINIT_METHOD_ACCESS_MASK	(CFR_ACC_STRICT | CFR_ACC_STATIC)
#define CFR_CLASS_ACCESS_NEWJDK5_MASK	(CFR_ACC_SYNTHETIC | CFR_ACC_ANNOTATION | CFR_ACC_ENUM)  /* Defines access flags not available prior to JDK 5.0 */
#define CFR_FIELD_ACCESS_NEWJDK5_MASK	(CFR_ACC_SYNTHETIC | CFR_ACC_ENUM)
#define CFR_METHOD_ACCESS_NEWJDK5_MASK	(CFR_ACC_BRIDGE | CFR_ACC_VARARGS | CFR_ACC_SYNTHETIC)
#define CFR_ATTRIBUTE_METHOD_PARAMETERS_MASK (CFR_ACC_FINAL | CFR_ACC_SYNTHETIC | CFR_ACC_MANDATED)
#define CFR_DECODE_SIMPLE  			1
#define CFR_DECODE_I8  				2
#define CFR_DECODE_I16  			3
#define CFR_DECODE_U8  				4
#define CFR_DECODE_U16  			5
#define CFR_DECODE_U8_I8  			6
#define CFR_DECODE_U16_I16  		7
#define CFR_DECODE_CP8  			8
#define CFR_DECODE_CP16  			9
#define CFR_DECODE_L16  			10
#define CFR_DECODE_L32 				11
#define CFR_DECODE_TABLESWITCH 		12
#define CFR_DECODE_LOOKUPSWITCH 	13
#define CFR_DECODE_NEWARRAY  		14
#define CFR_DECODE_MULTIANEWARRAY	15
#define CFR_DECODE_J9_LDC  			16
#define CFR_DECODE_J9_LDCW  		17
#define CFR_DECODE_J9_LDC2DW  		18
#define CFR_DECODE_J9_LDC2LW  		19
#define CFR_DECODE_J9_CLASSREF  	20
#define CFR_DECODE_J9_METHODREF  	21
#define CFR_DECODE_J9_FIELDREF  	22
#define CFR_DECODE_J9_MULTIANEWARRAY	23
#define CFR_DECODE_J9_METHODTYPEREF		24
#define CFR_J9FLAG_HAS_JSR  		1
#define CFR_J9FLAG_IS_RECORD        2
#define CFR_J9FLAG_IS_SEALED        4

#if defined(J9VM_ENV_DATA64)
#define ROM_ADDRESS_LENGTH 18
#define ROM_ADDRESS_FORMAT "0x%016x"
#else
#define ROM_ADDRESS_LENGTH 10
#define ROM_ADDRESS_FORMAT "0x%08x"
#endif

#define CFR_METHOD_EXT_HAS_METHOD_TYPE_ANNOTATIONS 0x01
#define CFR_METHOD_EXT_HAS_CODE_TYPE_ANNOTATIONS 0x02
#define CFR_METHOD_EXT_INVALID_CP_ENTRY 0x04
#if JAVA_SPEC_VERSION >= 16
#define CFR_METHOD_EXT_HAS_SCOPED_ANNOTATION 0x08
#endif /* JAVA_SPEC_VERSION >= 16 */
#define CFR_METHOD_EXT_NOT_CHECKPOINT_SAFE_ANNOTATION 0x10
#if JAVA_SPEC_VERSION >= 20
#define CFR_METHOD_EXT_JVMTIMOUNTTRANSITION_ANNOTATION 0x20
#endif /* JAVA_SPEC_VERSION >= 20 */

#define ANON_CLASSNAME_CHARACTER_SEPARATOR '/'

#define CFR_FOUND_CHARS_IN_EXTENDED_MUE_FORM 0x1
#define CFR_FOUND_SEPARATOR_IN_MUE_FORM 0x2

#ifdef __cplusplus
}
#endif

#endif /* CFR_H */
