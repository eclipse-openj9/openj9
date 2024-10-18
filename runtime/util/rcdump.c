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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cfreader.h"
#include "j9.h"

#include "pcstack.h"
#include "jbcmap.h"
#include "bcsizes.h"
#include "bcnames.h"
#include "j9port.h"
#include "j9protos.h"
#include "bcdump.h"
#include "rommeth.h"
#include "util_internal.h"

#define BITS_PER_UDATA (sizeof(UDATA)*8)

#define CF_ALLOC_INCREMENT 4096
#define OPCODE_RELATIVE_BRANCHES 1

#define ROUND_TO(granularity, number) ((number) +		\
	(((number) % (granularity)) ? ((granularity) - ((number) % (granularity))) : 0))

char *PreInitNames[] = {
	"InitNOP",
	"InitStatics",
	"InitStaticsW",
	"InitCopySingles",
	"InitCopySinglesW",
	"InitCopyDoubles",
	"InitCopyDoublesW",
	"InitVirtuals",
	"InitVirtualsW",
	"InitSpecials",
	"InitSpecialsW",
	"InitInterfaces",
	"InitInterfacesW",
	"InitCopyIntegers",
	"InitCopyIntegersW",
	"InitCopyFloats",
	"InitCopyFloatsW",
	"InitCopyLongs",
	"InitCopyLongsW",
	"InitInstanceFields",
	"InitInstanceFieldsW",
	"InitStaticFields",
	"InitStaticFieldsW",
};

