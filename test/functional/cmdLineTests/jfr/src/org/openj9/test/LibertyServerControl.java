/*
 * Copyright IBM Corp. and others 2026
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
package org.openj9.test;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.ArrayList;
import java.util.List;

/**
 * Cross-platform wrapper for Liberty server commands.
 * Handles platform-specific script execution (server.bat on Windows, server on Unix).
 * Usage: java LibertyServerControl <liberty-home> <command> <server-name>
 * Example: java LibertyServerControl /path/to/wlp create myServer
 */
public class LibertyServerControl {
    private static final boolean IS_WINDOWS = System.getProperty("os.name").startsWith("Windows");

    public static void main(String[] args) {
        if (args.length < 3) {
            System.err.println("Usage: LibertyServerControl <liberty-home> <command> <server-name>");
            System.err.println("Commands: create, start, stop, status, remove");
            System.exit(1);
        }

        String libertyHome = args[0];
        String command = args[1];
        String serverName = args[2];

        try {
            int exitCode;
            if ("remove".equals(command)) {
                exitCode = removeServer(libertyHome, serverName);
            } else {
                exitCode = executeServerCommand(libertyHome, command, serverName);
            }
            System.exit(exitCode);
        } catch (Exception e) {
            System.err.println("Error executing Liberty server command: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }

    private static int removeServer(String libertyHome, String serverName) {
        File serverDir = new File(libertyHome, "usr/servers/" + serverName);

        if (!serverDir.exists()) {
            System.out.println("Server " + serverName + " does not exist");
            return 0;
        }

        try {
            deleteDirectory(serverDir.toPath());
            System.out.println("Server " + serverName + " removed");
            return 0;
        } catch (IOException e) {
            System.err.println("Failed to remove server " + serverName + ": " + e.getMessage());
            return 1;
        }
    }

    private static void deleteDirectory(Path directory) throws IOException {
        Files.walkFileTree(directory, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) throws IOException {
                Files.delete(file);
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult postVisitDirectory(Path dir, IOException exc) throws IOException {
                Files.delete(dir);
                return FileVisitResult.CONTINUE;
            }
        });
    }

    private static int executeServerCommand(String libertyHome, String command, String serverName) throws Exception {
        File binDir = new File(libertyHome, "bin");
        if (!binDir.exists() || !binDir.isDirectory()) {
            throw new IllegalArgumentException("Liberty bin directory not found: " + binDir.getAbsolutePath());
        }

        // Build command based on platform
        List<String> cmdList = new ArrayList<>();

        if (IS_WINDOWS) {
            // Windows: use cmd.exe to execute .bat file
            cmdList.add("cmd.exe");
            cmdList.add("/c");
            File serverBat = new File(binDir, "server.bat");
            cmdList.add(serverBat.getAbsolutePath());
        } else {
            // Unix: execute shell script directly
            File serverScript = new File(binDir, "server");
            cmdList.add(serverScript.getAbsolutePath());
        }

        cmdList.add(command);
        cmdList.add(serverName);

        System.out.println("Executing: " + String.join(" ", cmdList));

        ProcessBuilder pb = new ProcessBuilder(cmdList);
        pb.redirectErrorStream(true);

        Process process = pb.start();

        // Read output in a separate thread to avoid blocking
        Thread outputReader = new Thread(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    System.out.println(line);
                }
            } catch (IOException e) {
                // Ignore - process may have terminated
            }
        });
        outputReader.setDaemon(true);
        outputReader.start();

        // Wait for process to complete with timeout
        int exitCode = process.waitFor();

        // Give output reader a moment to finish
        try {
            outputReader.join(1000);
        } catch (InterruptedException e) {
            // Ignore
        }

        System.out.println("Command completed with exit code: " + exitCode);

        return exitCode;
    }
}

// Made with Bob
