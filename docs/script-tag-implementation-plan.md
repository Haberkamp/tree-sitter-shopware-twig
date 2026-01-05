# Script Tag Support - Implementation Plan

**Date**: 2026-01-04  
**Based on**: tree-sitter-html script tag implementation

## Overview

Add `<script>` tag parsing to tree-sitter-shopware-twig, following the same three-layer approach already implemented for `<style>` tags. The scanner already has `SCRIPT` in the TagType enum—we just need to wire it up to the grammar.

## Current State Analysis

### Already Implemented
- `SCRIPT` exists in TagType enum (`scanner.c:60`)
- `style_element` fully working with raw text scanning
- `STYLE_START_TAG_NAME` token type (`scanner.c:24`)
- CSS injection query in `queries/injections.scm`

### Missing for Script Tags
- `SCRIPT_START_TAG_NAME` token type
- `scan_raw_text()` only handles STYLE, not SCRIPT (`scanner.c:333`)
- `scan_start_tag_name()` only checks for STYLE (`scanner.c:367-374`)
- No `script_element` or `script_start_tag` grammar rules
- No `$._script_start_tag_name` external
- No JavaScript injection query

## Desired End State

After implementation:
1. `<script>console.log("hi")</script>` parses as `script_element` with `raw_text` content
2. JavaScript injection query enables downstream JS parsing
3. All existing tests continue to pass
4. CI passes on all platforms (Ubuntu, Windows, macOS)

### Quick Verification
```bash
cd /Users/N.Haberkamp/Code/tree-sitter-shopware-twig
tree-sitter generate && tree-sitter test && cargo test
echo '<script>console.log("hi")</script>' | tree-sitter parse --stdin
```

Expected output:
```
(template
  (script_element
    (html_start_tag (html_tag_name))
    (raw_text)
    (html_end_tag (html_tag_name))))
```

## What We're NOT Doing

- Type attribute checking (`type="text/javascript"`, `type="module"`, etc.)
- TypeScript-specific injection based on attributes
- Self-closing script tags (not valid HTML)
- Inline event handlers like `onclick`

---

## Phase 1: Scanner Changes

**File**: `src/scanner.c`

### 1.1 Add SCRIPT_START_TAG_NAME token type

Add after `STYLE_START_TAG_NAME` in the TokenType enum (line 24):

```c
enum TokenType {
  START_TAG_NAME,
  STYLE_START_TAG_NAME,
  SCRIPT_START_TAG_NAME,  // ADD THIS
  END_TAG_NAME,
  ERRONEOUS_END_TAG_NAME,
  SELF_CLOSING_TAG_DELIMITER,
  IMPLICIT_END_TAG,
  RAW_TEXT,
  COMMENT,
};
```

### 1.2 Update scan_raw_text() to handle SCRIPT

Replace the STYLE-only check (line 333) with handling for both SCRIPT and STYLE:

```c
static bool scan_raw_text(Scanner *scanner, TSLexer *lexer) {
  if (scanner->tags.size == 0) return false;

  Tag *tag = array_back(&scanner->tags);
  if (tag->type != STYLE && tag->type != SCRIPT) return false;  // CHANGE THIS LINE

  lexer->mark_end(lexer);
  const char *end_delimiter = tag->type == SCRIPT ? "</SCRIPT" : "</STYLE";  // CHANGE THIS LINE
  unsigned delimiter_index = 0;

  while (lexer->lookahead) {
    if (ascii_toupper(lexer->lookahead) == end_delimiter[delimiter_index]) {
      delimiter_index++;
      if (delimiter_index == strlen(end_delimiter)) break;
      advance(lexer);
    } else {
      delimiter_index = 0;
      advance(lexer);
      lexer->mark_end(lexer);
    }
  }

  lexer->result_symbol = RAW_TEXT;
  return true;
}
```

### 1.3 Update scan_start_tag_name() to return SCRIPT_START_TAG_NAME

Add SCRIPT case to the switch statement (after line 368):

```c
static bool scan_start_tag_name(Scanner *scanner, TSLexer *lexer) {
  Array(char) tag_name = array_new();
  scan_tag_name(lexer, &tag_name);
  if (tag_name.size == 0) {
    array_delete(&tag_name);
    return false;
  }

  Tag tag = tag_for_name(tag_name.contents, tag_name.size);
  array_delete(&tag_name);
  array_push(&scanner->tags, tag);

  switch (tag.type) {
    case SCRIPT:                                    // ADD THIS CASE
      lexer->result_symbol = SCRIPT_START_TAG_NAME;
      break;
    case STYLE:
      lexer->result_symbol = STYLE_START_TAG_NAME;
      break;
    default:
      lexer->result_symbol = START_TAG_NAME;
      break;
  }
  return true;
}
```

