/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.attacher;

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

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.List;
import java.util.Properties;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import com.sun.tools.attach.VirtualMachine;
import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;
import com.ibm.tools.attach.target.AttachHandler;
import com.ibm.tools.attach.target.AttachmentConnection;
import com.ibm.tools.attach.target.Command;
import com.ibm.tools.attach.target.CommonDirectory;
import com.ibm.tools.attach.target.FileLock;
import com.ibm.tools.attach.target.IPC;
import com.ibm.tools.attach.target.Reply;
import com.ibm.tools.attach.target.Response;
import com.ibm.tools.attach.target.TargetDirectory;
import com.sun.tools.attach.AttachOperationFailedException;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.AgentInitializationException;
import com.sun.tools.attach.AgentLoadException;

import static com.ibm.oti.util.Msg.getString;

/**
 * Handles the initiator end of an attachment to a target VM
 * 
 */
public final class OpenJ9VirtualMachine extends VirtualMachine implements Response {

	/* 
	 * The expected string is "ATTACH_CONNECTED <32 bit hexadecimal key>". 
	 * If the target replies with an error, we may expect a longer string.
	 * Allow enough for ~100 40-character lines.
	 */
	private static final int ATTACH_CONNECTED_MESSAGE_LENGTH_LIMIT = 4000;
	/*[PR Jazz 35291 Remove socket timeout after attachment established]*/
	static final String COM_IBM_TOOLS_ATTACH_TIMEOUT = "com.ibm.tools.attach.timeout"; //$NON-NLS-1$
	static final String COM_IBM_TOOLS_COMMAND_TIMEOUT = "com.ibm.tools.attach.command_timeout"; //$NON-NLS-1$
	/* The units for timeouts are milliseconds, Set to 0 for no timeout. */	
	private static final int DEFAULT_ATTACH_TIMEOUT = 120000;	/* should be ~2* the TCP timeout, i.e. /proc/sys/net/ipv4/tcp_fin_timeout on Linux */
	private static final int DEFAULT_COMMAND_TIMEOUT = 0;

	private static int MAXIMUM_ATTACH_TIMEOUT = Integer.getInteger(COM_IBM_TOOLS_ATTACH_TIMEOUT, DEFAULT_ATTACH_TIMEOUT).intValue();
	private static int COMMAND_TIMEOUT = Integer.getInteger(COM_IBM_TOOLS_COMMAND_TIMEOUT, DEFAULT_COMMAND_TIMEOUT).intValue();
	
	private static final String INSTRUMENT_LIBRARY = "instrument"; //$NON-NLS-1$
	private OutputStream commandStream;
	private final OpenJ9VirtualMachineDescriptor descriptor;
	private final OpenJ9AttachProvider myProvider;
	private Integer portNumber;
	private InputStream responseStream;
	private boolean targetAttached;
	private String targetId;
	private FileLock[] targetLocks;
	private ServerSocket targetServer;
	private Socket targetSocket;

	/**
	 * @param provider
	 *            AttachProvider which created this VirtualMachine
	 * @param id
	 *            identifier for the VM
	 */
	OpenJ9VirtualMachine(AttachProvider provider, String id)
			throws NullPointerException {
		super(provider, id);
		if ((null == id) || (null == provider)) {
			/*[MSG "K0554", "Virtual machine ID or display name is null"]*/
			throw new NullPointerException(getString("K0554")); //$NON-NLS-1$
		}
		this.targetId = id;
		this.myProvider = (OpenJ9AttachProvider) provider;
		this.descriptor = (OpenJ9VirtualMachineDescriptor) myProvider.getDescriptor(id);
	}

	/**
	 * @throws IOException
	 *             if cannot open the communication files
	 * @throws AttachNotSupportedException
	 *             if the descriptor is null the target does not respond.
	 */
	void attachTarget() throws IOException, AttachNotSupportedException {
		if (null == descriptor) {
			/*[MSG "K0531", "target not found"]*/
			throw new AttachNotSupportedException(getString("K0531")); //$NON-NLS-1$
		}
		AttachNotSupportedException lastException = null;
		/*[PR CMVC 182802 ]*/
		int timeout = 500; /* start small in case there is a rogue process which is eating semaphores, grow big in case of system load. */
		while (timeout < MAXIMUM_ATTACH_TIMEOUT) {
			lastException = null;
			try {
				tryAttachTarget(timeout);
			} catch (AttachNotSupportedException e) {
				IPC.logMessage("attachTarget " + targetId + " timeout after " + timeout+" ms"); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
				lastException = e;
				timeout = (timeout * 3)/2;
			}
			if (null == lastException) {
				break;
			}
		}
		if (null != lastException) {
			throw lastException;
		}
	}

