/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/*
 * ROMClassWriter.hpp
 */

#ifndef ROMCLASSWRITER_HPP_
#define ROMCLASSWRITER_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"

#include "ClassFileOracle.hpp"
#include "ConstantPoolMap.hpp"
#include "SRPOffsetTable.hpp"
#include "ROMClassCreationContext.hpp"

class Cursor;
class SRPKeyProducer;
class BufferManager;

class ROMClassWriter
{
public:
	ROMClassWriter(BufferManager *bufferManager, ClassFileOracle *classFileOracle, SRPKeyProducer *srpKeyProducer, ConstantPoolMap *constantPoolMap, ROMClassCreationContext *context);
	~ROMClassWriter();

	void setSRPOffsetTable(SRPOffsetTable *srpOffsetTable)
	{
		_srpOffsetTable = srpOffsetTable;

		if (_context->isIntermediateDataAClassfile()) {
			/* Check if intermediateClassData can be re-used. It is re-usable when class is being re-transformed.
			 * However, do not re-use intermediateClassData from previous ROMClass if
			 * previous ROMClass is in private memory and the new class is share-able,
			 * otherwise we would end up with intermediateClassData of shared ROMClass pointing to private memory of the JVM.
			 *
			 * Do not re-use intermediateClassData if class is an anonClass. AnonClasses can be unloaded separately which would cause a
			 * problem if romClasses reference each other
			 */
			if ((false == _context->isClassAnon())
				&& ((false == _context->isROMClassShareable())
				|| (true == _context->isIntermediateClassDataShareable()))
			) {
				reuseIntermediateClassData();
			}
		}
	}

	void reuseIntermediateClassData()
	{
		if ((false == _context->isReusingIntermediateClassData())
			&& (true == _context->isRetransformAllowed())
			&& (true == _context->isRetransforming())
		) {
			_srpOffsetTable->setInternedAt(_intermediateClassDataSRPKey, _context->getIntermediateClassDataFromPreviousROMClass());
			_context->setReusingIntermediateClassData();
		}
	}

	enum MarkOrWrite {
		MARK_AND_COUNT_ONLY,
		WRITE
	};

	/*
	 * write out the entire ROMClass, including UTF8s
	 */
	void writeROMClass(Cursor *cursor,
			Cursor *lineNumberCursor,
			Cursor *variableInfoCursor,
			Cursor *utf8Cursor,
			Cursor *classDataCursor,
			U_32 romSize,
			U_32 modifiers,
			U_32 extraModifiers,
			U_32 optionalFlags,
			MarkOrWrite markOrWrite );

	/*
	 * writes out only UTF8s
	 */
	void writeUTF8s(Cursor *cursor);

	bool isOK() const { return OK == _buildResult; }
	BuildResult getBuildResult() const { return _buildResult; }
	U_32 getVarHandleMethodTypePaddedSize() { return _constantPoolMap->getVarHandleMethodTypePaddedCount() * sizeof(U_16); }



private:
	class AnnotationWriter;
	class AnnotationElementWriter;
	class NamedAnnotationElementWriter;
	class ConstantPoolWriter;
	class ConstantPoolShapeDescriptionWriter;
	class CheckSize;
	class Helper;
	class CallSiteWriter;

	struct MethodNotes {
		U_32 debugInfoSize;
		U_32 stackMapSize;
	};

	void writeConstantPool(Cursor *cursor, bool markAndCountOnly);
	void writeFields(Cursor *cursor, bool markAndCountOnly);
	void writeInterfaces(Cursor *cursor, bool markAndCountOnly);
	void writeInnerClasses(Cursor *cursor, bool markAndCountOnly);
	void writeNestMembers(Cursor *cursor, bool markAndCountOnly);
	void writeNameAndSignatureBlock(Cursor *cursor);
	void writeMethods(Cursor *cursor, Cursor *lineNumberCursor, Cursor *variableInfoCursor, bool markAndCountOnly);
	void writeMethodDebugInfo(ClassFileOracle::MethodIterator *methodIterator,  Cursor *lineNumberCursor, Cursor *variableInfoCursor, bool markAndCountOnly, bool existHasDebugInformation);
	void writeConstantPoolShapeDescriptions(Cursor *cursor, bool markAndCountOnly);
	void writeAnnotationInfo(Cursor *cursor);
	void writeSourceDebugExtension(Cursor *cursor);
	void writeStackMaps(Cursor *cursor);
	void writeOptionalInfo(Cursor *cursor);
	void writeCallSiteData(Cursor *cursor, bool markAndCountOnly);
	void writeVarHandleMethodTypeLookupTable(Cursor *cursor, bool markAndCountOnly);
	void writeStaticSplitTable(Cursor *cursor, bool markAndCountOnly);
	void writeSpecialSplitTable(Cursor *cursor, bool markAndCountOnly);
	void writeByteCodes(Cursor *cursor, ClassFileOracle::MethodIterator *methodIterator);
	U_32 computeNativeSignatureSize(U_8 *methodDescriptor);
	void writeNativeSignature(Cursor *cursor, U_8 *methodDescriptor, U_8 nativeArgCount);

	BufferManager *_bufferManager;
	ClassFileOracle *_classFileOracle;
	SRPKeyProducer *_srpKeyProducer;
	ConstantPoolMap *_constantPoolMap;
	SRPOffsetTable *_srpOffsetTable;
	ROMClassCreationContext *_context;
	MethodNotes *_methodNotes;
	BuildResult _buildResult;
	UDATA _interfacesSRPKey;
	UDATA _methodsSRPKey;
	UDATA _fieldsSRPKey;
	UDATA _cpDescriptionShapeSRPKey;
	UDATA _innerClassesSRPKey;
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
	UDATA _nestMembersSRPKey;
#endif /* J9VM_OPT_VALHALLA_NESTMATES */
	UDATA _optionalInfoSRPKey;
	UDATA _stackMapsSRPKey;
	UDATA _enclosingMethodSRPKey;
	UDATA _sourceDebugExtensionSRPKey;
	UDATA _intermediateClassDataSRPKey;
	UDATA _annotationInfoClassSRPKey;
	UDATA _typeAnnotationInfoSRPKey;
	UDATA _callSiteDataSRPKey;	
	UDATA _varHandleMethodTypeLookupTableSRPKey;
	UDATA _staticSplitTableSRPKey;
	UDATA _specialSplitTableSRPKey;
};

#endif /* ROMCLASSWRITER_HPP_ */