### Success Criteria

#### Automated Verification:
- [x] Code compiles without errors: `tree-sitter generate`
- [x] No C compiler warnings in scanner.c

#### Manual Verification:
- [ ] None for this phase (grammar changes needed first)

**Implementation Note**: Phase 1 alone won't produce testable results—proceed to Phase 2 immediately.

---

## Phase 2: Grammar Changes

**File**: `grammar.js`

### 2.1 Add external token

Add `$._script_start_tag_name` to externals array (after `$._style_start_tag_name`, line 19):

```javascript
externals: ($) => [
  $._start_tag_name,
  $._style_start_tag_name,
  $._script_start_tag_name,  // ADD THIS (must match TokenType enum order)
  $._end_tag_name,
  $.erroneous_end_tag_name,
  "/>",
  $._implicit_end_tag,
  $.raw_text,
  $.comment,
],
```

**IMPORTANT**: The order of externals must match the TokenType enum order in scanner.c!

### 2.2 Add script_element to template choices

Update the template rule (line 29-40) to include `$.script_element`:

```javascript
template: ($) =>
  repeat(
    choice(
      $.statement_directive,
      $.html_element,
      $.style_element,
      $.script_element,  // ADD THIS
      $.html_doctype,
      $.html_entity,
      $.content,
      $.erroneous_end_tag
    )
  ),
```

### 2.3 Add script_element and script_start_tag rules

Add after `style_start_tag` rule (line 63):

```javascript
script_element: ($) =>
  seq(
    alias($.script_start_tag, $.html_start_tag),
    optional($.raw_text),
    $.html_end_tag
  ),

script_start_tag: ($) =>
  seq("<", alias($._script_start_tag_name, $.html_tag_name), repeat($.html_attribute), ">"),
```

### Success Criteria

#### Automated Verification:
- [x] Parser generates without conflicts: `tree-sitter generate`
- [x] All existing tests still pass: `tree-sitter test`
- [x] Rust bindings compile and pass: `cargo test`
- [x] Script tag parses correctly:
  ```bash
  echo '<script>alert(1)</script>' | tree-sitter parse --stdin
  ```
- [x] Case-insensitive closing works:
  ```bash
  echo '<script>x</SCRIPT>' | tree-sitter parse --stdin
  echo '<SCRIPT>x</script>' | tree-sitter parse --stdin
  ```

#### Manual Verification:
- [ ] AST structure matches expected format (script_element with raw_text child)
- [ ] Verify partial closing sequences don't break parsing:
  ```bash
  echo '<script></ </s </sc </scr </scri </scrip</script>' | tree-sitter parse --stdin
  ```

**Implementation Note**: After Phase 2, the parser is functionally complete. Pause and run full verification before proceeding.

---

## Phase 3: Injection Query

**File**: `queries/injections.scm`

Append JavaScript injection query:

```scheme
((script_element
  (raw_text) @injection.content)
 (#set! injection.language "javascript"))
```

### Success Criteria

#### Automated Verification:
- [x] File exists and is valid: `tree-sitter query queries/injections.scm test/corpus/html.txt`

#### Manual Verification:
- [ ] Editors with tree-sitter support show JS highlighting inside script tags

---

## Phase 4: Tests

**File**: `test/corpus/html.txt`

Append test cases:

```
==================================
Script elements
==================================
<script>
  console.log("hello");
</script>

<script>
  </ </s </sc </scr </scri </scrip
</script>

---

(template
  (script_element
    (html_start_tag (html_tag_name))
    (raw_text)
    (html_end_tag (html_tag_name)))
  (script_element
    (html_start_tag (html_tag_name))
    (raw_text)
    (html_end_tag (html_tag_name))))

==================================
Script element with attributes
==================================
<script src="app.js" type="module"></script>

---

(template
  (script_element
    (html_start_tag
      (html_tag_name)
      (html_attribute (html_attribute_name) (html_quoted_attribute_value (html_attribute_value)))
      (html_attribute (html_attribute_name) (html_quoted_attribute_value (html_attribute_value))))
    (html_end_tag (html_tag_name))))

==================================
Empty script element
==================================
<script></script>

---

(template
  (script_element
    (html_start_tag (html_tag_name))
    (html_end_tag (html_tag_name))))

==================================
Mixed style and script elements
==================================
<style>body { color: red; }</style>
<script>console.log("test")</script>

---

(template
  (style_element
    (html_start_tag (html_tag_name))
    (raw_text)
    (html_end_tag (html_tag_name)))
  (script_element
    (html_start_tag (html_tag_name))
    (raw_text)
    (html_end_tag (html_tag_name))))
```

