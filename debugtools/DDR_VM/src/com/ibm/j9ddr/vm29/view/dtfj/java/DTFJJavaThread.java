/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import static com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers.*;
import static com.ibm.j9ddr.vm29.structure.J9Consts.*;
import static com.ibm.j9ddr.vm29.events.EventManager.register;
import static com.ibm.j9ddr.vm29.events.EventManager.unregister;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRCorruptData;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageProcess;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageSection;
import com.ibm.j9ddr.view.dtfj.image.J9DDRStubImageThread;
import com.ibm.j9ddr.vm29.j9.J9JavaStackIterator;
import com.ibm.j9ddr.vm29.j9.stackwalker.BaseStackWalkerCallbacks;
import com.ibm.j9ddr.vm29.j9.stackwalker.FrameCallbackResult;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkResult;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalker;
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaStackPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9VMThreadHelper;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

/**
 * Adapter for J9VMThreadPointer
 */
public class DTFJJavaThread implements JavaThread
{

	private static final String PRIORITY_FIELD = "priority";
	private static final String NAME_FIELD = "name";
	private static final String JAVA_LANG_THREAD_CLASS = "java/lang/Thread";
	private final J9VMThreadPointer thread;
	private String name = null;
	// -1 is returned for unknown priority so use MIN_VALUE
	// to indicate we haven't checked yet.
	private int priority = Integer.MIN_VALUE;
	private ImageThread imageThread = null;
	
