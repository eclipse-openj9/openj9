/*******************************************************************************
 * Copyright (c) 2003, 2019 IBM Corp. and others
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

/* Includes */
#include <string.h>
#include "j9protos.h"
#include "j2sever.h"
#include "HeapIteratorAPI.h"
#include "j9dmpnls.h"
#include "FileStream.hpp"

#include "ut_j9dmp.h"

/* Function prototypes */
extern "C" {
void writeClassicHeapdump(const char *label, J9RASdumpContext *context, J9RASdumpAgent *agent);
}

static jvmtiIterationControl binaryHeapDumpHeapIteratorCallback   (J9JavaVM* vm, J9MM_IterateHeapDescriptor*   heapDescriptor,    void* userData);
static jvmtiIterationControl binaryHeapDumpSpaceIteratorCallback  (J9JavaVM* vm, J9MM_IterateSpaceDescriptor*  spaceDescriptor,   void* userData);
static jvmtiIterationControl binaryHeapDumpRegionIteratorCallback (J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDescription, void* userData);
static jvmtiIterationControl binaryHeapDumpObjectIteratorCallback (J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDescriptor,  void* userData);

static jvmtiIterationControl binaryHeapDumpObjectReferenceIteratorTraitsCallback(J9JavaVM* virtualMachine, J9MM_IterateObjectDescriptor* objectDescriptor, J9MM_IterateObjectRefDescriptor* referenceDescriptor, void* userData);
static jvmtiIterationControl binaryHeapDumpObjectReferenceIteratorWriterCallback(J9JavaVM* virtualMachine, J9MM_IterateObjectDescriptor* objectDescriptor, J9MM_IterateObjectRefDescriptor* referenceDescriptor, void* userData);

#define allClassesStartDo(vm, state, loader) \
	vm->internalVMFunctions->allClassesStartDo(state, vm, loader)

#define allClassesNextDo(vm, state) \
	vm->internalVMFunctions->allClassesNextDo(state)

#define allClassesEndDo(vm, state) \
	vm->internalVMFunctions->allClassesEndDo(state)

/* Function prototypes for performance measurement 
void startTimer();
void stopTimer();
*/

/**************************************************************************************************/
/*                                                                                                */
/* Classes for manipulating strings                                                               */
/*                                                                                                */
/*   This really ought to be a template but there's a certain lack of confidence in the compilers */
/*   despite templates being used in the product already. In the mean time it doesn't actually    */
/*   have to be a template                                                                        */
/*                                                                                                */          
/**************************************************************************************************/
/* template<class DataType, class LengthType>class Strings */
class Strings
{
public :
	/* Declare the 'template' parameters */
  typedef char DataType; typedef UDATA LengthType;

	/* Constructors... */
		/* Pseudo default constructor */
	inline Strings(J9PortLibrary* portLibrary) :
		_PortLibrary(portLibrary),
		_Buffer(0)
	{
	}
	
	/* Destructor */
	inline ~Strings()
	{
		deleteBuffer(_Buffer);
		_Buffer = 0;
	}

	/* Assignment operators... */

	/* Assignment methods... */

	/* Append operators... */
	/* Operator for appending another string */
	inline Strings& operator+=(const Strings& source)
	{
		appendInternal(source.data(), source.length());
		return *this;
	}

	/* Operator for appending from a pointer to a zero terminated string of items */
	inline Strings& operator+=(const DataType* data)
	{
		appendInternal(data, length(data));
		return *this;
	}

	/* Operator for appending a single item */
	inline Strings& operator+=(const DataType& source)
	{
		appendInternal(&source, 1);
		return *this;
	}

	/* Append methods... */
	/* Method for appending from a substring of another string */
	inline Strings& append(const Strings& data, LengthType position, LengthType length)
	{
		/* Check the parameters */     
		LengthType sourceLength = data.length();
		if (position >= sourceLength) {
			return *this;
		}

		/* Limit length to not exceed the end of the source data */
		length = limitLength(sourceLength, position, length);

		appendInternal(data.data() + position, length);
		return *this;
	}
	
	/* Method for appending the remainder of another string */
	inline Strings& append(const Strings& data, LengthType position)
	{
		/* Check the parameters */     
		LengthType sourceLength = data.length();
		if (position >= sourceLength) {
			return *this;
		}

		/* Limit length to not exceed the end of the source data */
		LengthType length = sourceLength - position;

		appendInternal(data.data() + position, length);
		return *this;
	}
	
	/* Method for appending an array of items */
	inline Strings& append(const DataType* source, LengthType length)
	{
		appendInternal(source, length);
		return *this;
	}

	/* Method for clearing/emptying the string completely */
	inline void clear(void)
	{
		deleteBuffer(_Buffer);
		_Buffer = 0;
	}

	/* Method for getting the length of the data */
	inline LengthType length(void) const
	{
		if (_Buffer != 0) {
			return _Buffer->iLength;
		}

		return 0;
	}

	/* Method for getting a pointer to the data */
	inline const DataType* data(void) const
	{
		if (_Buffer != 0) {
			return _Buffer->data();
		}

		/* Use the pointer itself to act as a null         */
		/* NB : This makes assumptions about the data type */
		return (DataType*)&_Buffer;
	}

	/* Method for locating the first occurrence of a zero terminated substring in a string */
	inline LengthType find(const DataType* targetString, LengthType position = 0) const
	{
		/* Determine the length of the target string */
		LengthType targetStringLength = length(targetString);

		/* Limit the position parameter to the bounds of the string */
		LengthType stringLength = length();

		if (position > (stringLength - 1)) {
			position = stringLength - 1;
		}

		/* Compare the given string with each of the string's sub strings */
		const DataType* stringStart   = data();
		const DataType* stringEnd     = stringStart + stringLength;
		const DataType* currentString = stringStart + position;

		while (currentString < stringEnd) {
			/* Compare the first characters of the two strings */
			if (*currentString == *targetString) {
				/* If there is target string is longer than the current substring, they can't match */
				if (unsigned(stringEnd - currentString) < targetStringLength) {
					return (UDATA) -1;
				}

				/* Compare the subsequent characters of the strings */
				bool match = true;

				for (LengthType i = 0; i < targetStringLength; i++) {
					if (currentString[i] != targetString[i]) {
						match = false;
						break;
					}
				}

				if (match) { 
					return currentString - stringStart;
				}
			}

			currentString++;
		}

		return (UDATA) -1;
	}

	/* Static method for determining the length of a zero terminated array of items */
	inline static LengthType length(const DataType* source)
	{
		/* Make a null pointer equivalent to a zero length string */
		if (source == 0) {
			return 0;
		}
                                
		/* Count the items in the string */
		LengthType count = 0;
		for (const DataType* current = source; *current != 0; current++) {
			count++;
		}

		return count;
	}

protected :
	/* Declare the private, nested string buffer class */
	class Buffer
	{
	public :
		/* Declared data */
		LengthType iCapacity;
		LengthType iLength;
		DataType*  iDebugData;  /* Only required for debugging */

		/* Internal method for accessing the data associated with a header */
		inline DataType* data(void)
		{
			return(DataType*)((char*)this + sizeof(Buffer));
		}

		/* Internal method for accessing the data associated with a header */
		inline const DataType* data(void) const
		{
			return(DataType*)((char*)this + sizeof(Buffer));
		}

	private :
		/* Prevent use of the copy constructor and assignment operator */
		Buffer(const Buffer& source);
		Buffer& operator=(const Buffer& source);
	};

	/* Internal Method for getting a non const pointer to the data */
	inline DataType* dataPointer(void)
	{
		if (_Buffer != 0) {
			return _Buffer->data();
		}

		/* Use the pointer itself to act as a null         */
		/* NB : This makes assumptions about the data type */
		return (DataType*)&_Buffer;
	}

	/* Internal method for appending a new value to a String object */
	inline Strings& appendInternal(
		const DataType* appendData,
		LengthType      numberToAppend
	)
	{
		/* If the append length is zero, there's nothing to do */
		/* NB : Interpret a null pointer as an empty string    */
		if ((appendData == 0) || (numberToAppend == 0)) {
			return *this;
		}

		/* Establish the lengths */
		LengthType originalLength = length();
		LengthType newLength      = originalLength + numberToAppend;

		/* Establish the source and target buffers */
		DataType* sourceBufferPointer       = 0;
		DataType* targetBufferPointer       = 0;
		Buffer*   bufferPointerForUnlinking = 0;

		if (_Buffer != 0) {
			/* Get a pointer to the source buffer */
			sourceBufferPointer = _Buffer->data();

			/* If the capacity is insufficient, a new buffer is needed */
			if (_Buffer->iCapacity < newLength) {
				/* A new buffer needed is - prepare the old buffer unlinking */
				bufferPointerForUnlinking = _Buffer;

				/* Create a new remote buffer */
				_Buffer = createBuffer(newLength);
			}

			/* Otherwise use the existing buffer */

		} else {
			/* Create a new remote buffer */
			_Buffer = createBuffer(newLength);
		}

		targetBufferPointer = _Buffer->data();
		_Buffer->iLength    = newLength;

		/* Copy the existing data */
		if (sourceBufferPointer && targetBufferPointer != sourceBufferPointer) {
			copyBuffer(targetBufferPointer,  sourceBufferPointer, originalLength, true);
		}

		/* Copy in the append data */
		copyBuffer(targetBufferPointer + originalLength, appendData, numberToAppend, true);

		/* If a new remote buffer was created, release the original */
		if (bufferPointerForUnlinking) {
			deleteBuffer(bufferPointerForUnlinking);
		}

		return *this;
	}

