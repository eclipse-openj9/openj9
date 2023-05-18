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
 *******************************************************************************/

def FAIL = false
Boolean KEEP_WORKSPACE = params.KEEP_WORKSPACE ?: false
String NODE_LABEL = params.NODE_LABEL ?: 'worker'
String SRC_HOST = params.SRC_HOST ?: 'https://github.com/'
String SRC_REPO = "${SRC_HOST}${ghprbGhRepository}.git"
String GIT_CREDENTIALS_ID = params.GIT_CREDENTIALS_ID ?: ''
def HASHES = '###################################'
def COMMITS = []
def BAD_COMMITS = []

timeout(time: 6, unit: 'HOURS') {
    stage('SignedOffBy Check') {
        timestamps {
            node (NODE_LABEL) {
                try {
                    def remoteConfigParameters = [refspec: "+refs/pull/${ghprbPullId}/*:refs/remotes/origin/pr/${ghprbPullId}/* +refs/heads/${ghprbTargetBranch}:refs/remotes/origin/${ghprbTargetBranch}",
                                                      url: SRC_REPO]
                    if (GIT_CREDENTIALS_ID) {
                        remoteConfigParameters.put("credentialsId", "${GIT_CREDENTIALS_ID}")
                    }
                    String REPO_CACHE_DIR = params.REPO_CACHE_DIR ?: "${env.HOME}/openjdk_cache"

                    checkout changelog: false, poll: false,
                        scm: [$class: 'GitSCM',
                            branches: [[name: ghprbActualCommit]],
                            doGenerateSubmoduleConfigurations: false,
                            extensions: [[$class: 'CloneOption',
                                            depth: 0,
                                            noTags: false,
                                            reference: "${REPO_CACHE_DIR}",
                                            shallow: false]],
                            submoduleCfg: [],
                            userRemoteConfigs: [remoteConfigParameters]]

                    if (GIT_CREDENTIALS_ID) {
                        sshagent(credentials:["${GIT_CREDENTIALS_ID}"]) {
                            COMMITS = get_commits()
                        }
                    } else {
                        COMMITS = get_commits()
                    }
                    if (COMMITS == "") {
                        echo "There are no commits to check for signoffby message"
                    } else {
                        def COMMIT_RAW = COMMITS.split("(?=commit [a-f0-9]{40}+\n(Merge: [a-f0-9 ]+)*\n*Author: .*\nDate:[ ]+.*\n)");
                        COMMIT_RAW.each { COMMIT ->
                            def WRONG_COMMIT = true
                            def splitCommit = COMMIT.split("\\r?\\n")
                            def lastBlankLine = 0
                            if (splitCommit[-1] ==~ /[ ]{4}This reverts commit [0-9a-f]{40}[\.]/) {
                                echo "The following commit appears to be a revert, does not need sign-off:\n${COMMIT}"
                                WRONG_COMMIT = false
                            } else {
                                for (int i = splitCommit.size() - 1; i >= 0; i--) {
                                    if (splitCommit[i] ==~ /^\s*$/) {
                                        lastBlankLine = i
                                        break
                                    }
                                }
                                for (int j = splitCommit.size() - 1; j > lastBlankLine; j--) {
                                    if (splitCommit[j] ==~ /\s*Signed\-off\-by\:\s+[^<]+\s+\<[^@]+@[^@]+\.[^@]+\>/) {
                                        echo "The following commit appears to have a proper sign-off:\n${COMMIT}"
                                        WRONG_COMMIT = false
                                        break
                                    }
                                }
                                if (WRONG_COMMIT) {
                                    echo "FAILURE - The following commit appears to have an incorrect sign-off:\n${COMMIT}"
                                    BAD_COMMITS << "${COMMIT}"
                                    FAIL = true
                                }
                            }
                        }
                        if (FAIL) {
                            echo "${HASHES}"
                            echo "FAILURE - The following commits appear to have an incorrect sign-off"
                            BAD_COMMITS.each { commit ->
                                echo "${commit}"
                            }
                            echo "${HASHES}"
                            currentBuild.result = 'FAILURE'
                        } else {
                            echo "All commits appears to be have a proper sign-off"
                        }
                    }
                } finally {
                    if (!KEEP_WORKSPACE) {
                        cleanWs()
                    }
                }
            }
        }
    }
}

def get_commits() {
    return sh(
        script : "git log origin/${ghprbTargetBranch}..HEAD",
        returnStdout: true
        ).trim()
}
