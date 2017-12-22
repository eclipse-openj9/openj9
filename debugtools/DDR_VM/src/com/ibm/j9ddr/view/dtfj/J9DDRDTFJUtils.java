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
package com.ibm.j9ddr.view.dtfj;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.view.dtfj.image.J9DDRCorruptData;

/**
 * @author andhall
 *
 */
public class J9DDRDTFJUtils
{
	//copy the constant here from the DTFJ component so as to not create a binding to the DTFJ jar as it is defined on j9.ImageFactory
	private static final String DTFJ_LOGGER_NAME = "com.ibm.dtfj.log";
	private static final Logger logger = Logger.getLogger(DTFJ_LOGGER_NAME);


	@SuppressWarnings("unchecked")
	public static Iterator emptyIterator()
	{
		return new LinkedList().iterator();
	}

	/**
	 * Return an iterator which contains a single corrupt data item
	 * @param data
	 * @return
	 */
	@SuppressWarnings("unchecked")
	public static Iterator corruptIterator(CorruptData data) {
		LinkedList<CorruptData> corruptList = new LinkedList<CorruptData>();
		corruptList.add(data);
		return corruptList.iterator();
	}

	/**
	 * Create a j9ddr corrupt data 
	 * 
	 * @param process the process from the dtfj context
	 * @param e a DTFJ CDE
	 * @return a j9ddr corrupt data
	 */
	public static J9DDRCorruptData newCorruptData(IProcess process, CorruptDataException e) {
		return new J9DDRCorruptData(process, e);
	}
	
	/**
	 * Create a j9ddr corrupt data 
	 * 
	 * @param process the process from the dtfj context
	 * @return a j9ddr corrupt data
	 */
	public static J9DDRCorruptData newCorruptData(IProcess process) {
		return new J9DDRCorruptData(process);
	}

	/**
	 * Create a j9ddr corrupt data 
	 * 
	 * @param process the process from the dtfj context
	 * @param message message
	 * @return a j9ddr corrupt data
	 */
	public static J9DDRCorruptData newCorruptData(IProcess process, String message) {
		return new J9DDRCorruptData(process, message);
	}

	/**
	 * Convert a j9ddr corrupt data exception into a DTFJ corrupt data exception 
	 * 
	 * @param process the process from the dtfj context
	 * @param e the j9ddr CDE
	 * @return a DTFJ CDE
	 */
	public static com.ibm.dtfj.image.CorruptDataException newCorruptDataException(IProcess process, com.ibm.j9ddr.CorruptDataException e) {
		return new DTFJCorruptDataException(new J9DDRCorruptData(process,e), e);
	}

	/**
	 * Go through the standard handleAsCorruptDataException method to convert the supplied error condition 
	 * into a corrupt data exception as long as it is not present in the white list 
	 * AND especially for this method, if it is not a DataUnavailable. 
	 * The white list is a set of run time exceptions which must not be intercepted and must be re-thrown.  
	 * Typically this is where the DTFJ API allows RTE's to be thrown as well as checked exceptions.
	 * Likewise, DataUnavailable is an exception that must be re-thrown and
	 * not converted to CorruptDataException, hence this method's distinctive name.
	 * 
	 * @param p the process from the dtfj context
	 * @param t the error condition to handle
	 * @return the error condition expressed as a CDE
	 */
	public static com.ibm.dtfj.image.CorruptDataException handleAllButDataUnavailAsCorruptDataException(IProcess p, Throwable t) 
	throws DataUnavailable {
		if (t instanceof DataUnavailable) {
			throw (DataUnavailable) t;
		}
		return handleAsCorruptDataException(p,t);
	}
	