### Success Criteria

#### Automated Verification:
- [x] All corpus tests pass: `tree-sitter test`
- [x] Rust binding tests pass: `cargo test`
- [x] Sample file parsing succeeds: `tree-sitter parse test/**/*.txt`

#### Manual Verification:
- [ ] Review test output for expected AST structure

**Implementation Note**: After Phase 4, run the full verification checklist at the end of this document before pushing.

---

## Summary of Changes

| File | Changes |
|------|---------|
| `src/scanner.c:24` | Add `SCRIPT_START_TAG_NAME` to TokenType enum |
| `src/scanner.c:333` | Update `scan_raw_text()` to handle SCRIPT |
| `src/scanner.c:367-374` | Update `scan_start_tag_name()` to return `SCRIPT_START_TAG_NAME` |
| `grammar.js:17-26` | Add `$._script_start_tag_name` to externals |
| `grammar.js:29-40` | Add `$.script_element` to template choices |
| `grammar.js:63+` | Add `script_element` and `script_start_tag` rules |
| `queries/injections.scm` | Add JavaScript injection query |
| `test/corpus/html.txt` | Add script element test cases |

## Build & Test Commands

```bash
cd /Users/N.Haberkamp/Code/tree-sitter-shopware-twig

# Regenerate parser from grammar
tree-sitter generate

# Run corpus tests (test/corpus/*.txt)
tree-sitter test

# Run Rust binding tests (matches CI)
cargo test

# Parse sample files (matches CI parse-action)
tree-sitter parse test/**/*.txt

# Test parsing manually
echo '<script>alert(1)</script>' | tree-sitter parse --stdin

# Interactive playground (optional)
tree-sitter build --wasm && tree-sitter playground
```

---

## Final Verification Checklist

Run these before pushing to ensure CI passes on all platforms (Ubuntu, Windows, macOS):

### Core Tests
```bash
# Must all pass
tree-sitter generate
tree-sitter test
cargo test
```

### Parse Sample Files
```bash
# Should produce no errors
tree-sitter parse test/**/*.txt
```

### Manual Smoke Tests
```bash
# Basic script element
echo '<script>alert(1)</script>' | tree-sitter parse --stdin

# Script with attributes
echo '<script src="app.js" type="module"></script>' | tree-sitter parse --stdin

# Mixed style + script
echo '<style>body{}</style><script>x()</script>' | tree-sitter parse --stdin

# Case-insensitive closing tag
echo '<script>x</SCRIPT>' | tree-sitter parse --stdin
echo '<SCRIPT>x</script>' | tree-sitter parse --stdin
```

### Cross-Platform Considerations

The scanner uses `ascii_toupper()` instead of `towupper()` for case-insensitive matching. This is intentional—`towupper()` from `<wctype.h>` is locale-dependent and behaves differently on Windows vs Linux/macOS (see style-tag-implementation-plan.md for details).

No additional cross-platform testing needed since we're following the same pattern as the working `style_element` implementation.

### CI Pipeline (runs automatically on push)

The GitHub Actions workflow (`.github/workflows/ci.yml`) will:
1. Run `tree-sitter test` on Ubuntu, Windows, and macOS
2. Run Rust binding tests (`cargo test`)
3. Parse all files in `test/**`
4. **Fuzz the scanner** (only if `src/scanner.c` changed)

Since we're modifying `scanner.c`, the fuzzer will run. This is good—it helps catch edge cases.

---

## References

- tree-sitter-html script implementation: `/Users/N.Haberkamp/Code/tree-sitter-html/docs/script-tag-parsing.md`
- Style tag implementation plan: `/Users/N.Haberkamp/Code/tree-sitter-shopware-twig/docs/style-tag-implementation-plan.md`
- tree-sitter-html grammar.js: lines 62-66 (script_element), 81-86 (script_start_tag)
- tree-sitter-html scanner.c: lines 143-169 (scan_raw_text), 233-254 (scan_start_tag_name)

