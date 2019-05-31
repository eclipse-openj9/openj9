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

#include "j9.h"
#include "cfdumper_internal.h"
#include "util_api.h"
#include "romclasswalk.h"

#include <stdlib.h>
#include <ctype.h>

/* Constants and types. */

#define J9ROM_SECTION_START 1000
#define J9ROM_SECTION_END   2000

typedef struct J9ROMClassRegion {
	UDATA offset;
	UDATA length;
	UDATA type;
	BOOLEAN pointsToUTF8;
	const char *name;
} J9ROMClassRegion;

typedef struct J9ROMClassGatherLayoutInfoState {
	J9ROMClassValidateRangeCallback validateRangeCallback;
	J9Pool *regionsPool;
	J9ROMClassRegion **sortedRegions;
	UDATA numRegions;
	J9UTF8 *firstUTF8;
	J9UTF8 *lastUTF8;
} J9ROMClassGatherLayoutInfoState;

typedef struct J9ROMClassQueryComponent {
	UDATA componentIndex;
	const char *name;
	UDATA nameLength;
	UDATA arrayIndex;
	struct J9ROMClassQueryComponent *prev;
	struct J9ROMClassQueryComponent *next;
} J9ROMClassQueryComponent;


/* Private function declarations. */

static const char *getTypeName(UDATA type);
static UDATA getTypeSize(UDATA type);
static J9ROMClassRegion* addRegion(J9Pool *regionPool, UDATA offset, UDATA length, UDATA type, const char *name);
static UDATA getUTF8Length(J9UTF8 *utf8);
static void escapeUTF8(J9PortLibrary *portLib, J9UTF8 *utf8, char *str, size_t strSize);
static void addUTF8Region(J9ROMClassGatherLayoutInfoState *state, J9ROMClass *romClass, J9UTF8 *utf8, const char *name);
static void addUTF8RegionSRP(J9ROMClassGatherLayoutInfoState *state, J9ROMClass *romClass, J9SRP *srpPtr, const char *name);
static void addNASRegion(J9ROMClassGatherLayoutInfoState *state, J9ROMClass *romClass, J9SRP *srpPtr, const char *name);
static void addSlotCallback(J9ROMClass *romClass, U_32 type, void *slotPtr, const char *slotName, void *userData);
static void addSectionCallback(J9ROMClass *romClass, void *address, UDATA length, const char *name, void *userData);
static int compareRegions(const void *s1, const void *s2);
static BOOLEAN gatherROMClassLayoutInfo(J9PortLibrary *portLib, J9ROMClass *romClass,
	J9ROMClassValidateRangeCallback validateRangeCallback, J9ROMClassGatherLayoutInfoState *state);
static void cleanupGatherROMClassLayoutInfoState(J9PortLibrary *portLib, J9ROMClassGatherLayoutInfoState *state);
static void getRegionValueString(J9PortLibrary *portLib, J9ROMClass *romClass, J9ROMClassRegion *region,
	char *str, size_t strSize);
static void getRegionDetailString(J9PortLibrary *portLib, J9ROMClassGatherLayoutInfoState *state,
	J9ROMClass *romClass, J9ROMClassRegion *region, char *str, size_t strSize, UDATA base);
static void printRegionLine(J9PortLibrary *portLib, J9ROMClassGatherLayoutInfoState *state,
	J9ROMClass *romClass, J9ROMClassRegion *section, UDATA base);
static BOOLEAN isSameRegion(J9ROMClassRegion *region1, J9ROMClassRegion *region2);
static void reportSuspectedPadding(J9PortLibrary *portLib, J9ROMClass *romClass,
	J9ROMClassGatherLayoutInfoState *state, UDATA lastOffset, UDATA offset, UDATA base);
static J9ROMClassQueryComponent* parseROMQuery(J9Pool *allocator, const char *query);
static BOOLEAN regionNameMatchesComponent(J9ROMClassRegion *region, J9ROMClassQueryComponent *component);

/* Function definitions. */

static const char*
getTypeName(UDATA type)
{
	switch (type) {
		case J9ROM_U8:
			return "U_8";
		case J9ROM_U16:
			return "U_16";
		case J9ROM_U32:
			return "U_32";
		case J9ROM_U64:
			return "U_64";
		case J9ROM_UTF8:
			return "J9UTF8";
		case J9ROM_SRP:
			return "J9SRP";
		case J9ROM_WSRP:
			return "J9WSRP";
	}

	return "unknown";
}