static I_32 dumpAnnotationInfo ( J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static I_32 dumpMethodDebugInfo (J9PortLibrary *portLib, J9ROMClass *romClass, J9ROMMethod *romMethod, U_32 flags);
static I_32 dumpNative ( J9PortLibrary *portLib, J9ROMMethod * romMethod, U_32 flags);
static I_32 dumpGenericSignature (J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static I_32 dumpEnclosingMethod (J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static I_32 dumpPermittedSubclasses(J9PortLibrary *portLib, J9ROMClass *romClass);
#if JAVA_SPEC_VERSION >= 11
static I_32 dumpNest (J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
#endif /* JAVA_SPEC_VERSION >= 11 */
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
static I_32 dumpLoadableDescriptors(J9PortLibrary *portLib, J9ROMClass *romClass);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
static I_32 dumpImplicitCreationFlags (J9PortLibrary *portLib, J9ROMClass *romClass);
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
static I_32 dumpSimpleName (J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static I_32 dumpUTF ( J9UTF8 *utfString, J9PortLibrary *portLib, U_32 flags);
static I_32 dumpSourceDebugExtension (J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static I_32 dumpRomStaticField ( J9ROMFieldShape *romStatic, J9PortLibrary *portLib, U_32 flags);
static I_32 dumpRomField ( J9ROMFieldShape *romField, J9PortLibrary *portLib, U_32 flags);
static I_32 dumpSourceFileName (J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static I_32 dumpCPShapeDescription ( J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static I_32 dumpCallSiteData ( J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static I_32 dumpStaticSplitSideTable( J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static I_32 dumpSpecialSplitSideTable( J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags);
static void printMethodExtendedModifiers(J9PortLibrary *portLib, U_32 modifiers);
/*
	Dump a printed representation of the specified @romClass to stdout.
	Answer zero on success */

IDATA j9bcutil_dumpRomClass( J9ROMClass *romClass, J9PortLibrary *portLib, J9TranslationBufferSet *translationBuffers, U_32 flags)
{
	U_32 i;
	J9ROMFieldShape *currentField;
	J9ROMMethod *currentMethod;
	J9ROMFieldWalkState state;

	PORT_ACCESS_FROM_PORT( portLib );

	j9tty_printf( PORTLIB,  "ROM Size: 0x%X (%d)\n", romClass->romSize, romClass->romSize);

	j9tty_printf( PORTLIB,  "Class Name: ");
	dumpUTF( (J9UTF8 *) J9ROMCLASS_CLASSNAME(romClass), portLib, flags );
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB,  "Superclass Name: ");
	if (J9ROMCLASS_SUPERCLASSNAME(romClass)) {
		dumpUTF( (J9UTF8 *) J9ROMCLASS_SUPERCLASSNAME(romClass), portLib, flags );
	} else {
		j9tty_printf( PORTLIB,  "<none>");
	}
	j9tty_printf( PORTLIB,  "\n");

	/* dump the source file name */
	dumpSourceFileName(portLib, romClass, flags);

	/* dump the simple name */
	dumpSimpleName(portLib, romClass, flags);

	/* dump the class generic signature */
	dumpGenericSignature(portLib, romClass, flags);

	/* dump the enclosing method */
	dumpEnclosingMethod(portLib, romClass, flags);

	j9tty_printf( PORTLIB,  "Basic Access Flags (0x%X): ", romClass->modifiers);
	printModifiers(PORTLIB, romClass->modifiers, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_CLASS, J9ROMCLASS_IS_VALUE(romClass));
	j9tty_printf( PORTLIB,  "\n");
	j9tty_printf( PORTLIB,  "J9 Access Flags (0x%X): ", romClass->extraModifiers);
	j9_printClassExtraModifiers(portLib, romClass->extraModifiers);
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB,  "Class file version: %d.%d\n", (U_32) romClass->majorVersion, (U_32) romClass->minorVersion);

	j9tty_printf( PORTLIB,  "Instance Shape: 0x%X\n", romClass->instanceShape);
	j9tty_printf( PORTLIB,  "Intermediate Class Data (%d bytes): %p\n", romClass->intermediateClassDataLength, romClass->intermediateClassData);
	j9tty_printf( PORTLIB,  "Maximum Branch Count: %d\n", romClass->maxBranchCount);

	j9tty_printf( PORTLIB,  "Interfaces (%i):\n", romClass->interfaceCount);
	if (romClass->interfaceCount) {
		J9SRP * interfaces = J9ROMCLASS_INTERFACES(romClass);

		for(i = 0; i < romClass->interfaceCount; i++) {
			j9tty_printf( PORTLIB, "  ");
			dumpUTF(NNSRP_PTR_GET(interfaces, J9UTF8 *), portLib, flags );
			j9tty_printf( PORTLIB, "\n");
			interfaces++;
		}
	}

	if (J9_ARE_ALL_BITS_SET(romClass->extraModifiers, J9AccClassInnerClass)) {
		j9tty_printf( PORTLIB,  "Declaring Class: ");
		if (NULL != J9ROMCLASS_OUTERCLASSNAME(romClass)) {
			dumpUTF(J9ROMCLASS_OUTERCLASSNAME(romClass), portLib, flags);
		} else {
			j9tty_printf( PORTLIB, "<unknown>");
		}
		j9tty_printf( PORTLIB, "\n");
		j9tty_printf( PORTLIB,  "Member Access Flags (0x%X): ", romClass->memberAccessFlags);
		printModifiers(PORTLIB, romClass->memberAccessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_CLASS, J9ROMCLASS_IS_VALUE(romClass));

		j9tty_printf( PORTLIB,  "\n");
	}
	if(romClass->innerClassCount)
	{
		J9SRP * inners = J9ROMCLASS_INNERCLASSES(romClass);

		j9tty_printf( PORTLIB,  "Declared Classes (%i):\n", romClass->innerClassCount);
		for(i = 0; i < romClass->innerClassCount; i++)
		{
			j9tty_printf( PORTLIB, "  ");
			dumpUTF(NNSRP_PTR_GET(inners, J9UTF8 *), portLib, flags );
			j9tty_printf( PORTLIB, "\n");
			inners++;
		}
	}

	if (J9ROMCLASS_IS_SEALED(romClass)) {
		dumpPermittedSubclasses(portLib, romClass);
	}

#if JAVA_SPEC_VERSION >= 11
	/* dump the nest members or nest host, if defined */
	dumpNest(portLib, romClass, flags);
#endif /* JAVA_SPEC_VERSION >= 11 */

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	if (J9_ARE_ALL_BITS_SET(romClass->optionalFlags, J9_ROMCLASS_OPTINFO_LOADABLEDESCRIPTORS_ATTRIBUTE)) {
		dumpLoadableDescriptors(portLib, romClass);
	}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	if (J9_ARE_ALL_BITS_SET(romClass->optionalFlags, J9_ROMCLASS_OPTINFO_IMPLICITCREATION_ATTRIBUTE)) {
		dumpImplicitCreationFlags(portLib, romClass);
	}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

	j9tty_printf( PORTLIB, "Fields (%i):\n", romClass->romFieldCount);
	currentField = romFieldsStartDo(romClass, &state);
	while (currentField != NULL) {
		if (currentField->modifiers & J9AccStatic) {
			dumpRomStaticField( currentField, portLib, flags);
		} else {
			dumpRomField( currentField, portLib, flags);
		}
		j9tty_printf( PORTLIB, "\n");
		currentField = romFieldsNextDo(&state);
	}

	dumpCPShapeDescription( portLib, romClass, flags );

	j9tty_printf( PORTLIB, "Methods (%i):\n", romClass->romMethodCount);
	currentMethod = (J9ROMMethod *) J9ROMCLASS_ROMMETHODS(romClass);

	for(i = 0; i < romClass->romMethodCount; i++) {
		j9bcutil_dumpRomMethod( currentMethod, romClass, portLib, flags, i );

		j9tty_printf( PORTLIB, "\n");
		currentMethod = nextROMMethod(currentMethod);
	}

	/* dump source debug extension */
	dumpSourceDebugExtension(portLib, romClass, flags);

	/* dump annotation info */
	dumpAnnotationInfo(portLib, romClass, flags);

	dumpCallSiteData(portLib, romClass, flags);

	/* dump split side tables */
	dumpStaticSplitSideTable(portLib, romClass, flags);
	dumpSpecialSplitSideTable(portLib, romClass, flags);

	j9tty_printf( PORTLIB, "\n");
	return BCT_ERR_NO_ERROR;
}

static I_32 dumpCPShapeDescription( J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	U_32 *cpDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);
	U_32 descriptionLong;
	U_32 i, j, k, numberOfLongs;
	char symbols[] = ".CSIFJDi.vxyzTHA.cxv";

	PORT_ACCESS_FROM_PORT( portLib );

	numberOfLongs = (romClass->romConstantPoolCount + J9_CP_DESCRIPTIONS_PER_U32 - 1) / J9_CP_DESCRIPTIONS_PER_U32;

	j9tty_printf( PORTLIB, "CP Shape Description:\n");
	k = romClass->romConstantPoolCount;
	for(i = 0; i < numberOfLongs; i++)
	{
		j9tty_printf( PORTLIB, "  ");
		descriptionLong = cpDescription[i];
		for(j = 0; j < J9_CP_DESCRIPTIONS_PER_U32; j++, k--)
		{
			if(k == 0) break;
			j9tty_printf( PORTLIB,  "%c ", symbols[descriptionLong & J9_CP_DESCRIPTION_MASK]);
			descriptionLong >>= J9_CP_BITS_PER_DESCRIPTION;
		}
		j9tty_printf( PORTLIB, "\n");
	}
	j9tty_printf( PORTLIB, "\n");
	return BCT_ERR_NO_ERROR;
}

static I_32
dumpNative( J9PortLibrary *portLib, J9ROMMethod * romMethod, U_32 flags)
{
	U_32 temp;
	U_8 * bytecodes = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	U_8 argCount = bytecodes[0], returnType=bytecodes[1];
	U_8 *currentDescription = &bytecodes[2];
	char *descriptions[] = {"void", "boolean","byte","char","short","float","int","double","long","object"};
	U_32 i;

	PORT_ACCESS_FROM_PORT( portLib );

	j9tty_printf(PORTLIB,  "  Argument Count: %d\n", J9_ARG_COUNT_FROM_ROM_METHOD(romMethod));

	temp = J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
	j9tty_printf(PORTLIB,  "  Temp Count: %d\n", temp);

	j9tty_printf(PORTLIB,  "  Native Argument Count: %d, types: (", argCount);

	for(i=argCount; i > 0; i--) {
		j9tty_printf( PORTLIB, "%s",descriptions[*currentDescription++]);
		if (i != 1) j9tty_printf( PORTLIB, ",");
	}
	j9tty_printf( PORTLIB, ") %s\n ",descriptions[returnType]);

	return BCT_ERR_NO_ERROR;
}


/*
	Dump a printed representation of the specified @romField to stdout.
	Answer zero on success */

static I_32 dumpRomField( J9ROMFieldShape *romField, J9PortLibrary *portLib, U_32 flags)
{
	PORT_ACCESS_FROM_PORT( portLib );

	j9tty_printf( PORTLIB,  "  Name: " );
	dumpUTF( (J9UTF8 *) J9ROMFIELDSHAPE_NAME(romField), portLib, flags );
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB,  "  Signature: ");
	dumpUTF( (J9UTF8 *) J9ROMFIELDSHAPE_SIGNATURE(romField), portLib, flags );
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB, "  Access Flags (%X): ", romField->modifiers);
	printModifiers(PORTLIB, romField->modifiers, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_FIELD, FALSE);
	j9tty_printf( PORTLIB, "\n");
	return BCT_ERR_NO_ERROR;
}



/*
	Dump a printed representation of the specified @romStatic to stdout.
	Answer zero on success */

static I_32 dumpRomStaticField( J9ROMFieldShape *romStatic, J9PortLibrary *portLib, U_32 flags)
{
	PORT_ACCESS_FROM_PORT( portLib );

	j9tty_printf( PORTLIB,  "  Name: ");
	dumpUTF( (J9UTF8 *) J9ROMFIELDSHAPE_NAME(romStatic), portLib, flags );
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB,  "  Signature: ");
	dumpUTF( (J9UTF8 *) J9ROMFIELDSHAPE_SIGNATURE(romStatic), portLib, flags );
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB, "  Access Flags (%X): ", romStatic->modifiers);
	printModifiers(PORTLIB, romStatic->modifiers, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_FIELD, FALSE);
	j9tty_printf( PORTLIB, "\n");

	return BCT_ERR_NO_ERROR;
}



/*
	Dump a printed representation of the specified @utf8String to stdout.
	Answer zero on success */

static I_32 dumpUTF( J9UTF8 *utfString, J9PortLibrary *portLib, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	j9tty_printf(PORTLIB, "%.*s", J9UTF8_LENGTH(utfString), J9UTF8_DATA(utfString));
	return BCT_ERR_NO_ERROR;
}

static I_32
dumpSourceDebugExtension(J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9SourceDebugExtension *sde;
	U_32 temp;
	U_8 *current;

#if defined(J9VM_OPT_DEBUG_INFO_SERVER)
	if(!(flags & BCT_StripDebugAttributes)) {
		sde = getSourceDebugExtensionForROMClass(NULL, NULL, romClass);
		if (sde) {
			temp = sde->size;
			if (temp) {
				current = (U_8 *)sde;
				/*increment sde here if it needs to be used further*/
				j9tty_printf( PORTLIB, "  Source debug extension (%d bytes):\n    ", temp);
				while (temp--) {
					U_8 c = *current++;

					if (c == '\015') {
						if (temp) {
							if (*current == '\012') ++current;
							j9tty_printf( PORTLIB, "\n     ");
						}
					} else if (c == '\012') {
						j9tty_printf( PORTLIB, "\n    ");
					} else {
					j9tty_printf( PORTLIB, "%c", c);
					}
				}
			}
		}
	}
#endif

	return BCT_ERR_NO_ERROR;
}



static I_32
dumpGenericSignature(J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9UTF8 *genericSignature =  getGenericSignatureForROMClass(NULL, NULL, romClass);

	if (genericSignature) {
		j9tty_printf( PORTLIB, "Generic Signature: %.*s\n", J9UTF8_LENGTH(genericSignature), J9UTF8_DATA(genericSignature));
	}

	return BCT_ERR_NO_ERROR;
}


static I_32
dumpSourceFileName(J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9UTF8 *sourceFileName = NULL;
#if defined(J9VM_OPT_DEBUG_INFO_SERVER)
	if(!(flags & BCT_StripDebugAttributes)) {
		sourceFileName = getSourceFileNameForROMClass(NULL, NULL, romClass);
		if (sourceFileName) {
			j9tty_printf( PORTLIB, "Source File Name: %.*s\n", J9UTF8_LENGTH(sourceFileName), J9UTF8_DATA(sourceFileName));
		}
	}
#endif
	return BCT_ERR_NO_ERROR;
}

static I_32
dumpCallSiteData(J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	UDATA i;
	UDATA callSiteCount = romClass->callSiteCount;
	UDATA bsmCount = romClass->bsmCount;
	J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
	U_16 *bsmIndices = (U_16 *) (callSiteData + callSiteCount);

	PORT_ACCESS_FROM_PORT(portLib);

	if (0 != callSiteCount) {

		j9tty_printf(PORTLIB, "  Call Sites (%i):\n", callSiteCount);
		for (i = 0; i < callSiteCount; i++) {
			J9ROMNameAndSignature* nameAndSig = SRP_PTR_GET(callSiteData + i, J9ROMNameAndSignature*);

			j9tty_printf(PORTLIB, "    Name: ");
			dumpUTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig), PORTLIB, flags);
			j9tty_printf( PORTLIB, "\n");

			j9tty_printf(PORTLIB, "    Signature: ");
			dumpUTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), PORTLIB, flags);
			j9tty_printf(PORTLIB, "\n");

			j9tty_printf(PORTLIB, "    Bootstrap Method Index: %i\n", bsmIndices[i]);
		}
	}

	if (0 != bsmCount) {
		J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
		U_32 *cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);
		U_16 *bsmDataCursor = bsmIndices + callSiteCount;
		UDATA bigEndian = flags & BCT_BigEndianOutput;

		j9tty_printf(PORTLIB, "  Bootstrap Methods (%i):\n", bsmCount);
		for (i = 0; i < bsmCount; i++) {
			J9ROMMethodHandleRef *methodHandleRef = (J9ROMMethodHandleRef *) &constantPool[*bsmDataCursor++];
			/* methodRef will be either a field or a method ref - they both have the same shape so we can pretend it is always a methodref */
			J9ROMMethodRef *methodRef = (J9ROMMethodRef *) &constantPool[methodHandleRef->methodOrFieldRefIndex];
			J9ROMClassRef *classRef = (J9ROMClassRef *) &constantPool[methodRef->classRefCPIndex];
			J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(methodRef);
			UDATA bsmArgumentCount = *bsmDataCursor++;

			j9tty_printf(PORTLIB, "    Name: ");
			dumpUTF(J9ROMCLASSREF_NAME(classRef), PORTLIB, flags);
			j9tty_printf(PORTLIB, ".");
			dumpUTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig), PORTLIB, flags);
			j9tty_printf( PORTLIB, "\n");

			j9tty_printf(PORTLIB, "    Signature: ");
			dumpUTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), PORTLIB, flags);
			j9tty_printf(PORTLIB, "\n");

			j9tty_printf(PORTLIB, "    Bootstrap Method Arguments (%i):\n", bsmArgumentCount);
			for (; 0 != bsmArgumentCount; bsmArgumentCount--) {
				UDATA argCPIndex = *bsmDataCursor++;
				J9ROMConstantPoolItem *item = constantPool + argCPIndex;
				U_32 shapeDesc = J9_CP_TYPE(cpShapeDescription, argCPIndex);
				switch(shapeDesc) {
					case J9CPTYPE_CLASS:
						j9tty_printf(PORTLIB, "      Class: ");
						dumpUTF(J9ROMCLASSREF_NAME((J9ROMClassRef *)item), PORTLIB, flags);
						j9tty_printf(PORTLIB, "\n");
						break;

					case J9CPTYPE_STRING:
					case J9CPTYPE_ANNOTATION_UTF8:
						j9tty_printf(PORTLIB, "      String: ");
						dumpUTF(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)item), PORTLIB, flags);
						j9tty_printf(PORTLIB, "\n");
						break;

					case J9CPTYPE_INT:
						j9tty_printf(PORTLIB, "      Int: 0x%08X\n", ((J9ROMSingleSlotConstantRef *)item)->data);
						break;

					case J9CPTYPE_FLOAT:
						j9tty_printf(PORTLIB, "      Float: 0x%08X\n", ((J9ROMSingleSlotConstantRef *)item)->data);
						break;

					case J9CPTYPE_LONG:
						j9tty_printf(PORTLIB, "      Long: 0x%08X%08X\n",
							bigEndian ? item->slot1 : item->slot2,
							bigEndian ? item->slot2 : item->slot1);
						break;

					case J9CPTYPE_DOUBLE:
						j9tty_printf(PORTLIB, "      Double: 0x%08X%08X\n",
							bigEndian ? item->slot1 : item->slot2,
							bigEndian ? item->slot2 : item->slot1);
						break;

					case J9CPTYPE_FIELD:
						j9tty_printf(PORTLIB, "      Field: ");
						classRef = (J9ROMClassRef *) &constantPool[((J9ROMFieldRef *)item)->classRefCPIndex];
						nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *)item);
						dumpUTF(J9ROMCLASSREF_NAME(classRef), PORTLIB, flags);
						j9tty_printf(PORTLIB, ".");
						dumpUTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig), PORTLIB, flags);
						j9tty_printf(PORTLIB, " ");
						dumpUTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), PORTLIB, flags);
						j9tty_printf(PORTLIB, "\n");
						break;

					case J9CPTYPE_HANDLE_METHOD:
					case J9CPTYPE_INSTANCE_METHOD:
					case J9CPTYPE_STATIC_METHOD:
					case J9CPTYPE_INTERFACE_METHOD:
					case J9CPTYPE_INTERFACE_INSTANCE_METHOD:
					case J9CPTYPE_INTERFACE_STATIC_METHOD:
						j9tty_printf(PORTLIB, "      Method: ");
						classRef = (J9ROMClassRef *) &constantPool[((J9ROMMethodRef *)item)->classRefCPIndex];
						nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *)item);
						dumpUTF(J9ROMCLASSREF_NAME(classRef), PORTLIB, flags);
						j9tty_printf(PORTLIB, ".");
						dumpUTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig), PORTLIB, flags);
						dumpUTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), PORTLIB, flags);
						j9tty_printf(PORTLIB, "\n");
						break;

					case J9CPTYPE_METHOD_TYPE:
						j9tty_printf(PORTLIB, "      Method Type: ");
						dumpUTF(J9ROMMETHODTYPEREF_SIGNATURE((J9ROMMethodTypeRef *)item), PORTLIB, flags);
						j9tty_printf(PORTLIB, "\n");
						break;

					case J9CPTYPE_METHODHANDLE:
						j9tty_printf(PORTLIB, "      Method Handle: ");
						methodHandleRef = (J9ROMMethodHandleRef *) item;
						methodRef = (J9ROMMethodRef *) &constantPool[methodHandleRef->methodOrFieldRefIndex];
						classRef = (J9ROMClassRef *) &constantPool[methodRef->classRefCPIndex];
						nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(methodRef);
						dumpUTF(J9ROMCLASSREF_NAME(classRef), PORTLIB, flags);
						j9tty_printf(PORTLIB, ".");
						dumpUTF(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig), PORTLIB, flags);
						switch (methodHandleRef->handleTypeAndCpType >> J9DescriptionCpTypeShift) {
						case MH_REF_GETFIELD:
						case MH_REF_PUTFIELD:
						case MH_REF_GETSTATIC:
						case MH_REF_PUTSTATIC:
							j9tty_printf(PORTLIB, " ");
						default:
							break;
						}
						dumpUTF(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), PORTLIB, flags);
						j9tty_printf(PORTLIB, "\n");
						break;

					case J9CPTYPE_UNUSED:
					case J9CPTYPE_UNUSED8:
					default:
						/* unknown cp type */
						j9tty_printf(PORTLIB, "      <unknown type>\n");
						break;
				}
			}
		}
	}

	return BCT_ERR_NO_ERROR;
}

