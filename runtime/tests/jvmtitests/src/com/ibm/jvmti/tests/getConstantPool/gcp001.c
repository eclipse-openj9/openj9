/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include <string.h>

#if 0

#include "j9cfg.h"
#include "j9.h"
#include "j9comp.h"
#include "jbcmap.h"
#include "bcsizes.h"
#include "bcnames.h"
#include "j9port.h"
#include "cfr.h"
#include "cfreader.h"
#include "j9stdarg.h"

#include "ibmjvmti.h"
#include "rommeth.h"
#include "pcstack.h"

extern jboolean jvmti11;
J9PortLibrary * PORTLIB = NULL;

#ifdef J9VM_ENV_LITTLE_ENDIAN
static BOOLEAN bigEndian = FALSE;
#else
static BOOLEAN bigEndian = TRUE;
#endif


typedef struct testClassCPDescriptor {
	J9CfrConstantPoolInfo  *constantPool;
	U_16			constantPoolCount;
	U_8 * 			utf8Buf;
} testClassCPDescriptor;


typedef struct jvmtiTst_Error {
	U_8 *	errorMessage;
	va_list errorMessageFormat;
} jvmtiTst_Error;



static void JNICALL testGetConstantPool_classLoad(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jclass klass);
static I_32 testGetConstantPool_readPool(testClassCPDescriptor *cpDescriptor, U_8* constantPoolBuf, UDATA constantPoolBufSize);
static I_32 testGetConstantPool_checkPool(testClassCPDescriptor *cpDescriptor, U_8* constantPoolBuf);
static I_32 testGetConstantPool_verifyCanonisizeAndCopyUTF8 (U_8 *dest, U_8 *source, U_32 length);
static jvmtiTst_Error * testGetConstantPool_checkStaticConstants(testClassCPDescriptor *cpDescriptor, U_8 *bytecodes, U_32 size);
static jvmtiTst_Error * testGetConstantPool_checkStaticConstants_referedClass(testClassCPDescriptor *cpDescriptor, U_16 classCpIndex, const char *expectedClassName);
static jvmtiTst_Error * testGetConstantPool_checkStaticConstants_referedString(testClassCPDescriptor *cpDescriptor, U_16 stringCpIndex, const char *expectedString);
static jvmtiTst_Error * testGetConstantPool_checkStaticConstants_referedUtf8(testClassCPDescriptor *cpDescriptor, U_16 utf8CpIndex, const char *expectedString);
static jvmtiTst_Error * testGetConstantPool_checkStaticConstants_referedNameAndSig(testClassCPDescriptor *cpDescriptor, U_16 nasCpIndex, const char *expectedName, const char *expectedSignature);
static jvmtiTst_Error * testGetConstantPool_checkStaticConstants_referedReference(testClassCPDescriptor *cpDescriptor, U_16 refCpIndex, U_8 expectedReferenceType, const char *expectedClassName, const char *expectedName, const char *expectedSignature);

static jvmtiTst_Error * testGetConstantPool_checkStaticConstants_referedDoubleLong(testClassCPDescriptor *cpDescriptor, U_8  cpItemType, U_16 cpIndex, U_32 expectedValue_hi, U_32 expectedValue_lo); 

#define CHECK_FOR_EOF(nextRead) \
	if((index + (nextRead)) > dataEnd) \
	{ \
		errorCode = (U_32)J9NLS_CFR_ERR_UNEXPECTED_EOF__ID; \
		goto _errorFound; \
	}


jvmtiTst_Error *
jvmtiTst_ErrorCreate(char *format, ...)
{
	va_list args;
	jvmtiTst_Error * error;

	error = j9mem_allocate_memory(sizeof(jvmtiTst_Error), J9MEM_CATEGORY_VM);
	if (NULL == error) {
		return (jvmtiTst_Error *) 0x1;	 // TODO: fix this to return a static out of mem struct
	}

	error->errorMessage = j9mem_allocate_memory(strlen(format), J9MEM_CATEGORY_VM);
	if (NULL == error->errorMessage) {
		return (jvmtiTst_Error *) 0x2;	 // TODO: fix this to return a static out of mem struct
	}

	strcpy((char *) error->errorMessage, format);

	va_start(args, format);
	COPY_VA_LIST(error->errorMessageFormat, args);
	va_end(args);
	
	
	return error;
}
        

