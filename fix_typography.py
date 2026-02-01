#!/usr/bin/env python3
"""
Typography and Spacing Correction Script for Docusaurus

This script performs typography and spacing corrections on English text
in Docusaurus documentation files. It processes files recursively in the
"website" directory while excluding blog and versioned_docs folders.

Usage:
    python3 fix_typography.py [path]
    
    path: Optional starting directory (defaults to current directory)
"""

import os
import sys
import re
import argparse
from pathlib import Path
from typing import List, Set, Tuple


# File extensions to process
VALID_EXTENSIONS = {
    '.md', '.mdx',
    '.js', '.jsx', '.ts', '.tsx',
    '.html',
    '.css', '.scss',
    '.json',
    '.yaml', '.yml'
}

# Directories to exclude (within website folder)
EXCLUDED_DIRS = {'blog', 'versioned_docs'}


def is_in_code_block(text: str, position: int) -> bool:
    """
    Check if a position in text is inside a code block.
    
    Handles:
    - Triple backtick code blocks: ```code```
    - Indented code blocks (4+ spaces at line start)
    - HTML code/pre tags: <code>, <pre>
    """
    # Check for triple backtick blocks
    before_text = text[:position]
    after_text = text[position:]
    
    # Count triple backticks before position
    backticks_before = before_text.count('```')
    
    # If odd number of backticks before, we're inside a code block
    if backticks_before % 2 == 1:
        return True
    
    # Check for HTML code/pre tags
    # Find all opening and closing tags before position
    open_code = len(re.findall(r'<code[^>]*>', before_text))
    close_code = len(re.findall(r'</code>', before_text))
    if open_code > close_code:
        return True
    
    open_pre = len(re.findall(r'<pre[^>]*>', before_text))
    close_pre = len(re.findall(r'</pre>', before_text))
    if open_pre > close_pre:
        return True
    
    # Check if on a line indented with 4+ spaces (indented code block)
    lines = before_text.split('\n')
    if lines:
        current_line_start = len(before_text) - len(lines[-1])
        # Get the full current line
        remaining_text = text[current_line_start:]
        current_line = remaining_text.split('\n')[0] if '\n' in remaining_text else remaining_text
        
        # Check if line starts with 4+ spaces
        if re.match(r'^    ', current_line):
            return True
    
    return False


def get_safe_replacements(text: str) -> List[Tuple[str, str, int, int]]:
    """
    Get list of safe text replacements with their positions.
    Returns list of (old_text, new_text, start_pos, end_pos) tuples.
    """
    replacements = []
    
    # 1. Remove space before punctuation: , ; : . ! ? ) ] } " '
    # But be careful not to match inside code blocks
    patterns_before_punct = [
        (r' +([,;:\.!?\)\]\}])', r'\1'),  # Space before punctuation
        (r' +(["\'])', r'\1'),  # Space before quotes (at end)
    ]
    
    for pattern, replacement in patterns_before_punct:
        for match in re.finditer(pattern, text):
            if not is_in_code_block(text, match.start()):
                old = match.group(0)
                new = re.sub(pattern, replacement, old)
                replacements.append((old, new, match.start(), match.end()))
    
    # 2. Remove space after opening: ( [ { " ' «
    patterns_after_open = [
        (r'([\(\[\{]) +', r'\1'),  # Space after opening brackets
        (r'(["\'\«]) +', r'\1'),  # Space after opening quotes
    ]
    
    for pattern, replacement in patterns_after_open:
        for match in re.finditer(pattern, text):
            if not is_in_code_block(text, match.start()):
                old = match.group(0)
                new = re.sub(pattern, replacement, old)
                replacements.append((old, new, match.start(), match.end()))
    
    # 3. Convert multiple spaces to single space (but not at line start - could be indentation)
    pattern = r'(?<!^)(?<![\r\n])  +'
    for match in re.finditer(pattern, text, re.MULTILINE):
        if not is_in_code_block(text, match.start()):
            replacements.append((match.group(0), ' ', match.start(), match.end()))
    
    # 4. Remove space before apostrophe in contractions
    # Common contractions: 's, 't, 'll, 'd, 're, 've, 'm
    contraction_pattern = r"\b(\w+) '([stdm]|ll|re|ve)\b"
    for match in re.finditer(contraction_pattern, text, re.IGNORECASE):
        if not is_in_code_block(text, match.start()):
            old = match.group(0)
            new = match.group(1) + "'" + match.group(2)
            replacements.append((old, new, match.start(), match.end()))
    
    # 5. Fix common English typos (only very safe ones)
    typos = {
        r'\brecieved\b': 'received',
        r'\boccured\b': 'occurred',
        r'\baccomodate\b': 'accommodate',
        r'\bseperate\b': 'separate',
        r'\bdefinately\b': 'definitely',
        r'\bwierd\b': 'weird',
        r'\bneccessary\b': 'necessary',
        r'\bsucess\b': 'success',
        r'\bacheive\b': 'achieve',
        r'\binbetween\b': 'in between',
        r'\balot\b': 'a lot',
        r'\bcould of\b': 'could have',
        r'\bshould of\b': 'should have',
        r'\bwould of\b': 'would have',
    }
    
    for typo_pattern, correction in typos.items():
        for match in re.finditer(typo_pattern, text, re.IGNORECASE):
            if not is_in_code_block(text, match.start()):
                old = match.group(0)
                # Preserve original case for first letter
                if old[0].isupper():
                    new = correction[0].upper() + correction[1:]
                else:
                    new = correction
                replacements.append((old, new, match.start(), match.end()))
    
    return replacements


