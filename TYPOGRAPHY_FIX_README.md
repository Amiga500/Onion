# Typography Correction Script

A Python 3 script that automatically fixes typography and spacing issues in English text within Docusaurus documentation files.

## Quick Start

```bash
# Preview changes (recommended first)
python3 fix_typography.py --dry-run

# Apply corrections
python3 fix_typography.py
```

## What It Does

This script performs the following corrections on English text:

✅ Removes space before punctuation (`,`, `;`, `:`, `.`, `!`, `?`, etc.)  
✅ Removes space after opening brackets/parentheses  
✅ Converts multiple spaces to single space  
✅ Fixes contractions (e.g., `it 's` → `it's`, `don 't` → `don't`)  
✅ Corrects common English typos (e.g., `recieved` → `received`, `alot` → `a lot`)

## Safety Features

The script is designed to be safe and conservative:

🛡️ **Preserves code blocks** - Triple backtick blocks, indented code, `<code>` and `<pre>` tags  
🛡️ **Excludes directories** - Automatically skips `website/blog` and `website/versioned_docs`  
🛡️ **Dry-run mode** - Preview changes before applying them  
🛡️ **Comprehensive tests** - Includes test suite to verify correct behavior

## Files

- **`fix_typography.py`** - Main script
- **`test_fix_typography.py`** - Test suite (run with `python3 test_fix_typography.py`)
- **`TYPOGRAPHY_SCRIPT_README.md`** - Detailed documentation
- **`EXAMPLE_USAGE.md`** - Usage examples and before/after comparisons

## Requirements

- Python 3.6 or higher
- No external dependencies (uses only standard library)

## Documentation

For complete documentation, see:
- [TYPOGRAPHY_SCRIPT_README.md](./TYPOGRAPHY_SCRIPT_README.md) - Full technical documentation
- [EXAMPLE_USAGE.md](./EXAMPLE_USAGE.md) - Usage examples and demonstrations

## Example

```bash
# Run with dry-run to see what would be changed
python3 fix_typography.py --dry-run
```

Output:
```
======================================================================
Typography Correction Script for Docusaurus
======================================================================

DRY RUN MODE - No files will be modified

Scanning /path/to/website...
Excluding: blog, versioned_docs
Processing extensions: .css, .html, .js, .json, .jsx, .md, .mdx, .scss, .ts, .tsx, .yaml, .yml

[DRY RUN] website/docs/guide.md (15 changes)
[DRY RUN] website/docs/tutorial.mdx (8 changes)

======================================================================
Summary: 2 files modified, 23 total changes
======================================================================
```

## Testing

Run the test suite to verify the script works correctly:

```bash
python3 test_fix_typography.py
```

All tests should pass with output:
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

## License

Part of the Onion project. See [LICENSE](./LICENSE) for details.
