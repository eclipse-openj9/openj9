const axios = require('axios');

const apiUrl = "http://140.211.168.122/recommendation";

const sandboxIssueNumber = 1;
const sandboxOwner = context.repo.owner;
const sandboxRepo = context.repo.repo;

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
  await github.issues.createComment({
    issue_number: sandboxIssueNumber,
    owner: sandboxOwner,
    repo: sandboxRepo,
    body: `The TriagerX model is currently not responding to the issue ${issueNumber}. Please try again later.`
  });
}