void
jvmtiTst_ErrorPrint(jvmtiTst_Error *error)
{
	j9tty_vprintf((const char *) error->errorMessage, error->errorMessageFormat);
	j9tty_printf(PORTLIB, "\n");
}


jint
testGetConstantPool (JavaVM * vm, jvmtiEnv * jvmti_env, char *args)
{
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	jvmtiError err;                                

	if (!jvmti11) {
		j9tty_printf(PORTLIB, "Test requires JVMTI 1.1\n");
		return JNI_ERR;
	}

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_get_constant_pool = 1;
	capabilities.can_get_bytecodes = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to add capabilities"));
	}						
		 
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.ClassLoad = testGetConstantPool_classLoad;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to set callbacks"));
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_CLASS_LOAD, NULL);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to enable class load event notification"));
	}

	return JNI_OK;
}


/** 
 * \brief	classLoad handler used to intercept loading of the 'testGetConstantPool' test class
 * \ingroup	GetConstantPool
 * 
 * @param jvmti_env 
 * @param jni_env 
 * @param thread 
 * @param klass 
 * @return 
 * 
 */
static void JNICALL
testGetConstantPool_classLoad(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jclass klass)
{
	jvmtiError err;
	
	jint i;
	jint constant_pool_count_ptr;
	jint constant_pool_byte_count_ptr;
	unsigned char* constant_pool_bytes_ptr;
	jint methodCount;
	jmethodID * methods = NULL;
	J9UTF8 *className;
	testClassCPDescriptor cpDescriptor;

	jvmtiTst_Error *error;

	/* Ignore all class load events except testGetConstantPool */
	className = J9ROMCLASS_CLASSNAME((*(J9Class **) klass)->romClass);
	if (strncmp((const char *) className->data, "testGetConstantPool", className->length)) {
		return;
	}


	err = (*jvmti_env)->GetConstantPool(jvmti_env, klass, &constant_pool_count_ptr, &constant_pool_byte_count_ptr, &constant_pool_bytes_ptr);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to obtain constant pool."));
	}

	j9tty_printf(PORTLIB, "\t\t\tConstant Pool [items: %d  bytes: %d]:\n" , constant_pool_count_ptr, constant_pool_byte_count_ptr);
	/*
	   for (i = 0; i < constant_pool_byte_count_ptr; ++i) {
	   j9tty_printf(PORTLIB, " %02X", constant_pool_bytes_ptr[i]);
	   }
	*/

	cpDescriptor.constantPool = j9mem_allocate_memory(constant_pool_count_ptr * sizeof(J9CfrConstantPoolInfo), J9MEM_CATEGORY_VM);
	cpDescriptor.constantPoolCount = constant_pool_count_ptr;
	cpDescriptor.utf8Buf = j9mem_allocate_memory(constant_pool_byte_count_ptr, J9MEM_CATEGORY_VM);

	
	/* Check whether the constant pool is parsable */
	if (testGetConstantPool_readPool(&cpDescriptor, constant_pool_bytes_ptr, constant_pool_byte_count_ptr) != 0)
		return;

	/* Check whether the constant pool is logically valid */
	if (testGetConstantPool_checkPool(&cpDescriptor, constant_pool_bytes_ptr) != 0)
		return;

	/* Check the <clinit> method for specific constant initialization bytecodes */
	err = (*jvmti_env)->GetClassMethods(jvmti_env, klass, &methodCount, &methods);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "\t\t%s\n", errorName(jvmti_env, thread, err, "!!! Failed to get methods"));
	} else {
		jint i; 

		for (i = 0; i < methodCount; ++i) {
			J9UTF8 *romMethodName;
			jmethodID method = methods[i];
			jlocation start;
			jlocation end;
			jint size;
			jint bcIndex;
			jboolean isNative;
			unsigned char * bytecodes;

			err = (*jvmti_env)->GetMethodLocation(jvmti_env, method, &start, &end);
			if (err != JVMTI_ERROR_NONE) {
				j9tty_printf(PORTLIB, "\t\t\t%s\n", errorName(jvmti_env, thread, err, "!!! Failed to get location"));
				goto done;
			}

			if (start == -1 || end == -1)
				continue;

			err = (*jvmti_env)->IsMethodNative(jvmti_env, method, &isNative);
			if (err != JVMTI_ERROR_NONE) {
				j9tty_printf(PORTLIB, "\t%s\n", errorName(jvmti_env, thread, err, "!!! Failed to get nativeness"));
				goto done;
			} 

			if (isNative) {
				continue;
			}

			romMethodName = J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(((J9JNIMethodID *) method)->method));
			if (strncmp((const char *) romMethodName->data, "<clinit>", romMethodName->length)) {
				continue;
			}


			err = (*jvmti_env)->GetBytecodes(jvmti_env, method, &size, &bytecodes);
			if (err != JVMTI_ERROR_NONE) {
				j9tty_printf(PORTLIB, "\t\t\t%s\n", errorName(jvmti_env, thread, err, "!!! Failed to get bytecodes"));
				goto done;
			} 


			error = testGetConstantPool_checkStaticConstants(&cpDescriptor, bytecodes, size);
			if (error) {
				jvmtiTst_ErrorPrint(error);
				goto done;
			}


			j9tty_printf(PORTLIB, "\t\t\tPC range: %d...%d\n", (int) start, (int) end);
			j9tty_printf(PORTLIB, "\t\t\tBytecodes [%d]:", size);
			for (bcIndex = 0; bcIndex < size; ++bcIndex) {
				j9tty_printf(PORTLIB, " %02X", bytecodes[bcIndex]);
			}
			j9tty_printf(PORTLIB, "\n");

			maciek_dumpBytecodes(PORTLIB, bytecodes, (int) start, (int) end, BCT_BigEndianOutput);
			
			(*jvmti_env)->Deallocate(jvmti_env, bytecodes);
		}

	}

