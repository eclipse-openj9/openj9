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

properties([
    buildDiscarder(logRotator(numToKeepStr: '50')),
    parameters([
        string(name: 'REPO', defaultValue: '', description: 'Git repository to promote. Must be in https format', trim: true),
        string(name: 'TARGET_BRANCH', defaultValue: '', description: 'Git branch to merge COMMIT onto.', trim: true),
        string(name: 'COMMIT', defaultValue: '', description: 'Git SHA to merge onto TARGET_BRANCH.', trim: true),
        booleanParam(name: 'TAG', defaultValue: false, description: 'Lay down a tag on the merge commit'),
        string(name: 'TAG_DESCRIPTION', defaultValue: '', description: 'If TAG, the description message to be used in the tag')
    ])
])

if (!params.REPO && !REPO.contains('https://')) {
    error("Must supply REPO param in https format")
}
if (!params.TARGET_BRANCH) {
    error("Must supply TARGET_BRANCH param")
}
if (!params.COMMIT) {
    error("Must supply COMMIT param")
}

timestamps {
    timeout(time: 3, unit: 'HOURS') {
        node('worker') {
            stage('Promote') {
                try {
                    checkout scm
                    buildfile = load 'buildenv/jenkins/common/pipeline-functions'
                    variableFile = load 'buildenv/jenkins/common/variables-functions'
                    variableFile.parse_variables_file()

                    dir('repo') {
                        CRED_ID = variableFile.get_user_credentials_id('github')

                        git branch: TARGET_BRANCH, url: REPO

                        OLD_SHA = buildfile.get_sha(REPO, TARGET_BRANCH)

                        MERGE_DATE = variableFile.get_date()

                        sh "git merge ${COMMIT} -m 'Merge ${COMMIT} into ${OLD_SHA}'"

                        buildfile.git_push_auth(REPO, TARGET_BRANCH, CRED_ID)

                        if (TAG == 'true') {
                            sh """git tag -a 'promote_merge_${MERGE_DATE}' \
                                -m '${TAG_DESCRIPTION}'"""
                            buildfile.git_push_auth(REPO, '--tags', CRED_ID)
                        }

                        currentBuild.description = sh (
                            script: 'git log --oneline -1',
                            returnStdout: true
                        ).trim()
                    }
                } finally {
                    cleanWs()
                }
            }
        }
    }
}
