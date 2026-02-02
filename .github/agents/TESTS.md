# Code Review Expert Agent - Test Suite

This document outlines test cases to validate the Code Review Expert agent functionality.

## Test Overview

These tests verify that the agent correctly identifies issues across different categories and provides actionable feedback.

## Test Categories

### 1. Security Vulnerabilities

#### Test 1.1: SQL Injection Detection

**Test Code:**
```python
def get_user(username):
    query = f"SELECT * FROM users WHERE username = '{username}'"
    return database.execute(query)
```

**Expected Feedback:**
- ❌ Critical: SQL injection vulnerability detected
- ✅ Recommendation: Use parameterized queries
- ✅ Example: `cursor.execute("SELECT * FROM users WHERE username = ?", (username,))`

#### Test 1.2: XSS Vulnerability

**Test Code:**
```javascript
function displayMessage(msg) {
    document.getElementById('output').innerHTML = msg;
}
```

**Expected Feedback:**
- ❌ High: Cross-site scripting (XSS) vulnerability
- ✅ Recommendation: Use textContent or sanitize input
- ✅ Example: `element.textContent = msg` or use DOMPurify

#### Test 1.3: Hardcoded Credentials

**Test Code:**
```python
API_KEY = "sk-1234567890abcdef"
DATABASE_PASSWORD = "mypassword123"
```

**Expected Feedback:**
- ❌ Critical: Hardcoded credentials detected
- ✅ Recommendation: Use environment variables or secrets management
- ✅ Example: `os.getenv('API_KEY')` or AWS Secrets Manager

### 2. Performance Issues

#### Test 2.1: N+1 Query Problem

**Test Code:**
```python
def get_users_with_posts():
    users = User.objects.all()
    for user in users:
        posts = Post.objects.filter(user=user)  # N+1 query!
        user.posts = posts
    return users
```

**Expected Feedback:**
- ❌ High: N+1 query problem detected
- ✅ Recommendation: Use select_related or prefetch_related
- ✅ Example: `User.objects.prefetch_related('posts').all()`

#### Test 2.2: Inefficient Loop

**Test Code:**
```javascript
function processItems(items) {
    for (let i = 0; i < items.length; i++) {
        // Multiple DOM manipulations
        document.getElementById('list').innerHTML += `<li>${items[i]}</li>`;
    }
}
```

**Expected Feedback:**
- ❌ Medium: Inefficient DOM manipulation in loop
- ✅ Recommendation: Batch DOM updates
- ✅ Example: Build HTML string first, then update once

#### Test 2.3: Memory Leak

**Test Code:**
```javascript
class EventManager {
    constructor() {
        window.addEventListener('resize', this.handleResize);
    }
    handleResize() {
        console.log('Resized');
    }
}
```

**Expected Feedback:**
- ❌ Medium: Potential memory leak (missing cleanup)
- ✅ Recommendation: Remove event listeners in cleanup
- ✅ Example: Add destructor with `removeEventListener`

### 3. Code Quality

#### Test 3.1: Code Duplication

**Test Code:**
```python
def calculate_discount_bronze(price):
    tax = price * 0.1
    discount = price * 0.05
    return price + tax - discount

def calculate_discount_silver(price):
    tax = price * 0.1
    discount = price * 0.10
    return price + tax - discount

def calculate_discount_gold(price):
    tax = price * 0.1
    discount = price * 0.15
    return price + tax - discount
```

**Expected Feedback:**
- ❌ Low: Code duplication detected
- ✅ Recommendation: Extract common logic
- ✅ Example: Single function with discount rate parameter

#### Test 3.2: Magic Numbers

**Test Code:**
```javascript
function isEligible(age) {
    return age >= 18 && age <= 65;
}
```

**Expected Feedback:**
- ⚠️ Low: Magic numbers detected
- ✅ Recommendation: Use named constants
- ✅ Example: `const MIN_AGE = 18; const MAX_AGE = 65;`

#### Test 3.3: Complex Function

**Test Code:**
```python
def process_order(order):
    # 50+ lines of complex logic
    # Multiple nested if statements
    # Many responsibilities
    ...
```

**Expected Feedback:**
- ❌ Medium: High cyclomatic complexity
- ✅ Recommendation: Break into smaller functions
- ✅ Example: Extract validation, calculation, notification logic

### 4. Infrastructure & Configuration

#### Test 4.1: Insecure Kubernetes Config

**Test Code:**
```yaml
apiVersion: v1
kind: Pod
spec:
  containers:
  - name: app
    image: myapp:latest
    securityContext:
      privileged: true
```

**Expected Feedback:**
- ❌ Critical: Privileged container detected
- ✅ Recommendation: Remove privileged access
- ✅ Example: Use specific capabilities instead

#### Test 4.2: Missing Resource Limits

**Test Code:**
```yaml
apiVersion: v1
kind: Pod
spec:
  containers:
  - name: app
    image: myapp:latest
```

**Expected Feedback:**
- ⚠️ Medium: Missing resource limits
- ✅ Recommendation: Add resource requests and limits
- ✅ Example: Define CPU and memory constraints

#### Test 4.3: Insecure CI/CD Pipeline

**Test Code:**
```yaml
- name: Deploy
  run: |
    echo ${{ secrets.API_KEY }}
    curl -X POST https://api.example.com/deploy
```

