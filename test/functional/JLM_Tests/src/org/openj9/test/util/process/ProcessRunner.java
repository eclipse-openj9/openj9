/*******************************************************************************
 * Copyright (c) 2005, 2019 IBM Corp. and others
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

package org.openj9.test.util.process;

import java.io.File;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

/**
 * Simple Process Runner Protocol:
 *
 * <b>Testing Process                        New container process</b>
 *                   <-----------------------HANDSHAKE
 *          HANDSHAKE----------------------->
 *               TASK----------------------->
 *                   <-----------------------ACK
 *                   <-----------------------EXCEPTION                    [1]
 *                ACK----------------------->                             [1]
 *                   <-----------------------GOODBYE                      [2]
 *            GOODBYE----------------------->                             [2]
 *
 * [1] and [2] are conflict, and should not happen together.
 *
 * To use it in JUnit test case you may need to make a static task class instead
 * of a convenient inner task class, which may cause NotSerializableException
 * Since most of Harmony unit test classes are not Serializable.
 *
 * Typical usage is like:
 * // Define the task
 * static class MyTask implements tests.util.process.Task {
 *     public void run() throws Exceptions {
 *         ... ...
 *     }
 * }
 * ... ...
 *
 * // Run the task
 * try {
 *     new ProcessRunner(new MyTask()).run();
 * } catch (Exception e) {
 *     ... ...
 * }
 *
 *
 */
public class ProcessRunner {

    private Task task;

    private Process containerProcess;

    private Socket socket;

    private ObjectInputStream ois;

    private ObjectOutputStream oos;

    private String[] moreParams;

    /**
     * Spanning new process and establish socket communication.
     * <b>Notes :</b> running this kind of test needs proper network
     * permission from both OS and JRE, so it will not be wise to use
     * -Djava.security.manager at the beginning.
     *
     * @param task
     */
    public ProcessRunner(Task task) {
        this.task = task;
    }

    public ProcessRunner(Task task, String[] additionalParams) {
        this.task = task;
        this.moreParams = additionalParams;
    }

    protected void initializeCommunication() {
        ServerSocket serverSock = null;
        try {
            serverSock = new ServerSocket(0);
        } catch (IOException e) {
            e.printStackTrace();
        }

        if (serverSock != null) {
            // Create new Java process by copying the exact executing
            // environment of the caller process,
            File file = new File(".");
            try {
            System.out.println(file.getCanonicalPath());
            } catch (Exception ex) {

            }

            List<String> cmds = new ArrayList<String>();
            cmds.add(System.getProperty("java.home") + "/bin/java");
            cmds.add("-classpath");
            cmds.add(System.getProperty("java.class.path"));
            cmds.add("-Xbootclasspath:" + System.getProperty("sun.boot.class.path"));
            if (moreParams != null) {
                for (String param : moreParams) {
                    cmds.add(param);
                }
            }
            cmds.add(ContainerProcess.class.getCanonicalName());
            cmds.add(serverSock.getLocalPort() + "");
            ProcessBuilder pb = new ProcessBuilder(cmds);

            try {
                // Start the container process
                containerProcess = pb.start();
            } catch (Exception e) {
                e.printStackTrace();
            }

            try {
                socket = serverSock.accept();
                socket.setTcpNoDelay(true);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * Run a task in a new Java process, block until the new process
     * ends. So the caller method may process the result or exceptions
     * of new process
     *
     * @param task
     *            Task to be run in the new process
     * @throws IOException
     */
    public void run() throws Exception {
        initializeCommunication();

        try {
            waitForProcess();
        } catch (Exception e) {
            throw e;
        }
    }

    private void waitForProcess() throws Exception {
        Exception exception = null; // Exception thrown from container

        ois = new ObjectInputStream(socket.getInputStream());
        Packet incoming = (Packet) ois.readObject();
        if (incoming.getType() != Packet.Type.HANDSHAKE) {
            throw new IOException("Bad HANDSHAKE packet from container");
        }

        oos = new ObjectOutputStream(socket.getOutputStream());
        oos.writeObject(new Packet(Packet.Type.HANDSHAKE));

        if (task == null) {
            throw new NullPointerException("Runnable task should not be null!");
        }
        oos.writeObject(task);
        incoming = (Packet) ois.readObject();
        if (incoming.getType() != Packet.Type.ACK) {
            throw new IOException("BAD ACK packet from container");
        }

        // Wait for the child exception if any, or just say goodbye
        incoming = (Packet) ois.readObject();
        if (incoming.getType() == Packet.Type.EXCEPTION) {
            exception = (Exception) incoming.getObject();
            oos.writeObject(new Packet(Packet.Type.ACK));
        } else if (incoming.getType() == Packet.Type.GOODBYE) {
            oos.writeObject(new Packet(Packet.Type.GOODBYE));
        }

        // If exception occurs during child process, throw it.
        if (exception != null) {
            throw exception;
        }
    }
}
