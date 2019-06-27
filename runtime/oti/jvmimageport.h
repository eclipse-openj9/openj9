#ifndef jvmimageport_h
#define jvmimageport_h

/* Macros for jvmimage port access */
#define JVMIMAGEPORT_FROM_PORT(_portLibrary) ((JVMImagePortLibrary *)(_portLibrary))
#define JVMIMAGEPORT_FROM_JAVAVM(javaVM) ((JVMImagePortLibrary *)(javaVM->jvmImagePortLibrary))
#define JVMIMAGEPORT_ACCESS_FROM_JAVAVM(javaVM) JVMImagePortLibrary *privateImagePortLibrary = JVMIMAGEPORT_FROM_JAVAVM(javaVM)

/* Macros for omr port access */
#define IMAGE_OMRPORT_FROM_JAVAVM(javaVM) ((OMRPortLibrary *)(javaVM->jvmImagePortLibrary))
#define IMAGE_OMRPORT_FROM_JVMIMAGEPORT(jvmImagePortLibrary) ((OMRPortLibrary *)jvmImagePortLibrary)

/* Macros for image access */
#define IMAGE_ACCESS_FROM_PORT(_portLibrary) JVMImage *jvmImage = (JVMImage *)(JVMIMAGEPORT_FROM_PORT(_portLibrary)->jvmImage)
#define IMAGE_ACCESS_FROM_JAVAVM(javaVM) JVMImage *jvmImage = (JVMImage *)(JVMIMAGEPORT_FROM_JAVAVM(javaVM)->jvmImage)

/*
* OMR port library wrapper
* JVMImage instance to access image functions
*/
typedef struct JVMImagePortLibrary {
	OMRPortLibrary portLibrary;
	void* jvmImage;
} JVMImagePortLibrary;

#define imem_allocate_memory(param1, category) IMAGE_OMRPORT_FROM_JVMIMAGEPORT(privateImagePortLibrary)->mem_allocate_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1, J9_GET_CALLSITE(), category)
#define imem_free_memory(param1) IMAGE_OMRPORT_FROM_JVMIMAGEPORT(privatePortLibrary)->mem_free_memory(OMRPORT_FROM_J9PORT(privatePortLibrary),param1)

#endif