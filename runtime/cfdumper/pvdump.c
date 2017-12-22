/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
#include "j9protos.h"
#include "j9port.h"
#include "cfr.h"
#include "rommeth.h"

/* #defines from bcverify.h */
#define TAG_SPECIAL					1		/* clear bit means not a special object */
#define TAG_BASE_TYPE			2		/* clear bit means object or array */
#define TAG_BASE_ARRAY		4		/* set bit means base type array, clean bit means base type */

#define ARITY_MASK						0xFF000000
#define ARITY_SHIFT					24

#define CLASS_INDEX_MASK		0x00FFFFF0
#define CLASS_INDEX_SHIFT	4

#define TYPE_INT							0x02
#define TYPE_FLOAT 					0x03
#define TYPE_LONG 						0x04
#define TYPE_DOUBLE					0x05
#define TYPE_OBJECT					0x07
#define TYPE_INIT_OBJECT		0x08
#define TYPE_NEW_OBJECT		0x09
#define TYPE_ARG_OBJECT		0x0a
#define TYPE_LONG2					0x0c
#define TYPE_DOUBLE2				0x0d

#define BASE_TYPE_INT				0x010	
#define BASE_TYPE_FLOAT		0x020
#define BASE_TYPE_LONG			0x040
#define BASE_TYPE_DOUBLE	0x080
#define BASE_TYPE_SHORT		0x100
#define BASE_TYPE_BYTE			0x200
#define BASE_TYPE_CHAR			0x400 

/* #defines from bcvcfr.h */
#ifdef J9VM_ENV_LITTLE_ENDIAN
#define J9_READ_LITTLE_ENDIAN( value)	value
#define J9_READ_BIG_ENDIAN( value ) ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) | ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24)
#define J9_READ_LITTLE_ENDIAN16( value)	value
#define J9_READ_BIG_ENDIAN16( value ) ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8)
#else
#define J9_READ_LITTLE_ENDIAN( value ) ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) | ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24)
#define J9_READ_BIG_ENDIAN( value)	value
#define J9_READ_LITTLE_ENDIAN16( value)	((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8)
#define J9_READ_BIG_ENDIAN16( value ) 	value
#endif
#define J9_NEUTRAL_32( value ) 	(endian ? J9_READ_BIG_ENDIAN(value) :  J9_READ_LITTLE_ENDIAN(value))
#define J9_NEUTRAL_16( value ) 	(endian ? J9_READ_BIG_ENDIAN16(value) :  J9_READ_LITTLE_ENDIAN16(value))

/* Prototype from rcdump.c */

static UDATA decodeStackEntry (J9PortLibrary* portLib, U_8* stackDataPtr, UDATA endian);
static U_32 unalignedRead4 (U_8* ptr, UDATA isBigEndian );
static U_16 unalignedRead2 (U_8* ptr, UDATA isBigEndian );

static UDATA decodeStackDepth (J9PortLibrary* portLib, U_8* stackDataPtr, U_8* stackDataEnd);

static U_16 unalignedRead2(U_8* ptr, UDATA isBigEndian )
{
	if(isBigEndian) return (ptr[0] << 8) | ptr[1];
	else return (ptr[1] << 8) | ptr[0];
}


static U_32 unalignedRead4(U_8* ptr, UDATA isBigEndian )
{
	U_32 a,b,c,d;
	
	if(isBigEndian) {
		a = ptr[0];
		b = ptr[1];
		c = ptr[2];
		d = ptr[3];
	} else {
		d = ptr[0];
		c = ptr[1];
		b = ptr[2];
		a = ptr[3];
	}	
	return (a << 24) | (b << 16) | (c << 8) | d;
}


static UDATA decodeStackEntry(J9PortLibrary* portLib, U_8* stackDataPtr, UDATA endian)
{
	UDATA entrySize = 1;
	U_32 data;
	UDATA i, arity, index;

	PORT_ACCESS_FROM_PORT(portLib);

	switch(*stackDataPtr++)
	{
		case TYPE_INT:
			j9tty_printf(PORTLIB, " I");
			break;

		case TYPE_FLOAT:
			j9tty_printf(PORTLIB, " F");
			break;

		case TYPE_LONG:
			j9tty_printf(PORTLIB, " <J");
			break;

		case TYPE_DOUBLE:
			j9tty_printf(PORTLIB, " <D");
			break;

		case TYPE_OBJECT:
			data = unalignedRead4(stackDataPtr, endian);
			entrySize += 4;
			if(data & TAG_SPECIAL)
			{
				/* Special */
				j9tty_printf(PORTLIB, " !(%08X)", data);
			}
			else if(data & ARITY_MASK)
			{
				/* Array */
				arity = (data & ARITY_MASK) >> ARITY_SHIFT;
				index = (data & CLASS_INDEX_MASK) >> CLASS_INDEX_SHIFT;
				j9tty_printf(PORTLIB, " ");
				for(i = 0; i < arity; i++) j9tty_printf(PORTLIB, "[");
				if(data & TAG_BASE_ARRAY)
				{
					switch(data & CLASS_INDEX_MASK)
					{
						case BASE_TYPE_INT:
							j9tty_printf(PORTLIB, "I");
							break;
						case BASE_TYPE_FLOAT:
							j9tty_printf(PORTLIB, "F");
							break;
						case BASE_TYPE_LONG:
							j9tty_printf(PORTLIB, "J");
							break;
						case BASE_TYPE_DOUBLE:
							j9tty_printf(PORTLIB, "D");
							break;
						case BASE_TYPE_SHORT:
							j9tty_printf(PORTLIB, "S");
							break;
						case BASE_TYPE_BYTE:
							j9tty_printf(PORTLIB, "B");
							break;
						case BASE_TYPE_CHAR:
							j9tty_printf(PORTLIB, "C");
							break;
					}
				}
				else 
				{
					j9tty_printf(PORTLIB, "L(%i)", index);
				}
			}
			else
			{
				index = (data & CLASS_INDEX_MASK) >> CLASS_INDEX_SHIFT;
				j9tty_printf(PORTLIB, " L(%i)", index);
			}
			break;

		case TYPE_INIT_OBJECT:
			j9tty_printf(PORTLIB, " init");
			break;

		case TYPE_NEW_OBJECT:
			data = unalignedRead4(stackDataPtr, endian);
			entrySize += 4;
			index = (data & CLASS_INDEX_MASK) >> CLASS_INDEX_SHIFT;
			j9tty_printf(PORTLIB, " new(%i)", index);
			break;

		case TYPE_LONG2:
			j9tty_printf(PORTLIB, " J>");
			break;

		case TYPE_DOUBLE2:
			j9tty_printf(PORTLIB, " D>");
			break;

		case 0xFF:
			j9tty_printf(PORTLIB, " X");			
			break;

		default:
			j9tty_printf(PORTLIB, " ?%02X?", *(stackDataPtr - 1));
	}
	return entrySize;
}


static UDATA decodeStackDepth(J9PortLibrary* portLib, U_8* stackDataPtr, U_8* stackDataEnd)
{
	UDATA depth = 0;
	U_8 entry;

	PORT_ACCESS_FROM_PORT(portLib);

	while(stackDataPtr < stackDataEnd)
	{
		depth++;
		entry = *stackDataPtr++;
		if((entry == TYPE_OBJECT) || (entry == TYPE_NEW_OBJECT)) stackDataPtr += 4;
	}
	return depth;
}
