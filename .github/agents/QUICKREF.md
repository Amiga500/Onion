# Code Review Expert - Quick Reference

## Quick Start

```
@copilot using code-review-expert, [your request]
```

## Common Commands

| Task | Command |
|------|---------|
| **Full PR Review** | `@copilot using code-review-expert, review this PR` |
| **Security Scan** | `@copilot using code-review-expert, check for security vulnerabilities` |
| **Performance Check** | `@copilot using code-review-expert, analyze performance issues` |
| **Code Quality** | `@copilot using code-review-expert, assess code quality` |
| **Infrastructure** | `@copilot using code-review-expert, review this config for production` |

## Focus Areas

### 🔒 Security
- OWASP Top 10 vulnerabilities
- SQL injection, XSS, CSRF
- Authentication/authorization
- Secrets management
- API security

### ⚡ Performance
- Database optimization
- Memory leaks
- Caching strategies
- N+1 queries
- Resource management

### 📐 Code Quality
- Clean Code principles
- Design patterns
- Code duplication
- Technical debt
- Maintainability

### 🏗️ Infrastructure
- Kubernetes/Docker config
- CI/CD pipelines
- Infrastructure as Code
- Secrets management
- Monitoring setup

### 🧪 Best Practices
- Test coverage
- Error handling
- Documentation
- Observability
- Feature flags

## Language Support

✓ JavaScript/TypeScript
✓ Python
✓ Java
✓ Go
✓ Rust
✓ C#
✓ PHP
✓ SQL/NoSQL

## Severity Levels

| Level | Description |
|-------|-------------|
| 🔴 **Critical** | Security vulnerabilities, data loss risks |
| 🟠 **High** | Performance issues, production reliability |
| 🟡 **Medium** | Code quality, maintainability concerns |
| 🟢 **Low** | Style, documentation, minor improvements |

## Example Requests

### Security Review
```
@copilot using code-review-expert, review this authentication 
implementation for OAuth2 compliance and security best practices
```

### Performance Analysis
```
@copilot using code-review-expert, analyze this database query 
for optimization opportunities and potential N+1 problems
```

### Full Stack Review
```
@copilot using code-review-expert, review this microservice 
for security, performance, and reliability issues
```

### Infrastructure Review
```
@copilot using code-review-expert, review this Kubernetes 
deployment for security and production readiness
```

### Code Quality Assessment
```
@copilot using code-review-expert, assess this module for 
clean code principles and refactoring opportunities
```

## Best Practices

✓ **Be specific** about what you want reviewed
✓ **Provide context** about the changes
✓ **Ask follow-up questions** for clarification
✓ **Iterate** on the feedback
✓ **Learn** from the recommendations

## Integration

### GitHub PR Comments
```
/code-review-expert analyze the authentication changes
```

### VS Code Copilot Chat
```
@workspace using code-review-expert, review current file
```

### CLI
```bash
gh copilot agent code-review-expert --context pr
```

## Response Format

The agent provides:
1. **Summary** of findings
2. **Issues** organized by severity
3. **Recommendations** with code examples
4. **Rationale** for suggestions
5. **Best practices** and resources

## Documentation

- **README.md** - Complete documentation
- **EXAMPLES.md** - Detailed usage examples
- **TESTS.md** - Test cases and validation
- **code-review-expert.yml** - Agent configuration

## Support

📖 See `.github/agents/README.md` for full documentation
💡 See `.github/agents/EXAMPLES.md` for detailed examples
🧪 See `.github/agents/TESTS.md` for test scenarios

---

**Version:** 1.0 | **Updated:** 2026-02-02