	/**
	 * Go through the standard handleAsCorruptDataException method to convert the supplied error condition 
	 * into a corrupt data exception as long as it is not present in the white list 
	 * AND especially for this method, if it is not a MemoryAccessException. 
	 * The white list is a set of run time exceptions which must not be intercepted and must be re-thrown.  
	 * Typically this is where the DTFJ API allows RTE's to be thrown as well as checked exceptions.
	 * Likewise, MemoryAccessException is an exception that must be re-thrown and
	 * not converted to CorruptDataException, hence this method's distinctive name.
	 * 
	 * @param p the process from the dtfj context
	 * @param t the error condition to handle
	 * @param whitelist runtime exceptions which should be ignored and re-thrown
	 * @return the error condition expressed as a CDE
	 */
	public static com.ibm.dtfj.image.CorruptDataException handleAllButMemAccExAsCorruptDataException(IProcess p, Throwable t, Class<?>[] whitelist) 
	throws MemoryAccessException {
		if (t instanceof MemoryAccessException) {
			throw (MemoryAccessException) t;
		}
		return handleAsCorruptDataException(p,t,whitelist);
	}
	
	/**
	 * Go through the standard handleAsCorruptDataException method to convert the supplied error condition 
	 * into a corrupt data exception as long as it is not present in the white list 
	 * AND especially for this method, if it is neither MemoryAccessException or DataUnavailable. 
	 * The white list is a set of run time exceptions which must not be intercepted and must be re-thrown.  
	 * Typically this is where the DTFJ API allows RTE's to be thrown as well as checked exceptions.
	 * Likewise, MemoryAccessException and DataUnavailable are exceptions that must be re-thrown and
	 * not converted to CorruptDataException, hence this method's distinctive name.
	 * 
	 * @param p the process from the dtfj context
	 * @param t the error condition to handle
	 * @param whitelist runtime exceptions which should be ignored and re-thrown
	 * @return the error condition expressed as a CDE
	 */
	public static com.ibm.dtfj.image.CorruptDataException handleAllButMemAccExAndDataUnavailAsCorruptDataException(IProcess p, Throwable t, Class<?>[] whitelist) 
	throws MemoryAccessException, DataUnavailable{
		if (t instanceof MemoryAccessException) {
			throw (MemoryAccessException) t;
		}
		if (t instanceof DataUnavailable) {
			throw (DataUnavailable) t;
		}
		return handleAsCorruptDataException(p,t,whitelist);
	}
	
	/**
	 * Go through the standard handleAsCorruptDataException method to convert the supplied error condition 
	 * into a corrupt data exception as long as it is not present in the white list. 
	 * The white list is a set of run time exceptions which must not be intercepted and must be re-thrown.  
	 * Typically this is where the DTFJ API allows RTE's to be thrown as well as checked exceptions.
	 * 
	 * @param p the process from the dtfj context
	 * @param t the error condition to handle
	 * @param whitelist runtime exceptions which should be ignored and re-thrown
	 * @return the error condition expressed as a CDE
	 */
	public static com.ibm.dtfj.image.CorruptDataException handleAsCorruptDataException(IProcess p, Throwable t, Class<?>[] whitelist) {
		boolean isInWhitelist = false;
		for(Class<?> clazz : whitelist) {
			if(RuntimeException.class.isAssignableFrom(clazz) && (isInWhitelist = t.getClass().equals(clazz))) {
				//this exception is in the whitelist
				break;
			}
		}
		if(isInWhitelist) {
			throw (RuntimeException) t;
		} else {
			return handleAsCorruptDataException(p, t);
		}
	}
	
