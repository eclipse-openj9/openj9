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

/*
 * ROMClassBuilder.hpp
 *
 */

#ifndef ROMCLASSBUILDER_HPP_
#define ROMCLASSBUILDER_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"
#include "j9.h"
#include "j2sever.h"

#include "BuildResult.hpp"
#include "ClassFileParser.hpp"  /* included to obtain definition of VerifyClassFunction */
#include "StringInternTable.hpp"

class BufferManager;
class ClassFileOracle;
class ConstantPoolMap;
class ROMClassCreationContext;
class ROMClassStringInternManager;
class ROMClassWriter;
class SRPOffsetTable;
class SRPKeyProducer;

class ROMClassBuilder
{
public:
	static ROMClassBuilder *getROMClassBuilder(J9PortLibrary *portLibrary, J9JavaVM *vm);

	ROMClassBuilder(J9JavaVM *javaVM, J9PortLibrary *portLibrary, UDATA maxStringInternTableSize, U_8 * verifyExcludeAttribute, VerifyClassFunction verifyClassFunction);
	~ROMClassBuilder();

	bool isOK() { return _stringInternTable.isOK(); }

	/**
	 * Returns the current class file buffer - setting it to NULL in the ROMClassBuilder,
	 * thereby transferring its ownership to the caller. The caller is responsible for
	 * freeing the buffer using the port library.
	 */
	U_8 * releaseClassFileBuffer();

	BuildResult buildROMClass(ROMClassCreationContext *context);

protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

private:

	/* 
	 * Cursor Tags are used to identify what
	 * type of data is being written.
	 * These values must be linear from 0 as the max
	 * is used inside SRPOffsetTable to create an array.
	 */
	enum CursorTag
	{
		// Generic/main ROMClass data
		RC_TAG = 0,
		/* The header structure J9MethodDebugInfo and
		 * the J9LineNumber table are stored in this section */
		LINE_NUMBER_TAG = 1,
		// J9VariableInfo Table 
		VARIABLE_INFO_TAG = 2,
		// UTF8s section of the ROMClass
		UTF8_TAG = 3,
		// 'Original' or intermediate class file bytes
		INTERMEDIATE_TAG = 4,
		//
		MAX_TAG = INTERMEDIATE_TAG
	};
	
	/* 
	 * A helper structure to pass around the predicted and 
	 * updated size of various parts of the ROMClass.
	 */
	struct SizeInformation
	{
		UDATA rcWithOutUTF8sSize;
		UDATA lineNumberSize;
		UDATA variableInfoSize;
		UDATA utf8sSize;
		UDATA rawClassDataSize;
		UDATA varHandleMethodTypeLookupTableSize;
	};

	

	/* NOTE: Be sure to update J9DbgROMClassBuilder in j9nonbuilder.h when changing the state variables below. */
	J9JavaVM *_javaVM;
	J9PortLibrary * _portLibrary;
	U_8 * _verifyExcludeAttribute;
	VerifyClassFunction _verifyClassFunction;
	UDATA _classFileParserBufferSize;
	UDATA _bufferManagerSize;
	U_8 *_classFileBuffer;
	U_8 *_anonClassNameBuffer;
	UDATA _anonClassNameBufferSize;
	U_8 *_bufferManagerBuffer;
	StringInternTable _stringInternTable;

	BuildResult handleAnonClassName(J9CfrClassFile *classfile);
	U_32 computeExtraModifiers(ClassFileOracle *classFileOracle, ROMClassCreationContext *context);
	U_32 computeOptionalFlags(ClassFileOracle *classFileOracle, ROMClassCreationContext *context);
	BuildResult prepareAndLaydown( BufferManager *bufferManager, ClassFileParser *classFileParser, ROMClassCreationContext *context );
	void checkDebugInfoCompression(J9ROMClass *romClass, ClassFileOracle classFileOracle, SRPKeyProducer *srpKeyProducer, ConstantPoolMap *constantPoolMap, SRPOffsetTable *srpOffsetTable);
	U_32 finishPrepareAndLaydown(
			U_8 *romClassBuffer,
			U_8 *lineNumberBuffer,
			U_8 *variableInfoBuffer,
			SizeInformation *sizeInformation,
			U_32 modifiers,
			U_32 extraModifiers,
			U_32 optionalFlags,
			bool sharingROMClass,
			bool hasStringTableLock,
			ClassFileOracle *classFileOracle,
			SRPOffsetTable *srpOffsetTable,
			SRPKeyProducer *srpKeyProducer,
			ROMClassWriter *romClassWriter,
			ROMClassCreationContext *context,
			ConstantPoolMap *constantPoolMap);

	void layDownROMClass(
			ROMClassWriter *romClassWriter, SRPOffsetTable *srpOffsetTable, U_32 romSize, U_32 modifiers, U_32 extraModifiers, U_32 optionalFlags,
			ROMClassStringInternManager *internManager, ROMClassCreationContext *context, SizeInformation *sizeInformation);

	bool compareROMClassForEquality(U_8 *romClass, bool romClassIsShared,
			ROMClassWriter *romClassWriter, SRPOffsetTable *srpOffsetTable, SRPKeyProducer *srpKeyProducer, ClassFileOracle *classFileOracle,
			U_32 modifiers, U_32 extraModifiers, U_32 optionalFlags, ROMClassCreationContext * context);
	SharedCacheRangeInfo getSharedCacheSRPRangeInfo(void *address);
	void getSizeInfo(ROMClassCreationContext *context, ROMClassWriter *romClassWriter, SRPOffsetTable *srpOffsetTable, bool *countDebugDataOutOfLine, SizeInformation *sizeInformation);
};

#endif /* ROMCLASSBUILDER_HPP_ */