static UDATA
getTypeSize(UDATA type)
{
	switch (type) {
		case J9ROM_U8:
			return sizeof(U_8);
		case J9ROM_U16:
			return sizeof(U_16);
		case J9ROM_U32:
			return sizeof(U_32);
		case J9ROM_U64:
			return sizeof(U_64);
		case J9ROM_SRP:
			return sizeof(J9SRP);
		case J9ROM_WSRP:
			return sizeof(J9WSRP);
	}

	return 0;
}

static J9ROMClassRegion*
addRegion(J9Pool *regionPool, UDATA offset, UDATA length, UDATA type, const char *name)
{
	J9ROMClassRegion *region;

	region = pool_newElement(regionPool);
	if (NULL != region) {
		region->offset = offset;
		region->length = length;
		region->type = type;
		region->pointsToUTF8 = FALSE;
		region->name = name;
	}

	return region;
}

static UDATA
getUTF8Length(J9UTF8 *utf8)
{
	UDATA length = sizeof(J9UTF8) + J9UTF8_LENGTH(utf8) - sizeof(J9UTF8_DATA(utf8));
	if (length & 1) {
		length++;
	}
	return length;
}

static void
escapeUTF8(J9PortLibrary *portLib, J9UTF8 *utf8, char *str, size_t strSize)
{
	U_8 *data = (U_8 *) J9UTF8_DATA(utf8);
	const U_8 *end = data + J9UTF8_LENGTH(utf8);

	PORT_ACCESS_FROM_PORT(portLib);

	while ((data < end) && (strSize > 0)) {
		U_16 c;
		U_32 charLength;
		UDATA escapedLength;

		charLength = decodeUTF8Char(data, &c);

		if ((charLength > 1) || !isprint((int)c)) {

			if (strSize < 6) {
				/* Truncate on character boundaries only. */
				break;
			}

			escapedLength = j9str_printf(PORTLIB, str, strSize, "\\u%04x", (UDATA)c);
		} else {
			escapedLength = j9str_printf(PORTLIB, str, strSize, "%c", (char)c);
		}

		strSize -= (size_t)escapedLength;
		str += escapedLength;
		data += charLength;
	}
}

static void
addUTF8Region(J9ROMClassGatherLayoutInfoState *state, J9ROMClass *romClass, J9UTF8 *utf8, const char *name)
{
	UDATA base, offset, length;
	if (NULL != utf8) {

		base = (UDATA)romClass;
		offset = (UDATA)utf8;

		if ((offset >= base) && (offset < (base + romClass->romSize))) {
			length = getUTF8Length(utf8);
			addRegion(state->regionsPool, offset - base, length, J9ROM_UTF8, name);
			if (NULL == state->firstUTF8) {
				state->firstUTF8 = utf8;
				state->lastUTF8 = utf8;
			} else if (utf8 < state->firstUTF8) {
				state->firstUTF8 = utf8;
			} else if (utf8 > state->lastUTF8) {
				state->lastUTF8 = utf8;
			}
		}
	}
}

static void
addUTF8RegionSRP(J9ROMClassGatherLayoutInfoState *state, J9ROMClass *romClass, J9SRP *srpPtr, const char *name)
{
	J9UTF8 *utf8;
	utf8 = SRP_PTR_GET(srpPtr, J9UTF8 *);

	if (NULL != utf8) {
		addUTF8Region(state, romClass, utf8, name);
	}
}

static void
addNASRegion(J9ROMClassGatherLayoutInfoState *state, J9ROMClass *romClass, J9SRP *srpPtr, const char *name)
{
	J9ROMNameAndSignature *nas;
	UDATA base, offset;

	nas = SRP_PTR_GET(srpPtr, J9ROMNameAndSignature *);

	if (NULL != nas) {

		base = (UDATA)romClass;
		offset = (UDATA)nas;

		if ((offset >= base) && (offset < (base + romClass->romSize))) {
			addRegion(state->regionsPool, offset - base, sizeof(J9ROMNameAndSignature), J9ROM_SRP, "nameAndSignatureSRP");
			addUTF8RegionSRP(state, romClass, &nas->name, "name");
			addUTF8RegionSRP(state, romClass, &nas->signature, "signature");
		}
	}
}

