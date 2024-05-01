/*
 * Copyright IBM Corp. and others 2001
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
package j9vm.test.invoker;

import java.io.IOException;
import java.io.InputStream;

import org.openj9.test.access.UnsafeClasses;
import org.openj9.test.util.VersionCheck;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

interface Root {
	default Boolean identity() {
		return true;
	}
}

/* Defined in NodeInterface.jasm:
 *   interface NodeInterface implements Root {
 *     default Boolean callRootIdentity(Object receiver) {
 *       // aload_0 replaced with aload_1
 *       return Root.super.identity();
 *     }
 *   }
 *
 * Defined in NodeClass.jasm:
 *   class NodeClass implements NodeInterface {
 *     Boolean callRootIdentity(Object receiver) {
 *       // aload_0 replaced with aload_1
 *       return Root.super.identity();
 *     }
 *   }
 */

class Outlier {
	/* Invalid receiver - This class has no relation to NodeInterface */
}

class InScope implements NodeInterface {
	/* Valid receiver - This class is a subtype of NodeInterface */
}

@Test(groups = { "level.sanity" })
public class InvokeSpecialInsideInterface {

	public static final Logger logger = Logger.getLogger(InvokeSpecialInsideInterface.class);

	@Test
	public final void invokeSpecialInsideInterface() throws Exception {
		if (VersionCheck.major() >= 17) {
			/*
			 * Loading NodeClass as a hidden class fails due to
			 *   java.lang.ClassNotFoundException: j9vm.test.invoker.NodeClass.0x0000000000000000
			 * skip this test for now.
			 * See https://github.com/eclipse-openj9/openj9/issues/13529
			 */
			return;
		}

		Outlier invalidReceiver = new Outlier();
		InScope validReceiver = new InScope();

		NodeInterface invoker = new NodeInterface() {};
		NodeInterface anonInvoker = getAnonInvoker();

		/* Invokers are used to call interface methods. In the interface methods, aload_0 is replaced
		 * with aload_1 to change the receiver with a specified argument. Invalid receiver does not
		 * implement the interface. So, IllegalAccessError must be thrown when invokespecial is executed
		 * in an interface method with an incompatible receiver. A valid receiver implements the interface
		 * so no exception should be thrown. Below, we verify this behaviour. Two invokers are used: anonymous
		 * (Unsafe.defineAnonymousClass) and non-anonymous. For anonymous invoker, JVM should use the
		 * host class for the security check. Two receivers are used: valid and invalid. All four permutations
		 * of the two invokers and two receivers are verified below.
		 */

		interfaceCall(invoker, invalidReceiver);
		interfaceCall(invoker, validReceiver);

		interfaceCall(anonInvoker, invalidReceiver);
		interfaceCall(anonInvoker, validReceiver);

		/* No assertion is required. Test will fail by throwing an exception. Test is successful if no
		 * exception is thrown.
		 */
	}

	protected static void interfaceCall(NodeInterface invoker, InScope receiver) throws Exception {
		invoker.callRootIdentity(receiver);
		logger.debug("Passed. No exception should be thrown.");
	}

	protected static void interfaceCall(NodeInterface invoker, Outlier receiver) throws Exception {
		try {
			invoker.callRootIdentity(receiver);
			throw new Exception("Expected IllegalAccessError");
		} catch (IllegalAccessError e) {
			verifyErrorMessage(e, "NodeInterface", receiver.getClass().getName());
			logger.debug("Passed. [Correct behaviour] " + e);
		}
	}

	protected static void verifyErrorMessage(Throwable e, String interfaceName, String receiverName) throws Exception {
		String expectedMessage = "Receiver class "
				+ receiverName.replace('.', '/')
				+ " must be the current class or a subtype of interface j9vm/test/invoker/"
				+ interfaceName;
		String message = e.getMessage();
		if (!message.contains(expectedMessage)) {
			throw new Exception("Incorrect message: " + message);
		}
	}

	protected static NodeInterface getAnonInvoker() throws IllegalAccessException, IOException, InstantiationException {
		String resource = NodeClass.class.getName().replace('.', '/') + ".class";
		InputStream classFile = NodeClass.class.getClassLoader().getResourceAsStream(resource);
		byte[] classFileBytes = new byte[classFile.available()];
		classFile.read(classFileBytes);

		@SuppressWarnings("deprecation")
		NodeInterface anonInvoker = (NodeInterface) UnsafeClasses.defineAnonOrHiddenClass(NodeInterface.class, classFileBytes).newInstance();
		return anonInvoker;
	}

}
