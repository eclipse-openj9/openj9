/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

timestamps {
    timeout(time: 1, unit: 'HOURS') {
        def HTTP = 'https://'
        def SRC_REPO = 'github.com/eclipse/omr.git'
        def TARGET_REPO = 'github.com/eclipse/openj9-omr.git'
        def ARCHIVE_FILE = "OMR_COMMIT"

        stage("Mirror") {
            node('worker') {
                try {
                    retry(2) {
                        try {
                            //Clone/update
                            if (!fileExists('HEAD')) {
                                sh "git clone --mirror ${HTTP}${SRC_REPO} ."
                            } else {
                                sh 'git remote update --prune'
                            }

                            // Push
                            withCredentials([usernamePassword(credentialsId: 'b6987280-6402-458f-bdd6-7affc2e360d4', usernameVariable: 'USERNAME', passwordVariable: 'PASSWORD')]) {
                                sh "git push ${HTTP}${USERNAME}:${PASSWORD}@${TARGET_REPO} --all"
                                sh "git push ${HTTP}${USERNAME}:${PASSWORD}@${TARGET_REPO} --tags"
                            }

                            // Set the build description
                            LAST_COMMIT = sh (
                                    script: 'git log --oneline -1',
                                    returnStdout: true
                                ).trim()
                            currentBuild.description = "${LAST_COMMIT}"

                            current_sha = LAST_COMMIT.take(7)

                            // if there were changes, launch an acceptance build
                            copyArtifacts optional: true, projectName: JOB_NAME, selector: lastSuccessful()
                            def default_sha = [omr_sha: 'Default']
                            def previous = readProperties defaults: default_sha, file: "${ARCHIVE_FILE}"
                            echo "Previous SHA:${previous.omr_sha}\nCurrent SHA:${current_sha}"
                            if ( previous.omr_sha != current_sha ) {
                                build job: 'Pipeline-OMR-Acceptance', propagate: false, wait: false
                                writeFile file: "${ARCHIVE_FILE}", text: "omr_sha=${current_sha}"
                            }
                            archiveArtifacts "${ARCHIVE_FILE}"
                        } catch(e) {
                            cleanWs()
                            throw e
                        }
                    }
                } catch(e) {
                    slackSend channel: '#jenkins', color: 'danger', message: "Failed: ${JOB_NAME} #${BUILD_NUMBER} (<${BUILD_URL}|Open>)"
                    throw e
                }
            }
        }
    }
}