	private static String createLoadAgent(String agentName, String options) {
		String optString = (null == options) ? "" : //$NON-NLS-1$
				options;
		String cmdFrame = Command.LOADAGENT + '(' + INSTRUMENT_LIBRARY + ','
				+ agentName + '=' + optString + ')';
		return cmdFrame;
	}

	private static String createLoadAgentLibrary(String agentName, String options,
			boolean agentPath) {
		String cmd = (agentPath ? Command.LOADAGENTPATH
				: Command.LOADAGENTLIBRARY);
		String optString = ""; //$NON-NLS-1$
		if ((null != options) && !(options.equals(""))) { //$NON-NLS-1$
			optString = ',' + options;
		}
		String cmdFrame = cmd + '(' + agentName + optString + ')';
		return cmdFrame;
	}

	/**
	 * Detach a virtual machine by sending a detach command to the connected VM
	 * via the IPC channel.
	 */
	@Override
	public synchronized void detach() throws IOException {
		AttachmentConnection.streamSend(commandStream, Command.DETACH);
		try {
			AttachmentConnection.streamReceiveString(responseStream);
		} finally {
			IPC.logMessage("VirtualMachine.detach"); //$NON-NLS-1$
			if (null != commandStream) {
				commandStream.close();
				commandStream = null;
			}
			if (null != targetSocket) {
				targetSocket.close();
				targetSocket = null;
			}
			if (null != targetServer) {
				targetServer.close();
				targetServer = null;
			}
		}
		targetAttached = false;
	}

	@Override
	public Properties getAgentProperties() throws IOException {
		if (!targetAttached) {
			/*[MSG "K0544", "Target not attached"]*/
			throw new IOException(getString("K0544")); //$NON-NLS-1$
		}
		Properties props = getTargetProperties(false);

		return props;
	}

	@Override
	public Properties getSystemProperties() throws IOException {
		if (!targetAttached) {
			/*[MSG "K0544", "Target not attached"]*/
			throw new IOException(getString("K0544")); //$NON-NLS-1$
		}
		Properties props = getTargetProperties(true);
		return props;
	}

	/**
	 * Synchronized because multiple threads may use this object
	 * @param systemProperties flag to indicate if we want system properties (true) or agent properties (false)
	 * @return Properties object
	 * @throws IOException
	 */
	private synchronized Properties getTargetProperties(boolean systemProperties)
			throws IOException {
		AttachmentConnection.streamSend(commandStream,
				systemProperties ? Command.GET_SYSTEM_PROPERTIES
						: Command.GET_AGENT_PROPERTIES);
		return IPC.receiveProperties(responseStream, true);
	}

	@Override
	public synchronized void loadAgent(String agent, String options)
			throws AgentLoadException, AgentInitializationException,
			IOException {

		if (!targetAttached) {
			/*[MSG "K0544", "Target not attached"]*/
			throw new IOException(getString("K0544")); //$NON-NLS-1$
		}
		AttachmentConnection.streamSend(commandStream, (createLoadAgent(agent, options)));
		String response = AttachmentConnection.streamReceiveString(responseStream);
		parseResponse(response);
	}

	@Override
	public synchronized void loadAgentLibrary(String agentLibrary,
			String options) throws AgentLoadException,
			AgentInitializationException, IOException {

		if (!targetAttached) {
			/*[MSG "K0544", "Target not attached"]*/
			throw new IOException(getString("K0544")); //$NON-NLS-1$
		}
		AttachmentConnection.streamSend(commandStream, createLoadAgentLibrary(
				agentLibrary, options, false));
		String response = AttachmentConnection.streamReceiveString(responseStream);
		parseResponse(response);
	}

	@Override
	public synchronized void loadAgentPath(String agentPath, String options)
			throws AgentLoadException, AgentInitializationException,
			IOException {
		if (null == agentPath) {
			/*[MSG "K0577", "loadAgentPath: null agent path"]*/
			throw new AgentLoadException(getString("K0577")); //$NON-NLS-1$
		}
		if (!targetAttached) {
			/*[MSG "K0544", "Target not attached"]*/
			throw new IOException(getString("K0544")); //$NON-NLS-1$
		}
		AttachmentConnection.streamSend(commandStream, createLoadAgentLibrary(agentPath,
				options, true));
		String response = AttachmentConnection.streamReceiveString(responseStream);
		parseResponse(response);
	}