	/**
	 * Convert the supplied error condition into a corrupt data exception 
	 * or re-throw it if it is an instance of Error that we do not want to 
	 * intercept.
	 * 
	 * @param p the process from the DTFJ context
	 * @param t error condition to convert
	 * @return CorruptDataException
	 */
	public static com.ibm.dtfj.image.CorruptDataException handleAsCorruptDataException(IProcess p, Throwable t)  {
		if(t instanceof com.ibm.dtfj.image.CorruptDataException) {
			//prevent repeated logging of this error by ignoring DTFJ CorruptDataExceptions
			return (com.ibm.dtfj.image.CorruptDataException) t;
		}
		if(isErrorNotToBeIntercepted(t)) {
			if(t instanceof Error) {
				throw (Error) t;				//re-throw unhandled errors
			}
			throw new RuntimeException(t);		//should not hit this as we handle run time exceptions
		}
		if(t instanceof com.ibm.j9ddr.CorruptDataException) {
			com.ibm.j9ddr.CorruptDataException cde = (com.ibm.j9ddr.CorruptDataException)t;
			logger.log(Level.FINE, "Corrupt data encountered", t);
			return newCorruptDataException(p, cde);
		}
		String message = logError(t);
		CorruptData cd = newCorruptData(p, message);
		return new DTFJCorruptDataException(cd, t);
	}
	
	/**
	 * Convert the supplied error condition into a CorruptData object and return 
	 * it, typically for insertion into an iterator,
	 * or re-throw it if it is an instance of Error that we do not want to 
	 * intercept.
	 * 
	 * @param p the process from the dtfj context
	 * @param t the error condition to handle
	 * @return the error expressed as corrupt data
	 */
	public static CorruptData handleAsCorruptData(IProcess p, Throwable t)  {
		if(isErrorNotToBeIntercepted(t)) {
			if(t instanceof Error) {
				throw (Error) t;				//re-throw unhandled errors
			}
			throw new RuntimeException(t);		//should not hit this as we handle run time exceptions
		}
		if(t instanceof com.ibm.j9ddr.CorruptDataException) {
			com.ibm.j9ddr.CorruptDataException cde = (com.ibm.j9ddr.CorruptDataException)t;
			logger.log(Level.FINE, "Corrupt data encountered", t);
			return newCorruptData(p, cde);
		}
		String message = logError(t);
		CorruptData cd = newCorruptData(p, message);
		return cd;
	}
	
	/**
	 * Determine if this Error is to be rethrown. Currently we want to rethrow all Errors
	 * except NoSuchFieldError which we keep back, probably in order to convert into a
	 * corrupt data exception
	 * @param t the error condition to handle
	 * @return true or false according to whether the error is to be rethrown
	 */
	private static boolean isErrorNotToBeIntercepted(Throwable t) {
		return (t instanceof Error) && !(t instanceof NoSuchFieldError);
	}
	
	//general logging of the error
	private static String logError(Throwable t) {	
		String message = null;
		if(t instanceof NoSuchFieldError) {
			//this is likely to be because the DDR blob is out of sync with the code base
			message = "DDR structures in the core file are out of sync with the code base";
		}
		if(t instanceof RuntimeException) {
			//an unhandled runtime exception
			message = "Internal runtime exception was encountered";
		}
		if(null == message) {
			//catch all
			message = "An unexpected exception occurred";
		}
		logger.log(Level.FINE, message, t);
		return message;
	}
	
	/**
	 * Handle the supplied error condition and ultimately surface it as a
	 * data unavailable exception. This is required as not all API calls 
	 * allow a CorruptDataException to be thrown
	 * 
	 * @param t error condition to handle
	 * @throws CorruptDataException ultimately throw an CDE
	 */
	public static DataUnavailable handleAsDataUnavailable(Throwable t) {
		if(t instanceof DataUnavailable) {
			//prevent repeated logging of this error by ignoring DTFJ DataUnavailable
			return (DataUnavailable) t;
		}
		if(isErrorNotToBeIntercepted(t)) {
			if(t instanceof Error) {
				throw (Error) t;				//rethrow unhandled errors
			}
			throw new RuntimeException(t);		//should not hit this as we handle run time exceptions
		}
		if(t instanceof DataUnavailable) {
			logger.log(Level.FINE, "Data unavailable", t);
			return (DataUnavailable) t;		//
		}
		String message = logError(t);
		DataUnavailable du = new DataUnavailable(message);
		du.initCause(t);
		return du;
	}

	
}