	public DTFJJavaThread(J9VMThreadPointer thread)
	{
		this.thread = thread;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getImageThread()
	 */
	public ImageThread getImageThread() throws CorruptDataException, DataUnavailable
	{
		if (imageThread == null) {
			J9DDRImageProcess imageProcess = DTFJContext.getImageProcess();
			
			try {
				long tid = thread.osThread().tid().longValue();
			
				imageThread = imageProcess.getThread(tid);
							
				//We couldn't match one of the "proper" ImageThreads - so we'll return a stub image thread instead.
				if (null == imageThread) {
					imageThread = new J9DDRStubImageThread(DTFJContext.getProcess(), tid);
				}
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		
		return imageThread;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getJNIEnv()
	 */
	public ImagePointer getJNIEnv() throws CorruptDataException
	{
		return DTFJContext.getImagePointer(thread.getAddress());
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getName()
	 */
	public String getName() throws CorruptDataException
	{
		try {
			if(name == null) {
				JavaField nameField = getField(NAME_FIELD);
				if( nameField != null ) {
					name = nameField.getString(getObject());
				} else {
					name = "vmthread @" + thread.getAddress();
				}
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		return name;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getObject()
	 */
	public JavaObject getObject() throws CorruptDataException
	{
		try {
			if (thread.threadObject().isNull()) {
				return null;
			} else {
				JavaObject threadObj = new DTFJJavaObject(thread.threadObject());
				// For additional validation of the java.lang.Thread object try getting its class name
				threadObj.getJavaClass().getName();
				return threadObj;
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getPriority()
	 */
	public int getPriority() throws CorruptDataException
	{
		try {
			if(priority == Integer.MIN_VALUE) {
				JavaField priorityField = getField(PRIORITY_FIELD);
				if( priorityField != null ) {
					priority = priorityField.getInt(getObject());
				} else {
					priority = -1;
				}
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		return priority;
	}


	@SuppressWarnings("rawtypes")
	private JavaField getField(String fieldName) throws CorruptDataException, MemoryAccessException {
		JavaObject threadObj = getObject();
		if (threadObj == null) {
			return null;
		} else {
			JavaClass threadClass = threadObj.getJavaClass();
			// We need to query the field on the right class.
			while (!JAVA_LANG_THREAD_CLASS.equals(threadClass.getName())
					&& threadClass != null) {
				threadClass = threadClass.getSuperclass();
			}
			if (threadClass == null) {
				return null;
			} else {
				Iterator fieldIterator = threadClass
						.getDeclaredFields();
				while (fieldIterator.hasNext()) {
					Object nextField = fieldIterator.next();
					if (nextField instanceof JavaField) {
						JavaField field = (JavaField) nextField;
						if (fieldName.equals(field.getName())) {
							return field;
						}
					}
				}
			}
		}
		return null;
	}
	
	private List<Object> frames = null;
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getStackFrames()
	 */
	@SuppressWarnings("rawtypes")
	public Iterator getStackFrames()
	{
		if (frames == null) {
			walkStack();
		}
		
		return frames.iterator();
	}
	
	private class StackWalkerCallbacks extends BaseStackWalkerCallbacks implements IEventListener
	{
		private Object frame = null;
		
		public FrameCallbackResult frameWalkFunction(J9VMThreadPointer walkThread, WalkState walkState)
		{
			if (walkState.method.isNull()){
				return FrameCallbackResult.KEEP_ITERATING;
			}
			
			if (frame == null || frame instanceof J9DDRCorruptData) {
				setupFrame(walkState);
			}
			
			frames.add(frame);
			frame = null;
			
			return FrameCallbackResult.KEEP_ITERATING;
		}

		public void objectSlotWalkFunction(J9VMThreadPointer walkThread, WalkState walkState, PointerPointer objectSlot, VoidPointer stackAddress)
		{
			if (walkState.method.isNull()){
				/* adding an object slot iterator causes us to be called for
				 * xxx methods. These were previously ignored, since the frame
				 *does not have a valid method. We should continue to do so now.
				 */
				return;
			}
			
			try {
				J9ObjectPointer object = J9ObjectPointer.cast(objectSlot.at(0));
				
				addObjectReference(walkState, object);
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				if (frame instanceof DTFJJavaStackFrame) {
					((DTFJJavaStackFrame)frame).addReference(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e));
				}
			} catch (IllegalArgumentException e) {
				if (frame instanceof DTFJJavaStackFrame) {
					((DTFJJavaStackFrame)frame).addReference(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), new com.ibm.j9ddr.CorruptDataException(e)));
				}
			}
		}
		

		public void fieldSlotWalkFunction(J9VMThreadPointer walkThread,
				WalkState walkState, ObjectReferencePointer objectSlot,
				VoidPointer stackLocation)
		{
			if (walkState.method.isNull()){
				/* adding an object slot iterator causes us to be called for
				 * xxx methods. These were previously ignored, since the frame
				 *does not have a valid method. We should continue to do so now.
				 */
				return;
			}
			
			try {
				J9ObjectPointer object = objectSlot.at(0);
				addObjectReference(walkState, object);
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				if (frame instanceof DTFJJavaStackFrame) {
					((DTFJJavaStackFrame)frame).addReference(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e));
				}
			} catch (IllegalArgumentException e) {
				if (frame instanceof DTFJJavaStackFrame) {
					((DTFJJavaStackFrame)frame).addReference(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), new com.ibm.j9ddr.CorruptDataException(e)));
				}
			}
		}

		private void addObjectReference(WalkState walkState, J9ObjectPointer object)
		{
			if (frame == null) {
				setupFrame(walkState);
			}
			
			if (frame instanceof DTFJJavaStackFrame) {
				if (object.notNull()) {
					JavaObject dtfjObject = new DTFJJavaObject(object);
				
					JavaReference reference = new DTFJJavaReference(frame, 
																dtfjObject,
																"StackFrame Root",
																JavaReference.REFERENCE_UNKNOWN,
																JavaReference.HEAP_ROOT_STACK_LOCAL,
																JavaReference.REACHABILITY_STRONG);
				
					((DTFJJavaStackFrame)frame).addReference(reference);
				}
			}
		}

		private void setupFrame(WalkState walkState)
		{
			try {
				// JEXTRACT: This matches jextract behaviour but may be bogus.  Commented line below is likely better behaviour
				ImagePointer basePointer = DTFJContext.getImagePointer(walkState.arg0EA.longValue());
//				basePointer = DTFJContext.getImagePointer(walkState.bp.longValue());
				ImagePointer pc = DTFJContext.getImagePointer(walkState.pc.getAddress());
				J9DDRCorruptData corrupt = null;
				if (frame instanceof J9DDRCorruptData) {
					corrupt = (J9DDRCorruptData)frame;
				}
				
				DTFJJavaClass clazz = new DTFJJavaClass(J9_CLASS_FROM_METHOD(walkState.method));
				
				boolean jitted = walkState.jitInfo.notNull();
				
				frame = new DTFJJavaStackFrame(DTFJJavaThread.this, new DTFJJavaMethod(clazz, walkState.method), walkState.method, pc, basePointer,walkState.bytecodePCOffset,jitted);
				if (corrupt != null && frame instanceof DTFJJavaStackFrame) {
					((DTFJJavaStackFrame)frame).addReference(corrupt);
				}
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				frame = J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e);
			}
		}

		public void corruptData(String message,
				com.ibm.j9ddr.CorruptDataException e, boolean fatal) {
			if( frame == null ) {
				frame = J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e);
			} else if( frame instanceof DTFJJavaStackFrame ) {
				// Check this is a stack frame as it could already be corrupt data object.
				((DTFJJavaStackFrame)frame).addReference(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e));
			}
		}
	}

	
	//this function is the entry point for the stack walking and provides the top level catch for Exceptions and NoSuchFieldErrors
	private void walkStack()
	{
		frames = new LinkedList<Object>();
		
		WalkState walkState = new WalkState();
		walkState.walkThread = thread;
		walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET | J9_STACKWALK_ITERATE_O_SLOTS;

		walkState.callBacks = new StackWalkerCallbacks(); 		
		register((IEventListener)walkState.callBacks);
		StackWalkResult result = StackWalkResult.STACK_CORRUPT;
		try {
			result = StackWalker.walkStackFrames(walkState);
		} catch (Throwable t) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			result = StackWalkResult.STACK_CORRUPT;
		}
		unregister((IEventListener)walkState.callBacks);

		if(result != StackWalkResult.NONE) {
			frames.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Bad return from stack walker walking thread 0x" + Long.toHexString(walkState.walkThread.getAddress()) + ". Some stack frames may be missing. Final state = " + result));
		}
	}

	private List<Object> sections = null;

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getStackSections()
	 */
	@SuppressWarnings("rawtypes")
	public Iterator getStackSections()
	{
		if (sections == null) {
			walkSections();
		}
		
		return sections.iterator();
	}
	
	private void walkSections() {
		sections = new ArrayList<Object>();
		J9JavaStackIterator stacks;
		try {
			stacks = J9JavaStackIterator.fromJ9JavaStack(thread.stackObject());
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			sections.add(cd);
			return;
		}
		
		// JEXTRACT seems to be hard coded to only return 1 stack section ... walking the stack finds more than one.
		int count = 0;
		while (stacks.hasNext() && count < 1) {
			J9JavaStackPointer stack = stacks.next();
			try {
				long size = stack.size().longValue();
				long baseAddress = stack.end().longValue() - size;
				J9DDRImageSection newSection = DTFJContext.getImageSection(baseAddress, getSectionName());
				newSection.setSize(size);
				sections.add(newSection);
				count++;
			} catch (Throwable t) {
				CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
				sections.add(cd);
			}
		}
	}

	private String getSectionName() {
		try {
			return "JavaStackSection for JavaThread: " + getName();
		} catch (CorruptDataException e) {
			return "JavaStackSection for JavaThread: Invalid Thread Name"; 
		}
	}

	public boolean equals(Object object) {
		//note that we can't get image threads on all platforms and in all situations but we still need to return equals correctly
		
		ImageThread imageThread = null;
		
		try {
			imageThread = getImageThread();
		} catch (DataUnavailable e) {
			// Do nothing
		} catch (CorruptDataException e) {
			// Do nothing
		}
		
		boolean isEqual = (null == imageThread) ? (this == object) : false;
		
		// By definition objects from different images can not be equal
		if ((null != imageThread) && (object instanceof DTFJJavaThread)) {
			DTFJJavaThread local = (DTFJJavaThread) object;
			try {
				isEqual = imageThread.getID().equals(local.getImageThread().getID());
			} catch (CorruptDataException e) {
				// Do nothing
			} catch (DataUnavailable e) {
				// Do nothing
			}
		}
		return isEqual;
	}
	
	public int hashCode() {
		try {
			return getImageThread().getID().hashCode();
		} catch (Throwable t) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			return super.hashCode();
		}
	}

	public int getState() throws CorruptDataException {
		try {
			return J9VMThreadHelper.getDTFJState(thread);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	@Override
	public String toString() {
		StringBuilder builder = new StringBuilder();
		builder.append("JavaThread @ 0x");
		builder.append(Long.toHexString(thread.getAddress()));
		return builder.toString();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getBlockingObject()
	 */
	public JavaObject getBlockingObject() throws CorruptDataException, DataUnavailable {
		try {
			if (thread.blockingEnterObject().isNull()) {
				return null;
			} else {
				return new DTFJJavaObject(thread.blockingEnterObject());
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
}
