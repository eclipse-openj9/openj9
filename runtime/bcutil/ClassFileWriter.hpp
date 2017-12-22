/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
/*
 * ClassFileWriter.hpp
 */

#ifndef CLASSFILEWRITER_HPP_
#define CLASSFILEWRITER_HPP_

#include "cfr.h"
#include "hashtable_api.h"
#include "j9comp.h"
#include "j9port.h"
#include "ut_j9bcu.h"
#include "util_api.h"

#include "BuildResult.hpp"

class ClassFileWriter {
/*
 * Data members
 */
private:
	/*
	 * Hash table assumption: everything that needs to be written to the .class file exists somewhere in:
	 * 1) the ROM class; or
	 * 2) program constants.
	 *
	 * One possible violation of this assumption is that primitive array names are encoded
	 * directly in stack map table's Object variable info entries rather than as UTF8s.
	 */
	struct HashTableEntry {
		void * address;
		U_16 cpIndex;
		U_8 cpType;

		HashTableEntry(void * addr, U_16 index, U_8 type)
			: address(addr)
			, cpIndex(index)
			, cpType(type)
		{}
	};

	J9JavaVM * _javaVM;
	J9PortLibrary * _portLibrary;
	J9ROMClass * _romClass;
	U_8 * _classFileBuffer;
	U_8 * _classFileCursor;
	BuildResult _buildResult;
	J9HashTable * _cpHashTable;
	U_16 _constantPoolCount;
	U_32 _bsmAttributeLength;
	UDATA _classFileSize;
	bool _isAnon;
	J9UTF8* _anonClassName;
	J9UTF8* _originalClassName;
	
protected:

public:

/*
 * Function members
 */
private:

	static UDATA hashFunction(void * entry, void * userData)
	{
		/* TODO figure out if this is a good hash function - a lot of UTF-8 data may be spread around the address space */
		HashTableEntry * e = (HashTableEntry *) entry;
		switch (e->cpType) {
		case CFR_CONSTANT_Double: /* fall through */
		case CFR_CONSTANT_Long: {
			U_32 * address = (U_32 *) e->address;
			return address[0] ^ address[1] ^ UDATA(e->cpType);
		}
		case CFR_CONSTANT_Integer: /* fall through */
		case CFR_CONSTANT_Float:
			return (*(U_32 *) e->address) ^ UDATA(e->cpType);
		case CFR_CONSTANT_Class:
		case CFR_CONSTANT_Utf8: {
			J9UTF8 *utf8 = (J9UTF8 *) e->address;
			return computeHashForUTF8(J9UTF8_DATA(utf8), J9UTF8_LENGTH(utf8));
		}
		default:
			/* Mix cpType into the 4th byte of the address, which is not likely to vary much within the ROM class */
			return UDATA(e->address) ^ (UDATA(e->cpType) << 24);
		}
	}

	static UDATA equalFunction(void * leftEntry, void * rightEntry, void * userData)
	{
		HashTableEntry * left = (HashTableEntry *) leftEntry;
		HashTableEntry * right = (HashTableEntry *) rightEntry;

		if (left->cpType != right->cpType) {
			return 0;
		}

		switch (left->cpType) {
		case CFR_CONSTANT_Double: /* fall through */
		case CFR_CONSTANT_Long:
			return (*(U_64 *) left->address) == (*(U_64 *) right->address);
		case CFR_CONSTANT_Integer: /* fall through */
		case CFR_CONSTANT_Float:
			return (*(U_32 *) left->address) == (*(U_32 *) right->address);
		case CFR_CONSTANT_Class:
		case CFR_CONSTANT_Utf8: {
			J9UTF8 *utf81 = (J9UTF8 *) left->address;
			J9UTF8 *utf82 = (J9UTF8 *) right->address;
			return J9UTF8_EQUALS(utf81, utf82);
		}
		default:
			return left->address == right->address;
		}
	}

	/* canFail is passed 'true' only when recreating StackMapTable attribute. See writeVerificationTypeInfo() */
	U_16 indexForType(void * address, U_8 cpType, bool canFail = false)
	{
		HashTableEntry entry(address, 0, cpType);
		HashTableEntry * result = (HashTableEntry *) hashTableFind(_cpHashTable, &entry);
		if (NULL != result) {
			return result->cpIndex;
		}
		if (!canFail) {
			_buildResult = GenericError;
			Trc_BCU_Assert_ShouldNeverHappen();
		}
		return 0;
	}

	U_16 indexForUTF8(J9UTF8 * utf8) { return indexForType(utf8, CFR_CONSTANT_Utf8); }
	U_16 indexForClass(J9UTF8 * utf8, bool canFail = false) { return indexForType(utf8, CFR_CONSTANT_Class, canFail); }
	U_16 indexForDouble(U_32 * val) { return indexForType(val, CFR_CONSTANT_Double); }
	U_16 indexForLong(U_32 * val) { return indexForType(val, CFR_CONSTANT_Long); }
	U_16 indexForFloat(U_32 * val) { return indexForType(val, CFR_CONSTANT_Float); }
	U_16 indexForInteger(U_32 * val) { return indexForType(val, CFR_CONSTANT_Integer); }
	U_16 indexForNAS(J9ROMNameAndSignature * nas) { return indexForType(nas, CFR_CONSTANT_NameAndType); }
	U_16 indexForInvokeDynamic(void * val) { return indexForType(val, CFR_CONSTANT_InvokeDynamic); }