static void
addSlotCallback(J9ROMClass *romClass, U_32 type, void *slotPtr, const char *slotName, void *userData)
{
	J9ROMClassGatherLayoutInfoState *state = userData;
	J9Pool *regionPool = state->regionsPool;
	J9ROMClassRegion *region;
	UDATA base, size;

	base = (UDATA)slotPtr - (UDATA)romClass;
	size = getTypeSize(type);

	/* Simple types. */
	if (0 != size) {
		addRegion(regionPool, base, size, type, slotName);
		return;
	}

	/* Mapped types. */
	switch (type) {
		case J9ROM_UTF8:
			addUTF8RegionSRP(state, romClass, (J9SRP *)slotPtr, slotName);
			region = addRegion(regionPool, base, sizeof(J9SRP), J9ROM_SRP, slotName);
			if (NULL != region) {
				region->pointsToUTF8 = TRUE;
			}
			break;
		case J9ROM_UTF8_NOSRP:
			addUTF8Region(state, romClass, (J9UTF8 *)slotPtr, slotName);
			break;
		case J9ROM_NAS:
			addNASRegion(state, romClass, (J9SRP *)slotPtr, slotName);
			addRegion(regionPool, base, sizeof(J9SRP), J9ROM_SRP, slotName);
			break;
		case J9ROM_AOT:
			addRegion(regionPool, base, sizeof(J9SRP), J9ROM_SRP, slotName);
			break;
		case J9ROM_INTERMEDIATECLASSDATA:
			addRegion(regionPool, base, romClass->intermediateClassDataLength, type, slotName);
	}
}

static void
addSectionCallback(J9ROMClass *romClass, void *address, UDATA length, const char *name, void *userData)
{
	J9ROMClassGatherLayoutInfoState *state = userData;
	J9Pool *regionPool = state->regionsPool;
	UDATA base, end;

	if (0 != length) {
		base = (UDATA)address - (UDATA)romClass;
		end = base + length;

		addRegion(regionPool, base, length, J9ROM_SECTION_START, name);
		addRegion(regionPool, end,  length, J9ROM_SECTION_END,   name);
	}
}

static int
compareRegions(const void *r1, const void *r2)
{
	J9ROMClassRegion *region1 = *(J9ROMClassRegion **) r1;
	J9ROMClassRegion *region2 = *(J9ROMClassRegion **) r2;
	int delta = (int)(region1->offset - region2->offset);

	if (0 != delta) {
		return delta;
	}

	if (J9ROM_SECTION_START == region1->type) {
		if (J9ROM_SECTION_START == region2->type) {
			/* start of longest section first, if equal length, region with longer name goes first
			 * e.g., fields over field */
			if (region2->length == region1->length) {
				return (int)(strlen(region2->name)-strlen(region1->name));
			}
			return (int)(region2->length - region1->length);
		} else if (J9ROM_SECTION_END == region2->type) {
			/* previous section ends before next section starts */
			return 1;
		}
	} else if (J9ROM_SECTION_END == region1->type) {
		if (J9ROM_SECTION_END == region2->type) {
			/* end of longest section last if equal length, region with longer name goes last
			 * e.g., fields after field */
			if (region2->length == region1->length) {
				return (int)(strlen(region1->name)-strlen(region2->name));
			}
			return (int)(region1->length - region2->length);
		} else if (J9ROM_SECTION_START == region2->type) {
			/* previous section ends before next section starts */
			return -1;
		}
	}

	return (int)(region2->type - region1->type);
}

static BOOLEAN
gatherROMClassLayoutInfo(J9PortLibrary *portLib, J9ROMClass *romClass,
	J9ROMClassValidateRangeCallback validateRangeCallback, J9ROMClassGatherLayoutInfoState *state)
{
	pool_state walkState;
	UDATA i;

	PORT_ACCESS_FROM_PORT(portLib);

	memset(state, 0, sizeof(J9ROMClassGatherLayoutInfoState));

	state->validateRangeCallback = validateRangeCallback;
	state->regionsPool = pool_new(sizeof(J9ROMClassRegion), 64, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(PORTLIB));

	if (NULL == state->regionsPool) {
		j9tty_printf(PORTLIB, "Error: Could not create J9Pool.\n");
		return FALSE;
	}

	allSlotsInROMClassDo(romClass, addSlotCallback, addSectionCallback, state->validateRangeCallback, state);

	/* Add section for UTF8s. */
	if (NULL != state->lastUTF8) {
		J9UTF8 *utf8;
		UDATA utf8Length;
		UDATA sectionLength;

		utf8 = state->lastUTF8;
		utf8Length = getUTF8Length(utf8);

		sectionLength = (UDATA)utf8 + utf8Length - (UDATA)state->firstUTF8;
		addSectionCallback(romClass, state->firstUTF8, sectionLength, "UTF8s", state);
	}

	state->numRegions = pool_numElements(state->regionsPool);
	if (0 == state->numRegions) {
		j9tty_printf(PORTLIB, "Error: Failed to walk ROM class slots.\n");
		pool_kill(state->regionsPool);
		return FALSE;
	}

	state->sortedRegions = j9mem_allocate_memory(state->numRegions * sizeof(J9ROMClassRegion *), J9MEM_CATEGORY_CLASSES);
	if (NULL == state->sortedRegions) {
		j9tty_printf(PORTLIB, "Error: Could not create sortedSections array.\n");
		pool_kill(state->regionsPool);
		return FALSE;
	}

	state->sortedRegions[0] = pool_startDo(state->regionsPool, &walkState);
	for (i = 1; i < state->numRegions; i++) {
		state->sortedRegions[i] = pool_nextDo(&walkState);
	}
	qsort(state->sortedRegions, state->numRegions, sizeof(J9ROMClassRegion *), compareRegions);

	return TRUE;
}

