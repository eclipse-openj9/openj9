/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

package org.openj9.test.contendedfields;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import static org.openj9.test.util.StringPrintStream.logStackTrace;

public class ClassFieldResolutionTests {
	protected static Logger logger = Logger.getLogger(ContendedFieldsTests.class);

	int expectedMinimumSize = 4;
	int expectedContFieldMinimumSize = 4;
	int expectedMaximumSize = 256;

	@Test
	public void testSampleClasses() {
		for (String className: sampleClasses) {
			try {
				Object testObject = Class.forName(className).newInstance();
				FieldUtilities.checkSize(testObject, expectedMinimumSize, expectedMaximumSize);
			} catch ( ClassNotFoundException e) {
				logStackTrace(e, logger);
				Assert.fail("error loading "+className);
			} catch (InstantiationException
					|IllegalAccessException e) {
				logger.error("error loading "+className);  // TODO replace with logger
			}
		}
	}

	String sampleClasses[] = {"java.lang.Object",
			"java.lang.String",
			"sun.reflect.Reflection",
			"java.util.HashMap",
			"java.lang.ref.ReferenceQueue",
			"sun.misc.VM",
			"java.util.Hashtable",
			"java.util.Properties",
			"java.lang.Thread",
			"java.lang.Throwable",
			"java.lang.Error",
			"java.lang.StackOverflowError",
			"java.lang.Exception",
			"java.lang.ReflectiveOperationException",
			"java.lang.ClassNotFoundException",
			"java.lang.LinkageError",
			"java.lang.NoClassDefFoundError",
			"java.lang.OutOfMemoryError",
			"java.lang.ExceptionInInitializerError",
			"java.util.HashSet",
			"java.util.WeakHashMap",
			"java.lang.StringBuffer",
			"java.lang.StringBuilder",
			"sun.reflect.ReflectionFactory$GetReflectionFactoryAction",
			"com.ibm.jit.DecimalFormatHelper",
			"java.lang.ThreadLocal",
			"java.util.concurrent.atomic.AtomicInteger",
			"sun.nio.cs.StandardCharsets",
			"java.io.FileDescriptor",
			"sun.misc.SharedSecrets",
			"java.util.concurrent.atomic.AtomicLong",
			"sun.security.util.Debug",
			"sun.net.www.protocol.jar.Handler",
			"sun.net.www.protocol.file.Handler",
			"sun.misc.Launcher",
			"java.util.Vector",
			"java.util.LinkedHashMap",
			"java.util.ArrayList",
			"sun.net.www.ParseUtil",
			"java.util.BitSet",
			"java.util.concurrent.ConcurrentHashMap",
			"java.util.concurrent.locks.ReentrantLock",
			"com.ibm.oti.shared.Shared",
			"java.util.Stack",
			"com.ibm.misc.SystemIntialization",
			"java.lang.reflect.Modifier",
			"com.ibm.misc.SignalInfo",
			"com.ibm.misc.LinuxSignalInfo",
			"java.lang.RuntimeException",
			"java.lang.NullPointerException",
			"java.util.IdentityHashMap",
			"java.lang.ThreadDeath",
			"com.ibm.jvm.TracePermission",
			"com.ibm.tools.attach.javaSE.IPC",
			"sun.net.util.URLUtil",
			"sun.nio.cs.US_ASCII",
			"com.ibm.oti.util.Util",
			"java.util.ArrayDeque",
			"sun.misc.Perf$GetPerfAction",
			"sun.nio.cs.Surrogate$Parser",
			"sun.misc.JarIndex",
			"sun.misc.ExtensionDependency",
			"java.util.Date",
			"sun.util.calendar.ZoneInfo",
			"java.util.zip.CRC32",
			"sun.util.calendar.CalendarUtils",
			"java.util.zip.Inflater",
			"sun.misc.IOUtils",
			"java.util.LinkedList",
			"java.security.Permissions",
			"sun.net.www.MessageHeader",
			"java.security.AllPermission",
			"com.ibm.Compiler.Internal.Quad",
			"com.ibm.CORBA.MinorCodes",
			"javax.rmi.CORBA.ClassDesc",
			"org.omg.CosNaming._NamingContextStub",
			"com.ibm.nws.ejs.ras.Tr",
			"com.ibm.wsspi.channel.framework.exception.ChannelFactoryAlreadyInitializedException",
			"com.ibm.Copyright",
			"com.ibm.security.util.PropertyExpander",
			"com.ibm.security.x509.NetscapeCertTypeExtension",
			"javax.security.sasl.AuthenticationException",
			"com.ibm.security.a",
			"com.ibm.security.krb5.internal.PAEtypeInfo2",
			"com.ibm.Copyright",
			"com.ibm.Copyright",
			"com.ibm.xml.xlxp.scan.msg.ImplementationMessagesBundle_it",
			"com.ibm.xtq.bcel.generic.IAND",
			"com.ibm.xtq.scontext.CollationElements",
			"com.ibm.xtq.xml.types.DocumentType",
			"com.ibm.xtq.xslt.xylem.optimizers.MemoizeAbsolutePathOptimizer",
			"com.ibm.xylem.instructions.AutomatonInstruction$BindingOrLiteral",
			"com.ibm.xylem.instructions.SubstreamBeforeInstruction",
			"com.ibm.xylem.types.ICollectionType$BCELLoopState",
			"org.apache.xalan.res.XSLTErrorResources_ja_JP",
			"org.apache.xerces.impl.Version",
			"org.apache.xml.serialize.HTMLSerializer",
			"sun.nio.cs.ext.ISO2022_CN_GB",
			"sun.nio.cs.ext.JIS_X_0212_MS5022X",
			"sun.misc.Version",
			"com.ibm.net.rdma.jverbs.verbs.SendWorkRequest",
			"com.ibm.security.util.AuthResources_ru",
			"com.oracle.xmlns.internal.webservices.jaxws_databinding.XmlHandlerChain",
			"com.sun.corba.se.impl.orbutil.graph.GraphImpl",
			"com.sun.java.swing.plaf.windows.WindowsMenuItemUI",
			"com.sun.jmx.snmp.SnmpOidDatabaseSupport",
			"com.sun.org.omg.CORBA.ValueMemberSeqHelper",
			"com.sun.swing.internal.plaf.synth.resources.synth_sv",
			"com.sun.xml.internal.messaging.saaj.soap.ver1_1.SOAPPart1_1Impl",
			"com.sun.xml.internal.ws.spi.db.WrapperComposite",
			"com.sun.xml.internal.ws.util.ReadAllStream",
			"java.beans.beancontext.BeanContextServicesSupport",
			"java.lang.ThreadLocal",
			"java.net.ProtocolException",
			"java.nio.channels.NoConnectionPendingException",
			"java.util.Vector",
			"java.util.logging.ConsoleHandler",
			"javax.management.NotificationFilterSupport",
			"javax.management.relation.InvalidRelationIdException",
			"javax.naming.InterruptedNamingException",
			"javax.swing.SpinnerNumberModel",
			"javax.swing.plaf.metal.MetalScrollPaneUI",
			"javax.swing.plaf.synth.SynthTableHeaderUI",
			"org.omg.stub.java.lang._Appendable_Stub",
			"sun.nio.ch.LinuxAsynchronousChannelProvider",
			"sun.print.resources.serviceui_it",
			"sun.tools.jar.resources.jar_it",
			"sun.util.resources.en.CalendarData_en_IE"
};

}
