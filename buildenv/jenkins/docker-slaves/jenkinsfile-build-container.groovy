/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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
 *
 ******************************************************************************/

def ARCH = (params.ARCH) ? params.ARCH : "unknown"
def OS = (params.OS) ? params.OS : "unknown"
def validArch = ["x86", "s390x", "ppc64le"]
def DOCKER_URL = (params.DOCKER_URL) ? params.DOCKER_URL : "https://hub.docker.com/r"

if (params.ghprbPullId) {
    PARSE = "${ghprbCommentBody}".toLowerCase().split()
    for (i = 0; i < PARSE.length - 4; i++) {
        if (PARSE[i] == "jenkins" && PARSE[i+1] == "build" && PARSE[i+2] == "docker") {
            ARCH = PARSE[i+3]
            OS = PARSE[i+4]
            break
        }
    }
}

if ("${OS}" == "unknown" || !(validArch.contains(ARCH))) {
    error("Invalid Parameters. Either ARCH:'${ARCH}' or OS:'${OS}' were not declared properly")
}

def BUILD_OPTS = ""
if ("${OS}" == "centos6.9") {
    if ("${ARCH}" == "x86") {
        BUILD_OPTS = "--dist=centos --version=6.9"
    }
} else if ("${OS}" == "centos7") {
    if ("${ARCH}" == "ppc64le" || "${ARCH}" == "x86") {
        BUILD_OPTS = "--dist=centos --version=7"
    }
} else if ("${OS}" == "ubuntu16") {
    BUILD_OPTS = "--dist=ubuntu --version=16.04"
} else if ("${OS}" == "ubuntu18") {
    BUILD_OPTS = "--dist=ubuntu --version=18.04"
} else if ("${OS}" == "ubuntu20") {
    BUILD_OPTS = "--dist=ubuntu --version=20.04"
}
if ("${BUILD_OPTS}" == "") {
    error("Invalid Parameters. OS:'${OS}' is not supported on ${ARCH}")
}

def NODE = (params.NODE) ? params.NODE : "sw.tool.docker && hw.arch.${ARCH}"
def NAMESPACE = (params.NAMESPACE) ? params.NAMESPACE : "eclipse"
def FOLDER = (params.FOLDER) ? "/" + params.FOLDER : ""
def REPOSITORY = "${NAMESPACE}${FOLDER}/openj9-jenkins-agent-${ARCH}-${OS}"

timeout(time: 5, unit: 'HOURS') {
    timestamps {
        node(NODE) {
            try{
                def TEMP_DESC = (currentBuild.description) ? currentBuild.description + "<br>" : ""
                currentBuild.description = TEMP_DESC + "<a href=${JENKINS_URL}/computer/${NODE_NAME}>${NODE_NAME}</a><br>"
                currentBuild.description += "Docker image:<a href=${DOCKER_URL}/${REPOSITORY}>${REPOSITORY}</a>"
                stage("Clone") {
                    if (params.VENDOR_BRANCH && params.VENDOR_REPO) {
                        def VENDOR_CREDENTIALS = (params.VENDOR_CREDENTIALS) ? params.VENDOR_CREDENTIALS : ""
                        git branch: params.VENDOR_BRANCH, changelog: false, credentialsId: VENDOR_CREDENTIALS, poll: false, url: params.VENDOR_REPO
                    } else {
                        checkout scm
                    }
                }
                stage("Build") {
                    if (params.ghprbPullId) {
                        TAGS = "--tag=${REPOSITORY}:PR${ghprbPullId}"
                    } else {
                        TAGS = "--tag=${REPOSITORY}:${BUILD_NUMBER} --tag=${REPOSITORY}:latest"
                    }
                    dir("buildenv/docker") {
                        sh "cp ${WORKSPACE}/buildenv/jenkins/authorized_keys ./"
                        sh "touch known_hosts"
                        if (env.KNOWN_HOSTS) {
                            sh "ssh-keyscan ${KNOWN_HOSTS} >> known_hosts"
                        }
                        sh "bash mkdocker.sh --build ${BUILD_OPTS} ${TAGS}"
                    }
                }
                stage("Push") {
                    def CREDENTIALS_ID = (params.credentialsId) ? params.credentialsId : '7fb9f8f0-14bf-469a-9132-91db4dd80c48'
                    def DOCKER_LOGIN_URL = (params.DOCKER_LOGIN_URL) ? params.DOCKER_LOGIN_URL : ""
                    sh 'docker images'
                    withCredentials([usernamePassword(credentialsId: CREDENTIALS_ID, passwordVariable: 'PASS', usernameVariable: 'USER')]) {
                        sh "docker login --username=\"${USER}\" --password=\"${PASS}\" ${DOCKER_LOGIN_URL}"
                    }
                    if (params.ghprbPullId) {
                        sh "docker push ${REPOSITORY}:PR${ghprbPullId}"
                    } else {
                        sh "docker push ${REPOSITORY}:${BUILD_NUMBER}"
                        sh "docker push ${REPOSITORY}:latest"
                    }
                    sh "docker logout"
                }
            } finally {
                sh "docker system prune -af"
                cleanWs()
            }
        }
    }
}