static void cleanupGatherROMClassLayoutInfoState(J9PortLibrary *portLib, J9ROMClassGatherLayoutInfoState *state)
{
	PORT_ACCESS_FROM_PORT(portLib);

	j9mem_free_memory(state->sortedRegions);
	pool_kill(state->regionsPool);
}

static BOOLEAN
isSameRegion(J9ROMClassRegion *region1, J9ROMClassRegion *region2)
{
	BOOLEAN sameRegion =
		(region1->type == region2->type) &&
		(region1->offset == region2->offset) &&
		(region1->length == region2->length);

	/* Compare section names to not discard nested sections of same size. */
	if (sameRegion && ((J9ROM_SECTION_START == region1->type) || (J9ROM_SECTION_END == region1->type))) {
		sameRegion = !strcmp(region1->name, region2->name);
	}

	return sameRegion;
}

static void
printRegionLine(J9PortLibrary *portLib, J9ROMClassGatherLayoutInfoState *state,
	J9ROMClass *romClass, J9ROMClassRegion *region, UDATA base)
{
	char valueString[256];
	char detailString[256];

	PORT_ACCESS_FROM_PORT(portLib);

	getRegionValueString(portLib, romClass, region, valueString, sizeof(valueString));
	getRegionDetailString(portLib, state, romClass, region, detailString, sizeof(detailString), base);

	j9tty_printf(PORTLIB, "0x%p-0x%p [ %12s %-28s ]%s\n", base + region->offset,
		base + region->offset + region->length, valueString, region->name, detailString);
}

static void
getRegionValueString(J9PortLibrary *portLib, J9ROMClass *romClass, J9ROMClassRegion *region, char *str, size_t strSize)
{
	void *fieldPtr;

	PORT_ACCESS_FROM_PORT(portLib);

	fieldPtr = (void *)((UDATA)romClass + region->offset);

	switch (region->type) {
		case J9ROM_U8:
			j9str_printf(PORTLIB, str, strSize, "%12u", *(U_8 *)fieldPtr);
			return;
		case J9ROM_U16:
			j9str_printf(PORTLIB, str, strSize, "%12u", *(U_16 *)fieldPtr);
			return;
		case J9ROM_U32:
			j9str_printf(PORTLIB, str, strSize, "%12u", *(U_32 *)fieldPtr);
			return;
		case J9ROM_U64:
			j9str_printf(PORTLIB, str, strSize, "%12llu", *(U_64 *)fieldPtr);
			return;
		case J9ROM_UTF8:
			j9str_printf(PORTLIB, str, strSize, "(UTF-8)");
			return;
		case J9ROM_SRP:
			j9str_printf(PORTLIB, str, strSize, "0x%08x", *(J9SRP *)fieldPtr);
			return;
		case J9ROM_WSRP:
			j9str_printf(PORTLIB, str, strSize, "0x%08x", *(J9WSRP *)fieldPtr);
			return;
		case J9ROM_SECTION_END:
			j9str_printf(PORTLIB, str, strSize, "(SECTION)");
			return;
		case J9ROM_INTERMEDIATECLASSDATA:
			j9str_printf(PORTLIB, str, strSize, "");
			return;
	}

	j9str_printf(PORTLIB, str, strSize, "<error>");
}

