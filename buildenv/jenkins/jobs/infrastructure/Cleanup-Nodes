/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
import hudson.slaves.OfflineCause

defaultSetupLabel = 'worker'
defaultLabel = 'ci.role.build || ci.role.test'
defaultMode = 'cleanup'
defaultTime = '12'
defaultUnits = 'HOURS'
defaultReconnectTimeout = '20' //minutes
defaultSleepTime = '30' //seconds

SETUP_LABEL = params.SETUP_LABEL
if (!SETUP_LABEL) {
    SETUP_LABEL = defaultSetupLabel
}

LABEL = params.LABEL
if (!LABEL) {
    LABEL = defaultLabel
}

// expected MODE: cleanup | sanitize | all
MODES = []
if (!params.MODE) {
    MODES.add(defaultMode)
} else if (params.MODE.equals('all')) {
    MODES.addAll(['cleanup', 'sanitize'])
} else {
    MODES = params.MODE.trim().replaceAll("\\s","").tokenize(',')
}

TIMEOUT_TIME = params.TIMEOUT_TIME
if (!TIMEOUT_TIME) {
    TIMEOUT_TIME = defaultTime
}

TIMEOUT_UNITS = params.TIMEOUT_UNITS
if (!TIMEOUT_UNITS) {
    TIMEOUT_UNITS = defaultUnits
} else {
    TIMEOUT_UNITS = TIMEOUT_UNITS.toUpperCase()
}

SLACK_CHANNEL = params.SLACK_CHANNEL

RECONNECT_TIMEOUT = params.RECONNECT_TIMEOUT
if (!RECONNECT_TIMEOUT) {
    RECONNECT_TIMEOUT = defaultReconnectTimeout
}

SLEEP_TIME = params.SLEEP_TIME
if (!SLEEP_TIME) {
    SLEEP_TIME = defaultSleepTime
}

PARAMETERS = [string(name: 'SETUP_LABEL', defaultValue: defaultSetupLabel),
              string(name: 'LABEL', defaultValue: defaultLabel),
              choice(name: 'MODE', choices: ['cleanup', 'sanitize'], defaultValue: defaultMode),
              string(name: 'TIMEOUT_TIME', defaultValue: defaultTime),
              string(name: 'TIMEOUT_UNITS', defaultValue: defaultUnits),
              string(name: 'RECONNECT_TIMEOUT', defaultValue: defaultReconnectTimeout),
              string(name: 'SLEEP_TIME', defaultValue: defaultSleepTime),
              string(name: 'SLACK_CHANNEL', defaultValue: '')]
/*
properties([buildDiscarder(logRotator(artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '', numToKeepStr: '5')),
            pipelineTriggers([cron('''# Daily at 8am, 12pm, 4pm, 11pm (before nightly build)
                                        0 8,12,16,23 * * *''')]),
            parameters(PARAMETERS)])
*/

jobs = [:]
offlineSlaves = [:]
buildNodes = []

