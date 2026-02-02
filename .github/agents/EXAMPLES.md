# Code Review Expert Agent - Usage Examples

This document provides practical examples of how to use the Code Review Expert agent.

## Quick Start

### Basic Code Review Request

```
@copilot using code-review-expert, please review the changes in this PR
```

### Specific Security Review

```
@copilot using code-review-expert, analyze this authentication implementation for security vulnerabilities
```

### Performance Analysis

```
@copilot using code-review-expert, review this database query for performance optimization opportunities
```

## Detailed Examples

### Example 1: API Security Review

**Scenario:** You've implemented a new REST API endpoint for user authentication.

**Request:**
```
@copilot using code-review-expert, review this microservice API for security vulnerabilities and performance issues:

[paste code or reference PR]
```

**Expected Analysis:**
- OWASP Top 10 vulnerability check
- Input validation review
- Authentication mechanism assessment
- Rate limiting implementation
- Error handling and information disclosure
- SQL injection prevention
- Performance bottlenecks

### Example 2: Database Migration Review

**Scenario:** You need to review a database schema migration before deploying to production.

**Request:**
```
@copilot using code-review-expert, analyze this database migration for potential production impact:

[paste migration code]
```

**Expected Analysis:**
- Schema change impact on existing data
- Index performance implications
- Migration rollback safety
- Data integrity checks
- Query performance after migration
- Locking and blocking considerations
- Production deployment risks

### Example 3: React Component Review

**Scenario:** You've created a new React component and want to ensure it follows best practices.

**Request:**
```
@copilot using code-review-expert, assess this React component for accessibility and performance best practices:

[paste component code]
```

**Expected Analysis:**
- Accessibility (WCAG compliance)
- Performance optimization (memoization, lazy loading)
- React best practices
- State management patterns
- Error boundary implementation
- Code organization and maintainability
- Test coverage recommendations

### Example 4: CI/CD Pipeline Review

**Scenario:** You've updated the GitHub Actions workflow and want to ensure security and reliability.

**Request:**
```
@copilot using code-review-expert, review this CI/CD pipeline for security and deployment best practices:

[paste workflow YAML]
```

**Expected Analysis:**
- Secrets management
- Container security
- Deployment strategy safety
- Test coverage requirements
- Environment configuration
- Rollback mechanisms
- Monitoring and alerting integration
- Pipeline optimization opportunities

### Example 5: Infrastructure as Code Review

**Scenario:** You've written Terraform configuration for new AWS infrastructure.

**Request:**
```
@copilot using code-review-expert, review this Kubernetes deployment configuration for security and reliability:

[paste Kubernetes manifests or Terraform code]
```

**Expected Analysis:**
- Security best practices
- Resource limits and requests
- High availability configuration
- Network policies
- Secret management
- Monitoring and logging
- Cost optimization
- Disaster recovery considerations

## Integration with Pull Requests

### Adding to PR Comments

When reviewing a PR, you can invoke the Code Review Expert in comments:

```
/code-review-expert please analyze the authentication changes in `src/auth/oauth.js`
```

### Automated PR Analysis

Set up GitHub Actions to automatically invoke the agent on new PRs:

```yaml
name: Code Review
on: [pull_request]

jobs:
  review:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: AI Code Review
        run: |
          # Invoke code review expert via GitHub Copilot API
          gh copilot agent code-review-expert --context pr
```

## Best Practices

### 1. Be Specific About Scope

❌ Bad: "Review this code"
✅ Good: "Review this authentication implementation for OAuth2 compliance and security vulnerabilities"

### 2. Provide Context

Include relevant context such as:
- What the code does
- What changes were made
- Specific concerns or areas of focus
- Production environment details

### 3. Iterate on Feedback

- Review initial findings
- Ask follow-up questions
- Request clarification on recommendations
- Validate suggested improvements

### 4. Focus on Priorities

Ask the agent to prioritize:
- Critical security issues
- Production reliability concerns
- Performance bottlenecks
- Major architectural problems

### 5. Use for Learning

Use the agent's feedback as a learning opportunity:
- Understand why certain patterns are recommended
- Learn about security best practices
- Discover new tools and techniques
- Improve code quality over time

## Advanced Usage

### Custom Review Checklists

```
@copilot using code-review-expert, create a code review checklist for:
- Type: Microservice API
- Language: Go
- Framework: Gin
- Focus: Security, Performance, Error Handling
```

### Technical Debt Assessment

```
@copilot using code-review-expert, assess the technical debt in this module and provide a remediation plan:

[paste code]
```

### Architecture Review

```
@copilot using code-review-expert, evaluate this system architecture for scalability and reliability:

[describe or paste architecture diagram/code]
```

## Integration with Development Workflow

### Pre-Commit Review

Use the agent during development:
```bash
# Before committing
git diff | gh copilot agent code-review-expert --context diff
```

### IDE Integration

Configure your IDE to invoke the agent:
- VS Code: Use Copilot chat panel
- IntelliJ: Use Copilot integration
- Vim/Neovim: Use Copilot CLI

### Code Review Training

Use the agent to train team members on:
- Code review best practices
- Common vulnerability patterns
- Performance optimization techniques
- Clean code principles

## Feedback and Improvement

The Code Review Expert learns from:
- Code review outcomes
- Security incident reports
- Performance metrics
- Team feedback

Help improve the agent by:
- Reporting false positives
- Suggesting new review patterns
- Sharing successful remediation strategies
- Contributing to the knowledge base

## Support

For questions or issues:
1. Check the agent configuration in `.github/agents/code-review-expert.yml`
2. Review the main README in `.github/agents/README.md`
3. Open an issue in the repository
4. Contact the development team

## Version History

- v1.0 (2026-02-02): Initial release with comprehensive code review capabilities
  - AI-powered analysis
  - Security review
  - Performance analysis
  - Infrastructure review
  - Language-specific expertise
  - Integration automation