static void
getRegionDetailString(J9PortLibrary *portLib, J9ROMClassGatherLayoutInfoState *state,
	J9ROMClass *romClass, J9ROMClassRegion *region, char *str, size_t strSize, UDATA base)
{
	void *fieldPtr;

	PORT_ACCESS_FROM_PORT(portLib);

	fieldPtr = (void *)((UDATA)romClass + region->offset);

	*str = '\0';
	if (J9ROM_UTF8 == region->type) {
		J9UTF8 *utf8 = fieldPtr;

		if (NULL != utf8) {
			str[0] = ' ';
			escapeUTF8(PORTLIB, utf8, str + 1, strSize - 1);
		}
	} else if (J9ROM_SRP == region->type) {
		BOOLEAN printedUTF8 = FALSE;

		if (region->pointsToUTF8) {
			J9UTF8 *utf8 = SRP_PTR_GET((J9SRP*)fieldPtr, J9UTF8*);

			if (NULL != utf8) {
				BOOLEAN rangeValid = TRUE;

				if (NULL != state->validateRangeCallback) {
					rangeValid = state->validateRangeCallback(romClass, utf8, sizeof(J9UTF8), NULL);
					if (rangeValid) {
						rangeValid = state->validateRangeCallback(romClass, utf8, getUTF8Length(utf8), NULL);
					}
				}

				if (rangeValid) {
					UDATA length = j9str_printf(PORTLIB, str, strSize, " -> ");

					escapeUTF8(PORTLIB, utf8, str + length, strSize - length);
					printedUTF8 = TRUE;
				}
			}
		}

		if (FALSE == printedUTF8) {
			U_8 *addr = SRP_PTR_GET((J9SRP*)fieldPtr, U_8*);

			if (NULL != addr) {
				j9str_printf(PORTLIB, str, strSize, " -> 0x%p%s", (UDATA)addr - (UDATA)romClass,
					((UDATA)addr - (UDATA)romClass) > romClass->romSize ? " (external)" : "");
			}
		}
	} else if (J9ROM_INTERMEDIATECLASSDATA == region->type) {
		j9str_printf(PORTLIB, str, strSize, " %5d bytes  !j9x 0x%p,0x%p", region->length, base + region->offset, region->length);
	}

	if (J9ROM_SECTION_END == region->type) {
		j9str_printf(PORTLIB, str, strSize, " %5d bytes", region->length);
	}
}

static void
reportSuspectedPadding(J9PortLibrary *portLib, J9ROMClass *romClass, J9ROMClassGatherLayoutInfoState *state, UDATA lastOffset, UDATA offset, UDATA base)
{
	IDATA i = 0;
	IDATA nbytes = 0;
	U_8 *romBase = (U_8 *)romClass;
	BOOLEAN rangeValid = TRUE;

	PORT_ACCESS_FROM_PORT(portLib);
	nbytes = offset - lastOffset;
	if (nbytes < 0) {
		nbytes = 0;
	} else if (nbytes > 6) {
		nbytes = 6;
	}
	j9tty_printf(PORTLIB, "0x%p-0x%p [ %*s", base + lastOffset, base + offset, 12 - (2 * nbytes), "");

	if (NULL != state->validateRangeCallback) {
		rangeValid = state->validateRangeCallback(romClass, romBase + lastOffset, nbytes, NULL);
	}
	if (rangeValid) {
		for (i = 0; i < nbytes; i++) {
			j9tty_printf(PORTLIB, "%02x", romBase[i + lastOffset]);
		}
	}

	j9tty_printf(PORTLIB, " %-28s ] %5d bytes\n", "padding", offset - lastOffset);
}

