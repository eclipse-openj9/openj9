/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 1998, 2021 IBM Corp. and others
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
package java.lang;

import java.io.*;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.IdentityHashMap;
import java.util.Set;

import com.ibm.oti.util.Msg;
import com.ibm.oti.util.Util;
import static com.ibm.oti.util.Util.appendTo;
import static com.ibm.oti.util.Util.appendLnTo;
 
/**
 * This class is the superclass of all classes which
 * can be thrown by the virtual machine. The two direct
 * subclasses represent recoverable exceptions (Exception)
 * and unrecoverable errors (Error). This class provides
 * common methods for accessing a string message which
 * provides extra information about the circumstances in
 * which the throwable was created, and for filling in a 
 * walkback (i.e. a record of the call stack at a
 * particular point in time) which can be printed later.
 *
 * @author		OTI
 * @version		initial
 *
 * @see			Error
 * @see			Exception
 * @see			RuntimeException
 */
public class Throwable implements java.io.Serializable {
	private static final long serialVersionUID = -3042686055658047285L;

	/**
	 * The message provided when the exception was created.
	 */
	private String detailMessage;

	/**
	 * An object which describes the walkback. This field is stored
	 * by the fillInStackTrace() native, and used by the 
	 * J9VMInternals.getStackTrace() native.
	 */
	private transient Object walkback;

	/**
	 * The cause of this Throwable. Null when there is
	 * no cause.
	 */
	private Throwable cause = this;
	private StackTraceElement[] stackTrace;