	/**
	 * Request thread information, including stack traces, from a target VM.
	 * 
	 * @return properties object containing serialized thread information
	 * @throws IOException in case of a communication error
	 */
	public Properties getThreadInfo() throws IOException {
		IPC.logMessage("enter getThreadInfo"); //$NON-NLS-1$
		AttachmentConnection.streamSend(commandStream, Command.GET_THREAD_GROUP_INFO);
		return IPC.receiveProperties(responseStream, true);
	}

	private void lockAllAttachNotificationSyncFiles(
			List<VirtualMachineDescriptor> vmds) {

		int vmdIndex = 0;
		targetLocks = new FileLock[vmds.size()];
		for  (VirtualMachineDescriptor i : vmds) {
			OpenJ9VirtualMachineDescriptor vmd;
			try {
				vmd = (OpenJ9VirtualMachineDescriptor) i;
			} catch (ClassCastException e) {
				continue;
			}
			if (!vmd.id().equalsIgnoreCase(AttachHandler.getVmId())) { /*
																		 * avoid
																		 * overlapping
																		 * locks
																		 */
				String attachSyncFile = vmd.getAttachSyncFileValue();
				if (null != attachSyncFile) { /*
				 * in case of a malformed advert
				 * file
				 */
					IPC.logMessage("lockAllAttachNotificationSyncFiles locking targetLocks[", vmdIndex, "] ", attachSyncFile); //$NON-NLS-1$ //$NON-NLS-2$
					targetLocks[vmdIndex] = new FileLock(attachSyncFile, TargetDirectory.SYNC_FILE_PERMISSIONS);
					try {
						targetLocks[vmdIndex].lockFile(true);
					} catch (IOException e) {
						targetLocks[vmdIndex] = null;
						IPC.logMessage("lockAllAttachNotificationSyncFiles locking targetLocks[", vmdIndex, "] ", "already locked"); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
					}
				}
			} else {
				targetLocks[vmdIndex] = null;
			}
			++vmdIndex;
		}
	}

	private static boolean parseResponse(String response) throws IOException,
			AgentInitializationException, AgentLoadException, IllegalArgumentException
			, AttachOperationFailedException 
	{
		if (response.startsWith(ERROR)) {
			int responseLength = response.indexOf('\0');
			String trimmedResponse;
			if (-1 == responseLength) {
				trimmedResponse = response;
			} else {
				trimmedResponse = response.substring(0, responseLength);
			}
			if (response.contains(EXCEPTION_IOEXCEPTION)) {
				/*[MSG "K0576","IOException from target: {0}"]*/
				throw new IOException(getString("K0576", trimmedResponse)); //$NON-NLS-1$
			} else if (response.contains(EXCEPTION_AGENT_INITIALIZATION_EXCEPTION)) {
				Integer status = getStatusValue(trimmedResponse);
				if (null == status) {
					throw new AgentInitializationException(trimmedResponse);
				} else {
					throw new AgentInitializationException(trimmedResponse, status.intValue());
				}
			} else if (response.contains(EXCEPTION_AGENT_LOAD_EXCEPTION)) {
				throw new AgentLoadException(trimmedResponse);
			} else 	if (response.contains(EXCEPTION_IOEXCEPTION)) {
				/*[MSG "K0576","IOException from target: {0}"]*/
				throw new IOException(getString("K0576", trimmedResponse)); //$NON-NLS-1$
			} else if (response.contains(EXCEPTION_ILLEGAL_ARGUMENT_EXCEPTION)) {
				/*[MSG "K05de","IllegalArgumentException from target: {0}"]*/
				throw new IllegalArgumentException(getString("K05de", trimmedResponse)); //$NON-NLS-1$
			} else if (response.contains(EXCEPTION_ATTACH_OPERATION_FAILED_EXCEPTION)) {
				/*[MSG "k05dc","AttachOperationFailedException from target: {0}"]*/
				throw new AttachOperationFailedException(getString("k05dc", trimmedResponse)); //$NON-NLS-1$
			}
			return false;
		} else	if (response.startsWith(ACK) || response.startsWith(ATTACH_RESULT)) {
			return true;
		} else {
			return false;
		}
	}

	/**
	 * parse the status value from the end of the response string: this will be a numeric string at the end of the string.
	 * @param response
	 * @return Integer value of status, or null if the string does not end in a number
	 */
	private static Integer getStatusValue(String response) {
		Pattern rvPattern = Pattern.compile("(-?\\d+)\\s*$");  //$NON-NLS-1$
		Matcher rvMatcher = rvPattern.matcher(response);
		if (rvMatcher.find()) {
			String status = rvMatcher.group(1);
			try {
				return Integer.getInteger(status);
			} catch (NumberFormatException e) {
				IPC.logMessage("Error parsing response", response); //$NON-NLS-1$
				return null;
			}
		} else {
			return null;
		}
	}