done:
	if (NULL != constant_pool_bytes_ptr) {
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) constant_pool_bytes_ptr);
	}

}



static J9CfrConstantPoolInfo *
getCPInfo(testClassCPDescriptor *cpDescriptor, U_16 cpIndex)
{
	J9CfrConstantPoolInfo *info;

	if (cpIndex > cpDescriptor->constantPoolCount) {
		return NULL;
	}

	return &cpDescriptor->constantPool[cpIndex];
}


static jvmtiTst_Error *
testGetConstantPool_checkStaticConstants(testClassCPDescriptor *cpDescriptor, U_8 *bytecodes, U_32 size)
{
	jvmtiTst_Error *error;
	J9CfrConstantPoolInfo *info, *info1, *info2;
	U_8 *index = bytecodes;
	UDATA bytecodeSize;
	U_8 bc;
	U_8 u8;
	U_16 u16;

	/* Expect a load of a Double constant */
	bc = *index++;
	if (bc != JBldc2lw) {
		return jvmtiTst_ErrorCreate("Expected a ldc2lw bytecode. Got BC [%d]", bc);
	}
	NEXT_U16(u16, index);
	error = testGetConstantPool_checkStaticConstants_referedDoubleLong(cpDescriptor, CFR_CONSTANT_Double, u16, 0xc28f5c29, 0x3ff028f5);
	if (NULL != error)
		return error;


	/* Expect a putstatic of the loaded constant */
	bc = *index++;
	if (bc != JBputstatic) {
		return jvmtiTst_ErrorCreate("Expected a putstatic bytecode. Got BC [%d]", bc);
	}
	NEXT_U16(u16, index);
	error = testGetConstantPool_checkStaticConstants_referedReference(cpDescriptor, u16, CFR_CONSTANT_Fieldref, "testGetConstantPool", "testDouble1", "D");
	if (NULL != error)
		return error;


 	/* Expect a load of a Double constant */
	bc = *index++;
	if (bc != JBldc2lw) {
		return jvmtiTst_ErrorCreate("Expected a ldc2lw bytecode. Got BC [%d]", bc);
	}
	NEXT_U16(u16, index);                                                                                                              
	error = testGetConstantPool_checkStaticConstants_referedDoubleLong(cpDescriptor, CFR_CONSTANT_Double, u16, 0xe147ae14, 0x4000147a);
	if (NULL != error)
		return error;


	/* Expect a putstatic of the loaded constant */
	bc = *index++;
	if (bc != JBputstatic) {
		return jvmtiTst_ErrorCreate("Expected a putstatic bytecode. Got BC [%d]", bc);
	}
	NEXT_U16(u16, index);
	error = testGetConstantPool_checkStaticConstants_referedReference(cpDescriptor, u16, CFR_CONSTANT_Fieldref, "testGetConstantPool", "testDouble2", "D");
	if (NULL != error)
		return error;
 

        /* Expect a BIPUSH #20 */
 	bc = *index++;
	if (bc != JBbipush) {
		return jvmtiTst_ErrorCreate("Expected a bipush bytecode. Got BC [%d]", bc);
	}
	NEXT_U8(u8, index);

	if (u8 != 20) {
                return jvmtiTst_ErrorCreate("Expected a bipush bytecode that pushes a value of 20. Got [%s]", u8);
	}
	
        /* Expect a putstatic into a field reference */
        NEXT_U8(bc, index);
	if (bc != JBputstatic) {
		return jvmtiTst_ErrorCreate("Expected a putstatic bytecode. Got BC [%d]", bc);
	}
 	NEXT_U16(u16, index);
	error = testGetConstantPool_checkStaticConstants_referedReference(cpDescriptor, u16, CFR_CONSTANT_Fieldref, "testGetConstantPool", "testInt", "I");
	if (NULL != error)
		return error;
 
	/* Expect a ldc of a String */
	NEXT_U8(bc, index);
	if (bc != JBldc) {
		return jvmtiTst_ErrorCreate("Expected a ldc bytecode. Got BC [%d]", bc);
	}
	NEXT_U8(u8, index);
	error = testGetConstantPool_checkStaticConstants_referedString(cpDescriptor, (U_16) u8, "testStringContent");
	if (NULL != error)
		return error;

 
        /* Expect a putstatic into a field reference */
        NEXT_U8(bc, index);
	if (bc != JBputstatic) {
		return jvmtiTst_ErrorCreate("Expected a putstatic bytecode. Got BC [%d]", bc);
	}
 	NEXT_U16(u16, index);
	error = testGetConstantPool_checkStaticConstants_referedReference(cpDescriptor, u16, CFR_CONSTANT_Fieldref, "testGetConstantPool", "testString", "Ljava/lang/String;");
	if (NULL != error)
		return error;
 

	return NULL;	
}

       	     