void
j9bcutil_linearDumpROMClass(J9PortLibrary *portLib, J9ROMClass *romClass, void *baseAddress, UDATA nestingThreshold, J9ROMClassValidateRangeCallback validateRangeCallback)
{
	J9ROMClassGatherLayoutInfoState state;
	UDATA i, lastOffset, totalMissing, nesting, base;
	char buf[256];
	BOOLEAN success;

	PORT_ACCESS_FROM_PORT(portLib);

	success = gatherROMClassLayoutInfo(portLib, romClass, validateRangeCallback, &state);
	if (FALSE == success) {
		return;
	}

	lastOffset = 0;
	totalMissing = 0;
	nesting = 1;
	base = (UDATA)baseAddress;

	for (i = 0; i < state.numRegions; i++) {
		J9ROMClassRegion *region = state.sortedRegions[i];

		/* Skip duplicates (e.g. if there were two SRPs to the same thing). */
		if ((i > 0) && isSameRegion(region, state.sortedRegions[i - 1])) {
			continue;
		}

		if (J9ROM_SECTION_START == region->type) {
			if (region->offset != lastOffset) {
				reportSuspectedPadding(portLib, romClass, &state, lastOffset, region->offset, base);
				totalMissing += region->offset - lastOffset;
				lastOffset = region->offset;
			}
			if (nesting < nestingThreshold) {
				j9str_printf(PORTLIB, buf, sizeof(buf), "Section Start: %s (%d bytes)", region->name, region->length);
				j9tty_printf(PORTLIB, "=== %-59s ===\n", buf);
			}
			nesting++;
		} else if (J9ROM_SECTION_END == region->type) {
			nesting--;
			if (nesting < nestingThreshold) {
				if (lastOffset != region->offset) {
					if (nesting <= nestingThreshold) {
						reportSuspectedPadding(portLib, romClass, &state, lastOffset, region->offset, base);
					}
				}
				j9str_printf(PORTLIB, buf, sizeof(buf), "Section End: %s", region->name);
				j9tty_printf(PORTLIB, "=== %-59s ===\n", buf);
			} else if (nesting == nestingThreshold) {
				printRegionLine(portLib, &state, romClass, region, base - region->length);
			}
			if (lastOffset != region->offset) {
				/* found some padding
				 * TODO: add an ensure '0' function to make sure that padding is in fact 0
				 * */
				lastOffset = region->offset;
			}
		} else {
			/* Do not print regions or padding if they are nested under the nestingThreshold */
			if (nesting <= nestingThreshold) {
				if (region->offset != lastOffset) {
					reportSuspectedPadding(portLib, romClass, &state, lastOffset, region->offset, base);
					totalMissing += region->offset - lastOffset;
				}
				printRegionLine(portLib, &state, romClass, region, base);
			}
			lastOffset = region->offset + region->length;
		}
	}

	if (romClass->romSize != lastOffset) {
		reportSuspectedPadding(portLib, romClass, &state, lastOffset, romClass->romSize, base);
		totalMissing += romClass->romSize - lastOffset;
	}

	if (0 == totalMissing) {
		j9tty_printf(PORTLIB, "\nAll bytes were accounted for.\n");
	} else {
		j9tty_printf(PORTLIB, "\nNOTE: %d bytes were not accounted for.\n", totalMissing);
	}

	cleanupGatherROMClassLayoutInfoState(portLib, &state);
}

static J9ROMClassQueryComponent*
parseROMQuery(J9Pool *allocator, const char *query)
{
	UDATA componentIndex = 0;
	J9ROMClassQueryComponent *head;
	J9ROMClassQueryComponent *component;

	head = pool_newElement(allocator);
	if (NULL == head) {
		return NULL;
	}
	component = head;
	component->componentIndex = -1;

	do {
		if ('/' != *query) {
			return NULL;
		}
		query++;

		component->next = pool_newElement(allocator);
		component->next->prev = component;
		component = component->next;
		component->componentIndex = componentIndex++;

		if (!isalnum(*query)) {
			return NULL;
		}

		component->name = query;
		do {
			query++;
			component->nameLength++;
		} while (isalnum(*query));

		if ('[' == *query) {
			query++;
			if (!isdigit(*query)) {
				return NULL;
			}

			component->arrayIndex = atoi(query);
			do {
				query++;
			} while (isdigit(*query));

			if (']' != *query) {
				return NULL;
			}
			query++;
		}
	} while ('\0' != *query);

	return head;
}

static BOOLEAN
regionNameMatchesComponent(J9ROMClassRegion *region, J9ROMClassQueryComponent *component)
{
	return (0 == strncmp(region->name, component->name, component->nameLength)) &&
			('\0' == region->name[component->nameLength]);
}

