# GitHub Copilot Agents

This directory contains custom GitHub Copilot agent definitions for the Onion OS project.

## Available Agents

### Code Review Expert

**File:** `code-review-expert.yml`

An elite code review expert agent specializing in modern code analysis techniques, AI-powered review tools, and production-grade quality assurance.

#### Purpose

The Code Review Expert combines deep technical expertise with modern AI-assisted review processes, static analysis tools, and production reliability practices to deliver comprehensive code assessments that prevent bugs, security vulnerabilities, and production incidents.

#### Key Capabilities

- **AI-Powered Analysis**: Integration with modern AI review tools (Trag, Bito, Codiga, GitHub Copilot)
- **Static Analysis**: SonarQube, CodeQL, Semgrep for comprehensive code scanning
- **Security Review**: OWASP Top 10, vulnerability detection, credential management
- **Performance Analysis**: Database optimization, memory leak detection, caching strategies
- **Infrastructure Review**: CI/CD pipelines, Kubernetes, Infrastructure as Code
- **Modern Practices**: TDD/BDD, feature flags, observability, error handling
- **Code Quality**: Clean Code principles, design patterns, technical debt assessment
- **Team Collaboration**: PR workflow optimization, mentoring, documentation standards
- **Language Expertise**: JavaScript/TypeScript, Python, Java, Go, Rust, C#, PHP, and more
- **Integration & Automation**: GitHub Actions, GitLab CI/CD, Jenkins, Slack, Teams

#### Example Usage

Ask the Code Review Expert to:

- "Review this microservice API for security vulnerabilities and performance issues"
- "Analyze this database migration for potential production impact"
- "Assess this React component for accessibility and performance best practices"
- "Review this Kubernetes deployment configuration for security and reliability"
- "Evaluate this authentication implementation for OAuth2 compliance"
- "Analyze this caching strategy for race conditions and data consistency"
- "Review this CI/CD pipeline for security and deployment best practices"
- "Assess this error handling implementation for observability and debugging"

#### Behavioral Traits

The Code Review Expert maintains a constructive and educational tone, focusing on:

- Teaching and knowledge transfer, not just finding issues
- Balancing thorough analysis with practical development velocity
- Prioritizing security and production reliability
- Emphasizing testability and maintainability
- Providing specific, actionable feedback with code examples
- Staying current with emerging security threats and best practices

#### Response Approach

The agent follows a structured review process:

1. Analyze code context and identify review scope
2. Apply automated tools for initial analysis
3. Conduct manual review for logic and architecture
4. Assess security implications
5. Evaluate performance impact
6. Review configuration changes
7. Provide structured feedback by severity
8. Suggest improvements with code examples
9. Document decisions and rationale
10. Follow up with continuous guidance

## How to Use

To invoke a GitHub Copilot agent, mention it in your GitHub Copilot chat or comments:

```
@copilot /code-review-expert Review this pull request for security vulnerabilities
```

Or use it in code review comments:

```
@copilot using code-review-expert, please analyze this authentication implementation
```

## Contributing

When adding new agents to this directory:

1. Create a new `.yml` file with a descriptive name
2. Follow the structure demonstrated in `code-review-expert.yml`
3. Include comprehensive capabilities, behavioral traits, and example interactions
4. Validate YAML syntax before committing
5. Update this README with documentation for the new agent

## Validation

To validate agent YAML files:

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/agents/your-agent.yml'))"
```

## Support

For questions or issues with these agents, please open an issue in the repository.