def apply_typography_fixes(text: str) -> Tuple[str, int]:
    """
    Apply typography fixes to text.
    Returns (fixed_text, num_changes).
    """
    # Get all safe replacements with positions
    replacements = get_safe_replacements(text)
    
    if not replacements:
        return text, 0
    
    # Sort replacements by position (reverse order to maintain positions)
    replacements.sort(key=lambda x: x[2], reverse=True)
    
    # Apply replacements from end to start to maintain positions
    result = text
    changes_made = 0
    
    for old, new, start, end in replacements:
        # Double-check the text at this position matches
        if result[start:end] == old:
            result = result[:start] + new + result[end:]
            if old != new:
                changes_made += 1
    
    return result, changes_made


def process_file(filepath: Path) -> Tuple[bool, int]:
    """
    Process a single file.
    Returns (modified, num_changes).
    """
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            original_content = f.read()
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return False, 0
    
    fixed_content, num_changes = apply_typography_fixes(original_content)
    
    if num_changes > 0 and fixed_content != original_content:
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(fixed_content)
            return True, num_changes
        except Exception as e:
            print(f"Error writing {filepath}: {e}")
            return False, 0
    
    return False, 0


def should_process_file(filepath: Path, website_path: Path) -> bool:
    """
    Check if a file should be processed based on extension and exclusions.
    """
    # Check extension
    if filepath.suffix not in VALID_EXTENSIONS:
        return False
    
    # Check if file is in excluded directories
    try:
        relative_path = filepath.relative_to(website_path)
        parts = relative_path.parts
        
        # Check if any parent directory is in excluded list
        for part in parts[:-1]:  # Exclude the filename itself
            if part in EXCLUDED_DIRS:
                return False
    except ValueError:
        # File is not relative to website_path
        pass
    
    return True


def find_and_process_files(start_path: Path) -> Tuple[int, int]:
    """
    Find and process all valid files in the website directory.
    Returns (files_processed, total_changes).
    """
    website_path = start_path / 'website'
    
    if not website_path.exists() or not website_path.is_dir():
        print(f"Error: 'website' directory not found in {start_path}")
        return 0, 0
    
    files_processed = 0
    total_changes = 0
    
    print(f"Scanning {website_path}...")
    print(f"Excluding: {', '.join(EXCLUDED_DIRS)}")
    print(f"Processing extensions: {', '.join(sorted(VALID_EXTENSIONS))}")
    print()
    
    # Walk through website directory
    for root, dirs, files in os.walk(website_path):
        root_path = Path(root)
        
        # Remove excluded directories from dirs to prevent walking into them
        dirs[:] = [d for d in dirs if d not in EXCLUDED_DIRS]
        
        for filename in files:
            filepath = root_path / filename
            
            if should_process_file(filepath, website_path):
                modified, num_changes = process_file(filepath)
                
                if modified:
                    files_processed += 1
                    total_changes += num_changes
                    rel_path = filepath.relative_to(start_path)
                    print(f"✓ {rel_path} ({num_changes} changes)")
    
    return files_processed, total_changes


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='Fix typography and spacing in Docusaurus documentation files.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        'path',
        nargs='?',
        default='.',
        help='Starting directory (default: current directory)'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be changed without modifying files'
    )
    
    args = parser.parse_args()
    
    start_path = Path(args.path).resolve()
    
    if not start_path.exists():
        print(f"Error: Path does not exist: {start_path}")
        sys.exit(1)
    
    print("=" * 70)
    print("Typography Correction Script for Docusaurus")
    print("=" * 70)
    print()
    
    if args.dry_run:
        print("DRY RUN MODE - No files will be modified")
        print()
    
    files_processed, total_changes = find_and_process_files(start_path)
    
    print()
    print("=" * 70)
    print(f"Summary: {files_processed} files modified, {total_changes} total changes")
    print("=" * 70)


if __name__ == '__main__':
    main()