static I_32
dumpStaticSplitSideTable(J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	U_16 i = 0;
	U_16 *splitTable = J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass);
	PORT_ACCESS_FROM_PORT(portLib);

	if (romClass->staticSplitMethodRefCount > 0) {
		j9tty_printf(PORTLIB, "Static Split Table (%i):\n", romClass->staticSplitMethodRefCount); 
		j9tty_printf(PORTLIB, "  SplitTable Index -> CP Index\n");
		for (i = 0; i < romClass->staticSplitMethodRefCount; i++) {
			j9tty_printf(PORTLIB, "  %16d -> %d\n", i, splitTable[i]);
		}
	}
	
	return BCT_ERR_NO_ERROR;
}

static I_32
dumpSpecialSplitSideTable(J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	U_16 i = 0;
	U_16 *splitTable = J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass);
	PORT_ACCESS_FROM_PORT(portLib);

	if (romClass->specialSplitMethodRefCount > 0) {
		j9tty_printf(PORTLIB, "Special Split Table (%i):\n", romClass->specialSplitMethodRefCount);
		j9tty_printf(PORTLIB, "  SplitTable Index -> CP Index\n");
		for (i = 0; i < romClass->specialSplitMethodRefCount; i++) {
			j9tty_printf(PORTLIB, "  %16d -> %d\n", i, splitTable[i]);
		}
	}

	return BCT_ERR_NO_ERROR;
}