static jvmtiTst_Error *
testGetConstantPool_checkStaticConstants_referedClass(testClassCPDescriptor *cpDescriptor, 
						      U_16 classCpIndex,
						      const char *expectedClassName)
{
	jvmtiTst_Error *error;
	J9CfrConstantPoolInfo *info;
	
	info = getCPInfo(cpDescriptor, classCpIndex);
	if (NULL == info) {
		return jvmtiTst_ErrorCreate("Invalid Constant Pool index [%d]. Expected a Class constant.", classCpIndex);
	}

	if (info->tag != CFR_CONSTANT_Class) {
		return jvmtiTst_ErrorCreate("Expected a Class constant type. Got type [%d]", info->tag);
	}

	error = testGetConstantPool_checkStaticConstants_referedUtf8(cpDescriptor, info->slot1, expectedClassName);
		
	return error;
}
	
static jvmtiTst_Error *
testGetConstantPool_checkStaticConstants_referedString(testClassCPDescriptor *cpDescriptor, 
						      U_16 stringCpIndex,
						      const char *expectedString)
{
	jvmtiTst_Error *error;
	J9CfrConstantPoolInfo *info;
	
	info = getCPInfo(cpDescriptor, stringCpIndex);
	if (NULL == info) {
		return jvmtiTst_ErrorCreate("Invalid Constant Pool index [%d]. Expected a String constant.", stringCpIndex);
	}

	if (info->tag != CFR_CONSTANT_String) {
		return jvmtiTst_ErrorCreate("Expected a String constant type. Got type [%d]", info->tag);
	}

	error = testGetConstantPool_checkStaticConstants_referedUtf8(cpDescriptor, info->slot1, expectedString);
		
	return error;
}
 