	void addEntry(void * address, U_16 cpIndex, U_8 cpType)
	{
		HashTableEntry entry(address, cpIndex, cpType);
		HashTableEntry * result = (HashTableEntry *) hashTableFind(_cpHashTable, &entry);
		if (NULL == result) {
			result = (HashTableEntry *) hashTableAdd(_cpHashTable, &entry);
			if (NULL == result) {
				_buildResult = OutOfMemory;
			}
		} else if ((0 == result->cpIndex) && (0 != cpIndex)) {
			/* Update the existing entry to record the cpIndex - only expect this to happen for Annotation UTF8 entries in the ROM constant pool */
			Trc_BCU_Assert_True(CFR_CONSTANT_Utf8 == cpType);
			result->cpIndex = cpIndex;
		}
	}

	void addClassEntry(J9UTF8 * utf8, U_16 cpIndex)
	{
		addEntry(utf8, 0, CFR_CONSTANT_Utf8);
		addEntry(utf8, cpIndex, CFR_CONSTANT_Class);
	}

	void addNASEntry(J9ROMNameAndSignature * nas)
	{
		addEntry(J9ROMNAMEANDSIGNATURE_NAME(nas), 0,CFR_CONSTANT_Utf8);
		addEntry(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas), 0, CFR_CONSTANT_Utf8);
		addEntry(nas, 0, CFR_CONSTANT_NameAndType);
	}

#ifdef J9VM_ENV_LITTLE_ENDIAN
	void flip16bit(U_8 * code)
	{
		U_8 tmp = code[0];
		code[0] = code[1];
		code[1] = tmp;
	}

	void flip32bit(U_8 * code)
	{
		U_8 tmp = code[0];
		code[0] = code[3];
		code[3] = tmp;
		tmp = code[1];
		code[1] = code[2];
		code[2] = tmp;
	}
#else /* J9VM_ENV_LITTLE_ENDIAN */
	void flip16bit(U_8 * code) { /* nothing to do */ }
	void flip32bit(U_8 * code) { /* nothing to do */ }
#endif /* J9VM_ENV_LITTLE_ENDIAN */

	void allocateBuffer()
	{
		PORT_ACCESS_FROM_PORT(_portLibrary);

		_classFileBuffer = (U_8 *) j9mem_allocate_memory(_romClass->classFileSize, OMRMEM_CATEGORY_VM);
		if (NULL == _classFileBuffer) {
			_buildResult = OutOfMemory;
		}
		_classFileCursor = _classFileBuffer;
	}

	void writeU8(U_8 val)
	{
		*_classFileCursor = val;
		_classFileCursor += sizeof(U_8);
	}

	void writeU16(U_16 val)
	{
		U_16 * u16Addr = (U_16 *) _classFileCursor;
#ifdef J9VM_ENV_LITTLE_ENDIAN
		*u16Addr = ((val & 0xff00) >> 8) | ((val & 0x00ff) << 8);
#else /* J9VM_ENV_LITTLE_ENDIAN */
		*u16Addr = val;
#endif /* J9VM_ENV_LITTLE_ENDIAN */
		_classFileCursor += sizeof(U_16);
	}

	void writeU32(U_32 val)
	{
		U_32 * u32Addr = (U_32 *) _classFileCursor;
#ifdef J9VM_ENV_LITTLE_ENDIAN
		*u32Addr = ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24);
#else /* J9VM_ENV_LITTLE_ENDIAN */
		*u32Addr = val;
#endif /* J9VM_ENV_LITTLE_ENDIAN */
		_classFileCursor += sizeof(U_32);
	}

	void writeU8At(U_8 val, U_8 * address)
	{
		*address = val;
	}

	void writeU16At(U_16 val, U_8 * address)
	{
		U_16 * u16Addr = (U_16 *) address;
#ifdef J9VM_ENV_LITTLE_ENDIAN
		*u16Addr = ((val & 0xff00) >> 8) | ((val & 0x00ff) << 8);
#else /* J9VM_ENV_LITTLE_ENDIAN */
		*u16Addr = val;
#endif /* J9VM_ENV_LITTLE_ENDIAN */
	}

	void writeU32At(U_32 val, U_8 * address)
	{
		U_32 * u32Addr = (U_32 *) address;
#ifdef J9VM_ENV_LITTLE_ENDIAN
		*u32Addr = ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24);
#else /* J9VM_ENV_LITTLE_ENDIAN */
		*u32Addr = val;