static I_32
dumpAnnotationInfo( J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	U_32 i;

	U_32 *classAnnotationData = getClassAnnotationsDataForROMClass(romClass);
	U_32 *classTypeAnnotationData = getClassTypeAnnotationsDataForROMClass(romClass);

	j9tty_printf(PORTLIB, "  Annotation Info:\n");

	if (NULL != classAnnotationData) {
		U_32 length = classAnnotationData[0];
		U_32 *data = classAnnotationData + 1;
		j9tty_printf(PORTLIB, "    Class Annotations (%i bytes): %p\n", length, data);
	}

	if (NULL != classTypeAnnotationData) {
		U_32 length = classTypeAnnotationData[0];
		U_32 *data = classTypeAnnotationData + 1;
		j9tty_printf(PORTLIB, "    Class Type Annotations (%i bytes): %p\n", length, data);
	}

	/* print field annotations */
	{
		J9ROMFieldWalkState state;
		J9ROMFieldShape *romField = romFieldsStartDo(romClass, &state);
		j9tty_printf(PORTLIB, "    Field Annotations:\n");
		for (i = 0; (i < romClass->romFieldCount) && (NULL != romField); i++) {
			U_32 *fieldAnnotationData = getFieldAnnotationsDataFromROMField(romField);
			U_32 *fieldTypeAnnotationData = getFieldTypeAnnotationsDataFromROMField(romField);
			if ((NULL != fieldAnnotationData) || (NULL != fieldTypeAnnotationData)) {
				j9tty_printf(PORTLIB, "      Name: ");
				dumpUTF( (J9UTF8 *) J9ROMFIELDSHAPE_NAME(romField), portLib, flags );
				j9tty_printf(PORTLIB, "\n");

				j9tty_printf(PORTLIB, "      Signature: ");
				dumpUTF( (J9UTF8 *) J9ROMFIELDSHAPE_SIGNATURE(romField), portLib, flags );
				j9tty_printf(PORTLIB, "\n");
			}

			if (NULL != fieldAnnotationData) {
				U_32 length = fieldAnnotationData[0];
				U_32 *data = fieldAnnotationData + 1;
				j9tty_printf(PORTLIB, "      Annotations (%i bytes): %p\n", length, data);
			}

			if (NULL != fieldTypeAnnotationData) {
				U_32 length = fieldTypeAnnotationData[0];
				U_32 *data = fieldTypeAnnotationData + 1;
				j9tty_printf(PORTLIB, "      Type Annotations (%i bytes): %p\n", length, data);
			}
			romField = romFieldsNextDo(&state);
		}
		j9tty_printf(PORTLIB, "\n");
	}

	/* print method, parameter, type, and default annotations */
	{
		J9ROMMethod *romMethod = (J9ROMMethod *) J9ROMCLASS_ROMMETHODS(romClass);
		j9tty_printf(PORTLIB, "    Method Annotations:\n");
		for (i = 0; i < romClass->romMethodCount; i++) {
			U_32 *methodAnnotationData = getMethodAnnotationsDataFromROMMethod(romMethod);
			U_32 *parametersAnnotationData = getParameterAnnotationsDataFromROMMethod(romMethod);
			U_32 *defaultAnnotationData = getDefaultAnnotationDataFromROMMethod(romMethod);
			U_32 *methodTypeAnnotationData = getMethodTypeAnnotationsDataFromROMMethod(romMethod);
			U_32 *codeTypeAnnotationData = getCodeTypeAnnotationsDataFromROMMethod(romMethod);
			if ((NULL != methodAnnotationData) || (NULL != parametersAnnotationData) || (NULL != defaultAnnotationData)) {
				j9tty_printf(PORTLIB, "      Name: ");
				dumpUTF( (J9UTF8 *) J9ROMMETHOD_NAME(romMethod), portLib, flags );
				j9tty_printf( PORTLIB, "\n");

				j9tty_printf(PORTLIB, "      Signature: ");
				dumpUTF( (J9UTF8 *) J9ROMMETHOD_SIGNATURE(romMethod), portLib, flags );
				j9tty_printf(PORTLIB, "\n");
			}
			if (NULL != methodAnnotationData) {
				U_32 length = methodAnnotationData[0];
				U_32 *data = methodAnnotationData + 1;
				j9tty_printf(PORTLIB, "      Annotations (%i bytes): %p\n", length, data);
			}
			if (NULL != parametersAnnotationData) {
				U_32 length = parametersAnnotationData[0];
				U_32 *data = parametersAnnotationData + 1;
				j9tty_printf(PORTLIB, "      Parameters Annotations (%i bytes): %p\n", length, data);
			}
			if (NULL != methodTypeAnnotationData) {
				U_32 length = methodTypeAnnotationData[0];
				U_32 *data = methodTypeAnnotationData + 1;
				j9tty_printf(PORTLIB, "      Type Annotations on Method (%i bytes): %p\n", length, data);
			}
			if (NULL != codeTypeAnnotationData) {
				U_32 length = codeTypeAnnotationData[0];
				U_32 *data = codeTypeAnnotationData + 1;
				j9tty_printf(PORTLIB, "      Type Annotations on Code (%i bytes): %p\n", length, data);
			}
			if (NULL != defaultAnnotationData) {
				U_32 length = defaultAnnotationData[0];
				U_32 *data = defaultAnnotationData + 1;
				j9tty_printf(PORTLIB, "      Default Annotation (%i bytes): %p\n", length, data);
			}
			romMethod = nextROMMethod(romMethod);
		}
		j9tty_printf(PORTLIB, "\n");
	}


	return BCT_ERR_NO_ERROR;
}