	private static final Throwable[] ZeroElementArray = new Throwable[0];
	private static final StackTraceElement[] ZeroStackTraceElementArray = new StackTraceElement[0];
	/**
	 * The list containing the exceptions suppressed 
	 */
	private List<Throwable> suppressedExceptions = Collections.EMPTY_LIST;
	private transient boolean disableWritableStackTrace;
	
/**
 * Constructs a new instance of this class with its 
 * walkback filled in.
 */
public Throwable () {
	super ();
	fillInStackTrace();
}

/**
 * Constructs a new instance of this class with its 
 * walkback and message filled in.
 *
 * @param		detailMessage String
 *				The detail message for the exception.
 */
public Throwable (String detailMessage) {
	this ();
	this.detailMessage = detailMessage;
}

/**
 * Constructs a new instance of this class with its 
 * walkback, message and cause filled in.
 *
 * @param		detailMessage String
 *				The detail message for the exception.
 * @param		throwable The cause of this Throwable
 */
public Throwable (String detailMessage, Throwable throwable) {
	this ();
	this.detailMessage = detailMessage;
	cause = throwable;
}

/**
 * Constructs a new instance of this class with its 
 * walkback and cause filled in.
 *
 * @param		throwable The cause of this Throwable
 */
public Throwable (Throwable throwable) {
	this ();
	this.detailMessage = throwable==null ? null : throwable.toString();
	cause = throwable;
}

/**
 * Constructs a new instance of this class with its walkback, message 
 * and cause filled in. 
 * enableSuppression and enableWritableStackTrace are true by default 
 * in other constructors
 * If enableSuppression is false, suppression is disabled, getSuppressed()
 * returns a zero-length array and calls to addSuppressed(Throwable) have 
 * no effect.
 * If enableWritableStackTrace is false, fillInStackTrace() will not be
 * called within this constructor, stackTrace field will be set to null, 
 * subsequent calls to fillInStackTrace() and setStackTrace(StackTraceElement[])
 * will not set the stack trace, and  getStackTrace() will return a zero
 * length array.
 * 
 * @param		detailMessage String
 *				The detail message for the exception.
 * @param		throwable The cause of this Throwable
 * @param		enableSuppression boolean 
 * 				enable or disable suppression
 * @param		enableWritableStackTrace boolean
 * 				whether the stack trace is writable
 * 
 * @since 1.7
 */
protected Throwable(String detailMessage, Throwable throwable, 
		boolean enableSuppression, boolean enableWritableStackTrace) {
	super ();

	this.detailMessage = detailMessage;
	cause = throwable;

	if (enableSuppression == false)	{
		suppressedExceptions = null;
	}
	
	if (enableWritableStackTrace == false)	{
		this.disableWritableStackTrace = true;
	} else {
		fillInStackTrace();
	}
}

/**
 * Record in the receiver a walkback from the point 
 * where this message was sent. The message is
 * public so that code which catches a throwable and
 * then <em>re-throws</em> it can adjust the walkback
 * to represent the location where the exception was
 * re-thrown.
 * 
 * @return the receiver
 */
public native Throwable fillInStackTrace();

/**
 * Answers the extra information message which was provided 
 * when the throwable was created. If no message was provided
 * at creation time, then answer null.
 *
 * @return		String
 *				The receiver's message.
 */
public String getMessage() {
	return detailMessage;
}

/*[PR 1FDRSWI] : J9JCL:ALL - Plum Hall failures. (Added getLocalizedMessage)*/
/**
 * Answers the extra information message which was provided 
 * when the throwable was created. If no message was provided
 * at creation time, then answer null. Subclasses may override
 * this method to answer localized text for the message.
 *
 * @return		String
 *				The receiver's message.
 */
public String getLocalizedMessage() {
	return getMessage();
}

/**
 * Answers an array of StackTraceElement. Each StackTraceElement represents
 * a entry on the stack.
 *
 * @return an array of StackTraceElement representing the stack
 */
public StackTraceElement[] getStackTrace() {
	return (StackTraceElement[])getInternalStackTrace().clone();
}

/**
 * Sets the array of StackTraceElements. Each StackTraceElement represents
 * a entry on the stack. A copy of this array will be returned by getStackTrace()
 * and printed by printStackTrace().
 *
 * @param	trace The array of StackTraceElement
 */
public void setStackTrace(StackTraceElement[] trace) {
	/*[PR 95395]*/
	if (trace == null) {
		throw new NullPointerException();
	}
	StackTraceElement[] localCopy = trace.clone();
	for (int i=0; i<localCopy.length; i++)	{
		if (localCopy[i] == null) {
			throw new NullPointerException();
		}
	}
	
	if (disableWritableStackTrace) {
		return;
	}
	
	stackTrace = localCopy;
}

/**
 * Outputs a printable representation of the receiver's
 * walkback on the System.err stream.
 */
public void printStackTrace () {
	printStackTrace(System.err);
}

/**
 * Count the number of duplicate stack frames, starting from
 * the end of the stack.
 * 
 * @param currentStack a stack to compare
 * @param parentStack a stack to compare
 *  
 * @return the number of duplicate stack frames.
 */
private static int countDuplicates(StackTraceElement[] currentStack, StackTraceElement[] parentStack) {
	int duplicates = 0;
	int parentIndex = parentStack.length;
	for (int i=currentStack.length; --i >= 0 && --parentIndex >= 0;) {
		StackTraceElement parentFrame = parentStack[parentIndex];
		if (parentFrame.equals(currentStack[i])) {
			duplicates++;
		} else {
			break;
		}
	}
	return duplicates;
}

/**
 * Answers an array of StackTraceElement. Each StackTraceElement represents
 * a entry on the stack. Cache the stack trace in the stackTrace field, returning
 * the cached field when it has already been initialized.
 *
 * @return an array of StackTraceElement representing the stack
 */
StackTraceElement[] getInternalStackTrace() {
	if (disableWritableStackTrace) {
		return	ZeroStackTraceElementArray;
	}

	StackTraceElement[] localStackTrace = stackTrace;
	if (localStackTrace == null) {
		// Assign the result to a local variable to avoid refetching
		// the instance variable and any memory ordering issues
		localStackTrace = J9VMInternals.getStackTrace(this, true);
		stackTrace = localStackTrace;
	} 
	
	return localStackTrace;
}

/**
 * Outputs a printable representation of the receiver's
 * walkback on the stream specified by the argument.
 *
 * @param		err PrintStream
 *				The stream to write the walkback on.
 */
public void printStackTrace (PrintStream err) {
	printStackTraceHelper(err);
}

/**
 * Outputs a printable representation of the receiver's
 * walkback on the writer specified by the argument.
 *
 * @param		err PrintWriter
 *				The writer to write the walkback on.
 */
public void printStackTrace(PrintWriter err) {
	printStackTraceHelper(err);
}

/**
 * Outputs representation of the receiver's
 * walkback on the Appendable specified by the argument.
 *
 * @param	appendable Appendable 
 *		The Appendable object to the walkback will be written.
 */
private void printStackTraceHelper(Appendable appendable) {
	StackTraceElement[] stack;
	Set<Throwable> exceptionChainSet = null;
	try {
		exceptionChainSet = Collections.newSetFromMap(new IdentityHashMap<Throwable, Boolean>());
	} catch(OutOfMemoryError e) {
		/* If OOM is thrown when creating exception set, then we won't be able to check for circular exception chain,
		 * which can cause OOM to be thrown again. This should be ok as we are already running out of heap memory.
		 */
	}
	stack = printStackTrace(appendable, null, 0, false, exceptionChainSet);

	Throwable throwable = getCause();
	while (throwable != null && stack != null) {
		stack = throwable.printStackTrace(appendable, stack, 0, false, exceptionChainSet);
		throwable = throwable.getCause();
	}
}

/**
 * Answers a string containing a concise, human-readable
 * description of the receiver.
 *
 * @return		String
 *				a printable representation for the receiver.
 */
@Override
public String toString () {
	/*[PR 102230] Should call getLocalizedMessage() */
	String msg = getLocalizedMessage();
	String name = getClass().getName();
	if (msg == null) {
		return name;
	} else {
		int length = name.length() + 2 + msg.length();
		StringBuilder buffer = new StringBuilder(length);
		return buffer.append(name).append(": ").append(msg).toString(); //$NON-NLS-1$
	}
}

/**
 * Initialize the cause of the receiver. The cause cannot be
 * reassigned.
 *
 * @param		throwable The cause of this Throwable
 *
 * @exception	IllegalArgumentException when the cause is the receiver
 * @exception	IllegalStateException when the cause has already been initialized
 * 
 * @return		the receiver.
 */
public synchronized Throwable initCause(Throwable throwable) {
	if (cause != this) {
		/*[MSG "K05c9", "Cause already initialized"]*/
		throw new IllegalStateException(Msg.getString("K05c9")); //$NON-NLS-1$
	}
	if (throwable == this) {
		/*[MSG "K05c8", "Cause cannot be the receiver"]*/
		throw new IllegalArgumentException(Msg.getString("K05c8")); //$NON-NLS-1$
	}
	return setCause(throwable);
}

/**
 * Helper method to set Throwable cause without going through public method initCause.
 * There is no need for synchronization for this helper method cause the only caller Throwable 
 * object is instantiated within J9VMInternals.copyThrowable and not exposed to others.
 * Synchronization need to be considered if this assumption is NOT true.  
 *
 * @param		throwable The cause of this Throwable
 *
 * @exception	IllegalArgumentException when the cause is the receiver
 * @exception	IllegalStateException when the cause has already been initialized
 *
 * @return		the receiver.
 */
Throwable setCause(Throwable throwable) {
	cause = throwable;
	return this;
}

/**
 * Answers the cause of this Throwable, or null if there
 * is no cause.
 *
 * @return		Throwable
 *				The receiver's cause.
 */
public Throwable getCause() {
	if (cause == this) return null;
	return cause;
}

private void writeObject(ObjectOutputStream s) throws IOException {
	// ensure the stackTrace field is initialized
	getInternalStackTrace();
	s.defaultWriteObject();
}

private void readObject(ObjectInputStream s) 
	throws IOException, ClassNotFoundException	{
	s.defaultReadObject();
	
	disableWritableStackTrace = (stackTrace == null);
	
	if (stackTrace != null) {
		if (stackTrace.length == 1) {
			if (stackTrace[0] == null) {
				/*[MSG "K0560", "Null stack trace element not permitted in serial stream"]*/
				throw new NullPointerException(com.ibm.oti.util.Msg.getString("K0560")); //$NON-NLS-1$
			}
			if (stackTrace[0].equals(new StackTraceElement("", "", null, Integer.MIN_VALUE))) { //$NON-NLS-1$ //$NON-NLS-2$
				stackTrace = null;
			}
		} else {
			for (int i=0; i<stackTrace.length; i++) {
				if (stackTrace[i] == null) {
					/*[MSG "K0560", "Null stack trace element not permitted in serial stream"]*/
					throw new NullPointerException(com.ibm.oti.util.Msg.getString("K0560")); //$NON-NLS-1$
				}
			}
		}
	}
	
	
	if (suppressedExceptions != null) {
		List<Throwable> newList = Collections.EMPTY_LIST;
		try {
/*[IF Sidecar19-SE]*/
			Module classModule = suppressedExceptions.getClass().getModule();
			if (Object.class.getModule().equals(classModule)) {
/*[ELSE]*/
			ClassLoader listClassLoader = suppressedExceptions.getClass().getClassLoader();
			/* null ClassLoader from getClassLoader() call represents the bootstrap ClassLoader */
			if (listClassLoader == null) {
/*[ENDIF]*/
				int listSize = suppressedExceptions.size();
				if (listSize != 0) {
					newList = new ArrayList<Throwable>(listSize);
					for (Throwable t : suppressedExceptions) {
						if (t == null) {
							/*[MSG "K0561", "Null entries not permitted in suppressedExceptions serial stream"]*/
							throw new NullPointerException(com.ibm.oti.util.Msg.getString("K0561")); //$NON-NLS-1$
						} else if (t == this) {
							/*[MSG "K0562", "Self-pointers not permitted in suppressedExceptions serial stream"]*/
							throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0562")); //$NON-NLS-1$
						} else {
							newList.add(t);
						}
					}
				}
			} else {
				/*[MSG "K0C00", "Non-standard List class not permitted in suppressedExceptions serial stream"]*/
				throw new java.io.StreamCorruptedException(com.ibm.oti.util.Msg.getString("K0C00")); //$NON-NLS-1$
			}
		} finally {
			suppressedExceptions = newList;
		}
	}
}

/**
 * Print stack trace.
 *
 * The stack trace is constructed with appendTo() and avoids allocating Objects (i.e. Strings)
 * to try to continue printing as much as possible of the stack trace even in the presence of
 * OutOfMemoryErrors (CMVC 97756).
 *
 * @param	err	
 * 			the specified print stream/writer 
 * @param	parentStack	
 * 			parent stack elements
 * @param	indents	
 * 			number of indents (\t) to be printed
 * @param	suppressed	
 * 			if this is an exception suppressed
 * @param	exceptionChainSet
 * 			set of exceptions in the exception chain
 * 
 * @return	an array of stack trace elements printed 
 * 
 */
private StackTraceElement[] printStackTrace(
		Appendable err, StackTraceElement[] parentStack, int indents, boolean suppressed, Set<Throwable> exceptionChainSet) {
	/*[PR 120593] CDC 1.0 and CDC 1.1 TCK fails in java.lang.. Exception tests */
	if (err == null) throw new NullPointerException();
	StackTraceElement[] stack;
	boolean outOfMemory = this instanceof OutOfMemoryError;
	if ((exceptionChainSet != null) && (exceptionChainSet.contains(this))) {
		if (!outOfMemory) {
			try {
				appendTo(err, "\t[CIRCULAR REFERENCE:" + toString() + "]", 0); //$NON-NLS-1$
			} catch(OutOfMemoryError e) {
				outOfMemory = true;
			}
		}
		if (outOfMemory) {
			appendTo(err, "\t[CIRCULAR REFERENCE:"); //$NON-NLS-1$
			try {
				appendTo(err, getClass().getName());
			} catch(OutOfMemoryError e) {
				appendTo(err, "java.lang.OutOfMemoryError(?)");
			}
			appendTo(err, "]");
		}
		appendLnTo(err);
		return null;
	}
	try {
		exceptionChainSet.add(this);
	} catch(OutOfMemoryError e) {
		/* If OOM is thrown when adding Throwable to exception set, then we may not be able to identify circular exception chain,
		 * which can cause OOM to be thrown again. This should be ok as we are already running out of heap memory.
		 */
	}
	if (parentStack != null) {
		if (suppressed) {
			appendTo(err, "Suppressed: ", indents); //$NON-NLS-1$
		} else {
			appendTo(err, "Caused by: ", indents); //$NON-NLS-1$
		}
	}
	if (!outOfMemory) {
		try {
			appendTo(err, toString());
		} catch(OutOfMemoryError e) {
			outOfMemory = true;
		}
	}
	if (outOfMemory) {
		try {
			appendTo(err, getClass().getName());
		} catch(OutOfMemoryError e) {
			outOfMemory = true;
			appendTo(err, "java.lang.OutOfMemoryError(?)");			 //$NON-NLS-1$
		}
		try {
			String message = getLocalizedMessage();
			if (message != null) {
				appendTo(err, ": "); //$NON-NLS-1$
				appendTo(err, message);
			}
		} catch(OutOfMemoryError e) {
			outOfMemory = true;
		}
	}
	appendLnTo(err);
	int duplicates = 0;
	try {
		// Don't use getStackTrace() as it calls clone()
		// Get stackTrace, in case stackTrace is reassigned
		stack = getInternalStackTrace();
		/*[PR CMVC 90361] look for duplicate entries in parent stack traces */
		if (parentStack != null) {
			duplicates = countDuplicates(stack, parentStack);
		}
	} catch(OutOfMemoryError e) {
		appendTo(err, "\tat ?", indents); //$NON-NLS-1$
		appendLnTo(err);
		return null;
	}
	for (int i=0; i < stack.length - duplicates; i++) {
		StackTraceElement element = stack[i];
		if (!outOfMemory) {
			try {
				appendTo(err, "\tat " + element, indents); //$NON-NLS-1$
			} catch(OutOfMemoryError e) {
				outOfMemory = true;
			}
		}
		if (outOfMemory) {
			appendTo(err, "\tat ", indents); //$NON-NLS-1$
			Util.printStackTraceElement(element, null, err, false);
		}
		appendLnTo(err);
	}
	if (duplicates > 0) {
		if (!outOfMemory) {
			try {
				appendTo(err, "\t... " + duplicates + " more", indents); //$NON-NLS-1$ //$NON-NLS-2$
			} catch(OutOfMemoryError e) {
				outOfMemory = true;
			}
		}
		if (outOfMemory) {
			appendTo(err, "\t... ", indents); //$NON-NLS-1$
			appendTo(err, duplicates);
			appendTo(err, " more"); //$NON-NLS-1$
        	}		
		appendLnTo(err);
	}
    
	synchronized (this) {
		if (suppressedExceptions != null) {
			for (Throwable t : suppressedExceptions) {
				StackTraceElement[] stackSuppressed;
				stackSuppressed = t.printStackTrace(err, stack, indents + 1, true, exceptionChainSet);
				
				Throwable throwableSuppressed = t.getCause();
				while (throwableSuppressed != null && stackSuppressed != null) {
					stackSuppressed = throwableSuppressed.printStackTrace(err, stackSuppressed, indents + 1, false, exceptionChainSet);
					throwableSuppressed = throwableSuppressed.getCause();
				}
			}
		}
	}
	return stack;
}

/**
 * The specified exception is going to be suppressed in order to give priority
 * to this exception (primary exception) and to be appended to the list of 
 * suppressed exceptions. 
 * 
 * This method is typically called by the automatically generated code from the
 * try-with-resources statement. 
 * 
 * @param 	exception Throwable
 * 			an exception to be suppressed and added to 
 * 			the list of suppressed exceptions
 * 
 * @throws	IllegalArgumentException 	
 * 			if exception is this throwable, can't suppress itself  
 * @throws	NullPointerException 			
 * 			if exception is null and there is an exception suppressed before
 * 
 * @since 1.7
 * 
 */
public final void addSuppressed(Throwable exception) {
	/*[PR CMVC 181567] Java7:JCK:java_lang/Throwable/SuppressedTests fails */
	if (exception == null) {
		/*[MSG "K0563", "Null not permitted when an exception has already been suppressed"]*/
		throw new NullPointerException(com.ibm.oti.util.Msg.getString("K0563")); //$NON-NLS-1$
	}
	
	if (exception == this) {
		/*[MSG "K0559", "A throwable cannot suppress itself"]*/
		throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0559")); //$NON-NLS-1$
	}
	synchronized (this) {
		if (suppressedExceptions != null) {
			if (suppressedExceptions.size() == 0) {
				suppressedExceptions = new ArrayList<Throwable>(2);
			}
			suppressedExceptions.add(exception);
		}
	}
}

/**
 * Returns an array of exceptions suppressed, typically by the automatically 
 * generated code from the try-with-resources statement, in order to give 
 * priority to this exception (primary exception). 
 * 
 * @return	an array of exceptions representing all exceptions suppressed to
 * 			give priority to this exception (primary exception)
 * 
 * @since 1.7
 * 
 */
public final Throwable[] getSuppressed() {
	synchronized (this) {
		if (suppressedExceptions == null || suppressedExceptions.size() == 0) {
			return ZeroElementArray;
		} else {
			return suppressedExceptions.toArray(new Throwable[suppressedExceptions.size()]);
		}
	}
}
}