	/* Internal method for creating a new buffer */
	inline Buffer* createBuffer(LengthType requestedLength)
	{
		/* NB : This is never called with requestedLength == 0 */

		/* Granularize the buffer length. This helps performance when strings are  */
		/* altered in situ (e.g. append) and also means that heap allocations will */
		/* tend to be for the same sized block (i.e. the minimum)                  */
		LengthType newBufferLength         = sizeof(Buffer) + sizeof(DataType) * (requestedLength + 1);
		LengthType granularNewBufferLength = ((newBufferLength / 32) + 1) * 32;
		PORT_ACCESS_FROM_PORT(_PortLibrary);

		/* Create a new buffer */
		Buffer* newBufferPointer = (Buffer*)j9mem_allocate_memory(granularNewBufferLength, OMRMEM_CATEGORY_VM);

		/* Initialize it */
		newBufferPointer->iCapacity  = granularNewBufferLength - sizeof(Buffer) - sizeof(DataType);
		newBufferPointer->iLength    = 0;
		newBufferPointer->iDebugData = (DataType*)((char*)newBufferPointer + sizeof(Buffer));

		return newBufferPointer;
	}

	/* Internal method for copying buffer contents */
	inline void copyBuffer(DataType* target, const DataType* source, LengthType length, bool terminated)
	{
		/* Copy element by element */
		DataType* targetEnd = target + length;

		while (target < targetEnd) {
			*target++ = *source++;
		}

		/* If required, terminate the string */
		if (terminated) {
			*target = DataType(0);
		}
	}

	/* Internal method for deleting a buffer */
	inline void deleteBuffer(Buffer* buffer)
	{
		PORT_ACCESS_FROM_PORT(_PortLibrary);
		if (buffer != 0) {
			j9mem_free_memory(buffer);
		}
	}

	/* Method to limit the requested number of elements to the end of the available string */
	inline static LengthType limitLength(LengthType sourceLength, LengthType startPosition, LengthType requestedLength)
	{
		if (startPosition < sourceLength) {
			/* Reduce the source length by the offset */
			sourceLength -= startPosition;

			/* Return the minimum of the source length and the requested length */
			return sourceLength < requestedLength ? sourceLength : requestedLength;
		} else {
			/* Start position beyond length */
			return 0;
		}
	}

	/* Declared data */
	J9PortLibrary* _PortLibrary;
	Buffer*        _Buffer;
	
private :
	/* Prevent use of the unimplemented copy constructor and assignment operator */
	inline Strings& operator=(const Strings& source);
	inline Strings(const Strings& source);
};

/* String class methods that aren't needed at the moment */

	/* Copy constructor
	inline Strings(const Strings& source) :
		_PortLibrary(source._PortLibrary),
		_Buffer(0)
	{
		assign(source);
	}
	*/
	/* Constructor from a sub-string of a String
	inline Strings(const Strings& source, LengthType position, LengthType length) :
		iBuffer(0)
	{
		assign(source, position, length);
	}
	*/
	/* Constructor from the remainder of a String
	inline Strings(const Strings& source, LengthType position) :
		iBuffer(0)
	{
		assign(source, position);
	}
	*/
	/* Constructor from a null terminated string
	inline Strings(const DataType* data) : 
		iBuffer(0)
	{
		assignInternal(data, length(data));
	}
	*/

	/* Operator for assigning from another String object
	inline Strings& operator=(const Strings& source)
	{
		assign(source);
		return *this;
	}
	*/
	/* Method for assigning from another string
	inline Strings& assign(const Strings& source) {
		 If it's a self assignment, do nothing
		if (&source == this) {
			return *this;
		}

		 Dispose of the existing buffer
		if (iBuffer != 0) {
			unlinkFromBuffer(iBuffer);
			iBuffer = 0;
		}

		 Create a new buffer which is a copy of the original
		assignInternal(source.data(), source.length());
		return *this;
	}
	*/
	/* Method for assigning from a substring of another string
	inline Strings& assign(const Strings& data, LengthType position, LengthType length) {
		 Check the parameters
		LengthType sourceLength = data.length();
		if (position >= sourceLength) {
			return *this;
		}

		 Limit length to not exceed the end of the source data
		length = limitLength(sourceLength, position, length);

		assignInternal(data.data() + position, length);
		return *this;
	}
	*/
	/* Method for assigning from the remainder of a string
	inline Strings& assign(const Strings& data, LengthType position) {
		 Check the parameters
		LengthType sourceLength = data.length();
		if (position >= sourceLength) {
			return *this;
		}

		 Limit length to not exceed the end of the source data
		LengthType length = sourceLength - position;

		assignInternal(data.data() + position, length);
		return *this;
	}
	*/
	/* Operator for accessing the items
	inline DataType& operator[](LengthType index)
	{
		 Detect the case where the index is out of range
		LengthType len = length();
		if ((index < 0) || (index > len)) {
			return *(dataPointer() + len);
		}

		return *(dataPointer() + index);
	}
	*/
	/* Internal method for assigning a new value to a String object
	inline void assignInternal(const DataType* newData, LengthType newLength)
	{
		 If the result of this operation is a null string, call clear() and return
		if (newData == 0 || newLength == 0) {
			clear();
			return;
		}

		 Create a new buffer that's exactly the right size
		Buffer* newBufferPointer  = createBuffer(newLength);
		newBufferPointer->iLength = newLength; 

		 Copy in the new data
		copyBuffer(newBufferPointer->data(), newData, newLength, true);

		 Finalize the buffers
		deleteBuffer(iBuffer);
		iBuffer = newBufferPointer;
	}
	*/

/*typedef Strings<char, UDATA>CharacterStringBase;*/
typedef Strings CharacterStringBase;

class CharacterString : public CharacterStringBase
{
public :
	/* Constructor */
	CharacterString(J9PortLibrary* portLibrary) :
		CharacterStringBase(portLibrary)
	{
	}
	
	/* Append methods... */
	/* Method for appending from a character rendition of an unsigned integer. */
	inline CharacterString& appendAsCharacters(UDATA data, UDATA radix)
	{
		PORT_ACCESS_FROM_PORT(_PortLibrary);
		char buffer[1 + (sizeof(UDATA) * 8)];
		
		switch (radix) {
		case 16:
			j9str_printf(PORTLIB, buffer, sizeof(buffer), "%.*zX", sizeof(UDATA) * 2, data);
			break;
		default:
			j9str_printf(PORTLIB, buffer, sizeof(buffer), "%zu", data);
			break;
		}
		
		append(buffer, strlen(buffer));

		return *this;
	}
};

/**************************************************************************************************/
/*                                                                                                */
/* Class for walking a class constant pool to extract class references. See CMVC 177688.          */
/*  Note: adapted from GC_ConstantPoolClassSlotIterator in Modron/gc_structs                      */
/*                                                                                                */
/**************************************************************************************************/
class ConstantPoolClassIterator
{
	J9Object **_cpEntry;
	U_32 _cpEntryCount;
	U_32 _cpEntryTotal;
	U_32 *_cpDescriptionSlots;
	U_32 _cpDescription;
	UDATA _cpDescriptionIndex;

public:
	/* Constructor */
	ConstantPoolClassIterator(J9Class *clazz) :
		_cpEntry((J9Object **)J9_CP_FROM_CLASS(clazz)),
		_cpEntryCount((U_32)((J9ROMClass *)clazz->romClass)->ramConstantPoolCount),
		_cpDescriptionSlots(NULL),
		_cpDescription(0)
	{
		_cpEntryTotal = _cpEntryCount;
		if (_cpEntryCount) {
			_cpDescriptionSlots = SRP_PTR_GET(&((J9ROMClass *)clazz->romClass)->cpShapeDescription, U_32 *);
			_cpDescriptionIndex = 0;
		}
	}

	/* Method to return the next Class reference from the constant pool, or NULL if no more references. */
	j9object_t nextClassObject(void) {
		U_32 slotType;
		J9Object **slotPtr;

		while(_cpEntryCount) {
			if (0 == _cpDescriptionIndex) {
				_cpDescription = (U_32)*_cpDescriptionSlots;
				_cpDescriptionSlots += 1;
				_cpDescriptionIndex = J9_CP_DESCRIPTIONS_PER_U32;
			}

			slotType = _cpDescription & J9_CP_DESCRIPTION_MASK;
			slotPtr = _cpEntry;

			/* Adjust the CP slot and description information */
			_cpEntry = (J9Object **)( ((U_8 *)_cpEntry) + sizeof(J9RAMConstantPoolItem) );
			_cpEntryCount -= 1;

			_cpDescription >>= J9_CP_BITS_PER_DESCRIPTION;
			_cpDescriptionIndex -= 1;

			/* We are only interested if the slot is a class reference */
			if (slotType == J9CPTYPE_CLASS) {
				/* PHD needs the address of the on-heap java/lang/Class object (2.4 VM and later) */
				J9Class *clazz = (((J9RAMClassRef *) slotPtr)->value);
				if (NULL != clazz) {
					return clazz->classObject;
				}
			}
		}
		return NULL;
	}
};

/**************************************************************************************************/
/*                                                                                                */
/* Class for walking the superclass references from a class. See CMVC 177688.                     */
/*                                                                                                */
/**************************************************************************************************/
class SuperclassesIterator
{
	UDATA _classDepth;
	IDATA _index;
	J9Class **_superclassPtr;

public:
	/* Constructor */
	SuperclassesIterator(J9Class *clazz) :
		_classDepth(J9CLASS_DEPTH(clazz)),
		_index(-1),
		_superclassPtr((J9Class **)clazz->superclasses)
	{
	}