static I_32
dumpMethodDebugInfo(J9PortLibrary *portLib, J9ROMClass *romClass, J9ROMMethod *romMethod, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);

	J9MethodDebugInfo *methodInfo;
	U_8 *currentLineNumber;
	J9LineNumber lineNumber;
	U_32 j;
	lineNumber.lineNumber = 0;
	lineNumber.location = 0;

	if(!(flags & BCT_StripDebugAttributes)) {
		J9VariableInfoWalkState state;
		J9VariableInfoValues *values;
		J9MethodDebugInfo* debugInfo;

		debugInfo = methodDebugInfoFromROMMethod(romMethod);

		/* check for low tag to indicate inline debug information */
		methodInfo = getMethodDebugInfoFromROMMethod(romMethod);
		if(NULL != methodInfo) {
			j9tty_printf( PORTLIB, "\n  Debug Info:\n");
			j9tty_printf( PORTLIB, "    Line Number Table (%i):\n", getLineNumberCount(methodInfo));
			currentLineNumber = getLineNumberTable(methodInfo);
			for(j = 0; j < getLineNumberCount(methodInfo); j++) {
				if (!getNextLineNumberFromTable(&currentLineNumber, &lineNumber)) {
					UDATA sizeLeft = getLineNumberCompressedSize(methodInfo) - (currentLineNumber - getLineNumberTable(methodInfo));
					j9tty_printf( PORTLIB, "      Bad compressed data \n");
					while (0 < sizeLeft--) {
						j9tty_printf( PORTLIB, "      0x%d \n",*currentLineNumber);
						++currentLineNumber;
					}
					break;
				}
				j9tty_printf( PORTLIB, "      Line: %5i PC: %5i\n", lineNumber.lineNumber, lineNumber.location);
			}
			j9tty_printf( PORTLIB, "\n");

			j9tty_printf( PORTLIB, "    Variables (%i):\n", methodInfo->varInfoCount);
			values = variableInfoStartDo(methodInfo, &state);
			while(values != NULL) {
				j9tty_printf( PORTLIB, "      Slot: %i\n", values->slotNumber);
				j9tty_printf( PORTLIB, "      Visibility Start: %i\n", values->startVisibility);
				j9tty_printf( PORTLIB, "      Visibility End: %i\n", values->startVisibility + values->visibilityLength);
				j9tty_printf( PORTLIB, "      Visibility Length: %i\n", values->visibilityLength);
				j9tty_printf( PORTLIB, "      Name: ");
				if (values->name) {
					j9tty_printf( PORTLIB, "%.*s\n", J9UTF8_LENGTH(values->name), J9UTF8_DATA(values->name));
				} else {
					j9tty_printf( PORTLIB, "None\n");
				}
				j9tty_printf( PORTLIB, "      Signature: ");
				if (values->signature) {
					j9tty_printf( PORTLIB, "%.*s\n", J9UTF8_LENGTH(values->signature), J9UTF8_DATA(values->signature));
				} else {
					j9tty_printf( PORTLIB, "None\n");
				}
				j9tty_printf( PORTLIB, "      Generic Signature: ");
				if (values->genericSignature) {
					j9tty_printf( PORTLIB, "%.*s\n", J9UTF8_LENGTH(values->genericSignature), J9UTF8_DATA(values->genericSignature));
				} else {
					j9tty_printf( PORTLIB, "None\n");
				}
					values = variableInfoNextDo(&state);
			} /* end of the while loop */
		} /* end of the if(methodInfo) */
	}
	return BCT_ERR_NO_ERROR;
}


static I_32
dumpEnclosingMethod(J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9EnclosingObject *enclosingObject = getEnclosingMethodForROMClass(NULL, NULL, romClass);

	if (enclosingObject) {
		J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
		J9UTF8 *className = J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[enclosingObject->classRefCPIndex]);

		if (enclosingObject->nameAndSignature) {
			J9ROMNameAndSignature *nameAndSig = SRP_GET(enclosingObject->nameAndSignature, J9ROMNameAndSignature *);

			j9tty_printf( PORTLIB, "Enclosing Method: %.*s.%.*s%.*s\n",
				J9UTF8_LENGTH(className), J9UTF8_DATA(className),
				J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig)),
				J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig)),
				J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig)),
				J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig)));
		} else {
			j9tty_printf( PORTLIB, "Enclosing Class: %.*s\n", J9UTF8_LENGTH(className), J9UTF8_DATA(className));
		}
	}

	return BCT_ERR_NO_ERROR;
}

static I_32
dumpPermittedSubclasses(J9PortLibrary *portLib, J9ROMClass *romClass)
{
	PORT_ACCESS_FROM_PORT(portLib);

	U_32 *permittedSubclassesCountPtr = getNumberOfPermittedSubclassesPtr(romClass);
	U_16 i = 0;

	j9tty_printf(PORTLIB, "Permitted subclasses (%i):\n", *permittedSubclassesCountPtr);
	for (; i < *permittedSubclassesCountPtr; i++) {
		J9UTF8 *permittedSubclassNameUtf8 = permittedSubclassesNameAtIndex(permittedSubclassesCountPtr, i);
		j9tty_printf(PORTLIB, "  %.*s\n", J9UTF8_LENGTH(permittedSubclassNameUtf8), J9UTF8_DATA(permittedSubclassNameUtf8));
	}
	return BCT_ERR_NO_ERROR;
}

