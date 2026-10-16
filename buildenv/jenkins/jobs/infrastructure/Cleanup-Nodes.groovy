/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
 * Assisted-by: IBM Bob
 *******************************************************************************/
// #10071 This is a Jenkins class
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

// Windows only: recover idle nodes that the test pipeline took offline because workspace
// cleanup failed or the disk was full (see RECOVERABLE_CAUSES), instead of skipping them.
RECOVER_OFFLINE_NODES = params.RECOVER_OFFLINE_NODES ? params.RECOVER_OFFLINE_NODES.toString().toBoolean() : false
MIN_FREE_GB = (params.MIN_FREE_GB ?: '10').toString().toInteger()

// Substrings of the offline cause set by checkErrors() in aqa-tests openjdk_tests.
RECOVERABLE_CAUSES = ['ERROR: Cannot delete workspace',
                      'There is not enough space in the file system',
                      'No space left on device']
// Added to the offline cause when recovery fails, so the node is not retried until a human clears it.
FAILED_MARKER = '[auto-recovery failed, manual intervention required]'
// Labels a node carries while it is being recovered, so queued test jobs cannot be scheduled on it.
// sw.os.windows must stay: sanitize_node() uses it to choose reboot over kill -9.
RECOVERING_LABELS = 'sw.os.windows ci.recovering'

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
              booleanParam(name: 'RECOVER_OFFLINE_NODES', defaultValue: false),
              string(name: 'MIN_FREE_GB', defaultValue: '10'),
              string(name: 'SLACK_CHANNEL', defaultValue: '')]
/*
properties([buildDiscarder(logRotator(artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '', numToKeepStr: '5')),
            pipelineTriggers([cron('''# Daily at 8am, 12pm, 4pm, 11pm (before nightly build)
                                        0 8,12,16,23 * * *''')]),
            parameters(PARAMETERS)])
 */

jobs = [:]
offlineNodes = [:]
buildNodes = []
recoveryResults = [:] // nodeName -> 'recovered' | 'failed', used for the report only