	/* Method to return a reference to the next superclass, or NULL if no more superclasses */
	j9object_t nextClassObject(void) {
		J9Class **slotPtr;

		if(0 == _classDepth) {
			return NULL;
		}

		_index += 1;
		_classDepth -= 1;
		slotPtr = _superclassPtr++;
		/* PHD needs the address of the on-heap java/lang/Class object (2.4 VM and later) */
		J9Class *clazz = (((J9RAMClassRef *) slotPtr)->value);
		if (NULL != clazz) {
			return clazz->classObject;
		} else {
			return NULL;
		}
	}
};

/**************************************************************************************************/
/*                                                                                                */
/* Class for walking the interface references from a class. See CMVC 177688.                      */
/*  Note: adapted from GC_ClassLocalInterfaceIterator in Modron/gc_structs                        */
/*                                                                                                */
/**************************************************************************************************/
class InterfacesIterator
{
	J9ITable *_iTable;
	J9ITable *_superclassITable;

public:
	/* Constructor */
	InterfacesIterator(J9Class *clazz) :
		_iTable((J9ITable *)clazz->iTable)
	{
		J9Class *superclass = (J9Class *) ((struct J9Class**)clazz->superclasses[J9CLASS_DEPTH(clazz) - 1]);
		if(superclass) {
			_superclassITable = (J9ITable *)superclass->iTable;
		} else {
			_superclassITable = NULL;
		}
	}

	/* Method to return a reference to the next interface, or NULL if no more interfaces */
	j9object_t nextClassObject(void) {
		J9Class **slotPtr;

		if(_iTable == _superclassITable) {
			return NULL;
		}

		slotPtr = &_iTable->interfaceClass;
		_iTable = (J9ITable *)_iTable->next;
		/* PHD needs the address of the on-heap java/lang/Class object (2.4 VM and later) */
		J9Class *clazz = (((J9RAMClassRef *) slotPtr)->value);
		if (NULL != clazz) {
			return clazz->classObject;
		} else {
			return NULL;
		}
	}
};

/**************************************************************************************************/
/*                                                                                                */
/* Class for writing binary portable heap dump files                                              */
/*                                                                                                */
/**************************************************************************************************/
class BinaryHeapDumpWriter
{
public :
	/* Constructor */
	BinaryHeapDumpWriter(const char* fileName, J9RASdumpContext* context, J9RASdumpAgent* agent);

	/* Destructor */
	~BinaryHeapDumpWriter();
	
	UDATA             _Id;
	char*             _RegionStart;
	char*             _RegionEnd;


private :
	/* Prevent use of the copy constructor and assignment operator */
	BinaryHeapDumpWriter(const BinaryHeapDumpWriter& source);
	BinaryHeapDumpWriter& operator=(const BinaryHeapDumpWriter& source);

	/* Allow the callback functions access */
	friend jvmtiIterationControl binaryHeapDumpSpaceIteratorCallback  (J9JavaVM* virtualMachine, J9MM_IterateSpaceDescriptor*  spaceDescriptor,  void* userData);
	friend jvmtiIterationControl binaryHeapDumpObjectIteratorCallback (J9JavaVM* virtualMachine, J9MM_IterateObjectDescriptor* objectDescriptor, void* userData);
	friend jvmtiIterationControl binaryHeapDumpObjectReferenceIteratorTraitsCallback(J9JavaVM* virtualMachine, J9MM_IterateObjectDescriptor* objectDescriptor, J9MM_IterateObjectRefDescriptor* referenceDescriptor, void* userData);
	friend jvmtiIterationControl binaryHeapDumpObjectReferenceIteratorWriterCallback(J9JavaVM* virtualMachine, J9MM_IterateObjectDescriptor* objectDescriptor, J9MM_IterateObjectRefDescriptor* referenceDescriptor, void* userData);
	friend jvmtiIterationControl binaryHeapDumpHeapIteratorCallback(J9JavaVM* virtualMachine, J9MM_IterateHeapDescriptor* heapDescriptor, void* userData);
	friend jvmtiIterationControl binaryHeapDumpRegionIteratorCallback(J9JavaVM* virtualMachine, J9MM_IterateRegionDescriptor* regionDescription, void* userData);

	/* Nested class for determining the characteristics of the references */
	class ReferenceTraits
	{
	public :
		/* Constructor */
		ReferenceTraits(BinaryHeapDumpWriter* heapDumpWriter, j9object_t objectAddress);

		/* Method for adding a reference to the 'list' of those analysed */
		void addReference(j9object_t objectReference);

		/* Methods for getting the object's attributes */
		IDATA maximumOffset (void) const;
		UDATA count         (void) const;
		IDATA offset        (UDATA index) const;

		BinaryHeapDumpWriter* _HeapDumpWriter;

	private :
		/* Prevent use of the copy constructor and assignment operator */
		ReferenceTraits(const ReferenceTraits& source);
		ReferenceTraits& operator=(const ReferenceTraits& source);
	
		/* Declared data */
		j9object_t             _ObjectAddress;
		IDATA                 _MaximumOffset;
		IDATA                 _MinimumOffset;
		UDATA                 _Count;
		IDATA                 _Offsets[8];
	};

	/* Nested class for writing a reference to the file */
	class ReferenceWriter
	{
	public :
		/* Constructor */
		ReferenceWriter(BinaryHeapDumpWriter* heapDumpWriter, j9object_t objectAddress, UDATA count, int size);

		/* Method for writing a reference to the dump file */
		void writeNumber(j9object_t objectAddress);

		BinaryHeapDumpWriter* _HeapDumpWriter;

	private :
		/* Prevent use of the copy constructor and assignment operator */
		ReferenceWriter(const ReferenceWriter& source);
		ReferenceWriter& operator=(const ReferenceWriter& source);
	
		/* Declared data */
		j9object_t             _ObjectAddress;
		UDATA                 _Count;
		int                   _Size;
	};
	
	/* Nested class for managing the class cache */
	class ClassCache
	{
	public :
		/* Constructor */
		ClassCache();
		
		/* Method for adding a class */
		void add(const void* clazz);
		
		/* Method for finding a class */
		int find(const void* clazz);
		
		/* Methods for getting the object's attributes */
		int index(void) const;

		/* Method for setting the object back to its initial state (i.e. empty) */
		void clear(void);
		
	private :
		/* Prevent use of the copy constructor and assignment operator */
		ClassCache(const ClassCache& source);
		ClassCache& operator=(const ClassCache& source);	
	
		/* Declared data */
		const void* _Cache[4];
		int         _Index;
	};

	friend class ReferenceTraits;
	friend class ReferenceWriter;

	/* Internal methods */
	void             openNewDumpFile(J9MM_IterateSpaceDescriptor* spaceDesriptor);
	void             writeDumpFileHeader(void);
	void             writeDumpFileTrailer(void);
	void             writeFullVersionRecord(void);
	void             writeObjectRecord(J9MM_IterateObjectDescriptor* objectDescriptor);
	void             writeNormalObjectRecord(J9MM_IterateObjectDescriptor* objectDescriptor);
	void             writeArrayObjectRecord(J9MM_IterateObjectDescriptor* objectDescriptor);
	void             writeClassRecord(J9Class* clazz);
	static int       numberSize(IDATA number);
	int              getObjectHashCode(j9object_t object);
	static int       numberSizeEncoding(int numberSize);
	static int       wordSize(void);
	void             checkForIOError(void);
	/* Methods for writing data to output file (proxies to _OutputStream */
	void             writeCharacters (const char* data, IDATA length);
	void             writeCharacters (const char* data);
	void             writeNumber (IDATA data, int length);

	/* Declared data */
	/* NB : The initialization order is not guaranteed on all C++ compilers */
	J9RASdumpContext* _Context;
	J9RASdumpAgent*   _Agent;
	J9JavaVM*         _VirtualMachine;
	J9PortLibrary*    _PortLibrary;
	CharacterString   _FileName;
	FileStream        _OutputStream;
	void*             _CurrentObject;
	ClassCache        _ClassCache;
	bool              _FileMode;
	bool              _Error;

	/* Static methods returning constant values */
	inline static const char* identifierField(void)        {return "portable heap dump";}
	inline static char        versionField(void)           {return 0x06;}

#if defined(J9VM_OPT_NEW_OBJECT_HASH)
	inline static char        primaryFlagsField(void)
	{
#ifdef J9VM_ENV_DATA64
		return 0x05;
#else
		return 0x04;
#endif
	}
#else /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
	inline static char        primaryFlagsField(void)
	{
#ifdef J9VM_ENV_DATA64
		return 0x07;
#else
		return 0x06;
#endif
	}
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */

	inline static char        headerStartField(void)       {return 0x01;}
	inline static char        fullVersionRecordField(void) {return 0x04;}
	inline static char        headerEndField(void)         {return 0x02;}
	inline static char        dumpStartField(void)         {return 0x02;}
	inline static char        longObjectRecordField(void)  {return 0x04;}
	inline static char        longPrimitiveArrayRecordField(void)  {return 0x07;}
	inline static char        arrayObjectRecordField(void) {return 0x08;}
	inline static char        classObjectRecordField(void) {return 0x06;}
	inline static char        dumpEndField(void)           {return 0x03;}
};

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ReferenceTraits::ReferenceTraits() method implementation                 */
/*                                                                                                */
/**************************************************************************************************/
BinaryHeapDumpWriter::ReferenceTraits::ReferenceTraits(
	BinaryHeapDumpWriter* heapDumpWriter,
	j9object_t objectAddress
) :
	_HeapDumpWriter(heapDumpWriter),
	_ObjectAddress(objectAddress),
	_MaximumOffset(0),
	_MinimumOffset(0),
	_Count(0)
{
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ReferenceTraits::addReference() method implementation                    */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::ReferenceTraits::addReference(j9object_t objectReference) 
{
	/* Ignore null references */
	if (objectReference == 0) {
		return;
	}

	/* Calculate the offset (gap) from the current object */
	IDATA offset = ((char*)(objectReference) - (char*)_ObjectAddress);
	
	/* Remember the biggest offsets */
	if (offset > _MaximumOffset) {
		_MaximumOffset = offset;
	}

	if (offset < _MinimumOffset) {
		_MinimumOffset = offset;
	}

	/* Remember the first few offsets */
	if (_Count < 8) {
		_Offsets[_Count] = offset;
	}

	/* Increment the count of valid references */
	_Count++;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ReferenceTraits::maximumOffset() method implementation                   */
/*                                                                                                */
/**************************************************************************************************/
IDATA
BinaryHeapDumpWriter::ReferenceTraits::maximumOffset(void) const
{
	/* Calculate the biggest difference as a positive number */
	/* NB : A byte has a range of -128 to +127 but this will say -128 needs 2 bytes */
	IDATA result = -_MinimumOffset > _MaximumOffset ? -_MinimumOffset : _MaximumOffset;
	
	return result;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ReferenceTraits::count() method implementation                           */
/*                                                                                                */
/**************************************************************************************************/
UDATA
BinaryHeapDumpWriter::ReferenceTraits::count(void) const
{
	return _Count;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ReferenceTraits::offset() method implementation                          */
/*                                                                                                */
/**************************************************************************************************/
IDATA
BinaryHeapDumpWriter::ReferenceTraits::offset(UDATA index) const
{
	if (index > 8) {
		return 0;
	}
	
	return _Offsets[index];
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ReferenceWriter::ReferenceWriter() method implementation                 */
/*                                                                                                */
/**************************************************************************************************/
BinaryHeapDumpWriter::ReferenceWriter::ReferenceWriter(
	BinaryHeapDumpWriter* heapDumpWriter,
	j9object_t            objectAddress,
	UDATA                 count,
	int                   size
) :
	_HeapDumpWriter(heapDumpWriter),
	_ObjectAddress(objectAddress),
	_Count(count),
	_Size(size)
{
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ReferenceWriter::writeNumber() method implementation                     */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::ReferenceWriter::writeNumber(j9object_t objectReference)
{
	/* Ignore null references */
	if ( objectReference == 0 ) {
		return;
	}

	/* Calculate the offset (gap) from the current object */
	IDATA offset = ((char*)(objectReference) - (char*)_ObjectAddress) / 4;
	
	/* Write the data */
	_HeapDumpWriter->writeNumber(offset, _Size);
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ClassCache::ClassCache() method implementation                           */
/*                                                                                                */
/**************************************************************************************************/
BinaryHeapDumpWriter::ClassCache::ClassCache() :
	_Index(0)
{
	/* Initialize the class cache */
	clear();
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ClassCache::add() method implementation                                  */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::ClassCache::add(const void* clazz)
{
	_Cache[_Index] = clazz;
	_Index         = (_Index + 1) % 4;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ClassCache::find() method implementation                                 */
/*                                                                                                */
/**************************************************************************************************/
int
BinaryHeapDumpWriter::ClassCache::find(const void* clazz)
{
	for (int i = 0; i < 4; i++) {
		if (_Cache[i] == clazz) {
			return i;
		}
	}
	
	return -1;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ClassCache::index() method implementation                                */
/*                                                                                                */
/**************************************************************************************************/
int
BinaryHeapDumpWriter::ClassCache::index(void) const
{
	return _Index;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::ClassCache::clear() method implementation                                */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::ClassCache::clear(void)
{
	/* Initialize the class cache */
	for (int i = 0; i < 4; i++) {
		_Cache[i] = 0;
	} 

	_Index = 0;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::BinaryHeapDumpWriter() method implementation                             */
/*                                                                                                */
/**************************************************************************************************/
BinaryHeapDumpWriter::BinaryHeapDumpWriter(const char* fileName, J9RASdumpContext* context, J9RASdumpAgent* agent) :
	_Id(0),
	_RegionStart(NULL),
	_RegionEnd(NULL),
	_Context(context),
	_Agent(agent),
	_VirtualMachine(context->javaVM),
	_PortLibrary(context->javaVM->portLibrary),
	_FileName(context->javaVM->portLibrary),
	_OutputStream(context->javaVM->portLibrary),
	_CurrentObject(0),
	_FileMode(false),
	_Error(false)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);

	/* If a binary heap dump hasn't been requested there's nothing to do */
	if ((agent->dumpOptions != 0) && (strstr(agent->dumpOptions, "PHD") == 0)) {
		return;
	}
	
	/* Remember the file name */
	_FileName += fileName;
	
	/* Handle the cases of multiple dump files and a single dump file separately */
	if (!(_Agent->requestMask & J9RAS_DUMP_DO_MULTIPLE_HEAPS)) {
		/* Write a message to standard error saying we are about to write a dump file */
		reportDumpRequest(_PortLibrary,_Context,"Heap",fileName);
		
		/* It's a single file so open it */
		_OutputStream.open(_FileName.data());
	
		/* Performance measuring code 
		startTimer();
		*/

		/* Start writing the file */
		writeDumpFileHeader();
	}

	/* It's multiple files so iterate through the heaps and spaces */
	_VirtualMachine->memoryManagerFunctions->j9mm_iterate_heaps(_VirtualMachine, _PortLibrary, 0, binaryHeapDumpHeapIteratorCallback, this);

	/* Handle the cases of multiple dump files and a single dump file separately */
	if (!(_Agent->requestMask & J9RAS_DUMP_DO_MULTIPLE_HEAPS)) {
		/* Complete the dump file */
		if (! _Error) {
			writeDumpFileTrailer();
		}

		/* Performance measuring code 
		stopTimer();
		*/

		/* Record the status of the operation */
		_FileMode = _FileMode || _OutputStream.isOpen();

		/* Close the file */
		_OutputStream.close();
		
		/* Write a message to standard error saying we have written a dump file */
		/* If an error occurred, the error message has already been printed in checkForIOError() */
		if (! _Error) {
			if (_FileMode) {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "Heap", fileName);
				Trc_dump_reportDumpEnd_Event2("Heap", fileName);
			} else {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_NO_CREATE, fileName);
				Trc_dump_reportDumpEnd_Event2("Heap", fileName);
			}
		}
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::~BinaryHeapDumpWriter() method implementation                            */
/*                                                                                                */
/**************************************************************************************************/
BinaryHeapDumpWriter::~BinaryHeapDumpWriter()
{
	/* Nothing to do currently */
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::openNewDumpFile() method implementation                                  */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::openNewDumpFile(J9MM_IterateSpaceDescriptor* spaceDescriptor)
{
	/* Declare a local variable to hold the calculated file name */
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	CharacterString fileName(PORTLIB);
	
	/* Handle the single and multiple dump file cases separately */
	if (_Agent->requestMask & J9RAS_DUMP_DO_MULTIPLE_HEAPS) {
		/* Find the position of the '%id' in the file name */
		UDATA position = _FileName.find("%id");

		/* Substitute the current space identifier into the name */
		fileName.append(_FileName, 0, position);
		fileName += spaceDescriptor->name;
		fileName.appendAsCharacters(spaceDescriptor->id, 16);
		fileName.append(_FileName, position + 3);

		/* Write a message to standard error saying we are about to write a dump file */
		reportDumpRequest(PORTLIB, _Context,"Heap", fileName.data());

		/* Initialize the data members */
		_CurrentObject = 0;
		_ClassCache.clear();

		/* Open the file */
		_OutputStream.open(fileName.data());

		/* Start writing the file */
		writeDumpFileHeader();
	}

	/* Iterate through the regions etc. */
	_VirtualMachine->memoryManagerFunctions->j9mm_iterate_regions(
			_VirtualMachine,
			_PortLibrary,
			spaceDescriptor,
			j9mm_iterator_flag_regions_read_only,
			binaryHeapDumpRegionIteratorCallback,
			this);

	/* Handle the single and multiple dump file cases separately */
	if (_Agent->requestMask & J9RAS_DUMP_DO_MULTIPLE_HEAPS) {
		/* Complete the dump file */
		if (! _Error) {
			writeDumpFileTrailer();
		}

		/* Record the status of the operation */
		_FileMode = _FileMode || _OutputStream.isOpen();

		/* Close the file */
		_OutputStream.close();
		
		/* Write a message to standard error saying we have written a dump file */
		/* If an error occurred, the error message has already been printed in checkForIOError() */
		if (! _Error) {
			if (_FileMode) {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "Heap", fileName.data());
				Trc_dump_reportDumpEnd_Event2("Heap", fileName.data());
			} else {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_NO_CREATE, fileName.data());
				Trc_dump_reportDumpEnd_Event2("Heap", fileName.data());
			}
		}
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::writeDumpFileHeader() method implementation                              */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::writeDumpFileHeader(void)
{
	/* Write the file identifier */
	writeNumber(strlen(identifierField()), 2);
	if (_Error) {
		return;
	}

	writeCharacters(identifierField(), strlen(identifierField()));
	if (_Error) {
		return;
	}

	/* Write the version */
	writeNumber(versionField(), 4);
	if (_Error) {
		return;
	}

	/* Write the primary flags */
	writeNumber(primaryFlagsField(), 4);
	if (_Error) {
		return;
	}

	/* Write the header start tag */
	writeNumber(headerStartField(), 1);
	if (_Error) {
		return;
	}

	/* Write the version string */
	writeFullVersionRecord();
	if (_Error) {
		return;
	}

	/* Write the header end tag */
	writeNumber(headerEndField(), 1);
	if (_Error) {
		return;
	}

	/* Write the dump start tag */
	writeNumber(dumpStartField(), 1);
	if (_Error) {
		return;
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::writeDumpFileTrailer() method implementation                             */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::writeDumpFileTrailer(void)
{
	J9ClassWalkState state;
	J9Class *clazz;
	
	/* Iterate through the classes writing them */
	clazz = allClassesStartDo(_VirtualMachine, &state, NULL);
	while (clazz) {
		writeClassRecord(clazz);
		if (_Error) {
			/* Finish the class iteration to release any locks. */
			allClassesEndDo(_VirtualMachine, &state);
			return;
		}
		clazz = allClassesNextDo(_VirtualMachine, &state);
	}
	allClassesEndDo(_VirtualMachine, &state);

	/* Write the dump end tag */
	writeNumber(dumpEndField(), 1);
}

/**********************************************{****************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::writeFullVersionRecord() method implementation                           */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::writeFullVersionRecord(void)
{
	/* Write the full version record tag */
	writeNumber(fullVersionRecordField(), 1);
	if (_Error) {
		return;
	}

	/* Create a string buffer to hold the full version string */
	CharacterString fullVersion(_PortLibrary);

	/* Get the full version string from the j9ras structure */
	if( _VirtualMachine->j9ras->serviceLevel != NULL ) {
		fullVersion += _VirtualMachine->j9ras->serviceLevel;
	}

	/* Write the full version record data length */
	writeNumber(fullVersion.length(), 2);
	if (_Error) {
		return;
	}

	/* Write the full version record data */
	writeCharacters(fullVersion.data(), fullVersion.length());
	if (_Error) {
		return;
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::writeObjectRecord() method implementation                                */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::writeObjectRecord(J9MM_IterateObjectDescriptor* objectDescriptor)
{
	/* Extract pointers to the current object and its class */
	j9object_t currentObject = objectDescriptor->object;
	J9Class*  currentClass  = J9OBJECT_CLAZZ_VM(_VirtualMachine, currentObject);

	/* Handle class, array and normal objects separately */
	if (J9VM_IS_INITIALIZED_HEAPCLASS_VM(_VirtualMachine, currentObject)) {
		/* Do nothing - heap classes are handled in a separate walk */
	} else if (J9ROMCLASS_IS_ARRAY(currentClass->romClass)) {
		writeArrayObjectRecord(objectDescriptor);
	} else {
		writeNormalObjectRecord(objectDescriptor);
	}	
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::writeNormalObjectRecord() method implementation                          */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::writeNormalObjectRecord(J9MM_IterateObjectDescriptor* objectDescriptor)
{
	/* Extract a pointer to the current object */
	j9object_t currentObject = objectDescriptor->object;

	/* Calculate the address delta (gap) from the previous object                 */
	/* NB : The gap is defined in terms of 32 bit words regardless of the machine */
	IDATA addressOffset         = ((char*)(currentObject) - (char*)_CurrentObject) / 4;
	int   addressOffsetSize     = numberSize(addressOffset);
	int   addressOffsetEncoding = numberSizeEncoding(addressOffsetSize);

	/* Iterate through the references counting them and noting the biggest offset */
	ReferenceTraits referenceTraits(this, currentObject);

	_VirtualMachine->memoryManagerFunctions->j9mm_iterate_object_slots(
		_VirtualMachine,
		_PortLibrary,
		objectDescriptor,
		j9mm_iterator_flag_exclude_null_refs,
		binaryHeapDumpObjectReferenceIteratorTraitsCallback,
		&referenceTraits
	);

	/* Determine the length of the biggest reference offset */
	int referenceOffsetSize     = numberSize(referenceTraits.maximumOffset() / 4);
	int referenceOffsetEncoding = numberSizeEncoding(referenceOffsetSize);

	/* Calculate the object's class object's address */
	J9Class* objectClass = J9OBJECT_CLAZZ_VM(_VirtualMachine, currentObject);
	void* objectClassAddress = J9VM_J9CLASS_TO_HEAPCLASS(objectClass);

	/* Determine whether this class is cached */
	int classCacheIndex = _ClassCache.find(objectClassAddress);

	int hashCode = getObjectHashCode(currentObject);

	/* Determine whether the current object is a candidate for the short format */
	if ( (addressOffsetEncoding   <=  1) &&
	     (referenceTraits.count() <=  3) &&
	     (classCacheIndex         != -1) &&
	     (0 == hashCode)) {
		/* It is so generate a short format record */
		/* Calculate the flags */
		int flags = 
		    0x80                                    | 
		    ( classCacheIndex         << 5  & 0x60) |
		    (((int)referenceTraits.count() << 3) & 0x18) |
		    ( addressOffsetEncoding   << 2  & 0x04) |
		    ( referenceOffsetEncoding       & 0x03);
		    
		/* Write the tag/flags */
		writeNumber(flags, 1);
		if (_Error) {
			return;
		}
		
		/* Write the address delta (gap) */
		writeNumber(addressOffset, addressOffsetSize);
		if (_Error) {
			return;
		}

		/* Write the hash code */
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
        // If there is a 32 bit hashcode we write a long record.
#else /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
		writeNumber(hashCode, 2);
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
		if (_Error) {
			return;
		}
		
		/* Write the reference offsets */
		for (UDATA i = 0; i < referenceTraits.count(); i++) {
			writeNumber(referenceTraits.offset(i) / 4, referenceOffsetSize);
			if (_Error) {
				return;
			}
		}

		/* Determine whether the current object is a candidate for the medium format */
	} else if ((addressOffsetEncoding   <=  1) &&
	           (referenceTraits.count() <=  7) &&
	           (0 == hashCode)) {
		/* It is so generate a medium format record */		
		/* Calculate the flags */
		int flags = 
		    0x40                                    | 
		    (((int)referenceTraits.count() << 3) & 0x38) |
		    ( addressOffsetEncoding   << 2  & 0x04) |
		    ( referenceOffsetEncoding       & 0x03);
		    
		/* Write the tag/flags */
		writeNumber(flags, 1);
		if (_Error) {
			return;
		}

		/* Write the address delta (gap) */
		writeNumber(addressOffset, addressOffsetSize);
		if (_Error) {
			return;
		}

		/* Write the class address */
		writeNumber(IDATA(objectClassAddress), wordSize());
		if (_Error) {
			return;
		}

		/* Write the hash code */
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
		// If there is a 32 bit hashcode we write a long record.
#else /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
		writeNumber(hashCode, 2);
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
		if (_Error) {
			return;
		}
		
		/* Write the reference offsets */
		for (UDATA i = 0; i < referenceTraits.count(); i++) {
			writeNumber(referenceTraits.offset(i) / 4, referenceOffsetSize);
			if (_Error) {
				return;
			}
		}

		/* Now that the class has been described, add it to the class cache */
		_ClassCache.add(objectClassAddress);		
	} else {
		/* It isn't a candidate for either so generate a long format record */
		/* Calculate the flags */
		int flags = 1 | ((addressOffsetEncoding << 6) & 0xC0) | ((referenceOffsetEncoding << 4) & 0x30);

#if defined(J9VM_OPT_NEW_OBJECT_HASH)
		if( hashCode != 0 ) {
			// We only write the hashcode if it is set.
			flags = flags | 0x02;
		}
#endif

		/* Write the long record start tag */
		writeNumber(longObjectRecordField(), 1);
		if (_Error) {
			return;
		}

		/* Write the flags */
		writeNumber(flags, 1);
		if (_Error) {
			return;
		}

		/* Write the address delta (gap) */
		writeNumber(addressOffset, addressOffsetSize);
		if (_Error) {
			return;
		}

		/* Write the class address */
		writeNumber(IDATA(objectClassAddress), wordSize());
		if (_Error) {
			return;
		}

		/* Write the hash code */
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
		if( hashCode != 0 ) {
			writeNumber(hashCode, 4);
		}
#else /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
		writeNumber(hashCode, 2);
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
		if (_Error) {
			return;
		}

		/* Write the number of references */
		writeNumber(referenceTraits.count(), 4);
		if (_Error) {
			return;
		}

		/* Write the references */
		ReferenceWriter referenceWriter(this, currentObject, referenceTraits.count(), referenceOffsetSize);

		_VirtualMachine->memoryManagerFunctions->j9mm_iterate_object_slots(
			_VirtualMachine,
			_PortLibrary,
			objectDescriptor,
			j9mm_iterator_flag_exclude_null_refs,
			binaryHeapDumpObjectReferenceIteratorWriterCallback,
			&referenceWriter
		);

		/* Now that the class has been described, add it to the class cache */
		_ClassCache.add(objectClassAddress);
	}
	
	/* Update the state of the object */
	_CurrentObject = currentObject;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::writeArrayObjectRecord() method implementation                           */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::writeArrayObjectRecord(J9MM_IterateObjectDescriptor* objectDescriptor)
{
	/* Extract a pointer to the current object */
	j9object_t currentObject = objectDescriptor->object;

	/* Calculate the address offset (gap) from the previous object                */
	/* NB : The gap is defined in terms of 32 bit words regardless of the machine */
	IDATA addressOffset         = ((char*)(currentObject) - (char*)_CurrentObject) / 4;
	int   addressOffsetSize     = numberSize(addressOffset);
	
	/* Extract the object's class */
	J9ArrayClass* arrayClass = (J9ArrayClass*)J9OBJECT_CLAZZ_VM(_VirtualMachine, currentObject);

	int hashCode = getObjectHashCode(currentObject);

	/* Calculate the length of the array */
	U_32 arrayLength     = J9INDEXABLEOBJECT_SIZE_VM(_VirtualMachine, currentObject);
	int  arrayLengthSize = numberSize(arrayLength);

	/* Handle primitive arrays and normal arrays separately */
	J9Class*    leafClass = arrayClass->leafComponentType;
	J9ROMClass* leafType  = leafClass->romClass;

	if ((arrayClass->arity == 1) && J9ROMCLASS_IS_PRIMITIVE_TYPE(leafType)) {
		/* It's a primitive array */
		J9UTF8 *leafName = J9ROMCLASS_CLASSNAME(leafType);
		/* Calculate the data type */
		int dataType = 0; /* 0 means boolean */

		switch (J9UTF8_DATA(leafName)[0]) {
			case 'c': dataType = 0x01; break;
			case 'f': dataType = 0x02; break;
			case 'd': dataType = 0x03; break;
			case 'b': if (J9UTF8_DATA(leafName)[1] == 'y') dataType = 0x04; break;
			case 's': dataType = 0x05; break;
			case 'i': dataType = 0x06; break;
			case 'l': dataType = 0x07; break;
			default : dataType = 0x00; break; /* Code allows the debugger to break on this line */
		}

		/* Calculate the encoding to be used for the record */
		int overallSize     = addressOffsetSize > arrayLengthSize ? addressOffsetSize : arrayLengthSize;
		int overallEncoding = numberSizeEncoding(overallSize);

#if defined(J9VM_OPT_NEW_OBJECT_HASH)
		if( hashCode == 0 ) {
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
			/* Calculate the start tag / flags */
			int tag = 0x20 | ((dataType << 2) & 0x1C) | (overallEncoding & 0x03);

			/* Write the start tag / flags */
			writeNumber(tag, 1);
			if (_Error) {
				return;
			}

			/* Write the address delta (gap) */
			writeNumber(addressOffset, overallSize);
			if (_Error) {
				return;
			}

			/* Write the array length */
			writeNumber(arrayLength, overallSize);
			if (_Error) {
				return;
			}

			/* Write the hash code */
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
			// Primitive arrays don't have a hashcode flag bit
			// so we never write the field, we use a long array
			// record instead.
#else /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
			writeNumber(hashCode, 2);
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
			if (_Error) {
				return;
			}

			/* Write the instance size since PHD version 6              */
			/* divide the size by 4 and write into the datastream as an */
			/* unsigned int to make it possible to encode up to 16GB    */
			UDATA size = _VirtualMachine->memoryManagerFunctions->j9gc_get_object_total_footprint_in_bytes(_VirtualMachine, currentObject);
			writeNumber(size / 4, 4);
			if (_Error) {
				return;
			}
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
		} else {
			// Hashcode is set, write a long primitive array record.
			// Write a long primitive array record with the hash code.
			/* Calculate the start tag / flags */
			int flags =((dataType << 5) & 0xE0 );
			// We can only specify byte or word size.
			if( overallEncoding != 0 ) {
				flags = flags | 0x10;
			}

			// We only write the hashcode if it is set, but it
			// will be.
			flags = flags | 0x02;

			/* Write the long record start tag */
			writeNumber(longPrimitiveArrayRecordField(), 1);
			if (_Error) {
				return;
			}

			/* Write the flags */
			writeNumber(flags, 1);
			if (_Error) {
				return;
			}

			/* Write the address delta (gap) as a byte or a word. */
			if( overallEncoding == 0 ) {
				writeNumber(addressOffset, 1);
			} else {
				writeNumber(addressOffset, wordSize());
			}
			if (_Error) {
				return;
			}

			/* Write the array length as a byte or a word. */
			if( overallEncoding == 0 ) {
				writeNumber(arrayLength, 1);
			} else {
				writeNumber(arrayLength, wordSize());
			}
			if (_Error) {
				return;
			}

			/* Write the hash code */
			writeNumber(hashCode, 4);
			if (_Error) {
				return;
			}

			/* Write the instance size since PHD version 6              */
			/* divide the size by 4 and write into the datastream as an */
			/* unsigned int to make it possible to encode up to 16GB    */
			UDATA size = _VirtualMachine->memoryManagerFunctions->j9gc_get_object_total_footprint_in_bytes(_VirtualMachine, currentObject);
			writeNumber(size / 4, 4);
			if (_Error) {
				return;
			}
		}
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
	} else {
		/* It's an object array */
		J9Class *componentClass = arrayClass->componentType;

		if (NULL != componentClass) {
			/* Finish calculating the address offset (gap) from the previous object */
			int addressOffsetEncoding = numberSizeEncoding(addressOffsetSize);

			/* Iterate through the references counting and noting the biggest offset */
			ReferenceTraits referenceTraits(this, currentObject);

			_VirtualMachine->memoryManagerFunctions->j9mm_iterate_object_slots(
					_VirtualMachine,
					_PortLibrary,
					objectDescriptor,
					j9mm_iterator_flag_exclude_null_refs,
					binaryHeapDumpObjectReferenceIteratorTraitsCallback,
					&referenceTraits
			);

			/* Determine the length of the biggest reference offset... */
			int referenceOffsetSize     = numberSize(referenceTraits.maximumOffset() / 4);
			int referenceOffsetEncoding = numberSizeEncoding(referenceOffsetSize);

			/* Calculate the flags */
			int flags = 1 | ((addressOffsetEncoding << 6) & 0xC0) | ((referenceOffsetEncoding << 4) & 0x30);

			/* Write the long record start tag */
			writeNumber(arrayObjectRecordField(), 1);
			if (_Error) {
				return;
			}

#if defined(J9VM_OPT_NEW_OBJECT_HASH)
			if( hashCode != 0 ) {
				// We only write the hashcode if it is set.
				flags = flags | 0x02;
			}
#endif

			/* Write the flags */
			writeNumber(flags, 1);
			if (_Error) {
				return;
			}

			/* Write the address delta (gap) */
			writeNumber(addressOffset, addressOffsetSize);
			if (_Error) {
				return;
			}

			/* Write the class address of the element class */
			writeNumber(IDATA(J9VM_J9CLASS_TO_HEAPCLASS(componentClass)), wordSize());
			if (_Error) {
				return;
			}

			/* Write the hash code */
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
			if (hashCode != 0) {
				writeNumber(hashCode, 4);
			}
#else /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
			writeNumber(hashCode, 2);
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
			if (_Error) {
				return;
			}

			/* Write the number of references */
			writeNumber(referenceTraits.count(), 4);
			if (_Error) {
				return;
			}

			/* Write the reference offsets */
			if (referenceTraits.count() <= 7) {
				/* The references are already stored so just output them */
				for (UDATA i = 0; i < referenceTraits.count(); i++) {
					writeNumber(referenceTraits.offset(i) / 4, referenceOffsetSize);
					if (_Error) {
						return;
					}
				}
			} else {
				/* There were too many references to remember so scan the object again */	
				ReferenceWriter referenceWriter(this, currentObject, referenceTraits.count(), referenceOffsetSize);

				_VirtualMachine->memoryManagerFunctions->j9mm_iterate_object_slots(
						_VirtualMachine,
						_PortLibrary,
						objectDescriptor,
						j9mm_iterator_flag_exclude_null_refs,
						binaryHeapDumpObjectReferenceIteratorWriterCallback,
						&referenceWriter
				);
			}

			/* Write the true length of the array */
			writeNumber(arrayLength, 4);
			if (_Error) {
				return;
			}

			/* Write the instance size since PHD version 6              */
			/* divide the size by 4 and write into the datastream as an */
			/* unsigned int to make it possible to encode up to 16GB    */
			UDATA size = _VirtualMachine->memoryManagerFunctions->j9gc_get_object_total_footprint_in_bytes(_VirtualMachine, currentObject);
			writeNumber(size / 4, 4);
			if (_Error) {
				return;
			}
		}
	}

	/* Update the state of the object */
	_CurrentObject = currentObject;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::writeClassRecord() classes-on-heap version method implementation         */
/*                                                                                                */
/**************************************************************************************************/
void
BinaryHeapDumpWriter::writeClassRecord(J9Class *currentClass)
{
	if (J9CLASS_FLAGS(currentClass) & J9AccClassHotSwappedOut) {
		/* Skip classes which have been redefined */
		return;
	}
	
	if (J9CLASS_FLAGS(currentClass) & J9AccClassDying) {
		/* Skip classes which are dying */
		return;
	}
	
	/* Get the heap class object */
	j9object_t currentObject = J9VM_J9CLASS_TO_HEAPCLASS(currentClass);
	
	if (!J9VM_IS_INITIALIZED_HEAPCLASS_VM(_VirtualMachine, currentObject)) {
		return;
	}
	

	/* Calculate the address delta (gap) from the previous object                 */
	/* NB : The gap is defined in terms of 32 bit words regardless of the machine */
	IDATA addressOffset         = ((char*)(currentObject) - (char*)_CurrentObject) / 4;
	int   addressOffsetSize     = numberSize(addressOffset);
	int   addressOffsetEncoding = numberSizeEncoding(addressOffsetSize);

	/* Iterate through the instance references counting them and noting the biggest offset */
	ReferenceTraits referenceTraits(this, currentObject);

	/* add the class->classloader reference CMVC 136145 */
	if (NULL != currentClass && NULL != currentClass->classLoader) {
		referenceTraits.addReference((j9object_t)currentClass->classLoader->classLoaderObject);
	}

	J9MM_IterateObjectDescriptor objectDescriptor;
	_VirtualMachine->memoryManagerFunctions->j9mm_initialize_object_descriptor(_VirtualMachine, &objectDescriptor, currentObject);
	
	_VirtualMachine->memoryManagerFunctions->j9mm_iterate_object_slots(
		_VirtualMachine,
		_PortLibrary,
		&objectDescriptor,
		j9mm_iterator_flag_exclude_null_refs,
		binaryHeapDumpObjectReferenceIteratorTraitsCallback,
		&referenceTraits
	);
	
	/* Determine the number of references and the length of the biggest reference offset */
	UDATA instanceActiveReferenceCount = referenceTraits.count();
	int   instanceReferenceOffsetSize  = numberSize(referenceTraits.maximumOffset() / 4);

	/* Iterate through the static references counting them and noting the biggest offset... */
	/* Loop through the table of static references */
	j9object_t* staticReferencePointer       = (j9object_t*)(currentClass->ramStatics);
	UDATA      staticReferenceCount         = currentClass->romClass->objectStaticCount;
	UDATA      staticActiveReferenceCount   = 0;
	IDATA      staticReferenceMaximumOffset = 0;
	IDATA      staticReferenceMinimumOffset = 0;

	for (UDATA i = 0; i < staticReferenceCount; i++) {
		/* Extract the current reference and ignore nulls */
		j9object_t objectReference = staticReferencePointer[i];
		if (objectReference == 0) {
			continue;
		}
		
		/* Count the non null references */
		staticActiveReferenceCount++;
		
		/* Calculate the offset (gap) from the current object */
		IDATA offset = ((char*)(objectReference) - (char*)currentObject);
		
		/* Remember the biggest offsets */
		if (offset > staticReferenceMaximumOffset) {
			staticReferenceMaximumOffset = offset;
		}

		if (offset < staticReferenceMinimumOffset) {
			staticReferenceMinimumOffset = offset;
		}	
	}
	
	/* Constant pool class references restored to PHD heapdumps, see CMVC 177688 */
	ConstantPoolClassIterator constantPool(currentClass);

	/* Iterate through the constant pool counting class references and noting the biggest offset */
	j9object_t classObject = constantPool.nextClassObject();
	while (NULL != classObject) {
		/* Calculate the offset (gap) from the current object */
		IDATA offset = ((char*)(classObject) - (char*)currentObject);

		/* Remember the biggest offsets */
		if (offset > staticReferenceMaximumOffset) {
			staticReferenceMaximumOffset = offset;
		}
		if (offset < staticReferenceMinimumOffset) {
			staticReferenceMinimumOffset = offset;
		}

		/* Count the non null references */
		staticActiveReferenceCount++;

		classObject = constantPool.nextClassObject();
	}

	/* Superclass references restored to PHD heapdumps, see CMVC 177688 */
	SuperclassesIterator superclasses(currentClass);

	/* Iterate through the superclasses counting class references and noting the biggest offset */
	classObject = superclasses.nextClassObject();
	while (NULL != classObject) {
		/* Calculate the offset (gap) from the current object */
		IDATA offset = ((char*)(classObject) - (char*)currentObject);

		/* Remember the biggest offsets */
		if (offset > staticReferenceMaximumOffset) {
			staticReferenceMaximumOffset = offset;
		}
		if (offset < staticReferenceMinimumOffset) {
			staticReferenceMinimumOffset = offset;
		}

		/* Count the non null references */
		staticActiveReferenceCount++;

		classObject = superclasses.nextClassObject();
	}

	/* Interface references restored to PHD heapdumps, see CMVC 177688 */
	InterfacesIterator interfaces(currentClass);

	/* Iterate through the superclasses counting class references and noting the biggest offset */
	classObject = interfaces.nextClassObject();
	while (NULL != classObject) {
		/* Calculate the offset (gap) from the current object */
		IDATA offset = ((char*)(classObject) - (char*)currentObject);

		/* Remember the biggest offsets */
		if (offset > staticReferenceMaximumOffset) {
			staticReferenceMaximumOffset = offset;
		}
		if (offset < staticReferenceMinimumOffset) {
			staticReferenceMinimumOffset = offset;
		}

		/* Count the non null references */
		staticActiveReferenceCount++;

		classObject = interfaces.nextClassObject();
	}

	/* Calculate the biggest difference as a positive number */
	/* NB : A byte has a range of -128 to +127 but this will say -128 needs 2 bytes */
	IDATA biggestStaticOffset =
		-staticReferenceMinimumOffset > staticReferenceMaximumOffset ?
		-staticReferenceMinimumOffset : staticReferenceMaximumOffset;
	
	/* Determine the length of the biggest static reference offset */
	int staticReferenceOffsetSize = numberSize(biggestStaticOffset / 4);
	
	/* Determine the bigger of the references' sizes and encodings */
	int combinedReferenceOffsetSize = 
		instanceReferenceOffsetSize > staticReferenceOffsetSize ?
		instanceReferenceOffsetSize : staticReferenceOffsetSize;	
	
	int combinedReferenceOffsetEncoding = numberSizeEncoding(combinedReferenceOffsetSize);

	/* Calculate the flags */
	int flags = ((addressOffsetEncoding << 6) & 0xC0) | ((combinedReferenceOffsetEncoding << 4) & 0x30);

	/* Calculate the instance size preventing sizes big than 32 bits breaking the dump */
	UDATA instanceSize = (currentClass->totalInstanceSize + sizeof(J9NonIndexableObject)) & 0xFFFFFFFF;

	/* Extract the superclass address */
	J9Class* superClass = currentClass->superclasses[J9CLASS_DEPTH(currentClass) - 1];

	/* Calculate the class name */
	CharacterString className(_PortLibrary);

	/* Handle array and normal classes separately */
	if (J9ROMCLASS_IS_ARRAY(currentClass->romClass)) {
		/* Cast the class pointer to be an array class pointer                */
		/* NB : This only works because of the similarity of these structures */
		J9ArrayClass* arrayClass = (J9ArrayClass*)currentClass;

		/* Add a [ to the class name for each depth of the array */
		for (UDATA i = 1; i < arrayClass->arity; i++) {
			className += '[';
		}

		/* Extract a pointer to the array element class */
		J9Class* leafClass = arrayClass->leafComponentType;

		/* Complete the class name */
		J9UTF8* className1 =  J9ROMCLASS_CLASSNAME(leafClass->arrayClass->romClass);
		className.append((char*)J9UTF8_DATA(className1), J9UTF8_LENGTH(className1));

		if (!J9ROMCLASS_IS_PRIMITIVE_TYPE(leafClass->romClass)) {
			J9UTF8* className2 = J9ROMCLASS_CLASSNAME(leafClass->romClass);
			className.append((char*)J9UTF8_DATA(className2), J9UTF8_LENGTH(className2));
			className += ';';
		}

	} else {
		J9UTF8* className1 = J9ROMCLASS_CLASSNAME(currentClass->romClass);
		className.append((char*)J9UTF8_DATA(className1), J9UTF8_LENGTH(className1));
	}

	int hashCode = getObjectHashCode(currentObject);

	/* Write the class record start tag */
	writeNumber(classObjectRecordField(), 1);
	if (_Error) {
		return;
	}
	
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
	if( hashCode != 0 ) {
		// Only write out the hashcode if it is set.
		// We use a different bit for the flag in class records.
		flags = flags | 0x08;
	}
#endif

	/* Write the flags */
	writeNumber(flags, 1);
	if (_Error) {
		return;
	}

	/* Write the address delta (gap) */
	writeNumber(addressOffset, addressOffsetSize);
	if (_Error) {
		return;
	}

	/* Write the instance size */
	writeNumber(instanceSize, 4);
	if (_Error) {
		return;
	}

	/* Write the hash code */
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
	if( hashCode != 0 ) {
		writeNumber(hashCode, 4);
	}
#else /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
	writeNumber(hashCode, 2);
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
	if (_Error) {
		return;
	}

	/* Write the super class address */
	writeNumber(IDATA(J9VM_J9CLASS_TO_HEAPCLASS(superClass)), wordSize());
	if (_Error) {
		return;
	}

	/* Write the class name data length */
	writeNumber(className.length(), 2);
	if (_Error) {
		return;
	}

	/* Write the class name data */
	writeCharacters(className.data(), className.length());
	if (_Error) {
		return;
	}

	/* Write the combined number of references */
	writeNumber(instanceActiveReferenceCount + staticActiveReferenceCount, 4);
	if (_Error) {
		return;
	}

	/* Write the instance references */
	if (instanceActiveReferenceCount <= 7) {
		/* There are only a few so they can be written from the cache in the reference traits object */
		for (UDATA i = 0; i < instanceActiveReferenceCount; i++) {
			writeNumber(referenceTraits.offset(i) / 4, combinedReferenceOffsetSize);
			if (_Error) {
				return;
			}
		}
	} else {
		/* There are more than would fit in the cache, so the object must be re-scanned. First write out the 
		 * classloader reference (if any) as that was also added to the reference traits object - JTC-JAT 90676
		 */
		if (NULL != currentClass && NULL != currentClass->classLoader) {
			j9object_t objectReference = currentClass->classLoader->classLoaderObject;
			if (NULL != objectReference) {
				/* Calculate the offset (gap) from the current object */
				IDATA offset = ((char*)(objectReference) - (char*)currentObject);
				/*  Write the reference */
				writeNumber(offset / 4, combinedReferenceOffsetSize);
				if (_Error) {
					return;
				}
			}
		}
		
		/* Now re-scan the class object and write out its instance references */
		ReferenceWriter referenceWriter(this, currentObject, instanceActiveReferenceCount, combinedReferenceOffsetSize);

		_VirtualMachine->memoryManagerFunctions->j9mm_iterate_object_slots(
			_VirtualMachine,
			_PortLibrary,
			&objectDescriptor,
			j9mm_iterator_flag_exclude_null_refs,
			binaryHeapDumpObjectReferenceIteratorWriterCallback,
			&referenceWriter
		);	
	}
	
	/* Write the static references */
	for (UDATA j = 0; j < staticReferenceCount; j++) {
		/* Extract the current reference and ignore nulls */
		j9object_t objectReference = staticReferencePointer[j];
		if (objectReference == 0) {
			continue;
		}

		/* Calculate the offset (gap) from the current object */
		IDATA offset = ((char*)(objectReference) - (char*)currentObject);
		
		/* Write the record */
		writeNumber(offset / 4, combinedReferenceOffsetSize);
		if (_Error) {
			return;
		}
	}

	/* Constant pool class references restored to PHD heapdumps, see CMVC 177688 */
	ConstantPoolClassIterator constantPoolRefs(currentClass);

	classObject = constantPoolRefs.nextClassObject();
	while (NULL != classObject) {

		/* Calculate the offset (gap) from the current object */
		IDATA offset = ((char*)(classObject) - (char*)currentObject);

		/* Write the record */
		writeNumber(offset / 4, combinedReferenceOffsetSize);
		if (_Error) {
			return;
		}
		classObject = constantPoolRefs.nextClassObject();
	}

	/* Superclass references restored to PHD heapdumps, see CMVC 177688 */
	SuperclassesIterator superclassesRefs(currentClass);

	classObject = superclassesRefs.nextClassObject();
	while (NULL != classObject) {

		/* Calculate the offset (gap) from the current object */
		IDATA offset = ((char*)(classObject) - (char*)currentObject);

		/* Write the record */
		writeNumber(offset / 4, combinedReferenceOffsetSize);
		if (_Error) {
			return;
		}
		classObject = superclassesRefs.nextClassObject();
	}

	/* Interfaces references restored to PHD heapdumps, see CMVC 177688 */
	InterfacesIterator interfacesRefs(currentClass);

	classObject = interfacesRefs.nextClassObject();
	while (NULL != classObject) {

		/* Calculate the offset (gap) from the current object */
		IDATA offset = ((char*)(classObject) - (char*)currentObject);

		/* Write the record */
		writeNumber(offset / 4, combinedReferenceOffsetSize);
		if (_Error) {
			return;
		}
		classObject = interfacesRefs.nextClassObject();
	}

	/* Update the state of the object */
	_CurrentObject = currentObject;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::getObjectHashCode(j9object_t object) method implementation                                       */
/*                                                                                                */
/**************************************************************************************************/
int BinaryHeapDumpWriter::getObjectHashCode(j9object_t object) {
	/* Determine the hash code */
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
	/* Sniff hash code without growing of objects
	 */
	UDATA objectFlags = TMP_J9OBJECT_FLAGS(object);
	UDATA hashed = objectFlags & (OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS | OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS);

	int hashCode = 0;
	if ( hashed ) {
		hashCode = _VirtualMachine->memoryManagerFunctions->j9gc_objaccess_getObjectHashCode(_VirtualMachine, object );
	}

#else /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
	int hashCode = (object->flags & OBJECT_HEADER_HASH_MASK) >> 16;
	if (hashCode == 0) {
		objectHashCode(_VirtualMachine, object);
		hashCode = (object->flags & OBJECT_HEADER_HASH_MASK) >> 16;
	}
#endif /* defined(J9VM_OPT_NEW_OBJECT_HASH) */
	return hashCode;
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::numberSize() method implementation                                       */
/*                                                                                                */
/**************************************************************************************************/
int
BinaryHeapDumpWriter::numberSize(IDATA number)
{
	if ((number <= (I_8)I_8_MAX) && (number >= (I_8)I_8_MIN)) {
		return 1;
	} else if ((number <= ((I_16)I_16_MAX) && (number >= (I_16)I_16_MIN))) {
		return 2;
#ifdef J9VM_ENV_DATA64
	} else if ((number <= (I_32)I_32_MAX) && (number >= (I_32)I_32_MIN)) {
		return 4;
	} else {
		return 8;
	}
#else
	} else {
		return 4;
	}
#endif
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::numberSizeEncoding() method implementation                               */
/*                                                                                                */
/**************************************************************************************************/
int
BinaryHeapDumpWriter::numberSizeEncoding(int numberSize)
{
	switch (numberSize) {
	case 1 : return 0;
	case 2 : return 1;
	case 4 : return 2;
	case 8 : return 3;
	}
	
	return 0; 
}

/**************************************************************************************************/
/*                                                                                                */
/* BinaryHeapDumpWriter::wordSize() method implementation                                         */
/*                                                                                                */
/**************************************************************************************************/
int
BinaryHeapDumpWriter::wordSize(void)
{
#ifdef J9VM_ENV_DATA64
	return 8;
#else
	return 4;
#endif
}

void
BinaryHeapDumpWriter::checkForIOError(void)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	if (_OutputStream.hasError()) {
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR, "Heap", j9error_last_error_message());
		Trc_dump_reportDumpError_Event2("Heap", j9error_last_error_message());
		_Error = true;
	}
}

void
BinaryHeapDumpWriter::writeCharacters (const char* data, IDATA length)
{
	if (!_Error) {
		_OutputStream.writeCharacters(data,length);

		checkForIOError();
	}
}

void
BinaryHeapDumpWriter::writeCharacters (const char* data)
{
	if (!_Error) {
		_OutputStream.writeCharacters(data);

		checkForIOError();
	}
}

void
BinaryHeapDumpWriter::writeNumber (IDATA data, int length)
{
	if (!_Error) {
		_OutputStream.writeNumber(data, length);

		checkForIOError();
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* Iterator call back functions                                                                   */
/*                                                                                                */
/**************************************************************************************************/
static jvmtiIterationControl
binaryHeapDumpHeapIteratorCallback(J9JavaVM* vm, J9MM_IterateHeapDescriptor* heapDescriptor, void* userData)
{
	vm->memoryManagerFunctions->j9mm_iterate_spaces(vm, vm->portLibrary, heapDescriptor, 0, binaryHeapDumpSpaceIteratorCallback, userData);
	return ((BinaryHeapDumpWriter*)userData)->_Error ? JVMTI_ITERATION_ABORT : JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
binaryHeapDumpSpaceIteratorCallback(J9JavaVM* vm, J9MM_IterateSpaceDescriptor* spaceDescriptor, void* userData)
{
	((BinaryHeapDumpWriter*)userData)->openNewDumpFile(spaceDescriptor);
	return ((BinaryHeapDumpWriter*)userData)->_Error ? JVMTI_ITERATION_ABORT : JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
binaryHeapDumpRegionIteratorCallback(J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDescription, void* userData)
{
	BinaryHeapDumpWriter* heapDumpWriter = (BinaryHeapDumpWriter*)userData;
	
	heapDumpWriter->_Id = regionDescription->id;
	heapDumpWriter->_RegionStart = (char*)regionDescription->regionStart;
	heapDumpWriter->_RegionEnd = (char*)((UDATA)regionDescription->regionStart + regionDescription->regionSize);
	vm->memoryManagerFunctions->j9mm_iterate_region_objects(vm, vm->portLibrary, regionDescription, 0, binaryHeapDumpObjectIteratorCallback, userData);
	return ((BinaryHeapDumpWriter*)userData)->_Error ? JVMTI_ITERATION_ABORT : JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
binaryHeapDumpObjectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDescriptor, void* userData)
{
	((BinaryHeapDumpWriter*)userData)->writeObjectRecord(objectDescriptor);
	return ((BinaryHeapDumpWriter*)userData)->_Error ? JVMTI_ITERATION_ABORT : JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
binaryHeapDumpObjectReferenceIteratorTraitsCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDescriptor, J9MM_IterateObjectRefDescriptor* referenceDescriptor, void* userData)
{
	BinaryHeapDumpWriter::ReferenceTraits* referenceTraits = (BinaryHeapDumpWriter::ReferenceTraits*)userData;

	referenceTraits->addReference(referenceDescriptor->object);
	return referenceTraits->_HeapDumpWriter->_Error ? JVMTI_ITERATION_ABORT : JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
binaryHeapDumpObjectReferenceIteratorWriterCallback(J9JavaVM* virtualMachine, J9MM_IterateObjectDescriptor* objectDescriptor, J9MM_IterateObjectRefDescriptor* referenceDescriptor, void* userData)
{
	BinaryHeapDumpWriter::ReferenceWriter* referenceWriter = (BinaryHeapDumpWriter::ReferenceWriter*)userData;

	referenceWriter->writeNumber(referenceDescriptor->object);
	return referenceWriter->_HeapDumpWriter->_Error ? JVMTI_ITERATION_ABORT : JVMTI_ITERATION_CONTINUE;
}

void
writePHD(char *label, J9RASdumpContext *context, J9RASdumpAgent* agent)
{
	BinaryHeapDumpWriter(label, context, agent);
}

/* Primary entry point */
extern "C" void
runHeapdump(char *label, J9RASdumpContext *context, J9RASdumpAgent* agent)
{
	PORT_ACCESS_FROM_JAVAVM(context->javaVM);

	if (agent->requestMask & J9RAS_DUMP_DO_MULTIPLE_HEAPS) {
		if (strstr(label, "%id") == NULL) {
			/* multiple heaps requested but the %id insertion point has been removed */
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_DMP_MULTIPLE_DUMP_OVERWRITE);
		}
	}

	if (agent->dumpOptions && strstr(agent->dumpOptions, "CLASSIC")) {
		writeClassicHeapdump(label, context, agent);
	}

	if (agent->dumpOptions && strstr(agent->dumpOptions, "PHD")) {
		writePHD(label, context, agent);
	}
}