#if JAVA_SPEC_VERSION >= 11
static I_32
dumpNest(J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	U_16 nestMemberCount = romClass->nestMemberCount;
	J9UTF8 *nestHostName = J9ROMCLASS_NESTHOSTNAME(romClass);

	if (0 != nestMemberCount) {
		/* The class has a "nest members" attribute (non-zero nestMemberCount) */
		U_16 i = 0;
		J9SRP *nestMemberNames = J9ROMCLASS_NESTMEMBERS(romClass);
		j9tty_printf(PORTLIB, "Nest members (%i):\n", nestMemberCount);
		for (i = 0; i < nestMemberCount; i++) {
			J9UTF8 *nestMemberClassNames = NNSRP_GET(nestMemberNames[i], J9UTF8 *);
			j9tty_printf(PORTLIB, "  %.*s\n", J9UTF8_LENGTH(nestMemberClassNames), J9UTF8_DATA(nestMemberClassNames));
		}
	}

	if (NULL != nestHostName) {
		/* The class has a "member of nest" attribute (non-NULL nest host name) */
		j9tty_printf(PORTLIB, "Nest host class: %.*s\n", J9UTF8_LENGTH(nestHostName), J9UTF8_DATA(nestHostName));
	}
	return BCT_ERR_NO_ERROR;
}
#endif /* JAVA_SPEC_VERSION >= 11 */

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
static I_32
dumpLoadableDescriptors(J9PortLibrary *portLib, J9ROMClass *romClass)
{
	PORT_ACCESS_FROM_PORT(portLib);
	U_32 *loadableDescriptorsInfoPtr = getLoadableDescriptorsInfoPtr(romClass);
	U_32 loadableDescriptorsCount = *loadableDescriptorsInfoPtr;
	U_16 i = 0;

	j9tty_printf(PORTLIB, "Loadable descriptors (%i):\n", loadableDescriptorsCount);
	for (; i < loadableDescriptorsCount; i++) {
		J9UTF8 *loadableDescriptorUtf8 = loadableDescriptorAtIndex(loadableDescriptorsInfoPtr, i);
		j9tty_printf(PORTLIB, "  %.*s\n", J9UTF8_LENGTH(loadableDescriptorUtf8), J9UTF8_DATA(loadableDescriptorUtf8));
	}
	return BCT_ERR_NO_ERROR;
}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
static I_32
dumpImplicitCreationFlags(J9PortLibrary *portLib, J9ROMClass *romClass)
{
	PORT_ACCESS_FROM_PORT(portLib);
	U_16 implicitCreationFlags = getImplicitCreationFlags(romClass);
	j9tty_printf(PORTLIB, "ImplicitCreation flags: 0x%X\n", implicitCreationFlags);
	return BCT_ERR_NO_ERROR;
}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

static I_32
dumpSimpleName(J9PortLibrary *portLib, J9ROMClass *romClass, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9UTF8 *simpleName =  getSimpleNameForROMClass(NULL, NULL, romClass);

	if (simpleName) {
		j9tty_printf( PORTLIB, "Simple Name: %.*s\n", J9UTF8_LENGTH(simpleName), J9UTF8_DATA(simpleName));
	}

	return BCT_ERR_NO_ERROR;
}


/*
	Dump a printed representation of the specified @romMethod to stdout.
	Answer zero on success */

I_32 j9bcutil_dumpRomMethod( J9ROMMethod *romMethod, J9ROMClass *romClass, J9PortLibrary *portLib, U_32 flags, U_32 methodIndex)
{
	I_32 i;
	J9SRP *currentThrowName;
	J9ExceptionHandler *handler;
	J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);

	PORT_ACCESS_FROM_PORT( portLib );

	j9tty_printf( PORTLIB,  "  Name: ");
	dumpUTF( (J9UTF8 *) J9ROMMETHOD_NAME(romMethod), portLib, flags );
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB,  "  Signature: ");
	dumpUTF( (J9UTF8 *) J9ROMMETHOD_SIGNATURE(romMethod), portLib, flags );
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB, "  Access Flags (%X): ", romMethod->modifiers);
	printModifiers(PORTLIB, (U_32)romMethod->modifiers, INCLUDE_INTERNAL_MODIFIERS, MODIFIERSOURCE_METHOD, FALSE);
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	if (J9_ARE_ALL_BITS_SET(romClass->optionalFlags, J9_ROMCLASS_OPTINFO_IMPLICITCREATION_ATTRIBUTE)) {
		if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)), J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)), "<init>")) {
			j9tty_printf(PORTLIB, " implicit");
		}
	}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB, "  Extended modifiers (%X): ", getExtendedModifiersDataFromROMMethod(romMethod));
	printMethodExtendedModifiers(PORTLIB, (U_32)getExtendedModifiersDataFromROMMethod(romMethod));
	j9tty_printf( PORTLIB,  "\n");

	j9tty_printf( PORTLIB, "  Max Stack: %d\n", J9_MAX_STACK_FROM_ROM_METHOD(romMethod));

	if (J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)) {
		J9ExceptionInfo * exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);

		if (exceptionData->catchCount) {
			handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);
			j9tty_printf( PORTLIB, "  Caught Exceptions (%i):\n", exceptionData->catchCount);
			j9tty_printf( PORTLIB, "     start   end   handler   catch type\n");
			j9tty_printf( PORTLIB, "     -----   ---   -------   ----------\n");
			for (i=0; i < exceptionData->catchCount; i++) {
				j9tty_printf( PORTLIB, "     %5i%6i%10i   ",
					handler->startPC,
					handler->endPC,
					handler->handlerPC,
					0);

				if (handler->exceptionClassIndex) {
					dumpUTF(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) (&constantPool[handler->exceptionClassIndex])), portLib, 0);
					j9tty_printf( PORTLIB, "\n");
				} else {
					j9tty_printf( PORTLIB, "(any)\n");
				}
				handler += 1;
			}
		}

		if (exceptionData->throwCount) {
			j9tty_printf( PORTLIB, "  Thrown Exceptions (%i):\n", exceptionData->throwCount);
			currentThrowName = J9EXCEPTIONINFO_THROWNAMES(exceptionData);
			for (i=0; i < exceptionData->throwCount; i++) {
				J9UTF8 * currentName = NNSRP_PTR_GET(currentThrowName, J9UTF8 *);
				currentThrowName++;

				j9tty_printf( PORTLIB, "  ");
				dumpUTF( currentName, portLib, 0);
				j9tty_printf( PORTLIB,  "\n" );
			}
		}
	}

	if (J9ROMMETHOD_HAS_METHOD_PARAMETERS(romMethod)) {
		J9MethodParametersData * methodParametersData = methodParametersFromROMMethod(romMethod);
		J9MethodParameter * parameters = &methodParametersData->parameters;
		j9tty_printf( PORTLIB, "\n  Method Parameters (%i):\n", methodParametersData->parameterCount);
		for (i = 0; i < methodParametersData->parameterCount; i++) {
			J9UTF8 * parameterNameUTF8 = SRP_GET(parameters[i].name, J9UTF8 *);
			j9tty_printf( PORTLIB, "    ");
			if (NULL == parameterNameUTF8) {
				j9tty_printf( PORTLIB, "<no name>");
			} else {
				dumpUTF( parameterNameUTF8, portLib, 0);
			}
			j9tty_printf( PORTLIB, "    0x%x ( ", parameters[i].flags);
			printModifiers(PORTLIB, parameters[i].flags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_METHODPARAMETER, FALSE);
			j9tty_printf( PORTLIB, " )\n");
		}
		j9tty_printf( PORTLIB, "\n");
	}

	if ( romMethod->modifiers & CFR_ACC_NATIVE) {
		dumpNative( portLib, romMethod, flags );
	} else {
		dumpBytecodes( portLib, romClass, romMethod, flags );
	}

	dumpMethodDebugInfo( portLib, romClass, romMethod, flags );

	j9tty_printf( PORTLIB, "\n");

	return BCT_ERR_NO_ERROR;
}

