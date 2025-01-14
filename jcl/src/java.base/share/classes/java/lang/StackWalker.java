/*[INCLUDE-IF JAVA_SPEC_VERSION > 8]*/
/*
 * Copyright IBM Corp. and others 2016
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
 */
package java.lang;

/*[IF JAVA_SPEC_VERSION >= 10]*/
import java.lang.invoke.MethodType;
/*[ENDIF] JAVA_SPEC_VERSION >= 10 */
import java.lang.module.ModuleDescriptor;
import java.lang.module.ModuleDescriptor.Version;
/*[IF JAVA_SPEC_VERSION < 24]*/
import java.security.Permission;
/*[ENDIF] JAVA_SPEC_VERSION < 24 */
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.stream.Collectors;
import java.util.stream.Stream;
/*[IF JAVA_SPEC_VERSION >= 19]*/
import jdk.internal.vm.Continuation;
import jdk.internal.vm.ContinuationScope;
/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

/**
 * This provides a facility for iterating over the call stack of the current
 * thread. A StackWalker object may be used multiple times by different threads,
 * but it will represent the state of the current thread at the time the stack
 * is walked. A StackWalker may be provided with one or more Option settings to
 * include information and stack frames such as reflection methods, hidden
 * frames, and Class objects.
 */
public final class StackWalker {

	private static final int DEFAULT_BUFFER_SIZE = 1;

	/* Java StackWalker flag constants cloned from java_lang_StackWalker.cpp. */
	private static final int J9_RETAIN_CLASS_REFERENCE = 0x01;
	private static final int J9_SHOW_REFLECT_FRAMES    = 0x02;
	private static final int J9_SHOW_HIDDEN_FRAMES     = 0x04;
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private static final int J9_GET_MONITORS           = 0x08;
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	/*[IF JAVA_SPEC_VERSION >= 22]*/
	private static final int J9_DROP_METHOD_INFO       = 0x10;
	/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
	private static final int J9_GET_CALLER_CLASS       = 0x20;