static jvmtiTst_Error *
testGetConstantPool_checkStaticConstants_referedReference(testClassCPDescriptor *cpDescriptor, 
							  U_16 refCpIndex,
							  U_8 expectedReferenceType,
							  const char *expectedClassName,
							  const char *expectedName,
							  const char *expectedSignature)
{
	jvmtiTst_Error *error;
	J9CfrConstantPoolInfo *info;

	info = getCPInfo(cpDescriptor, refCpIndex);
	if (NULL == info) {
		return jvmtiTst_ErrorCreate("Invalid Constant Pool index [%d]. Expected a Reference constant.", refCpIndex);
	}

	if (info->tag != expectedReferenceType) {
		return jvmtiTst_ErrorCreate("Expected a Reference constant of type [%d]. Got type [%d]", expectedReferenceType, info->tag);
	}


	error = testGetConstantPool_checkStaticConstants_referedClass(cpDescriptor, info->slot1, expectedClassName);
	if (NULL != error) {
		return error;
	}

	error = testGetConstantPool_checkStaticConstants_referedNameAndSig(cpDescriptor, info->slot2, expectedName, expectedSignature);
	if (NULL != error) {
		return error;
	}

	return NULL;
}



static jvmtiTst_Error *
testGetConstantPool_checkStaticConstants_referedNameAndSig(testClassCPDescriptor *cpDescriptor, 
							   U_16 nasCpIndex,
							   const char *expectedName,
							   const char *expectedSignature)
{
	jvmtiTst_Error *error;
	J9CfrConstantPoolInfo *info;

	info = getCPInfo(cpDescriptor, nasCpIndex);
	if (NULL == info) {
		return jvmtiTst_ErrorCreate("Invalid Constant Pool index [%d]. Expected a NameAndType constant.", nasCpIndex);
	}
			
	error = testGetConstantPool_checkStaticConstants_referedUtf8(cpDescriptor, info->slot1, expectedName);
	if (NULL != error) {
		return error;
	}

	error = testGetConstantPool_checkStaticConstants_referedUtf8(cpDescriptor, info->slot2, expectedSignature);
	if (NULL != error) {
		return error;
	}

	return NULL;
}

static jvmtiTst_Error *
testGetConstantPool_checkStaticConstants_referedUtf8(testClassCPDescriptor *cpDescriptor, 
						     U_16 utf8CpIndex,
						     const char *expectedString)
{
	J9CfrConstantPoolInfo *info;
	
	info = getCPInfo(cpDescriptor, utf8CpIndex);
	if (NULL == info) {
		return jvmtiTst_ErrorCreate("Invalid Constant Pool index [%d]. Expected a UTF8 constant.", utf8CpIndex);
	}

	if (info->tag != CFR_CONSTANT_Utf8) {
		return jvmtiTst_ErrorCreate("Expected a UTF8 constant type. Got type [%d]", info->tag);
	}

	if (strcmp((const char *) info->bytes, (const char *) expectedString)) { // info->slot1)) {
		return jvmtiTst_ErrorCreate("Expected a UTF8 containing [%s]. Got [%*s]", expectedString, info->slot1, info->bytes);
	}

	return NULL;
}
																					   