/**
 * This method prints the modifiers for class, method, field, nested class or methodParameters.
 * Following is the list of valid modifiers for each element:
 *
 * 		:: CLASS ::
 * 			-ACC_PUBLIC
 *			-ACC_FINAL
 *			-ACC_SUPER
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
 *          See comments in ROMClassWriter::writeMethods
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
 *
 *		:: METHODPARAMETERS ::
 *			-ACC_FINAL
 *			-ACC_SYNTHETIC
 *			-ACC_MANDATED
 *
 *
 */

typedef struct J9ModifierInfo {
	U_32 flag; /* modifier bit */
	const char *modifierName; /* Description of the bit */
} J9ModifierInfo;

/**
 * iterate over a list of J9ModifierInfo, test each modifier bit, and print the corresponding modifier name
 * @param portLib port library
 * @param modifiers modifier word
 * @param modInfo array of J9ModifierInfo structs, terminated by a zero "flag" field
 * @return modifiers with recognized bits cleared
 */
static U_32
dumpModifierWord(J9PortLibrary *portLib, U_32 modifiers, J9ModifierInfo *modInfo) {
	PORT_ACCESS_FROM_PORT(portLib);
	J9ModifierInfo *modCursor = modInfo;
	U_32 modBuffer = modifiers;
	if (NULL != modCursor) {
		while ((0 != modBuffer) && (0 != modCursor->flag)) {
			if (J9_ARE_ANY_BITS_SET(modBuffer, modCursor->flag)) {
				j9tty_printf(PORTLIB, modCursor->modifierName);
				modBuffer &= ~modCursor->flag;
				if (0 != modBuffer) {
					j9tty_printf(PORTLIB, " ");
				}
			}
			++modCursor;
		}
	}
	return modBuffer;
}

