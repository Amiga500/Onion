#!/usr/bin/env python3
"""
Test suite for fix_typography.py

This script tests the typography correction functionality.
"""

import sys
import os

# Add parent directory to path to import fix_typography
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from fix_typography import apply_typography_fixes, is_in_code_block


def test_spacing_before_punctuation():
    """Test removing space before punctuation."""
    text = "Hello world , this is a test ."
    fixed, count = apply_typography_fixes(text)
    assert fixed == "Hello world, this is a test.", f"Expected 'Hello world, this is a test.' but got '{fixed}'"
    assert count == 2, f"Expected 2 changes but got {count}"
    print("✓ test_spacing_before_punctuation passed")


def test_spacing_after_opening():
    """Test removing space after opening brackets."""
    text = "This is ( a test ) with brackets."
    fixed, count = apply_typography_fixes(text)
    assert fixed == "This is (a test) with brackets.", f"Expected 'This is (a test) with brackets.' but got '{fixed}'"
    print("✓ test_spacing_after_opening passed")


def test_multiple_spaces():
    """Test converting multiple spaces to single space."""
    text = "Hello  world   with    spaces."
    fixed, count = apply_typography_fixes(text)
    assert fixed == "Hello world with spaces.", f"Expected 'Hello world with spaces.' but got '{fixed}'"
    print("✓ test_multiple_spaces passed")


def test_contractions():
    """Test fixing contractions with spaces."""
    text = "It 's a test , don 't you think ? Can 't wait !"
    fixed, count = apply_typography_fixes(text)
    assert "it's" in fixed.lower(), f"Expected 'it's' in '{fixed}'"
    assert "don't" in fixed.lower(), f"Expected 'don't' in '{fixed}'"
    assert "can't" in fixed.lower(), f"Expected 'can't' in '{fixed}'"
    print("✓ test_contractions passed")


def test_common_typos():
    """Test fixing common typos."""
    test_cases = [
        ("The message was recieved", "received"),
        ("This occured yesterday", "occurred"),
        ("That is definately true", "definitely"),
        ("This is alot of work", "a lot"),
        ("I could of done it", "could have"),
    ]
    
    for original, expected_word in test_cases:
        fixed, count = apply_typography_fixes(original)
        assert expected_word in fixed, f"Expected '{expected_word}' in '{fixed}' from '{original}'"
    
    print("✓ test_common_typos passed")


def test_code_block_preservation():
    """Test that code blocks are not modified."""
    text = """
Some text before .

```javascript
const test = "value" ;
let x = [ 1 , 2 , 3 ] ;
```

Some text after .
"""
    fixed, count = apply_typography_fixes(text)
    
    # Check that text outside code blocks was fixed
    assert "before." in fixed, f"Expected 'before.' in fixed text"
    assert "after." in fixed, f"Expected 'after.' in fixed text"
    
    # Check that code inside blocks was preserved
    assert '"value" ;' in fixed, f"Expected code to be preserved but got: {fixed}"
    assert '[ 1 , 2 , 3 ]' in fixed, f"Expected array spacing to be preserved but got: {fixed}"
    
    print("✓ test_code_block_preservation passed")


def test_indented_code_preservation():
    """Test that indented code blocks are not modified."""
    text = """
Normal text .

    # This is indented code
    value = [ 1 , 2 , 3 ]

More normal text .
"""
    fixed, count = apply_typography_fixes(text)
    
    # Check that regular text was fixed
    assert "Normal text." in fixed
    assert "More normal text." in fixed
    
    # Check that indented code was preserved
    assert '[ 1 , 2 , 3 ]' in fixed, f"Expected indented code to be preserved"
    
    print("✓ test_indented_code_preservation passed")


def test_html_code_tag_preservation():
    """Test that HTML code tags are preserved."""
    text = "This is <code>inline code : test</code> with text ."
    fixed, count = apply_typography_fixes(text)
    
    # Check that text outside was fixed
    assert "text." in fixed
    
    # Check that code inside tags was preserved
    assert "code : test" in fixed, f"Expected code tag content preserved but got: {fixed}"
    
    print("✓ test_html_code_tag_preservation passed")


def test_is_in_code_block():
    """Test the is_in_code_block function."""
    text = """
Normal text.
```python
code here
```
More normal text.
"""
    
    # Find position of "Normal text"
    pos_normal = text.find("Normal text")
    assert not is_in_code_block(text, pos_normal), "Normal text should not be in code block"
    
    # Find position inside code block
    pos_code = text.find("code here")
    assert is_in_code_block(text, pos_code), "Code should be detected as in code block"
    
    # Find position of "More normal text"
    pos_after = text.find("More normal text")
    assert not is_in_code_block(text, pos_after), "Text after code block should not be in code block"
    
    print("✓ test_is_in_code_block passed")


def run_all_tests():
    """Run all tests."""
    print("=" * 70)
    print("Running Typography Correction Tests")
    print("=" * 70)
    print()
    
    tests = [
        test_spacing_before_punctuation,
        test_spacing_after_opening,
        test_multiple_spaces,
        test_contractions,
        test_common_typos,
        test_code_block_preservation,
        test_indented_code_preservation,
        test_html_code_tag_preservation,
        test_is_in_code_block,
    ]
    
    failed = 0
    for test in tests:
        try:
            test()
        except AssertionError as e:
            print(f"✗ {test.__name__} failed: {e}")
            failed += 1
        except Exception as e:
            print(f"✗ {test.__name__} error: {e}")
            failed += 1
    
    print()
    print("=" * 70)
    if failed == 0:
        print(f"All {len(tests)} tests passed!")
        return 0
    else:
        print(f"{failed} out of {len(tests)} tests failed.")
        return 1


if __name__ == '__main__':
    sys.exit(run_all_tests())