#endif /* J9VM_ENV_LITTLE_ENDIAN */
	}

	void writeData(U_32 length, void * bytes)
	{
		memcpy(_classFileCursor, bytes, length);
		_classFileCursor += length;
	}

	/*
	 * Walk the ROM class, adding entries to the _cpHashTable for all referenced UTF8s & NASs, and all J9CPTYPE_INT, J9CPTYPE_LONG, J9CPTYPE_DOUBLE CP entries.
	 * If the hashtable or a hashtable entry can't be allocated, returns with _buildResult = OutOfMemory.
	 * If a class file CP entry can't be found, returns with _buildResult = GenericError.
	 */
	void analyzeROMClass();
	void analyzeConstantPool();
	void analyzeInterfaces();
	void analyzeFields();
	void analyzeMethods();

	void writeClassFile();
	void writeConstantPool();
	void writeInterfaces();
	void writeField(J9ROMFieldShape * fieldShape);
	void writeFields();
	void writeMethod(J9ROMMethod * method);
	void writeMethods();
	void writeAttributes();
	void writeCodeAttribute(J9ROMMethod * method);
	void writeVerificationTypeInfo(U_16 count, U_8 ** typeInfo);
	void writeStackMapTableAttribute(J9ROMMethod * romMethod);
	void writeSignatureAttribute(J9UTF8 * genericSignature);
	void writeAnnotationElement(U_8 **annotationElement);
	void writeAnnotation(U_8 **annotation);
	void writeAnnotationsAttribute(U_32 * annotationsData);
	void writeParameterAnnotationsAttribute(U_32 *parameterAnnotationsData);
	void writeTypeAnnotationsAttribute(U_32 *typeAnnotationsData);
	void writeAnnotationDefaultAttribute(U_32 *annotationsDefaultData);
	void writeAttributeHeader(J9UTF8 * name, U_32 length);

	/* Similar to ClassFileOracle::computeSendSlotCount().
	 * This is used to get the argument count for the method called using invokeinterface bytecode.
	 */
	U_8 computeArgsCount(U_16 methodRefIndex);
	void rewriteBytecode(J9ROMMethod * method, U_32 length, U_8 * code);

protected:

public:
	ClassFileWriter(J9JavaVM * javaVM, J9PortLibrary * portLibrary, J9ROMClass * romClass)
		: _javaVM(javaVM)
		, _portLibrary(portLibrary)
		, _romClass(romClass)
		, _classFileBuffer(NULL)
		, _classFileCursor(NULL)
		, _buildResult(OK)
		, _cpHashTable(NULL)
		, _constantPoolCount(romClass->romConstantPoolCount)
		, _bsmAttributeLength(0)
		, _classFileSize(0)
		, _isAnon(FALSE)
		, _anonClassName(NULL)
		, _originalClassName(NULL)
	{
		/* anonClasses have the following name format: '[originalName]/[ROMSegmentAddress]' */
		if (J9_ARE_ALL_BITS_SET(_romClass->extraModifiers, J9AccClassAnonClass)) {
			PORT_ACCESS_FROM_JAVAVM(_javaVM);
			_isAnon = true;
			_anonClassName = J9ROMCLASS_CLASSNAME(_romClass);
			U_16 anonNameLength = J9UTF8_LENGTH(_anonClassName);
			U_16 originalNameLength = anonNameLength - ROM_ADDRESS_LENGTH - 1;
			/* nameLength field + nameBytes field + NULL terminator */
			_originalClassName = (J9UTF8*) j9mem_allocate_memory(sizeof(U_16) + originalNameLength + 1, J9MEM_CATEGORY_CLASSES);
			if (NULL == _originalClassName) {
				_buildResult = OutOfMemory;
			} else {
				J9UTF8_SET_LENGTH(_originalClassName, originalNameLength);
				memcpy(((U_8*) J9UTF8_DATA(_originalClassName)), J9UTF8_DATA(_anonClassName), originalNameLength);
				*(((U_8*) J9UTF8_DATA(_originalClassName)) + originalNameLength) = '\0';
			}
		}
		if (isOK()) {
			analyzeROMClass();
		}
		if (isOK()) {
			allocateBuffer();
		}
		if (isOK()) {
			writeClassFile();
			_classFileSize = UDATA(_classFileCursor - _classFileBuffer);
			/* We should never overshoot _classFileBuffer size */
			Trc_BCU_Assert_True(_classFileSize <= _romClass->classFileSize);
		}
	}

	~ClassFileWriter()
	{
		if (NULL != _cpHashTable) {
			hashTableFree(_cpHashTable);
			_cpHashTable = NULL;
		}

		if (!isOK()) {
			PORT_ACCESS_FROM_PORT(_portLibrary);

			/* If an error occurred during class file recreation, free the classFileBuffer */
			j9mem_free_memory(_classFileBuffer);
			_classFileBuffer = NULL;
		}
		if (_isAnon) {
			PORT_ACCESS_FROM_JAVAVM(_javaVM);
			j9mem_free_memory(_originalClassName);
		}
	}

	UDATA classFileSize() { return isOK() ? _classFileSize : 0; }
	U_8 * classFileData() { return isOK() ? _classFileBuffer : NULL; }
	bool isOK() { return OK == _buildResult; }
	BuildResult getResult() { return _buildResult; }
};

#endif /* CLASSFILEWRITER_HPP_ */
