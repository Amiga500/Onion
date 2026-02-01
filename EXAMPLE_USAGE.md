# Typography Script - Usage Examples

## Quick Start

### Preview changes without modifying files (recommended first step)
```bash
python3 fix_typography.py --dry-run
```

### Apply corrections to the website directory
```bash
python3 fix_typography.py
```

### Apply corrections to a specific directory
```bash
python3 fix_typography.py /path/to/project
```

## Example Output

```
======================================================================
Typography Correction Script for Docusaurus
======================================================================

Scanning /home/user/project/website...
Excluding: blog, versioned_docs
Processing extensions: .css, .html, .js, .json, .jsx, .md, .mdx, .scss, .ts, .tsx, .yaml, .yml

✓ website/docs/installation/guide.md (12 changes)
✓ website/docs/features/overview.mdx (8 changes)
✓ website/src/components/Header.jsx (3 changes)

======================================================================
Summary: 3 files modified, 23 total changes
======================================================================
```

## What Gets Fixed

### Before and After Examples

#### Example 1: Spacing Issues
**Before:**
```markdown
This is a test document with issues .

Here are examples :
- Space before comma ,  semicolon ;  and period .
- Space after opening ( parenthesis ) and brackets [ test ] .
```

**After:**
```markdown
This is a test document with issues.

Here are examples:
- Space before comma, semicolon; and period.
- Space after opening (parenthesis) and brackets [test].
```

#### Example 2: Contractions
**Before:**
```markdown
It 's important that we don 't forget . Can 't wait !
```

**After:**
```markdown
It's important that we don't forget. Can't wait!
```

#### Example 3: Common Typos
**Before:**
```markdown
The message was recieved yesterday. This occured after we
definately decided to proceed. It was alot of work, but we
could of done more.
```

**After:**
```markdown
The message was received yesterday. This occurred after we
definitely decided to proceed. It was a lot of work, but we
could have done more.
```

#### Example 4: Code Blocks Are Preserved
**Before:**
```markdown
Some normal text .

```javascript
// Code with intentional spacing
const test = "value" ;
let x = [ 1 , 2 , 3 ] ;
```

More text .
```

**After:**
```markdown
Some normal text.

```javascript
// Code with intentional spacing
const test = "value" ;
let x = [ 1 , 2 , 3 ] ;
```

More text.
```

Note: The text before and after the code block is fixed, but the code itself is preserved exactly as written.

## Safety Features

The script will NOT modify:
- Code inside triple backtick blocks (` ```code``` `)
- Indented code blocks (4+ spaces at line start)
- Content inside `<code>` tags
- Content inside `<pre>` tags
- Anything that appears to be code, URLs, or technical identifiers

## Tips

1. **Always use dry-run first**: Preview changes before applying them
   ```bash
   python3 fix_typography.py --dry-run
   ```

2. **Use version control**: Commit your work before running the script so you can review or revert changes

3. **Review the output**: Check the list of modified files to ensure only expected files were changed

4. **Check specific files**: If you're unsure about changes, examine specific files using git diff:
   ```bash
   git diff website/docs/your-file.md
   ```

## Excluded Directories

The script automatically excludes:
- `website/blog` - Blog posts (often archived or versioned separately)
- `website/versioned_docs` - Versioned documentation (should not be modified)

All other directories within `website/` are processed recursively.

## Supported File Types

The script processes these file extensions:
- **Documentation**: `.md`, `.mdx`
- **JavaScript/TypeScript**: `.js`, `.jsx`, `.ts`, `.tsx`
- **Styling**: `.css`, `.scss`, `.html`
- **Configuration**: `.json`, `.yaml`, `.yml`

## Running Tests

To verify the script is working correctly:

```bash
python3 test_fix_typography.py
```

Expected output:
```
======================================================================
Running Typography Correction Tests
======================================================================

✓ test_spacing_before_punctuation passed
✓ test_spacing_after_opening passed
✓ test_multiple_spaces passed
✓ test_contractions passed
✓ test_common_typos passed
✓ test_code_block_preservation passed
✓ test_indented_code_preservation passed
✓ test_html_code_tag_preservation passed
✓ test_is_in_code_block passed

======================================================================
All 9 tests passed!
```
