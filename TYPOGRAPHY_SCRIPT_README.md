# Typography Correction Script

## Overview

This Python 3 script performs typography and spacing corrections on English text in Docusaurus documentation files. It's designed to automatically fix common spacing issues and typos while being careful not to modify code blocks.

## Features

### File Processing
- Recursively processes files in the `website` directory
- Automatically excludes `website/blog` and `website/versioned_docs` directories
- Processes only relevant file types:
  - Documentation: `.md`, `.mdx`
  - Code: `.js`, `.jsx`, `.ts`, `.tsx`
  - Styling: `.css`, `.scss`, `.html`
  - Configuration: `.json`, `.yaml`, `.yml`

### Typography Corrections

The script applies the following corrections to **English text only**:

1. **Remove space before punctuation**: `,`, `;`, `:`, `.`, `!`, `?`, `)`, `]`, `}`, `"`, `'`
   - Example: `word ,` → `word,`

2. **Remove space after opening**: `(`, `[`, `{`, `"`, `'`, `«`
   - Example: `( word` → `(word`

3. **Convert multiple spaces to single space**
   - Example: `word   word` → `word word`

4. **Remove space before apostrophe in contractions**
   - Example: `it 's` → `it's`, `don 't` → `don't`, `can 't` → `can't`

5. **Fix common English typos** (only very safe, high-probability errors):
   - `recieved` → `received`
   - `occured` → `occurred`
   - `accomodate` → `accommodate`
   - `seperate` → `separate`
   - `definately` → `definitely`
   - `wierd` → `weird`
   - `neccessary` → `necessary`
   - `sucess` → `success`
   - `acheive` → `achieve`
   - `inbetween` → `in between`
   - `alot` → `a lot`
   - `could of` → `could have`
   - `should of` → `should have`
   - `would of` → `would have`

### Safety Features

The script **will NOT modify**:
- Code inside triple backtick blocks: ` ```code``` `
- Indented code blocks (4+ spaces at line start)
- Content inside `<code>` and `<pre>` HTML tags
- Any text that appears to be code, URLs, or technical identifiers

## Usage

### Basic Usage

Run from the repository root:

```bash
python3 fix_typography.py
```

### Specify a Different Starting Directory

```bash
python3 fix_typography.py /path/to/project
```

### Dry Run (Preview Changes)

To see what would be changed without modifying files:

```bash
python3 fix_typography.py --dry-run
```

### Command-Line Options

```
positional arguments:
  path        Starting directory (default: current directory)

optional arguments:
  -h, --help  Show help message and exit
  --dry-run   Show what would be changed without modifying files
```

## Output

The script provides detailed output:

```
======================================================================
Typography Correction Script for Docusaurus
======================================================================

Scanning /path/to/website...
Excluding: versioned_docs, blog
Processing extensions: .css, .html, .js, .json, .jsx, .md, .mdx, .scss, .ts, .tsx, .yaml, .yml

✓ website/docs/example.md (15 changes)
✓ website/docs/guide/tutorial.mdx (8 changes)

======================================================================
Summary: 2 files modified, 23 total changes
======================================================================
```

## Requirements

- Python 3.6 or higher
- No external dependencies (uses only Python standard library)

## Example Transformations

### Before
```markdown
This is a test document with issues .

Here are examples :
- Space before comma ,  semicolon ;  and period .
- Contractions : it 's , don 't , can 't .
- Common typos : recieved , occured , definately .
```

### After
```markdown
This is a test document with issues.

Here are examples:
- Space before comma, semicolon; and period.
- Contractions: it's, don't, can't.
- Common typos: received, occurred, definitely.
```

### Code Blocks Preserved
```markdown
```javascript
// This will NOT be changed
const test = "value" ;
let x = [ 1 , 2 , 3 ] ;
```
```

## Notes

- The script modifies files in place. Consider using version control or backups.
- The dry-run mode is recommended for first-time usage to preview changes.
- The script is conservative and only fixes high-confidence issues.
- It's designed specifically for English text in Docusaurus documentation.