	/* Map the given options to the corresponding set of flags. */
	private static int flagsFor(Set<Option> options) {
		int flags = 0;

		if (options.contains(Option.RETAIN_CLASS_REFERENCE)) {
			flags |= J9_RETAIN_CLASS_REFERENCE;
		}

		if (options.contains(Option.SHOW_REFLECT_FRAMES)) {
			flags |= J9_SHOW_REFLECT_FRAMES;
		}

		if (options.contains(Option.SHOW_HIDDEN_FRAMES)) {
			flags |= J9_SHOW_HIDDEN_FRAMES;
		}

		/* There is no option corresponding to J9_GET_MONITORS. */

		/*[IF JAVA_SPEC_VERSION >= 22]*/
		if (options.contains(Option.DROP_METHOD_INFO)) {
			flags |= J9_DROP_METHOD_INFO;
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 22 */

		return flags;
	}

	private final int bufferSize;
	private final int flags;

	/*[IF JAVA_SPEC_VERSION >= 19]*/
	private StackWalker(int flags, int estimatedDepth) {
		this(flags, null, null, null, estimatedDepth);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

	private StackWalker(
			int flags,
			/*[IF JAVA_SPEC_VERSION >= 19]*/
			ExtendedOption extendedOption,
			ContinuationScope contScope,
			Continuation continuation,
			/*[ENDIF] JAVA_SPEC_VERSION >= 19 */
			int estimatedDepth) {
		super();
		this.flags = flags;
		/*[IF JAVA_SPEC_VERSION >= 19]*/
		this.extendedOption = extendedOption;
		this.scope = contScope;
		this.cont = continuation;
		/*[ENDIF] JAVA_SPEC_VERSION >= 19 */
		this.bufferSize = estimatedDepth;

		if (estimatedDepth <= 0) {
			/*[MSG "K0641", "estimatedDepth must be greater than 0"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0641")); //$NON-NLS-1$
		}

		if ((flags & J9_RETAIN_CLASS_REFERENCE) != 0) {
			/*[IF JAVA_SPEC_VERSION < 24]*/
			@SuppressWarnings("removal")
			SecurityManager securityMgr = System.getSecurityManager();
			if (null != securityMgr) {
				securityMgr.checkPermission(PermissionSingleton.perm);
			}
			/*[ENDIF] JAVA_SPEC_VERSION < 24 */
			/*[IF JAVA_SPEC_VERSION >= 19]*/
			this.retainClassRef = true;
		} else {
			this.retainClassRef = false;
			/*[ENDIF] JAVA_SPEC_VERSION >= 19 */
		}
	}

	/*[IF JAVA_SPEC_VERSION >= 21]*/
	/**
	 * Return a new StackWalker suitable for use by PinnedThreadPrinter
	 * with appropriate flags set (including J9_GET_MONITORS).
	 */
	static StackWalker newInstanceWithMonitors() {
		int flags = J9_GET_MONITORS | J9_SHOW_REFLECT_FRAMES | J9_RETAIN_CLASS_REFERENCE;

		return new StackWalker(flags, DEFAULT_BUFFER_SIZE);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

	/**
	 * Factory method to create a StackWalker instance with no options set.
	 *
	 * @return StackWalker StackWalker object
	 */
	public static StackWalker getInstance() {
		return new StackWalker(0, DEFAULT_BUFFER_SIZE);
	}

	/**
	 * Factory method to create a StackWalker with one option. This is provided
	 * for the case where only a single option is required.
	 *
	 * @param option
	 *            select the type of information to include
	 * @return StackWalker instance configured with the value of option
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException
	 *             if option is RETAIN_CLASS_REFERENCE and the security manager
	 *             check fails
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public static StackWalker getInstance(Option option) {
		Objects.requireNonNull(option);
		return getInstance(Collections.singleton(option), DEFAULT_BUFFER_SIZE);
	}

	/**
	 * Factory method to create a StackWalker with any number of options.
	 *
	 * @param options
	 *            select the types of information to include
	 * @return StackWalker instance configured with the given options
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException
	 *             if RETAIN_CLASS_REFERENCE is requested and not permitted by
	 *             the security manager
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public static StackWalker getInstance(Set<Option> options) {
		return getInstance(options, DEFAULT_BUFFER_SIZE);
	}

	/**
	 * Factory method to create a StackWalker.
	 *
	 * @param options
	 *            select the types of information to include
	 * @param estimatedDepth
	 *            hint for the size of buffer to use. Must be 1 or greater
	 * @return StackWalker instance with the given options specifying the stack
	 *         frame information it can access
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException
	 *             if RETAIN_CLASS_REFERENCE is requested and not permitted by
	 *             the security manager
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public static StackWalker getInstance(Set<Option> options, int estimatedDepth) {
		Objects.requireNonNull(options);
		return new StackWalker(flagsFor(options), estimatedDepth);
	}

	/**
	 * @param action
	 *            {@link Consumer} object Iterate over the stack from top to
	 *            bottom and apply the {@link Consumer} to each
	 *            {@link StackFrame}
	 */
	public void forEach(Consumer<? super StackFrame> action) {
		walkWrapperImpl(flags, "forEach", s -> { //$NON-NLS-1$
			s.forEach(action);
			return null;
		});
	}

	/**
	 * Get the caller of the caller of this function, eliding any reflection or
	 * hidden frames.
	 *
	 * @return Class object for the method calling the current method.
	 * @throws UnsupportedOperationException
	 *             if the StackWalker was not created with
	 *             {@link Option#RETAIN_CLASS_REFERENCE}
	 * @throws IllegalStateException
	 *             if the caller is at the bottom of the stack
	 */
	public Class<?> getCallerClass() {
		if ((flags & J9_RETAIN_CLASS_REFERENCE) == 0) {
			/*[MSG "K0639", "Stack walker not configured with RETAIN_CLASS_REFERENCE"]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0639")); //$NON-NLS-1$
		}
		/*
		 * Get the top two stack frames: the client calling getCallerClass and
		 * the client's caller. Ignore reflection and special frames.
		 */
		List<StackFrame> result = StackWalker.walkWrapperImpl(J9_RETAIN_CLASS_REFERENCE | J9_GET_CALLER_CLASS, "getCallerClass", //$NON-NLS-1$
				s -> s.limit(2).collect(Collectors.toList()));
		if (result.size() < 2) {
			/*[MSG "K0640", "getCallerClass() called from method with no caller"]*/
			throw new IllegalCallerException(com.ibm.oti.util.Msg.getString("K0640")); //$NON-NLS-1$
		}
		if (((StackFrameImpl)result.get(0)).callerSensitive) {
			/*[MSG "K0644", "Caller-sensitive method called StackWalker.getCallerClass()"]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0644")); //$NON-NLS-1$
		}
		StackFrame clientsCaller = result.get(1);

		return clientsCaller.getDeclaringClass();
	}

	private static native <T> T walkWrapperImpl(int flags, String walkerMethod,
			Function<? super Stream<StackFrame>, ? extends T> function);

	/**
	 * Traverse the calling thread's stack at the time this method is called and
	 * apply {@code function} to each stack frame.
	 *
	 * @param <T> the type of the return value from applying function to the stream
	 * @param function operation to apply to the stream
	 * @param walkState Pointer to a J9StackWalkState struct
	 * @return the value returned by {@code function}
	 */
	private static <T> T walkImpl(Function<? super Stream<StackFrame>, ? extends T> function, long walkState) {
		T result;
		try (Stream<StackFrame> frameStream = Stream.iterate(getImpl(walkState), x -> (null != x), x -> getImpl(walkState))) {
			result = function.apply(frameStream);
		}
		return result;
	}

	private static native StackFrameImpl getImpl(long walkState);

	/**
	 * Traverse the calling thread's stack at the time this method is called and
	 * apply {@code function} to each stack frame.
	 *
	 * @param <T> the type of the return value from applying function to the stream
	 * @param function operation to apply to the stream
	 * @return the value returned by {@code function}
	 */
	public <T> T walk(Function<? super Stream<StackFrame>, ? extends T> function) {
		/*[IF JAVA_SPEC_VERSION >= 19]*/
		if (null != cont) {
			if (cont.trylockAccess()) {
				try {
					return walkContinuationImpl(flags, function, cont);
				} finally {
					cont.unlockAccess();
				}
			} else {
				throw new IllegalStateException("Continuation is mounted.");
			}
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 19 */
		return walkWrapperImpl(flags, "walk", function); //$NON-NLS-1$
	}

	/*[IF JAVA_SPEC_VERSION >= 19]*/
	final boolean retainClassRef;

	private final ExtendedOption extendedOption;
	private final ContinuationScope scope;
	private final Continuation cont;

	private static native <T> T walkContinuationImpl(int flags, Function<? super Stream<StackFrame>, ? extends T> function, Continuation cont);

	static StackWalker newInstance(Set<Option> options, ExtendedOption extendedOption) {
		return newInstance(options, extendedOption, null, null);
	}

	static StackWalker newInstance(Set<Option> options, ExtendedOption extendedOption, ContinuationScope contScope) {
		return newInstance(options, extendedOption, contScope, null);
	}

	static StackWalker newInstance(Set<Option> options, ExtendedOption extendedOption, ContinuationScope contScope, Continuation continuation) {
		Objects.requireNonNull(options);
		return new StackWalker(flagsFor(options), extendedOption, contScope, continuation, DEFAULT_BUFFER_SIZE);
	}

	enum ExtendedOption {
		LOCALS_AND_OPERANDS;
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

	/**
	 * Selects what type of stack and method information is provided by the
	 * StackWalker.
	 */
	public static enum Option {
		/**
		 * Allow clients to obtain a method's Class object.
		 */
		RETAIN_CLASS_REFERENCE,
		/*[IF JAVA_SPEC_VERSION >= 22]*/
		/**
		 * A client may use this option to signal that method information is not
		 * required. UnsupportedOperationException is thrown if an attempt is
		 * made to access that suppressed information.
		 */
		DROP_METHOD_INFO,
		/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
		/**
		 * Include stack frames for reflection methods.
		 */
		SHOW_REFLECT_FRAMES,
		/**
		 * Include stack frames for reflection methods, as well as JVM special
		 * stack frames, such as frames for anonymous classes.
		 */
		SHOW_HIDDEN_FRAMES;
	}

	/**
	 * Contains information about the StackWalker's current stack frame.
	 */
	public static interface StackFrame {

		/**
		 * @return the offset of the current bytecode in the method represented
		 *         by this frame
		 */
		int getByteCodeIndex();

		/**
		 * @return the binary name of the declaring class of this frame's
		 *         method
		 */
		String getClassName();

		/**
		 * @return the Class object of the declaring class of this frame's
		 *         method
		 * @throws UnsupportedOperationException
		 *             if the StackWalker was not created with
		 *             Option.RETAIN_CLASS_REFERENCE
		 */
		Class<?> getDeclaringClass();

		/**
		 * @return file name of the class containing the current method (may be
		 *         null)
		 */
		String getFileName();

		/**
		 * @return location of the current point of execution in the source
		 *         file, or a negative number if this information is unavailable
		 *         or the method is native
		 */
		int getLineNumber();

		/**
		 * @return the name of this StackFrame's method
		 */
		String getMethodName();

		/**
		 * @return true if the method represented by this StackFrame is a native
		 *         method
		 */
		boolean isNativeMethod();

		/**
		 * Converts this StackFrame into a StackTraceElement.
		 *
		 * @return StackTraceElement
		 */
		StackTraceElement toStackTraceElement();

		/*[IF JAVA_SPEC_VERSION >= 10]*/
		/**
		 * @throws UnsupportedOperationException if this method is not overridden
		 * @return MethodType containing the parameter and return types for the associated method
		 * @since 10
		 */
		default MethodType getMethodType() {
			throw new UnsupportedOperationException();
		}

		/**
		 * @throws UnsupportedOperationException if this method is not overridden or the StackWalker
		 * instance is not configured with RETAIN_CLASS_REFERENCE
		 * @return method descriptor string representing the type of this frame's method
		 * @since 10
		 */
		default String getDescriptor() {
			throw new UnsupportedOperationException();
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 10 */
	}

	static final class StackFrameImpl implements StackFrame {

		private Class<?> declaringClass;
		private String fileName;
		private int bytecodeIndex;
		private String classLoaderName;
		private String className;
		private int lineNumber;
		private Module frameModule;
		private String methodName;
		private String methodSignature;
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		private Object[] monitors;
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
		/*[IF JAVA_SPEC_VERSION >= 22]*/
		private int flags; /* a copy of StackWalker.flags */
		/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
		private boolean callerSensitive;

		/*
		 * Throw UnsupportedOperationException if Option.DROP_METHOD_INFO
		 * was specified when the StackWalker was created (Java 22+).
		 */
		private void ensureMethodInfo() {
			/*[IF JAVA_SPEC_VERSION >= 22]*/
			if ((flags & J9_DROP_METHOD_INFO) != 0) {
				/*[MSG "K0639D","Stack walker configured with DROP_METHOD_INFO"]*/
				throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0639D")); //$NON-NLS-1$
			}
			/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
		}

		@Override
		public int getByteCodeIndex() {
			ensureMethodInfo();
			return bytecodeIndex;
		}

		@Override
		public String getClassName() {
			return className;
		}

		@Override
		public Class<?> getDeclaringClass() {
			if (null == declaringClass) {
				/*[MSG "K0639","Stack walker not configured with RETAIN_CLASS_REFERENCE"]*/
				throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0639")); //$NON-NLS-1$
			} else {
				return declaringClass;
			}
		}

		@Override
		public String getFileName() {
			ensureMethodInfo();
			return fileName;
		}

		@Override
		public int getLineNumber() {
			ensureMethodInfo();
			return lineNumber;
		}

		@Override
		public String getMethodName() {
			ensureMethodInfo();
			return methodName;
		}

		@Override
		public boolean isNativeMethod() {
			ensureMethodInfo();
			return -2 == lineNumber;
		}

		@Override
		public StackTraceElement toStackTraceElement() {
			ensureMethodInfo();

			String moduleName = null;
			String moduleVersion = null;
			if (null != frameModule && frameModule.isNamed()) {
				ModuleDescriptor desc = frameModule.getDescriptor();
				moduleName = desc.name();
				Optional<Version> versionInfo = desc.version();
				if (versionInfo.isPresent()) {
					moduleVersion = versionInfo.get().toString();
				}
			}

			StackTraceElement element = new StackTraceElement(classLoaderName,
					moduleName, moduleVersion, className, methodName, fileName,
					lineNumber);

			/**
			 * Disable including classloader name and module version in stack trace output
			 * until StackWalker StackTraceElement include info flags can be set properly.
			 *
			 * See: https://github.com/eclipse-openj9/openj9/issues/11774
			 */
			element.disableIncludeInfoFlags();

			return element;
		}

		@Override
		public String toString() {
			StackTraceElement stackTraceElement = toStackTraceElement();
			return stackTraceElement.toString();
		}

		/*[IF JAVA_SPEC_VERSION >= 10]*/
		/**
		 * Creates a MethodType object for the method associated with this frame.
		 * @throws UnsupportedOperationException if the StackWalker object is not configured with RETAIN_CLASS_REFERENCE
		 * @return MethodType object
		 * @since 10
		 */
		@Override
		public MethodType getMethodType() {
			if (null == declaringClass) {
				/*[MSG "K0639","Stack walker not configured with RETAIN_CLASS_REFERENCE"]*/
				throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0639")); //$NON-NLS-1$
			}
			ensureMethodInfo();
			return MethodType.fromMethodDescriptorString(methodSignature, declaringClass.internalGetClassLoader());
		}

		/**
		 * Creates a string containing the signature of the method associated with this frame.
		 * @return String signature
		 * @since 10
		 */
		@Override
		public String getDescriptor() {
			ensureMethodInfo();
			return methodSignature;
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 10 */

		/*[IF JAVA_SPEC_VERSION >= 21]*/
		Object[] getMonitors() {
			return monitors;
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	static final class PermissionSingleton {
		static final Permission perm =
				new RuntimePermission("getStackWalkerWithClassReference"); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
}
