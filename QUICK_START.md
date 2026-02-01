# Typography Script - Quick Start Guide

## Installation

No installation needed! The script uses Python 3 standard library only.

## Usage

### 1. Preview Changes First (Recommended)
```bash
python3 fix_typography.py --dry-run
```

This shows you what would be changed without modifying any files.

### 2. Apply Changes
```bash
python3 fix_typography.py
```

This will modify files in place within the `website/` directory.

### 3. Review Changes
```bash
git diff
```

Review the changes made by the script.

### 4. Commit or Revert
```bash
# If changes look good
git add .
git commit -m "Fix typography issues"

# If you want to undo
git checkout .
```

## What Gets Fixed

- ✅ Spacing before punctuation: `word ,` → `word,`
- ✅ Spacing after opening brackets: `( word` → `(word`
- ✅ Multiple spaces: `word   word` → `word word`
- ✅ Contractions: `it 's` → `it's`, `don 't` → `don't`
- ✅ Common typos: `recieved` → `received`, `alot` → `a lot`

## What Stays Safe

- ✅ Code in triple backtick blocks
- ✅ Indented code blocks
- ✅ Content in `<code>` and `<pre>` tags
- ✅ Files in `website/blog` and `website/versioned_docs`

## Run Tests

```bash
python3 test_fix_typography.py
```

Should show: **All 9 tests passed!**

## Need More Info?

- **Full documentation**: [TYPOGRAPHY_SCRIPT_README.md](./TYPOGRAPHY_SCRIPT_README.md)
- **Examples**: [EXAMPLE_USAGE.md](./EXAMPLE_USAGE.md)
- **Overview**: [TYPOGRAPHY_FIX_README.md](./TYPOGRAPHY_FIX_README.md)

## Help

```bash
python3 fix_typography.py --help
```
