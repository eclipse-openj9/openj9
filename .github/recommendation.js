const axios = require('axios');
const core = require('@actions/core');
const github = require('@actions/github');

async function run() {
  const sandboxIssueNumber = 1;
  const sandboxOwner = 'Tigers-X';
  const sandboxRepo = 'openj9';

  const input = {
    issue_title: process.env.ISSUE_TITLE,
    issue_description: process.env.ISSUE_DESCRIPTION,
  };

  const apiUrl = "http://140.211.168.122/recommendation";

  try {
    const response = await axios.post(apiUrl, JSON.stringify(input), {
      headers: {
        'Accept': 'application/json',
        'Content-Type': 'application/json'
      }
    });

    const issueComment = response.data;
    predictedAssignees = issueComment.recommended_developers;
    predictedLabels = issueComment.recommended_components;

    resultString = `Issue Number: ${issueNumber}\n`;
    resultString += 'Status: Open\n';
    resultString += `Recommended Components: ${predictedLabels.join(', ')}\n`;
    resultString += `Recommended Assignees: ${predictedAssignees.join(', ')}\n`;
    await github.issues.createComment({
      issue_number: sandboxIssueNumber,
      owner: sandboxOwner,
      repo: sandboxRepo,
      body: resultString
    });
  } catch (error) {
    core.setFailed(`Action failed with error: ${error}`);
    await github.issues.createComment({
      issue_number: sandboxIssueNumber,
      owner: sandboxOwner,
      repo: sandboxRepo,
      body: `The TriagerX model is currently not responding to the issue ${issueNumber}. Please try again later.`
    });
  }
}
run();