	private void tryAttachTarget(int timeout) throws IOException,
			AttachNotSupportedException {
		Reply replyFile = null;
		AttachHandler.waitForAttachApiInitialization(); /* ignore result: we can still attach to another target if API is disabled */
		IPC.logMessage("VirtualMachineImpl.tryAttachtarget"); //$NON-NLS-1$
		Object myIn = AttachHandler.getMainHandler().getIgnoreNotification();

		synchronized (myIn) {
			int numberOfTargets = 0;
			try {
				CommonDirectory.obtainAttachLock();
				List<VirtualMachineDescriptor> vmds = myProvider.listVirtualMachines();
				if (null == vmds) {
					return;
				}

				targetServer = new ServerSocket(0); /* select a free port */
				portNumber = Integer.valueOf(targetServer.getLocalPort());
				String key = IPC.getRandomString();
				replyFile = new Reply(portNumber, key, TargetDirectory.getTargetDirectoryPath(descriptor.id()), descriptor.getUid());
				try {
					replyFile.writeReply();
				} catch (IOException e) { /*
										 * target shut down while we were trying
										 * to attach
										 */
					/*[MSG "K0457", "Target no longer available"]*/
					AttachNotSupportedException exc = new AttachNotSupportedException(getString("K0457")); //$NON-NLS-1$
					exc.initCause(e);
					throw exc;
				}

				if (descriptor.id().equals(AttachHandler.getVmId())) {
					String allowAttachSelf_Value = AttachHandler.allowAttachSelf;
					boolean selfAttachAllowed = "".equals(allowAttachSelf_Value) || Boolean.parseBoolean(allowAttachSelf_Value); //$NON-NLS-1$
					if (!selfAttachAllowed) {
						/*[MSG "K0646", "Late attach connection to self disabled. Set jdk.attach.allowAttachSelf=true"]*/
						throw new IOException(getString("K0646")); //$NON-NLS-1$
					}
					/* I am connecting to myself: bypass the notification and launch the attachment thread directly */
					if (AttachHandler.isAttachApiInitialized()) {
						AttachHandler.getMainHandler().connectToAttacher();
					} else {
						/*[MSG "K0558", "Attach API initialization failed"]*/
						throw new AttachNotSupportedException(getString("K0558")); //$NON-NLS-1$
					}
				} else {
					lockAllAttachNotificationSyncFiles(vmds);
					numberOfTargets = CommonDirectory.countTargetDirectories();
					int status = CommonDirectory.notifyVm(numberOfTargets, descriptor.isGlobalSemaphore());
					/*[MSG "K0532", "status={0}"]*/
					if ((IPC.JNI_OK != status)
							&& (CommonDirectory.J9PORT_INFO_SHSEM_OPENED_STALE != status)) {
						throw new AttachNotSupportedException(getString("K0532", status)); //$NON-NLS-1$
					}
				}

				try {
					IPC.logMessage("attachTarget " + targetId + " on port " + portNumber); //$NON-NLS-1$ //$NON-NLS-2$
					targetServer.setSoTimeout(timeout);
					targetSocket = targetServer.accept();
				} catch (SocketTimeoutException e) {
					targetServer.close();
					IPC.logMessage("attachTarget SocketTimeoutException on " + portNumber + " to " + targetId); //$NON-NLS-1$ //$NON-NLS-2$
					/*[MSG "K0539","acknowledgement timeout from {0} on port {1}"]*/
					AttachNotSupportedException exc = new AttachNotSupportedException(getString("K0539", targetId, portNumber)); //$NON-NLS-1$
					exc.initCause(e);
					throw exc;
				}
				commandStream = targetSocket.getOutputStream();
				targetSocket.setSoTimeout(COMMAND_TIMEOUT);
				responseStream = targetSocket.getInputStream();
				
				/* 
				 * Limit data until the target is verified. 
				 */
				String response = AttachmentConnection.streamReceiveString(responseStream, ATTACH_CONNECTED_MESSAGE_LENGTH_LIMIT);
				/*[MSG "K0533", "key error: {0}"]*/
				if (!response.contains(' ' + key + ' ')) {
					throw new AttachNotSupportedException(getString("K0533", response)); //$NON-NLS-1$
				}
				IPC.logMessage("attachTarget connected on ", portNumber.toString()); //$NON-NLS-1$
				targetAttached = true;
			} finally {
				if (null != replyFile) {
					replyFile.deleteReply();
				}
				if (numberOfTargets > 0) { /*[PR 48044] if number of targets is 0, then the VM is attaching to itself  and the semaphore was not involved */
					unlockAllAttachNotificationSyncFiles();
					CommonDirectory.cancelNotify(numberOfTargets, descriptor.isGlobalSemaphore());

					if (numberOfTargets > 2) {
						try {
							int delayTime = 100 * ((numberOfTargets > 10) ? 10
									: numberOfTargets);
							IPC.logMessage("attachTarget sleep for ", delayTime); //$NON-NLS-1$
							Thread.sleep(delayTime);
						} catch (InterruptedException e) {
							IPC.logMessage("attachTarget sleep interrupted"); //$NON-NLS-1$
						}
					}
				}
				CommonDirectory.releaseAttachLock();
			}
		}
	}

