/*[INCLUDE-IF Sidecar16]*/
/*******************************************************************************
 * Copyright (c) 2009, 2010 IBM Corp. and others
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

/**
 * <h1>Implementation of the Attach API for the J9 Java VM.</h1>
 * 
 * <h2>Execution and Protocols</h2>
 * <p>The key aspects of this interface are:</p>
 * <ul>
 * 	<li>Discovery.  The API provides a method for an attaching process, the "attacher", to discover potential targets, i.e. J9 processes executing on a host.</li>
 * 	<li>Notification. The API provides to initiate the attachment process from the attacher (e.g. JConsole) to the target (e.g. and application program).  This implements the com.sun.tools.attach.VirtualMachine.attach() method.</li>
 * 	<li>Communication. This is a simple mechanism for communicating status and control information. This is typically used to command the loading of libraries and getting information about the target.</li>
 * </ul>
 * 
 * 
 * <p>The API is implemented principally in Java as part of the boot classes, with help from port library natives.</p>
 * 
 * <h3>Discovery</h3>
 * <ol>
 * 	<li>At startup, the target creates an "AttachHandler" thread at boot time during the JCL initialization.<br/>
 * The AttachHandler thread creates an advertisement directory in a well-known location in the file system, by default 
 * /tmp/.com.ibm.tools.attach.targets/&lt;VM ID&gt; (on Microsoft Windows, substitute C:\temp or C:\Documents and Settings\/&lt;user ID&gt;\Local Settings\Tempfor /tmp). The location of the directory is configurable by a command-line argument "com.ibm.tools.attach.directory".</li>
 * 	<li>AttachHandler opens a semaphore shared among all VMs.</li>
 * 	<li>AttachHandler creates an advertisement file and a reply file in the  in the advertisement directory. The advertisement file is a Java properties file containing:
 * 	<ul>
 * 		<li>Virtual Machine ID.  This is based on the processor ID by default but can be set by the "com.ibm.tools.attach.id" system property</li>
 * 		<li>Display name.  By default this is the same as the ID, but can be set using the "com.ibm.tools.attach.displayName" property.</li>
 * 		<li>Shared semaphore identifier  (see above)</li>
 * 	</ul>
 * 	</li>
 * 	<li>AttachHandler waits on the semaphore</li>
 * </ol>
 * <p>Creation and updating of the advertisement directory occurs under the protection of a common master lock file.</p>
 * 
 * <h3>Notification</h3>
 * <p>The API uses the VM's port library shared semaphore mechanism.  </p>
 * <ol>
 * 	<li>The attacher scans the advertisement   directory to find the process IDs of candidate processes. This information is provided by the com.sun.tools.attach.spi.AttachProvider.listVirtualMachines() method.</li>
 * 	<li>When an attacher wishes to notify the target it:
 * 	<ol>
 * 		<li>opens a server socket on a port</li>
 * 		<li>writes the port number to the  reply file</li>
 * 		<li>posts to the shared semaphore.  See below for the mechanics of the notifications.</li>
 * 		<li>reads from the socket</li>
 * 	</ol>
 * 	</li>
 * 	<li>The target's AttachHandler wakes up and sees if it has a reply file in its advertisement directory. If so, it creates an "attachment " thread</li>
 * 
 * 	<li>The target waits until the all other VMs have been woken up (see "Notification protocol" below) and waits again on the semaphore.</li>
 * 	<li>The attachment thread reads the socket number from the reply file</li>
 * 	<li>The attachment thread opens the socket and writes an acknowledgment to the socket</li>
 * 	<li>The attacher's socket read completes.  The attach process is now complete.</li>
 * 	<li>The attacher deletes the reply file.</li>
 * </ol>
 * 
 * <h3>Communication</h3>
 * <ol>
 * 	<li>The attacher sends commands such as loadAgent() by writing text strings to the socket.</li>
 * 	<li>The target's attachment reads these commands, parses them,  and performs the required action.</li>
 * 	<li>The attacher detaches by writing a detach command to the socket and closing the socket.</li>
 * 	<li>The attachment thread in the target reads the detach command, closes the socket, and terminates.
 * </ol>
 * <h3>Shutdown</h3>
 * 
 * <p>On shutdown, the attachHandler:</p>
 * <ol>
 * </li>
 * 	<li>deletes its advertisement directory</li>
 * 	<li>destroys the semaphore if there are no other active targets.</li>
 * 	<li>posts to the semaphore to wake up the attach handler thread if other targets are using the semaphore.</li>
 * 	<li>terminates. Note that the attacher and target may still be communicating by other means, e.g. JMXRemote, when the attachment terminates.</li>
 * </ol>
 * <p>The target and attacher can have multiple concurrent attachments in progress.</p>
 * <h2>Security</h2>
 * <p>The advertisement directory and contained files have Unix owner read permissions and owner write permissions. 
 * On Windows, the directory and files are read-only permissions except when the attacher is writing the port number. <br/>
 * The attacher writes a "key" pass string to the reply file.  The target must echo this string when acknowledging the attachment.</p>
 * <h2>File system artifacts</h2>
 * <p>Attach API uses semaphore, files, and file locks to advertise, synchronize and communicate amongst VMs. 
 * File system permissions to provide security.
 * <h3>Common files</h3>
 * The common directory and its files has user, group, and world full permissions, but the sticky bit prevents interference between userids.</p>
 * <p>
 * A common directory, usually /tmp/.com_ibm_tools_attach on non-Windows systems, holds common control files
 * for the shared semaphore used for interprocess notification, and lock files for mutual exclusion
 * master lock and notifier files:</p>
 * <table border="1">
 * <tr> <th>Filename</th><th>Java class</th><th>Purpose</th> </tr>
 * <tr> <td>_master</td><td>FileLock</td><td><ul>
 *	<li> provides an inter-JVM mutex</li>
 * 	<li> used to protect per-directory creation/removal</li>
 * </ul></td>
 * </tr>
 * <tr> <td>_notifier<td>(none)<td>Provides inter-process wait/notify semantics </td> </tr>
 * <tr> <td>_attachlock<td>FileLockmanager<td>Provides inter-process wait/notify semantics</td> </tr>
 * </table>	
 * <h3>Per-target files</h3>
 * <p>As JVM instances start up they create their own "target" directory using the virtual machine ID, usually the process id (PID)
 * as the directory name under protection of the masterLock. 
 * The target has full owner permissions and execute permissions for group and world. </p>
 * <p>Inside the target's directory are the following files:</p>
 * <table border="1">
 * <tr> <th>Filename</th><th>Java class</th><th>Purpose</th> </tr>
 * <tr><td>attachInfo</td><td>Advertisement</td>
 * <td>
 * 	contains attach version, user, pid, vm-id, display name, etc.  This has full permissions for the owner and no permissions for group or world.
 *	written by the target, read by attacher.
 * </td>
 * </tr>
 *<tr><td>attachNotificationSync</td><td>FileLock</td><td> empty file for synchronization.  Used to prevent a target from waking more than once in a notification cycle.</td></tr>
 *<tr><td>reply</td><td>Reply</td><td>contains port number and a security key for one attach session. written by the attacher, read by target.</td></tr>
 * </table>
 *<h2>Options</h2>
 *<p>The attach API has a number of runtime options which can be set via system properties:</p>
 *<table border="1">
 *<tr>
 *<th>Property</th><th>Description</th>
 *</tr>
 *<tr> <td>com.ibm.tools.attach.id</td><td>Specify an ID for the target.  Value must start with an alphabetic character and be followed by alphanumerics.</td></tr>
 *<tr><td>com.ibm.tools.attach.displayName</td><td>Specify a name for the target.</td></tr>
 *<tr><td>com.ibm.tools.attach.enable</td><td>Enable the ability for other VMs to attach.  Valid values are "yes" and "no".</td></tr>
 *<tr><td>com.ibm.tools.attach.logging</td><td>Turn on tracing of attach API events. Valid values are "yes" and "no".</td></tr>
 *<tr><td>com.ibm.tools.attach.log.name</td><td>Override the default path for the log files generated by the "logging" option.</td></tr>
 *<tr><td>com.ibm.tools.attach.timeout</td><td>Specify the timeout in milliseconds for communication between attacher and attachment.  Set to 0 for no timeout.</td></tr>
 *</table>
 */
package com.ibm.tools.attach.target;
