/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

def FAIL = false
def SRC_REPO = 'https://github.com/${ghprbGhRepository}.git'
def HASHES = '###################################'
def COMMITS = []
def BAD_COMMITS = []

timeout(time: 6, unit: 'HOURS') {
    stage('SignedOffBy Check') {
        timestamps {
            node ('worker') {
                try {
                    checkout changelog: false, poll: false,
                        scm: [$class: 'GitSCM',
                            branches: [[name: ghprbActualCommit]],
                            doGenerateSubmoduleConfigurations: false,
                            extensions: [[$class: 'CloneOption',
                                            depth: 0,
                                            noTags: false,
                                            reference: '/home/jenkins/openjdk_cache',
                                            shallow: false]],
                            submoduleCfg: [],
                            userRemoteConfigs: [[refspec: "+refs/pull/${ghprbPullId}/*:refs/remotes/origin/pr/${ghprbPullId}/* +refs/heads/${ghprbTargetBranch}:refs/remotes/origin/${ghprbTargetBranch}",
                                                    url: SRC_REPO]]]

                    COMMITS = sh(
                         script : "git log origin/${ghprbTargetBranch}..HEAD",
                         returnStdout: true
                         ).trim()
                    if (COMMITS == "") {
                        echo "There are no commits to check for signoffby message"
                    } else {
                        def COMMIT_RAW = COMMITS.split("(?=commit [a-f0-9]{40}+\n(Merge: [a-f0-9 ]+)*\n*Author: .*\nDate:[ ]+.*\n)");
                        COMMIT_RAW.each { COMMIT ->
                            def WRONG_COMMIT = true
                            def splitCommit = COMMIT.split("\\r?\\n")
                            def lastBlankLine = 0
                            if (splitCommit[-1] ==~ /[ ]{4}This reverts commit [0-9a-f]{40}[\.]/){
                            	echo "The following commit appears to be a revert, does not need sign-off:\n${COMMIT}" 
                            	WRONG_COMMIT = false
                            }else{
	                            for (int i = splitCommit.size() - 1; i >= 0; i--) {
	                                if ( splitCommit[i] ==~ /^\s*$/ ) {
	                                    lastBlankLine = i
	                                    break  
	                                }
	                            }
	                            for (int j = splitCommit.size() - 1; j > lastBlankLine; j-- ) {
	                                if ( splitCommit[j] ==~ /\s*Signed\-off\-by\:\s+[^<]+\s+\<[^@]+@[^@]+\.[^@]+\>/ ) {
	                                    echo "The following commit appears to have a proper sign-off:\n${COMMIT}" 
	                                    WRONG_COMMIT = false
	                                    break
	                                }
	                            }
	                            if ( WRONG_COMMIT ) {
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
                    cleanWs()
                }
            }
        }
    }
}

