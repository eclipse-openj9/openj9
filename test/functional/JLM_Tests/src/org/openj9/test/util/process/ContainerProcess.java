/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;

/**
 * This class will be invoked and run in new process
 *
 */
public class ContainerProcess {

    private Task task;

    private Socket socket;

    private ObjectInputStream ois;

    private ObjectOutputStream oos;

    protected ContainerProcess(int port) {
        try {
            socket = new Socket();
            socket.setTcpNoDelay(true);

            // So far localhost only
            socket.connect(new InetSocketAddress(InetAddress.getLocalHost(),
                    port), 1000);

            // Handshaking
            oos = new ObjectOutputStream(socket.getOutputStream());
            oos.writeObject(new Packet(Packet.Type.HANDSHAKE));
            ois = new ObjectInputStream(socket.getInputStream());
            Packet response = (Packet) ois.readObject();
            if (response.getType() != Packet.Type.HANDSHAKE) {
                throw new IOException("Bad response: HANDSHAKE !="
                        + response.getType());
            }

            // Read task object
            task = (Task) ois.readObject();
            oos.writeObject(new Packet(Packet.Type.ACK));
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    protected void execute() {
        try {
            if (task != null) {
                task.run();
            }
        } catch (Exception e) {
            // Deal with exceptions from task, other exceptions from
            // within this container process is non of our business.
            try {
                oos.writeObject(new Packet(Packet.Type.EXCEPTION, e));
                Packet response = (Packet) ois.readObject();
                if (response.getType() != Packet.Type.ACK) {
                    throw new IOException("Bad response: ACK != "
                            + response.getType());
                }
            } catch (Exception ex) {
                ex.printStackTrace();
            }
            return;
        }

        try {
            oos.writeObject(new Packet(Packet.Type.GOODBYE));
            Packet response = (Packet) ois.readObject();
            if (response.getType() != Packet.Type.GOODBYE) {
                throw new IOException("Bad response: GOODBYE !="
                        + response.getType());
            }

            socket.close();
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    public static void main(String[] args) throws Exception {
        if (args.length == 1) {
            new ContainerProcess(Integer.parseInt(args[0])).execute();
        }
    }
}