static jvmtiTst_Error *
testGetConstantPool_checkStaticConstants_referedDoubleLong(testClassCPDescriptor *cpDescriptor, 
							   U_8  cpItemType,
							   U_16 cpIndex,
							   U_32 expectedValue_hi,
							   U_32 expectedValue_lo)
{
	J9CfrConstantPoolInfo *info;
	
	info = getCPInfo(cpDescriptor, cpIndex);
	if (NULL == info) {
		return jvmtiTst_ErrorCreate("Invalid Constant Pool index [%d]. Expected a %s constant.", cpIndex, 
					    (cpItemType == CFR_CONSTANT_Double) ? "Double" : "Long");
	}

	if (info->tag != cpItemType) {
		return jvmtiTst_ErrorCreate("Expected a %s constant type. Got type [%d]", 
					    (cpItemType == CFR_CONSTANT_Double) ? "Double" : "Long", info->tag);
	}

#ifdef J9VM_ENV_LITTLE_ENDIAN  
	if (info->slot1 != expectedValue_hi || info->slot2 != expectedValue_lo) {
		return jvmtiTst_ErrorCreate("Expected a double value with slot1 [0x%08x] slot2 [0x%08x]. Got slot1: [0x%08x] slot2: [0x%08x]", 
					    expectedValue_hi, expectedValue_lo,
					    info->slot1, info->slot2);
	}
#else
	if (info->slot1 != expectedValue_lo || info->slot2 != expectedValue_hi) {
		return jvmtiTst_ErrorCreate("Expected a double value with slot1 [0x%08x] slot2 [0x%08x]. Got slot1: [0x%08x] slot2: [0x%08x]", 
					    expectedValue_lo, expectedValue_hi,
					    info->slot2, info->slot1);
	}
#endif	

	return NULL;
}





static I_32 
testGetConstantPool_readPool(testClassCPDescriptor *cpDescriptor, 
			     U_8* constantPoolBuf,
			     UDATA constantPoolBufSize)
{
	U_8* dataEnd = constantPoolBuf + constantPoolBufSize;
	U_8* index = constantPoolBuf;
	U_8* freePointer = cpDescriptor->utf8Buf;
	J9CfrConstantPoolInfo* info;
	U_32 size, errorCode, offset;
	U_32 i;

	/* Explicitly zero the null entry */
	info = &(cpDescriptor->constantPool[0]);
	info->tag = CFR_CONSTANT_Null;
	info->flags1 = info->flags2 = info->flags3 = 0;
	info->slot1 = 0;
	info->slot2 = 0;
	info->bytes = 0;
	info->romAddress = 0;

	/* Read each entry. */
	for (i = 1; i < cpDescriptor->constantPoolCount;) {
//		printf("INDEX: 0x%x\n", i);
		info = &(cpDescriptor->constantPool[i]);
		CHECK_FOR_EOF(1);
		NEXT_U8(info->tag, index);
		info->flags1 = info->flags2 = info->flags3 = 0;
		info->romAddress = 0;
		switch (info->tag) {
		case CFR_CONSTANT_Utf8:
			CHECK_FOR_EOF(2);
			NEXT_U16(size, index);
			info->slot2 = 0;

			info->bytes = freePointer;
			freePointer += size + 1;
			
			CHECK_FOR_EOF(size);
			info->slot1 = (U_32) testGetConstantPool_verifyCanonisizeAndCopyUTF8(info->bytes, index, size);
			if ((I_32)info->slot1 < 0) {
				errorCode = J9NLS_CFR_ERR_BAD_UTF8__ID;
				offset = (U_32) (index - 1);
				goto _errorFound;
			}
			info->bytes[info->slot1] = (U_8) '\0';
//			printf("	utf8: [%*s]\n", size, info->bytes);
			/* correct for compression */
			freePointer -= (size - info->slot1);
			index += size;
			i++;
			break;

		case CFR_CONSTANT_Integer:
		case CFR_CONSTANT_Float:
			CHECK_FOR_EOF(4);
			NEXT_U32(info->slot1, index);
//			printf("	int/float: 0x%x\n", info->slot1);
			info->slot2 = 0;
			i++;
			break;
	
		case CFR_CONSTANT_Long:
		case CFR_CONSTANT_Double:
			CHECK_FOR_EOF(8);
#ifdef J9VM_ENV_LITTLE_ENDIAN
			NEXT_U32(info->slot2, index);
			NEXT_U32(info->slot1, index);
#else
			NEXT_U32(info->slot1, index);
			NEXT_U32(info->slot2, index);
#endif
			i++;
			cpDescriptor->constantPool[i].tag = CFR_CONSTANT_Null;
			i++;
//			printf("        Long/Double: 0x%x%x\n", info->slot1, info->slot2);
			if (i > cpDescriptor->constantPoolCount) {
				/* Incomplete long or double constant. This means that n+1 is out of range. */
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				offset = (U_32) (index - 1);
				goto _errorFound;
			}
			break;

		case CFR_CONSTANT_Class:
		case CFR_CONSTANT_String:
			CHECK_FOR_EOF(2);
			NEXT_U16(info->slot1, index);
//			printf("        Class/String: 0x%x\n", info->slot1);
			info->slot2 = 0;
			i++;
			break;

		case CFR_CONSTANT_Fieldref:
		case CFR_CONSTANT_Methodref:
		case CFR_CONSTANT_InterfaceMethodref:
		case CFR_CONSTANT_NameAndType:
			CHECK_FOR_EOF(4);
			NEXT_U16(info->slot1, index);
			NEXT_U16(info->slot2, index);
//			printf("        Ref: 0x%04x 0x%04x\n", info->slot1, info->slot2);
			i++;
			break;

		default:
			errorCode = J9NLS_CFR_ERR_UNKNOWN_CONSTANT__ID;
			offset = (U_32) (index - 1);
			goto _errorFound;
		}
	}	
	return 0;

_errorFound:
	
	return -1;
}

