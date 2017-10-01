/*[INCLUDE-IF Panama]*/
/*
 * Licensed Materials - Property of IBM,
 *     Copyright IBM Corp. 2009, 2017  All Rights Reserved
 */
package java.lang.invoke;

import java.nicl.LibrarySymbol;


/**
 * NativeMethodHandleVargarg is a specialized version of NativeMethodHandle
 * where the method contains varargs. 
 * 
 * The reference to J9NativeCalloutData is not stored for varargs because 
 * the argument types of the varargs will only be known in the MHInterpreter, 
 * after the method has been called. 
 * 
 * Therefore, the J9NativeCalloutData needs to created every time a ffi_call 
 * is made in the MHInterpreter.
 */

public class NativeMethodHandleVararg extends NativeMethodHandle {
	
	NativeMethodHandleVararg(String methodName, MethodType type, LibrarySymbol symbol) throws IllegalAccessException {
		super(methodName, type, symbol);
	}
}