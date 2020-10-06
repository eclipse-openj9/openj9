package com.ibm.testbundle;

/**
 * A dummy program to print some message
 *  
 * @author administrator
 *
 */
public class SomeMessageV1 {

	/* If weaving hook bundle (com.ibm.weavinghooktest) is installed and running, 
	 * it will replace the class bytes of this class with that of SomeMessageV2.
	 */
	public void printMessage() {
		System.out.println("A message from original class");
	}
}