timeout(time: TIMEOUT_TIME.toInteger(), unit: TIMEOUT_UNITS) {
    timestamps {
        node(SETUP_LABEL) {
            try {
                def cleanDirs = ['NoEntryTest*.zip',
                                 'auth*.login',
                                 'tmp*',
                                 'classes*',
                                 'aci*',
                                 'append*',
                                 'mlib*',
                                 'resource-*',
                                 'openj9tr_resources*',
                                 'testParentDir',
                                 'jni-*',
                                 'mauve',
                                 'test*',
                                 'blah-*.tmp',
                                 'lines*.tmp',
                                 'prefix*.json',
                                 'sink*.tmp',
                                 'source*.tmp',
                                 'target*.tmp',
                                 'sharedcacheapi',
                                 'intermediateClassCreateTest',
                                 'sh-np.*']

                for (aNode in jenkins.model.Jenkins.instance.getLabel(LABEL).getNodes()) {
                    def nodeName = aNode.getDisplayName()

                    if (aNode.toComputer().isOffline()) {
                        // skip offline slave
                        def offlineCause = aNode.toComputer().getOfflineCause()
                        if (offlineCause instanceof OfflineCause.UserCause) {
                            // skip offline node disconnected by users
                            offlineSlaves.put(nodeName, offlineCause.toString())
                        } else {
                            // cache nodes, will attempt to reconnect nodes disconnected by system later
                            buildNodes.add(nodeName)
                        }
                        continue
                    }

                    def nodeLabels = []
                    if (aNode.getLabelString()) {
                        nodeLabels.addAll(aNode.getLabelString().tokenize(' '))
                    }

                    buildNodes.add(nodeName)

                    // cache job
                    jobs["${nodeName}"] = {
                        node("${nodeName}") {
                            if (MODES.contains('cleanup')) {
                                stage("${nodeName} - Cleanup Workspaces") {
                                    def buildWorkspace = "${env.WORKSPACE}"
                                    if (nodeLabels.contains('sw.os.windows')) {
                                        // convert windows path to unix path
                                        buildWorkspace = sh(script: "cygpath -u '${env.WORKSPACE}'", returnStdout: true).trim()
                                    }

                                    def cleanDirsStr = "/tmp/${cleanDirs.join(' /tmp/')}"
                                    if (nodeLabels.contains('sw.os.windows')) {
                                        // test resources
                                        cleanDirsStr += " ${buildWorkspace}/../../"
                                        cleanDirsStr += cleanDirs.join(" ${buildWorkspace}/../../")
                                        // shared classes cache
                                        cleanDirsStr += " ${buildWorkspace}/../../javasharedresources /tmp/javasharedresources /temp/javasharedresources"
                                    }

                                    // cleanup test results
                                    sh "rm -fr ${cleanDirsStr}"

                                    // Cleanup OSX shared memory
                                    if (nodeLabels.contains('sw.os.osx')) {
                                        retry(2) {
                                            sh """
                                                ipcs -ma
                                                ipcs -ma | awk '/^m / { if (\$9 == 0) { print \$2 }}' | xargs -n 1 ipcrm -m
                                                ipcs -ma
                                            """
                                        }
                                    }

                                    // Clean up defunct pipelines workspaces
                                    def retStatus = 0
                                    def cleanWSDirs = get_other_workspaces("${buildWorkspace}/../")

                                    if (cleanWSDirs) {
                                        def cleanWSDirsStr = "${buildWorkspace}/../"
                                        cleanWSDirsStr += cleanWSDirs.join(" ${buildWorkspace}/../")

                                        retry(3) {
                                            if (retStatus != 0) {
                                                sleep(time: SLEEP_TIME.toInteger(), unit: 'SECONDS')
                                            }

                                            retStatus = sh script: "rm -rf ${cleanWSDirsStr}", returnStatus: true
                                        }

                                        if (retStatus != 0) {
                                            throw new Exception("Could not delete old builds workspaces on ${nodeName}!")
                                        }
                                    }
                                }
                            }

                            if (MODES.contains('sanitize')) {
                                stage("${nodeName} - Sanitize slave") {
                                    sanitize_node(nodeName)
                                }
                            }
                        }
                    }
                }

                if (offlineSlaves) {
                    println("Offline slaves: ${offlineSlaves.toString()}")
                }

            } catch(e) {
                if (SLACK_CHANNEL) {
                    slackSend channel: SLACK_CHANNEL, color: 'danger', message: "Failed: ${env.JOB_NAME} #${env.BUILD_NUMBER} (<${env.BUILD_URL}|Open>)"
                }
                throw e
            } finally {
                cleanWs()
            }
        }

        try {
            parallel jobs
        } finally {
            if (MODES.contains('sanitize')) {
                def offlineNodes = []
                for (label in buildNodes.sort()) {
                    if (jenkins.model.Jenkins.instance.getNode(label)) {
                        def aComputer = jenkins.model.Jenkins.instance.getNode(label).toComputer()

                        if (aComputer.isOffline() && !(aComputer.getOfflineCause() instanceof OfflineCause.UserCause)) {
                            // reconnect slave (asynchronously)
                            println("${label}: Reconnecting...")
                            aComputer.connect(true)

                            if (aComputer.isOffline()) {
                                echo "Node: ${JENKINS_URL}${aComputer.getUrl()} - Status: offline - Cause: ${aComputer.getOfflineCause().toString()}"
                                offlineNodes.add("<${JENKINS_URL}${aComputer.getUrl()}|${aComputer.getDisplayName()}>")
                            } else {
                                println("${label} is back online: ${aComputer.isOnline()}")
                            }
                        }
                    }
                }

                if (!offlineNodes.isEmpty() && SLACK_CHANNEL) {
                    slackSend channel: SLACK_CHANNEL, color: 'warning', message: "${env.JOB_NAME} #${env.BUILD_NUMBER} (<${env.BUILD_URL}|Open>) left nodes offline: ${offlineNodes.join(',')}"
                }
            }
        }
    }
}

/*
* Return a list of workspace directories (current build workspace excluded)
*/
def get_other_workspaces(workspaceDir) {
    // fetch all directories in workspaceDir (this should not fail)
    def workspaces = sh(script: "ls ${workspaceDir}", returnStdout: true).trim().tokenize(System.lineSeparator())
    // remove current build workspace
    def otherWS = workspaces.findAll { ws -> ws.startsWith(JOB_NAME) == false }

    return otherWS
}

/*
* Kill all processes and reconnect a Jenkins node
*/
def sanitize_node(nodeName) {
    def workingNode = jenkins.model.Jenkins.instance.getNode(nodeName)
    def workingComputer = workingNode.toComputer()

    workingComputer.setTemporarilyOffline(true, null)
    try {
        def cmd = ''

        if (workingNode.getLabelString().indexOf("sw.os.windows") != -1) {
            println("\t ${nodeName}: Rebooting...")
            // NB: user requires shut down permissions (SeShutdownPrivilege) or
            // belongs to the Administrators group
            cmd = "cmd.exe /K shutdown /f /r"
        } else {
            println("\t ${nodeName}: Killing all owned processes...")

            cmd = "kill -9 -1"
            if (workingNode.getLabelString().indexOf("sw.os.zos") != -1) {
                cmd = "ps -f -u ${env.USER} | awk '{print \$2}' | xargs kill -s KILL"
            }
        }

        // execute command
        sh "${cmd}"

    } catch(e) {
        println(e.getMessage())
    }

    //reconnect slave
    timeout(time: RECONNECT_TIMEOUT.toInteger(), unit: 'MINUTES') {
        println("\t ${nodeName}: Disconnecting...")
        workingComputer.disconnect(null)
        workingComputer.waitUntilOffline()

        println("\t ${nodeName}: Connecting...")
        workingComputer.connect(false)
        workingComputer.setTemporarilyOffline(false, null)
        workingComputer.waitUntilOnline()
        println("\t ${nodeName} is back online: ${workingComputer.isOnline()}")
    }
}
