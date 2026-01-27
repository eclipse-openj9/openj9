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

timestamps {
    timeout(time: 8, unit: 'HOURS') {
        stage('Queue') {
            node('sw.os.linux && hw.arch.x86 && sw.tool.docker') {
                def tmpDesc = currentBuild.description ? currentBuild.description + "<br>" : ""
                currentBuild.description = tmpDesc + "<a href=${JENKINS_URL}computer/${NODE_NAME}>${NODE_NAME}</a>"
                try {
                    def gitConfig = scm.getUserRemoteConfigs().get(0)
                    def refspec = gitConfig.getRefspec() ? gitConfig.getRefspec() : ""
                    checkout changelog: false, poll: false,
                        scm: [$class: 'GitSCM',
                            branches: [[name: scm.branches[0].name]],
                            userRemoteConfigs: [[
                                refspec: "${refspec}",
                                url: "${gitConfig.getUrl()}"]]
                        ]
                    stage('Pull Infra') {
                        sh "bash buildenv/jenkins/clang_format/pullInfra.sh"
                    }
                    stage('Docker Build') {
                        dir('buildenv/jenkins/clang_format') {
                            sh "docker build -t clang-format -f Dockerfile ."
                        }
                    }
                    stage('Format Check') {
                        sh "bash buildenv/jenkins/clang_format/clangFormatCheck.sh"
                    }
                    stage('Cleanup Infra') {
                        sh "bash buildenv/jenkins/clang_format/cleanupInfra.sh"
                    }
                } finally {
                    cleanWs()
                }
            }
        }
    }
}
