/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
def LABEL = (params.LABEL) ?: "artifactory"
def WARNING_THRESHOLD = (params.WARNING_THRESHOLD) ? params.WARNING.toInteger() : 75
def DANGER_THRESHOLD = (params.DANGER_THRESHOLD) ? params.DANGER_THRESHOLD.toInteger() : 90
def SLACK_CHANNEL = (params.SLACK_CHANNEL) ?: "#jenkins-sandbox"
def SLACK_MESSAGE = "${JOB_NAME} jenkins job\n"
def HIGH_ROOT_DISK_PERCENTAGE = 0
def HIGH_ARTIFACTORY_DISK_PERCENTAGE = 0
def HIGH_NODE_ROOT
def HIGH_NODE_ARTIFACTORY

timestamps {
	stage("Artifactory data collection") {
		for (sNode in jenkins.model.Jenkins.instance.getLabel(LABEL).getNodes()) {
			node(sNode.getNodeName()) {
				def nodeLabels = sNode.getLabelString().tokenize(" ")
				def artifactoryGeo
				nodeLabels.each {
					if (it.contains("ci.geo")) {
						artifactoryGeo = it.tokenize('.') [2]
					}
				}
				def rootDiskDetails = sh (label: "Get / disk details", returnStdout: true, script: "df / -h | tail -1 | awk '{print \$3 \" / \" \$2}'").trim()
				def rootDiskPercentage = sh (label: "Get / disk percentage", returnStdout: true, script: "df / -h | tail -1 | awk '{print \$5}'").trim()
				def artifactoryDiskDetails = sh (label: "Get Artifactory disk details", returnStdout: true, script: "df -h | grep jenkins | awk '{print \$3 \" / \" \$2}'").trim()
				def artifactoryDiskPercentage = sh (label: "Get Artifactory disk percentage", returnStdout: true, script: "df -h | grep jenkins | awk '{print \$5}'").trim()
				def message = "Artifactory on ${sNode.getNodeName()} hosted at ${artifactoryGeo.toUpperCase()}:\n  Root disk used: ${rootDiskDetails} -> ${rootDiskPercentage}\n  Data disk used: ${artifactoryDiskDetails} -> ${artifactoryDiskPercentage}\n"
				SLACK_MESSAGE += "${message}"
				echo "${message}"
				if (HIGH_ROOT_DISK_PERCENTAGE < (rootDiskPercentage - '%').toInteger()) {
					HIGH_ROOT_DISK_PERCENTAGE = (rootDiskPercentage - '%').toInteger()
					HIGH_NODE_ROOT = sNode.getNodeName()
				}
				if (HIGH_ARTIFACTORY_DISK_PERCENTAGE < (artifactoryDiskPercentage - "%").toInteger()) {
					HIGH_ARTIFACTORY_DISK_PERCENTAGE = (artifactoryDiskPercentage - "%").toInteger()
					HIGH_NODE_ARTIFACTORY = sNode.getNodeName()
				}
			}
		}
	}

	stage("Artifactory data review") {
		def slackStatus = "good"
		
		if ((HIGH_ROOT_DISK_PERCENTAGE > DANGER_THRESHOLD) || (HIGH_ROOT_DISK_PERCENTAGE > DANGER_THRESHOLD)) {
			slackStatus = "danger"
		}
		if ((HIGH_ARTIFACTORY_DISK_PERCENTAGE > WARNING_THRESHOLD) || (HIGH_ARTIFACTORY_DISK_PERCENTAGE > WARNING_THRESHOLD)) {
			slackStatus = "warning"
		}
		message = "Highest root disk percentage ${HIGH_ROOT_DISK_PERCENTAGE}% on ${HIGH_NODE_ROOT}\nHighest artifactory disk percentage ${HIGH_ARTIFACTORY_DISK_PERCENTAGE}% on ${HIGH_NODE_ARTIFACTORY}"
		echo "${message}\nSlack status: ${slackStatus}"
		if (slackStatus != "good") {
			SLACK_MESSAGE += "${message}"
			slackSend channel: SLACK_CHANNEL, color: slackStatus, message: SLACK_MESSAGE
		}
	}
}