timeout(time: TIMEOUT_TIME.toInteger(), unit: TIMEOUT_UNITS) {
    timestamps {
        node(SETUP_LABEL) {
            try {
                def cleanDirs = [
                    '*.dat',
                    '*.policy',
                    '*props',
                    'aci*',
                    'append*',
                    'auth*.login',
                    'blah*.tmp',
                    'buffer*txt',
                    'classes*',
                    'Compression*',
                    'exp*tmp',
                    'foo*.tmp',
                    'foo*bar',
                    'imageio*.tmp',
                    'intermediateClassCreateTest',
                    'jffi*.dll',
                    'jni-*',
                    'key*.ser',
                    'libzstd*.dll',
                    'lines*.tmp',
                    'mauve',
                    'mlib*',
                    'mockittoboot*.jar',
                    'name*',
                    'NoEntryTest*.zip',
                    'openj9tr_resources*',
                    'packsrv*.jar',
                    'prefix*.json',
                    'readString*.tmp',
                    'remote*',
                    'resource-*',
                    'rmid*.tmp',
                    'sh-np*',
                    'sharedcacheapi',
                    'sink*.tmp',
                    'Snap*.trc',
                    'socket_*',
                    'source*.tmp',
                    'src*.dat',
                    'surefire*',
                    'target*.tmp',
                    'temp*',
                    'test*',
                    'testParentDir',
                    'tmp*',
                    'TransferToTrusted*',
                    'unpacksrv*.jar',
                    'veryverylongprefix*',
                    'xfer.fch*',
                    'xlc*'
                ]

                for (aNode in jenkins.model.Jenkins.instance.getLabel(LABEL).getNodes()) {
                    def nodeName = aNode.getDisplayName()
                    def computer = aNode.toComputer()

                    // recovering: this offline node is brought back instead of skipped (see is_recoverable).
                    def recovering = false
                    // originalCause: why it was offline, kept so it can be restored if recovery fails.
                    def originalCause = null
                    if (computer.isOffline()) {
                        def offlineCause = computer.getOfflineCause()
                        if (is_recoverable(aNode, computer, offlineCause)) {
                            recovering = true
                            originalCause = offlineCause.toString()
                        } else {
                            // skip offline nodes
                            if (offlineCause instanceof OfflineCause.UserCause) {
                                // skip offline node disconnected by users
                                offlineNodes.put(nodeName, offlineCause.toString())
                            } else {
                                // cache nodes, will attempt to reconnect nodes disconnected by system later
                                buildNodes.add(nodeName)
                            }
                            continue
                        }
                    }

                    def nodeLabels = []
                    if (aNode.getLabelString()) {
                        nodeLabels.addAll(aNode.getLabelString().tokenize(' '))
                    }

                    buildNodes.add(nodeName)

                    // cache job
                    def nodeJob = {
                        node("${nodeName}") {
                            if (recovering || MODES.contains('cleanup')) {
                                stage("${nodeName} - Cleanup Workspaces") {
                                    def buildWorkspace = "${env.WORKSPACE}"
                                    def cleanPaths = cleanDirs.collect { "/tmp/${it}" }
                                    if (nodeLabels.contains('sw.os.windows')) {
                                        // Convert Windows path to unix path.
                                        buildWorkspace = sh(script: "cygpath -u '${env.WORKSPACE}'", returnStdout: true).trim()
                                        // Resolve TEMP directory and convert to Unix path.
                                        def tempDir = "${env.TEMP}" ?: "/cygdrive/c/temp"
                                        tempDir = sh(script: "cygpath -u '${tempDir}'", returnStdout: true).trim()
                                        def baseDir = sh(script: "cd '${buildWorkspace}/../..' && pwd", returnStdout: true).trim()
                                        // test resources
                                        cleanPaths += cleanDirs.collect { "${baseDir}/${it}" }
                                        // shared classes cache
                                        cleanPaths += ["${baseDir}/javasharedresources", "/tmp/javasharedresources", "/temp/javasharedresources"]
                                        // Clean up system TEMP directory.
                                        cleanPaths += cleanDirs.collect { "${tempDir}/${it}" }
                                    }
                                    def cleanDirsStr = cleanPaths.join(' ')
                                    // Clean up test results.
                                    sh "rm -fr ${cleanDirsStr}"

                                    // Cleanup OSX shared memory and content in /cores
                                    if (nodeLabels.contains('sw.os.mac')) {
                                        retry(2) {
                                            sh """
                                                ipcs -ma
                                                ipcs -ma | awk '/^m / { if (\$9 == 0) { print \$2 }}' | xargs -n 1 ipcrm -m
                                                ipcs -ma
                                                du -sh /cores
                                                rm -rf /cores/*
                                                du -sh /cores
                                            """
                                        }
                                    }

                                    // Cleanup zOS datasets
                                    if (nodeLabels.contains('sw.os.zos')) {
                                        def listcat = sh(script: "tso listcat | grep '${env.USER}' | grep 'JVM' | cut -d. -f 2-", returnStdout: true).trim()
                                        if (!listcat.isEmpty()) {
                                            listcat.split('\n').each {
                                                sh "tso delete ${it}"
                                            }
                                        }
                                    }

                                    // Clean up defunct pipeline workspaces.
                                    def cleanWSDirs = get_other_workspaces("${buildWorkspace}/../").collect { "${buildWorkspace}/../${it}" }

                                    if (cleanWSDirs) {
                                        def cleanWSDirsStr = cleanWSDirs.join(' ')
                                        def retStatus = 0

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

                            if (recovering) {
                                stage("${nodeName} - Verify disk space") {
                                    def freeKb = sh(script: "df -Pk . | awk 'NR==2 { print \$4 }'", returnStdout: true).trim().toLong()
                                    def freeGb = freeKb.intdiv(1024 * 1024)
                                    println("${nodeName}: ${freeGb} GB free")
                                    if (freeGb < MIN_FREE_GB) {
                                        error("${nodeName}: only ${freeGb} GB free, need ${MIN_FREE_GB} GB")
                                    }
                                }
                            } else if (MODES.contains('sanitize')) {
                                stage("${nodeName} - Sanitize node") {
                                    sanitize_node(nodeName)
                                }
                            }
                        }
                    }
                    jobs["${nodeName}"] = recovering ? { recover_node(nodeName, computer, originalCause, nodeJob) } : nodeJob
                }

                if (offlineNodes) {
                    println("Offline nodes: ${offlineNodes.toString()}")
                }

            } catch (e) {
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
            report_recovery()
            if (MODES.contains('sanitize')) {
                def offlineNodes = []
                for (label in buildNodes.sort()) {
                    if (jenkins.model.Jenkins.instance.getNode(label)) {
                        def aComputer = jenkins.model.Jenkins.instance.getNode(label).toComputer()

                        if (aComputer.isOffline() && !(aComputer.getOfflineCause() instanceof OfflineCause.UserCause)) {
                            // reconnect node (asynchronously)
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
 * True if an offline node should be recovered instead of skipped. Windows only: the node must have
 * been taken offline by checkErrors() in aqa-tests openjdk_tests because workspace cleanup failed or
 * the disk was full, must not have failed recovery before, and must be idle.
 */
def is_recoverable(aNode, computer, offlineCause) {
    if (!RECOVER_OFFLINE_NODES || !(offlineCause instanceof OfflineCause.UserCause)) {
        return false
    }
    if (!aNode.getLabelString() || !aNode.getLabelString().tokenize(' ').contains('sw.os.windows')) {
        return false
    }
    def cause = offlineCause.toString()
    if (cause.contains(FAILED_MARKER) || !RECOVERABLE_CAUSES.any { cause.contains(it) }) {
        return false
    }
    return computer.countBusy() == 0
}

/*
 * Recover one offline Windows node: reboot it, then run its cleanup job (nodeJob), which also checks
 * free disk space. While this runs, the node's labels are replaced by RECOVERING_LABELS, so queued
 * test jobs cannot be scheduled on it before it is clean. The original labels are restored when
 * recovery ends, whether it succeeded or not. If anything fails the node is put straight back
 * offline with its original cause plus FAILED_MARKER, so it takes no work and is not retried until
 * a human clears it. A failed recovery marks the build UNSTABLE instead of failing it, so the other
 * nodes are not affected.
 */
def recover_node(nodeName, computer, originalCause, nodeJob) {
    def originalLabels = jenkins.model.Jenkins.instance.getNode(nodeName).getLabelString()
    try {
        // Logged so the labels can be restored by hand if the controller dies before they are.
        println("${nodeName}: original labels: ${originalLabels}")
        set_node_labels(nodeName, RECOVERING_LABELS)

        // Reboot first: it releases the file handles that stopped the workspace cleanup.
        // node() cannot schedule on an offline node, so bring it online for the reboot.
        // The cleanup job is scheduled by node name, which every node keeps as a label.
        computer.setTemporarilyOffline(null)
        node("${nodeName}") {
            stage("${nodeName} - Reboot") {
                sanitize_node(nodeName)
            }
        }
        nodeJob()
        recoveryResults[nodeName] = 'recovered'
    } catch (e) {
        println("${nodeName}: recovery failed: ${e.getMessage()}")
        computer.setTemporarilyOffline(new OfflineCause.UserCause(null, "${originalCause} ${FAILED_MARKER}"))
        recoveryResults[nodeName] = 'failed'
        currentBuild.result = 'UNSTABLE'
    } finally {
        set_node_labels(nodeName, originalLabels)
    }
}

/*
 * Replace the labels of a node and save the change.
 */
def set_node_labels(nodeName, labels) {
    def aNode = jenkins.model.Jenkins.instance.getNode(nodeName)
    aNode.setLabelString(labels)
    jenkins.model.Jenkins.instance.updateNode(aNode)
}

/*
 * Report recovered and failed nodes to Slack.
 */
def report_recovery() {
    if (!SLACK_CHANNEL) {
        return
    }
    def recovered = []
    def failed = []
    for (entry in recoveryResults.entrySet().toList()) {
        if (entry.value == 'recovered') {
            recovered.add(entry.key)
        } else {
            failed.add(entry.key)
        }
    }
    if (recovered) {
        slackSend channel: SLACK_CHANNEL, color: 'good', message: "${env.JOB_NAME} #${env.BUILD_NUMBER} (<${env.BUILD_URL}|Open>) recovered offline Windows nodes: ${recovered.join(',')}"
    }
    if (failed) {
        slackSend channel: SLACK_CHANNEL, color: 'danger', message: "${env.JOB_NAME} #${env.BUILD_NUMBER} (<${env.BUILD_URL}|Open>) could not recover Windows nodes, left offline: ${failed.join(',')}"
    }
}

/*
 * Return a list of workspace directories (current build workspace excluded)
 */
def get_other_workspaces(workspaceDir) {
    // fetch all directories in workspaceDir (this should not fail)
    def workspaces = sh(
                        script: """
                            ls "${workspaceDir}"
                        """,
                        returnStdout: true).trim().tokenize(System.lineSeparator())
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

    workingComputer.setTemporarilyOffline(new OfflineCause.UserCause(null, "sanitizing node ${nodeName}"))
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

    } catch (e) {
        println(e.getMessage())
    }

    //reconnect node
    timeout(time: RECONNECT_TIMEOUT.toInteger(), unit: 'MINUTES') {
        println("\t ${nodeName}: Disconnecting...")
        workingComputer.disconnect(null)
        workingComputer.waitUntilOffline()

        println("\t ${nodeName}: Connecting...")
        workingComputer.connect(false)
        workingComputer.setTemporarilyOffline(null)
        workingComputer.waitUntilOnline()
        println("\t ${nodeName} is back online: ${workingComputer.isOnline()}")
    }
}
