/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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

Boolean FAIL = false
Boolean KEEP_WORKSPACE = params.KEEP_WORKSPACE ?: true
String NODE_LABEL = params.NODE_LABEL ?: 'worker'
String SRC_HOST = params.SRC_HOST ?: 'https://github.com/'
String SRC_REPO = "${SRC_HOST}${ghprbGhRepository}.git"
String GIT_CREDENTIALS_ID = params.GIT_CREDENTIALS_ID ?: ''
def BAD_FILES = []
String HASHES = '###################################'

timeout(time: 6, unit: 'HOURS') {
    stage('Line Endings Check') {
        node (NODE_LABEL) {
            timestamps {
                try {
                    def remoteConfigParameters = [
                        refspec: "+refs/pull/${ghprbPullId}/*:refs/remotes/origin/pr/${ghprbPullId}/* +refs/heads/${ghprbTargetBranch}:refs/remotes/origin/${ghprbTargetBranch}",
                        url: SRC_REPO
                    ]
                    if (GIT_CREDENTIALS_ID) {
                        remoteConfigParameters.put("credentialsId", "${GIT_CREDENTIALS_ID}")
                    }
                    String REPO_CACHE_DIR = params.REPO_CACHE_DIR ?: "${env.HOME}/openjdk_cache"

                    checkout changelog: false,
                        poll: false,
                        scm: [
                            $class: 'GitSCM',
                            branches: [[name: sha1]],
                            doGenerateSubmoduleConfigurations: false,
                            extensions: [[
                                $class: 'CloneOption',
                                depth: 0,
                                noTags: false,
                                reference: "${REPO_CACHE_DIR}",
                                shallow: false
                            ]],
                            userRemoteConfigs: [remoteConfigParameters]
                        ]

                    if (GIT_CREDENTIALS_ID) {
                        sshagent(credentials:["${GIT_CREDENTIALS_ID}"]) {
                            FILES = get_files()
                        }
                    } else {
                        FILES = get_files()
                    }
                    echo FILES
                    if (FILES == "") {
                        echo "There are no files to check for line endings"
                    } else {
                        def FILES_LIST = FILES.split("\\r?\\n")
                        FILES_LIST.each() {
                            echo "Checking file: '${it}'"
                            TYPE = sh (
                                script: "file -b '${it}'",
                                returnStdout: true
                            ).trim()

                            switch (TYPE) {
                                case ~/empty/:
                                    echo "Empty file: '${it}'"
                                    break
                                case ~/.*text.*/:
                                    switch (it.toLowerCase()) {
                                        case ~/.*\.bat/:
                                            switch (TYPE) {
                                                case ~/.*CRLF line terminators.*/:
                                                    // check for CRLF at EOF
                                                    NEWLINES = sh (
                                                        script: "tail -c2 '${it}' | wc -l",
                                                        returnStdout: true
                                                    ).trim()
                                                    if (NEWLINES == "1") {
                                                        echo "Good Windows script: '${it}' type: '${TYPE}'"
                                                    } else {
                                                        echo "ERROR - should have final line terminator: '${it}' type: '${TYPE}'"
                                                        FAIL = true
                                                        BAD_FILES << "${it}"
                                                    }
                                                    break
                                                default:
                                                    echo "ERROR - should have CRLF line terminators: '${it}' type: '${TYPE}'"
                                                    FAIL = true
                                                    BAD_FILES << "${it}"
                                            }
                                        default:
                                            switch (TYPE) {
                                                case ~/.*CR.* line terminators.*/:
                                                    echo "ERROR - should have LF line terminators: '${it}' type: '${TYPE}'"
                                                    FAIL = true
                                                    BAD_FILES << "${it}"
                                                    break
                                                case ~/.*EBCDIC text.*/:
                                                    // Check for line end at EOF, which, in EBCDIC, may be (octal) 0045 or 0205:
                                                    // Map them to LF so wc will recognize them.
                                                    NEWLINES = sh (
                                                        script: "tail -c1 '${it}' | tr '\045\205' '\n' | wc -l",
                                                        returnStdout: true
                                                    ).trim()
                                                    if (NEWLINES == "1") {
                                                        echo "Good text file: '${it}' type: '${TYPE}'"
                                                    } else {
                                                        echo "ERROR - should have final line terminator: '${it}' type: '${TYPE}'"
                                                        FAIL = true
                                                        BAD_FILES << "${it}"
                                                    }
                                                    break
                                                default:
                                                    // check for LF at EOF
                                                    NEWLINES = sh (
                                                        script: "tail -c1 '${it}' | wc -l",
                                                        returnStdout: true
                                                    ).trim()
                                                    if (NEWLINES == "1") {
                                                        echo "Good text file: '${it}' type: '${TYPE}'"
                                                    } else {
                                                        echo "ERROR - should have final line terminator: '${it}' type: '${TYPE}'"
                                                        FAIL = true
                                                        BAD_FILES << "${it}"
                                                    }
                                                    break
                                            }
                                    }
                                default:
                                    echo "Non-text file: '${it}' type: '${TYPE}'"
                            }
                        }
                        if (FAIL) {
                            echo "${HASHES}"
                            echo "The following files were modified and have incorrect line endings"
                            BAD_FILES.each() {
                                echo "${it}"
                            }
                            echo "${HASHES}"
                            sh 'exit 1'
                        } else {
                            echo "Checking for added trailing whitespace..."
                            if (GIT_CREDENTIALS_ID) {
                                sshagent(credentials:["${GIT_CREDENTIALS_ID}"]) {
                                    WHITESPACE_ERRORS = get_whitespaces()
                                }
                            } else {
                                WHITESPACE_ERRORS = get_whitespaces()
                            }
                            if (WHITESPACE_ERRORS.trim() == "") {
                                echo "All modified files appear to have correct line endings"
                            } else {
                                echo "${WHITESPACE_ERRORS}"
                            }
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

def get_files() {
    return sh (
        script: "git diff -C --diff-filter=ACM --name-only origin/${ghprbTargetBranch} HEAD",
        returnStdout: true
        ).trim()
}

def get_whitespaces() {
    sh 'git config core.whitespace blank-at-eof,blank-at-eol,cr-at-eol,space-before-tab'
    return sh (
        script: "git diff --check origin/${ghprbTargetBranch} HEAD",
        returnStdout: true
        )
}
