/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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
 *******************************************************************************/

// Remove all bootjdks from build machines.
// Trigger initially setup for every 6 months.
// Fire downstream job "all-nodes-matching-labels" with various params in parallel.

 def defaultLocation = '$HOME/bootjdks'
 def slackChannel = params.SLACK_CHANNEL ?: '#jenkins-sandbox'

// Do not run on z/OS.
def GROUPS = [
    [name:"AIX", labels:"ci.role.build&&sw.os.aix", location:"/opt/bootjdks"],
    [name:"alinux", labels:"ci.role.build&&sw.os.linux&&hw.arch.aarch64", location:defaultLocation],
    [name:"plinux", labels:"ci.role.build&&sw.os.linux&&hw.arch.ppc64le", location:defaultLocation],
    [name:"xlinux", labels:"ci.role.build&&sw.os.linux&&hw.arch.x86", location:defaultLocation],
    [name:"zlinux", labels:"ci.role.build&&sw.os.linux&&hw.arch.s390x", location:defaultLocation],
    [name:"Mac", labels:"ci.role.build&&sw.os.mac", location:defaultLocation],
    [name:"Windows", labels:"ci.role.build&&sw.os.windows", location:defaultLocation]
]

branches = [:]
for (i in GROUPS) {
    // def local variable for each loop to avoid all iterations using the last value of 'i'.
    def group = i
    branches["${group.name}"] = {
        build job: 'all-nodes-matching-labels',
            parameters: [
                string(name: 'LABEL', value: group.labels),
                string(name: 'COMMAND', value: "rm -rf ${group.location}/*"),
                string(name: 'TIMEOUT_TIME', value: '12'),
                booleanParam(name: 'PARALLEL', value: true),
                booleanParam(name: 'CLEANUP', value: false)
            ]
    }
}
try {
    parallel branches
} catch(e) {
    slackSend channel: slackChannel, color: 'danger', message: "Failed: ${env.JOB_NAME} #${env.BUILD_NUMBER} (<${env.BUILD_URL}|Open>)"
    throw e
}