static I_32 
testGetConstantPool_checkPool(testClassCPDescriptor *cpDescriptor, U_8* constantPoolBuf)
{
	J9CfrConstantPoolInfo* info;
	J9CfrConstantPoolInfo* utf8; 
	J9CfrConstantPoolInfo* cpBase; 
	U_32 count, i, errorCode;
	U_8* index;

	cpBase = cpDescriptor->constantPool;
	count = cpDescriptor->constantPoolCount;
	index = constantPoolBuf;

	info = &cpBase[1];

	for (i = 1; i < count; i++, info++) {

		switch (info->tag) {

		case CFR_CONSTANT_Utf8:
			/* Check format? */
			index += info->slot1 + 3;
			break;

		case CFR_CONSTANT_Integer:
		case CFR_CONSTANT_Float:
			index += 5;
			break;

		case CFR_CONSTANT_Long:
		case CFR_CONSTANT_Double:
			index += 9;
			i++;
			info++;
			break;

		case CFR_CONSTANT_Class:
			/* Must be a UTF8. */
			if ((!(info->slot1)) || (info->slot1 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}

			utf8 = &cpBase[info->slot1];
			if (utf8->tag != CFR_CONSTANT_Utf8) {
				errorCode = J9NLS_CFR_ERR_BAD_NAME_INDEX__ID;
				goto _errorFound;
			}

			index += 3;
			break;
		
		case CFR_CONSTANT_String:
			/* Must be a UTF8. */
			if ((!(info->slot1)) || (info->slot1 > count)) {
				errorCode = CFR_ERR_BAD_INDEX;
				goto _errorFound;
			}
			if (cpBase[info->slot1].tag != CFR_CONSTANT_Utf8) {
				errorCode = CFR_ERR_BAD_STRING_INDEX;
				goto _errorFound;
			}					
			index += 3;
			break;

		case CFR_CONSTANT_Fieldref:
		case CFR_CONSTANT_Methodref:
		case CFR_CONSTANT_InterfaceMethodref:
			if (!(info->slot1) || (info->slot1 > count)) {
				errorCode = CFR_ERR_BAD_INDEX;
				goto _errorFound;
			}
			if (cpBase[info->slot1].tag != CFR_CONSTANT_Class) {
				errorCode = CFR_ERR_BAD_CLASS_INDEX;
				goto _errorFound;
			}					
					
			if (!(info->slot1) || (info->slot2 > count)) {
				errorCode = CFR_ERR_BAD_INDEX;
				goto _errorFound;
			}
			if (cpBase[info->slot2].tag != CFR_CONSTANT_NameAndType) {
				errorCode = CFR_ERR_BAD_NAME_AND_TYPE_INDEX;
				goto _errorFound;
			}					
			index += 5;
			break;

		case CFR_CONSTANT_NameAndType:
			if ((!(info->slot1)) || (info->slot1 > count)) {
				errorCode = CFR_ERR_BAD_INDEX;
				goto _errorFound;
			}
			if (cpBase[info->slot1].tag != CFR_CONSTANT_Utf8) {
				errorCode = CFR_ERR_BAD_NAME_INDEX;
				goto _errorFound;
			}					
					
			if ((!(info->slot2)) || (info->slot2 > count)) {
				errorCode = CFR_ERR_BAD_INDEX;
				goto _errorFound;
			}
			if (cpBase[info->slot2].tag != CFR_CONSTANT_Utf8) {
				errorCode = CFR_ERR_BAD_DESCRIPTOR_INDEX;
				goto _errorFound;
			}					
			index += 5;
			break;

		default:
			errorCode = CFR_ERR_UNKNOWN_CONSTANT;
			goto _errorFound;
		}
	}

	return 0;

_errorFound:

//	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, (UDATA) (index - poolStart + 10));
	return -1;
}


static I_32 
testGetConstantPool_verifyCanonisizeAndCopyUTF8 (U_8 *dest, U_8 *source, U_32 length)
{
	U_8 *originalDest = dest;
	U_8 *originalSource = source;
	U_8 *sourceEnd = source + length;
	UDATA firstByte, nextByte, outWord, charLength, compression = 0;
	I_32 result = 0;

	/* Assumes success */
	memcpy (dest, source, length);

	while (source != sourceEnd) {

		/* Handle multibyte */
		if (((UDATA) ((*source++) - 1)) < 0x7F) {
			continue;
		}

		firstByte = source[-1];
		if (firstByte == 0) {
			return -1;
		}

		/* if multi-byte then first byte must be 11xxxxxx and more to come */
		if (((firstByte & 0x40) == 0) || (source == sourceEnd)) {
			return -1;
		}

		charLength = 2;

		nextByte = *source++;
		if ((nextByte & 0xC0) != 0x80) {
			return -1;
		}

		/* Assemble first two bytes */
		/* 0x1F mask is okay for 3-byte codes as legal ones have a 0 bit at 0x10 */
		outWord = ((firstByte & 0x1F) << 6) | (nextByte & 0x3F);

		if ((firstByte & 0x20) != 0) {
			/* three bytes */
			if (((firstByte & 0x10) != 0) || (source == sourceEnd)) {
				return -1;
			}

			charLength = 3;

			nextByte = *source++;
			if ((nextByte & 0xC0) != 0x80) {
				return -1;
			}

			/* Add the third byte */
			outWord = (outWord << 6) | (nextByte & 0x3F);
		}

		/* Overwrite the multibyte UTF8 character only if shorter */
		if ((outWord != 0) && (outWord < 0x80)) {
			/* One byte must be shorter in here */
			dest = originalDest + (source - originalSource - charLength - compression);
			*dest++ = (U_8) outWord;
			compression += charLength - 1;
			memcpy (dest, source, (UDATA) (sourceEnd - source));

		} else if ((outWord < 0x800) && (charLength == 3)) {
			dest = originalDest + (source - originalSource - charLength - compression);
			*dest++ = (U_8) ((outWord >> 6) | 0xC0);
			*dest++ = (U_8) ((outWord & 0x3F) | 0x80);
			compression++;
			memcpy (dest, source, (UDATA) (sourceEnd - source));
		}
	}

	result = (I_32) (length - compression);

	return result;
}

#endif