void
printModifiers(J9PortLibrary *portLib, U_32 modifiers, modifierScope modScope, modifierSource modifierSrc, BOOLEAN valueTypeClass)
{
	PORT_ACCESS_FROM_PORT(portLib);

	switch (modifierSrc)
	{
		case MODIFIERSOURCE_CLASS :
			modifiers &= CFR_CLASS_ACCESS_MASK;
			break;

		case MODIFIERSOURCE_METHOD :
			if (ONLY_SPEC_MODIFIERS == modScope) {
				modifiers &= CFR_METHOD_ACCESS_MASK;
			}
			break;

		case MODIFIERSOURCE_FIELD :
			if (ONLY_SPEC_MODIFIERS == modScope) {
				modifiers &= CFR_FIELD_ACCESS_MASK;
			} else {
				modifiers &= (CFR_FIELD_ACCESS_MASK | J9FieldFlagConstant);
			}
			break;

		case MODIFIERSOURCE_METHODPARAMETER:
			if (ONLY_SPEC_MODIFIERS == modScope) {
				modifiers &= CFR_ATTRIBUTE_METHOD_PARAMETERS_MASK;
			} else {
				/* We dont have any internal modifiers now, once we have them, they need to be or'ed with the mask below.  */
				modifiers &= CFR_ATTRIBUTE_METHOD_PARAMETERS_MASK;
			}
			break;

		default :
			j9tty_printf(PORTLIB, "TYPE OF MODIFIER IS INVALID");
			return;
	}

	/* Parse internal flags first.
	 * Since we might be using same bit for internal purposes if we know that bit can not be used.
	 * For instance, Class (not nested class) can not be protected or private, so these bits can be used internally.
	 */
	if (INCLUDE_INTERNAL_MODIFIERS == modScope) {
		if (MODIFIERSOURCE_METHOD == modifierSrc) {
			J9ModifierInfo modInfo[] = {
					{CFR_ACC_EMPTY_METHOD , "(empty)"},
					{CFR_ACC_FORWARDER_METHOD, "(forwarder)"},
					{CFR_ACC_VTABLE, "(vtable)"},
					{CFR_ACC_HAS_EXCEPTION_INFO, "(hasExceptionInfo)"},
					{CFR_ACC_METHOD_HAS_DEBUG_INFO, "(has debug info)"},
					{CFR_ACC_METHOD_FRAME_ITERATOR_SKIP, "(method frame iterator skip)"},
					{CFR_ACC_METHOD_CALLER_SENSITIVE, "(caller sensitive)"},
					{CFR_ACC_METHOD_HAS_STACK_MAP, "(has stack map)"},
					{J9AccMethodHasBackwardBranches, "(has backward branches)"},
					{J9AccMethodObjectConstructor, "(method object constructor)"},
					{J9AccMethodHasMethodParameters, "(has method parameters)"},
					{J9AccMethodHasGenericSignature, "(has generic signature)"},
					{J9AccMethodHasExtendedModifiers, "(has extended modifiers)"},
					{J9AccMethodHasMethodHandleInvokes, "(has method handle invokes)"},
					{J9AccMethodHasMethodAnnotations, "(has method annotations)"},
					{J9AccMethodHasParameterAnnotations, "(has parameter annotations)"},
					{J9AccMethodHasDefaultAnnotation, "(has default annotation)"},
					{J9AccMethodAllowFinalFieldWrites, "(allows final field writes)"},
					{0, ""} /* terminator */
			};
			modifiers = dumpModifierWord(portLib, modifiers,modInfo);
		}

		if (MODIFIERSOURCE_FIELD == modifierSrc) {
			if (modifiers & J9FieldFlagConstant)
			{
				j9tty_printf(PORTLIB, "(constant)");
				modifiers &= ~J9FieldFlagConstant;
				if (modifiers) j9tty_printf(PORTLIB, " ");
			}
		}
	}

	j9tty_printf(PORTLIB, "\n  access: ");

	/* Method params don't use the scope modifiers. Scope is always within a method */
	if (MODIFIERSOURCE_METHODPARAMETER != modifierSrc) {
		if(0 == (modifiers & CFR_PUBLIC_PRIVATE_PROTECTED_MASK)){
			j9tty_printf(PORTLIB, "default");
			if(modifiers) j9tty_printf(PORTLIB, " ");
		} else {
			if(modifiers & CFR_ACC_PUBLIC)
			{
				j9tty_printf(PORTLIB, "public");
				modifiers &= ~CFR_ACC_PUBLIC;
				if(modifiers) j9tty_printf(PORTLIB, " ");
			}

			if(modifiers & CFR_ACC_PRIVATE)
			{
				j9tty_printf(PORTLIB, "private");
				modifiers &= ~CFR_ACC_PRIVATE;
				if(modifiers) j9tty_printf(PORTLIB, " ");
			}

			if(modifiers & CFR_ACC_PROTECTED)
			{
				j9tty_printf(PORTLIB, "protected");
				modifiers &= ~CFR_ACC_PROTECTED;
				if(modifiers) j9tty_printf(PORTLIB, " ");
			}
		}
	}

	if(modifiers & CFR_ACC_STATIC)
	{
		j9tty_printf(PORTLIB, "static");
		modifiers &= ~CFR_ACC_STATIC;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if(modifiers & CFR_ACC_FINAL)
	{
		j9tty_printf(PORTLIB, "final");
		modifiers &= ~CFR_ACC_FINAL;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if(modifiers & CFR_ACC_SYNTHETIC)
	{
		j9tty_printf(PORTLIB, "synthetic");
		modifiers &= ~CFR_ACC_SYNTHETIC;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if (MODIFIERSOURCE_METHODPARAMETER == modifierSrc) {
		if (CFR_ACC_MANDATED & modifiers) {
			j9tty_printf(PORTLIB, "mandated");
			modifiers &= ~CFR_ACC_MANDATED;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}
	}

	if (MODIFIERSOURCE_FIELD != modifierSrc) {
		if(modifiers & CFR_ACC_ABSTRACT)
		{
			j9tty_printf(PORTLIB, "abstract");
			modifiers &= ~CFR_ACC_ABSTRACT;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}

		if(modifiers & CFR_ACC_ENUM)
		{
			j9tty_printf(PORTLIB, "enum");
			modifiers &= ~CFR_ACC_ENUM;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}
	}

	if ((MODIFIERSOURCE_CLASS == modifierSrc)) {
		if(modifiers & CFR_ACC_INTERFACE)
		{
			j9tty_printf(PORTLIB, "interface");
			modifiers &= ~CFR_ACC_INTERFACE;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}

		if(modifiers & CFR_ACC_SUPER)
		{
			j9tty_printf(PORTLIB, "super");
			modifiers &= ~CFR_ACC_SUPER;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}

		if(modifiers & CFR_ACC_ANNOTATION)
		{
			j9tty_printf(PORTLIB, "annotation");
			modifiers &= ~CFR_ACC_ANNOTATION;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}
	}

	if (MODIFIERSOURCE_METHOD == modifierSrc) {
		if(modifiers & CFR_ACC_SYNCHRONIZED)
		{
			j9tty_printf(PORTLIB, "synchronized");
			modifiers &= ~CFR_ACC_SYNCHRONIZED;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}

		if(modifiers & CFR_ACC_BRIDGE)
		{
			j9tty_printf(PORTLIB, "bridge");
			modifiers &= ~CFR_ACC_BRIDGE;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}

		if(modifiers & CFR_ACC_VARARGS)
		{
			j9tty_printf(PORTLIB, "varargs");
			modifiers &= ~CFR_ACC_VARARGS;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}
		if(modifiers & CFR_ACC_NATIVE)
		{
			j9tty_printf(PORTLIB, "native");
			modifiers &= ~CFR_ACC_NATIVE;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}

		if(modifiers & CFR_ACC_STRICT)
		{
			j9tty_printf(PORTLIB, "strict");
			modifiers &= ~CFR_ACC_STRICT;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}
	}

	if (MODIFIERSOURCE_FIELD == modifierSrc) {
		if (modifiers & CFR_ACC_VOLATILE)
		{
			j9tty_printf(PORTLIB, "volatile");
			modifiers &= ~CFR_ACC_VOLATILE;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}

		if (modifiers & CFR_ACC_TRANSIENT)
		{
			j9tty_printf(PORTLIB, "transient");
			modifiers &= ~CFR_ACC_TRANSIENT;
			if(modifiers) j9tty_printf(PORTLIB, " ");
		}
	}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	if (MODIFIERSOURCE_CLASS == modifierSrc && valueTypeClass) {
		j9tty_printf(PORTLIB, "value");
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

	if (modifiers) j9tty_printf(PORTLIB, "unknown_flags = 0x%X" , modifiers);
}

void
j9_printClassExtraModifiers(J9PortLibrary *portLib, U_32 modifiers)
{
	U_32 originalModifiers = modifiers;
	PORT_ACCESS_FROM_PORT(portLib);

	if(modifiers & CFR_ACC_REFERENCE_WEAK)
	{
		j9tty_printf(PORTLIB, "(weak)");
		modifiers &= ~CFR_ACC_REFERENCE_WEAK;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if(modifiers & CFR_ACC_REFERENCE_SOFT)
	{
		j9tty_printf(PORTLIB, "(soft)");
		modifiers &= ~CFR_ACC_REFERENCE_SOFT;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if(modifiers & CFR_ACC_REFERENCE_PHANTOM)
	{
		j9tty_printf(PORTLIB, "(phantom)");
		modifiers &= ~CFR_ACC_REFERENCE_PHANTOM;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if(modifiers & CFR_ACC_HAS_FINAL_FIELDS)
	{
		j9tty_printf(PORTLIB, "(final fields)");
		modifiers &= ~CFR_ACC_HAS_FINAL_FIELDS;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if(modifiers & CFR_ACC_HAS_VERIFY_DATA)
	{
		j9tty_printf(PORTLIB, "(preverified)");
		modifiers &= ~CFR_ACC_HAS_VERIFY_DATA;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if(modifiers & J9AccClassIsUnmodifiable)
	{
		j9tty_printf(PORTLIB, "(unmodifiable)");
		modifiers &= ~J9AccClassIsUnmodifiable;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if(modifiers & J9AccRecord) {
		j9tty_printf(PORTLIB, "(record)");
		modifiers &= ~J9AccRecord;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}

	if(modifiers & J9AccSealed) {
		j9tty_printf(PORTLIB, "(sealed)");
		modifiers &= ~J9AccSealed;
		if(modifiers) j9tty_printf(PORTLIB, " ");
	}
}

static void
printMethodExtendedModifiers(J9PortLibrary *portLib, U_32 modifiers)
{
	U_32 originalModifiers = modifiers;
	PORT_ACCESS_FROM_PORT(portLib);

	if (J9_ARE_ANY_BITS_SET(modifiers, CFR_METHOD_EXT_HAS_METHOD_TYPE_ANNOTATIONS)) {
		j9tty_printf(PORTLIB, "(type annotations in method_info)");
		modifiers &= ~CFR_METHOD_EXT_HAS_METHOD_TYPE_ANNOTATIONS;
		if (0 != modifiers) {
			j9tty_printf(PORTLIB, " ");
		}
	}

	if (J9_ARE_ANY_BITS_SET(modifiers, CFR_METHOD_EXT_HAS_CODE_TYPE_ANNOTATIONS)) {
		j9tty_printf(PORTLIB, "(type annotations in code)");
		modifiers &= ~CFR_METHOD_EXT_HAS_CODE_TYPE_ANNOTATIONS;
		if (0 != modifiers) {
			j9tty_printf(PORTLIB, " ");
		}
	}

#if JAVA_SPEC_VERSION >= 16
	if (J9_ARE_ANY_BITS_SET(modifiers, CFR_METHOD_EXT_HAS_SCOPED_ANNOTATION)) {
		j9tty_printf(PORTLIB, "(scoped annotation)");
		modifiers &= ~CFR_METHOD_EXT_HAS_SCOPED_ANNOTATION;
		if (0 != modifiers) {
			j9tty_printf(PORTLIB, " ");
		}
	}
#endif /* JAVA_SPEC_VERSION >= 16*/

#if defined(J9VM_OPT_CRIU_SUPPORT)
	if (J9_ARE_ANY_BITS_SET(modifiers, CFR_METHOD_EXT_NOT_CHECKPOINT_SAFE_ANNOTATION)) {
		j9tty_printf(PORTLIB, "(NotCheckpointSafe annotation)");
		modifiers &= ~CFR_METHOD_EXT_NOT_CHECKPOINT_SAFE_ANNOTATION;
		if (0 != modifiers) {
			j9tty_printf(PORTLIB, " ");
		}
	}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
}