void
j9bcutil_queryROMClass(J9PortLibrary *portLib, J9ROMClass *romClass, void *baseAddress, const char **queries, UDATA numQueries, J9ROMClassValidateRangeCallback validateRangeCallback)
{
	J9ROMClassGatherLayoutInfoState state;
	J9Pool *queryComponentPool;
	UDATA base, queryIndex;

	PORT_ACCESS_FROM_PORT(portLib);

	queryComponentPool = pool_new(sizeof(J9ROMClassQueryComponent), 64, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(PORTLIB));
	if (NULL == queryComponentPool) {
		j9tty_printf(PORTLIB, "Error: Could not create queryComponentPool J9Pool.\n");
		return;
	}

	if (FALSE == gatherROMClassLayoutInfo(portLib, romClass, validateRangeCallback, &state)) {
		return;
	}

	base = (UDATA)baseAddress;

	for (queryIndex = 0; queryIndex < numQueries; queryIndex++) {
		UDATA i, nesting, currentArrayIndex;
		BOOLEAN queryMatched;
		J9ROMClassQueryComponent *componentsHead;
		J9ROMClassQueryComponent *componentToMatch;

		pool_clear(queryComponentPool);
		componentsHead = parseROMQuery(queryComponentPool, queries[queryIndex]);
		if (NULL == componentsHead) {
			j9tty_printf(PORTLIB, "Syntax error in query '%s'\n", queries[queryIndex]);
			continue;
		}

		queryMatched = FALSE;
		currentArrayIndex = -1;
		nesting = 0;
		componentToMatch = componentsHead->next;

		for (i = 0; i < state.numRegions; i++) {
			J9ROMClassRegion *region = state.sortedRegions[i];

			/* Skip duplicates (e.g. if there were two SRPs to the same thing). */
			if ((i > 0) && isSameRegion(region, state.sortedRegions[i - 1])) {
				continue;
			}

			/* Are we matching? */
			if (NULL != componentToMatch) {
				/* Does the nesting level correspond to what we're looking for? */
				if (nesting == componentToMatch->componentIndex) {
					if (J9ROM_SECTION_END != region->type) {
						/* Either a section start, or a field - check if it matches the next component. */
						if (regionNameMatchesComponent(region, componentToMatch)) {
							currentArrayIndex++;
							/* It's a match if the currentArrayIndex equals the arrayIndex of the component. */
							if (currentArrayIndex == componentToMatch->arrayIndex) {
								componentToMatch = componentToMatch->next;
								if (NULL == componentToMatch) {
									/* Done matching. Reset nesting for printing stage. */
									nesting = 0;
								} else {
									/* Reset currentArrayIndex for matching the next query component. */
									currentArrayIndex = -1;
								}
							}
						}
					} else if (regionNameMatchesComponent(region, componentToMatch->prev)) {
						/* This is the end of the section that matched the previous component. Query matched nothing. */
						break;
					}
				}

				if (NULL != componentToMatch) {
					if (J9ROM_SECTION_START == region->type) {
						nesting++;
					} else if (J9ROM_SECTION_END == region->type) {
						nesting--;
					}
				}
			}

			/* Are we printing? */
			if (NULL == componentToMatch) {
				char buf[256];

				if (J9ROM_SECTION_START == region->type) {
					j9str_printf(PORTLIB, buf, sizeof(buf), "Section Start: %s (%d bytes)", region->name, region->length);
					j9tty_printf(PORTLIB, "=== %-59s ===\n", buf);
					nesting++;
				} else if (J9ROM_SECTION_END == region->type) {
					nesting--;
					j9str_printf(PORTLIB, buf, sizeof(buf), "Section End: %s", region->name);
					j9tty_printf(PORTLIB, "=== %-59s ===\n", buf);
					if (0 == nesting) {
						queryMatched = TRUE;
						break;
					}
				} else {
					printRegionLine(portLib, &state, romClass, region, base);
					if (0 == nesting) {
						queryMatched = TRUE;
						break;
					}
				}
			}
		}

		if (FALSE == queryMatched) {
			j9tty_printf(PORTLIB, "Warning: Query '%s' matched nothing!\n", queries[queryIndex]);
		}
	}

	pool_kill(queryComponentPool);
	cleanupGatherROMClassLayoutInfoState(portLib, &state);
}