	private void unlockAllAttachNotificationSyncFiles() {

		if (null != targetLocks) {
			for (int i = 0; i < targetLocks.length; ++i) {
				IPC.logMessage("unlockAllAttachNotificationSyncFiles unlocking targetLocks[", i, "]"); //$NON-NLS-1$ //$NON-NLS-2$
				if (null != targetLocks[i]) {
					targetLocks[i].unlockFile();
				}
			}
		}
	}

	/**
	 * request that the local VM start its JMX agent
	 * @param agentProperties Configuration information for the agent
	 * @throws IOException, IllegalArgumentException, AttachOperationFailedException
	 */
	@Override
	public void startManagementAgent(Properties agentProperties)
			throws IOException, IllegalArgumentException, AttachOperationFailedException {
		if (!targetAttached) {
			/*[MSG "K0544", "Target not attached"]*/
			throw new IOException(getString("K0544")); //$NON-NLS-1$
		} else if (null == agentProperties) {
			throw new NullPointerException();
		}
		AttachmentConnection.streamSend(commandStream, Command.START_MANAGEMENT_AGENT);
		IPC.sendProperties(agentProperties, commandStream);	
		String response = AttachmentConnection.streamReceiveString(responseStream);
		try {
			parseResponse(response);
		} catch (AgentInitializationException|AgentLoadException e) {
			IPC.logMessage("Unexpected exception " + e + " in startManagementAgent");  //$NON-NLS-1$//$NON-NLS-2$
		}
	}

	/**
	 * request that the local VM start its JMX agent
	 * @return Service address of local connector
	 * @throws IOException if error communicating with target
	 */
	@Override
	public String startLocalManagementAgent() throws IOException {
		if (!targetAttached) {
			/*[MSG "K0544", "Target not attached"]*/
			throw new IOException(getString("K0544")); //$NON-NLS-1$
		}
		AttachmentConnection.streamSend(commandStream, Command.START_LOCAL_MANAGEMENT_AGENT);
		String response = AttachmentConnection.streamReceiveString(responseStream);
		String result = "";  //$NON-NLS-1$

		try {
			if (!parseResponse(response)) {
				/*[MSG "k05dd", "unrecognized response: {0}"]*/
				throw new IOException(getString("k05dd", response)); //$NON-NLS-1$
			} else if (response.startsWith(ACK)) { /* this came from a legacy VM with dummy start*Agent()s */
				result = response.substring(ACK.length());
			} else if (response.startsWith(ATTACH_RESULT)) {
				result = response.substring(ATTACH_RESULT.length());
			} else {
				/*[MSG "k05dd", "unrecognized response: {0}"]*/
				throw new IOException(getString("k05dd", response)); //$NON-NLS-1$
			}
		} catch (AgentLoadException | IllegalArgumentException | AgentInitializationException e) {
			IPC.logMessage("Unexpected exception " + e + " in startLocalManagementAgent");  //$NON-NLS-1$//$NON-NLS-2$
		}
		return result;

	}

	/**
	 * 
	 * @note Public API, signature compatible with
	 *       com.sun.tools.attach.spi.AttachProvider.
	 */
	@Override
	public boolean equals(Object comparand) {
	
		if (!(comparand instanceof VirtualMachine)) {
			return false;
		}
	
		VirtualMachine otherVM = (VirtualMachine) comparand;
		return id().equals(otherVM.id());
	}

	/**
	 * 
	 * @note Public API, signature compatible with
	 *       com.sun.tools.attach.spi.AttachProvider.
	 */
	@Override
	public int hashCode() {
		return provider().hashCode() + id().hashCode();
	}

	/**
	 * 
	 * @note Public API, signature compatible with
	 *       com.sun.tools.attach.spi.AttachProvider.
	 */
	@Override
	public String toString() {
		return getClass().getName() + "@" + //$NON-NLS-1$
				hashCode() + ";" + id(); //$NON-NLS-1$
	}
}