**Expected Feedback:**
- ❌ High: Secret exposed in logs
- ✅ Recommendation: Pass secrets as environment variables
- ✅ Example: Use `env:` block, don't echo secrets

### 5. Modern Practices

#### Test 5.1: Missing Error Handling

**Test Code:**
```python
def fetch_data(url):
    response = requests.get(url)
    return response.json()
```

**Expected Feedback:**
- ❌ Medium: Missing error handling
- ✅ Recommendation: Add try-except block
- ✅ Example: Handle network errors, timeouts, invalid JSON

#### Test 5.2: No Test Coverage

**Test Code:**
```python
def calculate_total(items):
    total = 0
    for item in items:
        total += item.price * item.quantity
    return total
```

**Expected Feedback:**
- ⚠️ Low: Missing test coverage
- ✅ Recommendation: Add unit tests
- ✅ Example: Test with empty list, single item, multiple items

#### Test 5.3: Missing Documentation

**Test Code:**
```python
def process(data, opts=None):
    if opts:
        return transform(data, opts)
    return data
```

**Expected Feedback:**
- ⚠️ Low: Missing documentation
- ✅ Recommendation: Add docstring
- ✅ Example: Document parameters, return value, exceptions

## Validation Checklist

When testing the Code Review Expert agent, verify:

- [ ] Identifies all security vulnerabilities correctly
- [ ] Provides specific, actionable recommendations
- [ ] Includes code examples for fixes
- [ ] Prioritizes issues by severity (Critical, High, Medium, Low)
- [ ] Uses constructive and educational tone
- [ ] Explains rationale for recommendations
- [ ] Considers context (language, framework, environment)
- [ ] Balances thoroughness with pragmatism
- [ ] Focuses on production reliability
- [ ] Suggests automation opportunities
- [ ] Provides learning resources when appropriate

## False Positive Tests

### Test FP.1: Intentional Pattern

**Test Code:**
```python
# Safe use of innerHTML with sanitized input
def render_html(content):
    sanitized = bleach.clean(content, tags=['p', 'br'])
    return f"<div>{sanitized}</div>"
```

**Expected Feedback:**
- ✅ Should recognize sanitization is present
- ✅ May suggest using template engine for maintainability
- ✅ Should not flag as XSS vulnerability

### Test FP.2: Framework-Provided Security

**Test Code:**
```python
# Django ORM with parameterization
users = User.objects.filter(username=username)
```

**Expected Feedback:**
- ✅ Should recognize ORM provides SQL injection protection
- ✅ May suggest optimization or best practices
- ✅ Should not flag as SQL injection vulnerability

## Integration Tests

### Test INT.1: Full PR Review

**Scenario:** Review a complete pull request with multiple files

**Expected Behavior:**
- Analyze all changed files
- Provide summary of findings
- Organize feedback by file and severity
- Suggest priority order for fixes
- Estimate impact on production

### Test INT.2: Incremental Review

**Scenario:** Review individual commits in a PR

**Expected Behavior:**
- Track changes across commits
- Identify when issues are introduced
- Recognize when issues are fixed
- Provide commit-level feedback

### Test INT.3: Cross-File Analysis

**Scenario:** Review changes that span multiple files

**Expected Behavior:**
- Identify relationships between changes
- Check for breaking changes
- Verify consistency across files
- Detect missing related updates

## Performance Tests

### Test PERF.1: Large Codebase

**Scenario:** Review PR with 50+ file changes

**Expected Behavior:**
- Complete review in reasonable time (< 5 minutes)
- Prioritize critical issues
- Provide summary before details
- Allow focusing on specific areas

### Test PERF.2: Complex Analysis

**Scenario:** Deep security analysis of authentication system

**Expected Behavior:**
- Thorough analysis without timeout
- Identify subtle security issues
- Provide comprehensive recommendations
- Include relevant security references

## Regression Tests

Maintain a suite of previously reviewed code samples to ensure:
- Consistent feedback across versions
- No degradation in detection accuracy
- Improvements in recommendation quality
- Reduction in false positives over time

## Feedback Loop

After each test cycle:
1. Document actual vs. expected results
2. Identify gaps in detection or recommendations
3. Update agent configuration if needed
4. Add new test cases for edge cases
5. Share learnings with development team

## Test Execution

### Manual Testing

```bash
# Test individual scenarios
@copilot using code-review-expert, review [paste test code]

# Compare against expected feedback
# Document any discrepancies
```

### Automated Testing

```python
# Example test harness
def test_sql_injection_detection():
    code = """
    def get_user(username):
        query = f"SELECT * FROM users WHERE username = '{username}'"
        return database.execute(query)
    """
    
    feedback = invoke_code_review_expert(code)
    
    assert "SQL injection" in feedback
    assert "parameterized queries" in feedback
    assert feedback.severity == "Critical"
```

## Continuous Improvement

- Review test results regularly
- Update test cases as new vulnerabilities emerge
- Incorporate real-world examples from production issues
- Share successful detection stories with the team
- Iterate on agent configuration based on feedback

## Support

For test failures or unexpected behavior:
1. Document the test case and actual output
2. Compare with expected output from this document
3. Check agent configuration for recent changes
4. Report issues in the repository
5. Suggest improvements to test suite