void
j9bcutil_queryROMClassCommaSeparated(J9PortLibrary *portLib, J9ROMClass *romClass, void *baseAddress, const char *commaSeparatedQueries, J9ROMClassValidateRangeCallback validateRangeCallback)
{
	char **queries;
	UDATA i, numQueries;
	char *string;
	UDATA stringLength;

	PORT_ACCESS_FROM_PORT(portLib);

	stringLength = strlen(commaSeparatedQueries);
	string = j9mem_allocate_memory(stringLength + 1, J9MEM_CATEGORY_CLASSES);
	if (NULL == string) {
		j9tty_printf(PORTLIB, "Insufficient memory to complete operation\n");
		return;
	}

	numQueries = 1;
	for (i = 0; i <= stringLength; i++) {
		string[i] = commaSeparatedQueries[i];
		if (',' == string[i]) {
			numQueries++;
		}
	}

	queries = j9mem_allocate_memory(sizeof(char *) * numQueries, J9MEM_CATEGORY_CLASSES);
	if (NULL == queries) {
		j9mem_free_memory(string);
		j9tty_printf(PORTLIB, "Insufficient memory to complete operation\n");
		return;
	}

	queries[0] = string;
	numQueries = 1;

	for (i = 0; i < stringLength; i++) {
		if (',' == string[i]) {
			string[i] = '\0';
			queries[numQueries++] = &string[i] + 1;
		}
	}

	j9bcutil_queryROMClass(portLib, romClass, baseAddress, (const char **)queries, numQueries, validateRangeCallback);

	j9mem_free_memory(queries);
	j9mem_free_memory(string);
}

static void
printIndentation(J9PortLibrary *portLib, UDATA n)
{
	UDATA i;
	PORT_ACCESS_FROM_PORT(portLib);

	for (i = 0; i < n; i++) {
		j9tty_printf(PORTLIB, "\t");
	}
}

static void
getRegionValueStringXML(J9PortLibrary *portLib, J9ROMClass *romClass, J9ROMClassRegion *region, char *str, size_t strSize)
{
	void *fieldPtr;

	PORT_ACCESS_FROM_PORT(portLib);

	fieldPtr = (void *)((UDATA)romClass + region->offset);

	switch (region->type) {
		case J9ROM_U8:
			j9str_printf(PORTLIB, str, strSize, "%u", *(U_8 *)fieldPtr);
			return;
		case J9ROM_U16:
			j9str_printf(PORTLIB, str, strSize, "%u", *(U_16 *)fieldPtr);
			return;
		case J9ROM_U32:
			j9str_printf(PORTLIB, str, strSize, "%u", *(U_32 *)fieldPtr);
			return;
		case J9ROM_U64:
			j9str_printf(PORTLIB, str, strSize, "%llu", *(U_64 *)fieldPtr);
			return;
		case J9ROM_UTF8: {
			J9UTF8 *utf8 = fieldPtr;
			escapeUTF8(PORTLIB, utf8, str, strSize);
			return;
		}
		case J9ROM_SRP:
			j9str_printf(PORTLIB, str, strSize, "0x%08x", *(J9SRP *)fieldPtr);
			return;
		case J9ROM_WSRP:
			j9str_printf(PORTLIB, str, strSize, "0x%08x", *(J9WSRP *)fieldPtr);
			return;
	}

	j9str_printf(PORTLIB, str, strSize, "error>");
}

void
j9bcutil_linearDumpROMClassXML(J9PortLibrary *portLib, J9ROMClass *romClass, J9ROMClassValidateRangeCallback validateRangeCallback)
{
	J9ROMClassGatherLayoutInfoState state;
	UDATA i, nesting;
	BOOLEAN success;

	PORT_ACCESS_FROM_PORT(portLib);

	success = gatherROMClassLayoutInfo(portLib, romClass, validateRangeCallback, &state);
	if (FALSE == success) {
		return;
	}

	j9tty_printf(PORTLIB, "<J9ROMClass>\n\n");
	nesting = 1;

	for (i = 0; i < state.numRegions; i++) {
		J9ROMClassRegion *region = state.sortedRegions[i];

		/* Skip duplicates (e.g. if there were two SRPs to the same thing). */
		if ((i > 0) && isSameRegion(region, state.sortedRegions[i - 1])) {
			continue;
		}

		if (J9ROM_SECTION_START == region->type) {
			printIndentation(portLib, nesting);
			j9tty_printf(PORTLIB, "<section name=\"%s\">\n", region->name);
			nesting++;
		} else if (J9ROM_SECTION_END == region->type) {
			nesting--;
			printIndentation(portLib, nesting);
			j9tty_printf(PORTLIB, "</section>\n");
		} else {
			const char *typeName;
			char value[256];

			typeName = getTypeName(region->type);
			getRegionValueStringXML(portLib, romClass, region, value, sizeof(value));

			printIndentation(portLib, nesting);
			j9tty_printf(PORTLIB, "<%s name=\"%s\">%s</%s>\n", typeName, region->name, value, typeName);
		}
	}
	j9tty_printf(PORTLIB, "</J9ROMClass>\n");

	cleanupGatherROMClassLayoutInfoState(portLib, &state);
}
