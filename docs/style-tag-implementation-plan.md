# Style Tag Support - Implementation Plan

## Current State

- `tree-sitter-shopware-twig` has HTML support but no `style_element`
- Scanner already has `SCRIPT` and `STYLE` in TagType enum (line 44-45)
- Missing: raw text scanning, separate token types for style tags, grammar rules

## tree-sitter-html Approach (3 layers)

1. **Grammar**: `style_element` → `style_start_tag` + `raw_text` + `end_tag`
2. **Scanner**: Detects `<style>`, returns special token, scans raw content until `</style>`
3. **Injections**: `queries/injections.scm` declares CSS parser for raw_text

---

## Phase 1: Scanner Changes

**File**: `src/scanner.c`

### 1.1 Add token type (after line 14)

```c
enum TokenType {
  START_TAG_NAME,
  STYLE_START_TAG_NAME,  // NEW
  END_TAG_NAME,
  // ... rest unchanged
  RAW_TEXT,              // already exists but unused
  COMMENT,
};
```

### 1.2 Add raw text scanning (new function before `scan_start_tag_name`)

```c
static bool scan_raw_text(Scanner *scanner, TSLexer *lexer) {
  if (scanner->tags.size == 0) return false;
  
  Tag *tag = array_back(&scanner->tags);
  if (tag->type != STYLE) return false;
  
  lexer->mark_end(lexer);
  const char *end_delimiter = "</STYLE";
  unsigned delimiter_index = 0;
  
  while (lexer->lookahead) {
    if (towupper(lexer->lookahead) == end_delimiter[delimiter_index]) {
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

### 1.3 Update `scan_start_tag_name` (modify existing function ~line 313)

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

### 1.4 Update `scan` function to handle raw_text (modify ~line 362)

```c
static bool scan(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  // ADD at top - raw text check before whitespace skip
  if (valid_symbols[RAW_TEXT] && !valid_symbols[START_TAG_NAME] && !valid_symbols[END_TAG_NAME]) {
    return scan_raw_text(scanner, lexer);
  }
  
  while (iswspace(lexer->lookahead)) {
  // ... rest unchanged
```

### Verification

```bash
cd /Users/N.Haberkamp/Code/tree-sitter-shopware-twig
tree-sitter generate
```

---

## Phase 2: Grammar Changes

**File**: `grammar.js`

### 2.1 Add external token (line 17-25)

```javascript
externals: ($) => [
  $._start_tag_name,
  $._style_start_tag_name,  // NEW
  $._end_tag_name,
  $.erroneous_end_tag_name,
  "/>",
  $._implicit_end_tag,
  $.raw_text,
  $.comment,
],
```

### 2.2 Add style_element rules (after html_element ~line 51)

```javascript
style_element: ($) =>
  seq(
    alias($.style_start_tag, $.html_start_tag),
    optional($.raw_text),
    $.html_end_tag,
  ),

style_start_tag: ($) =>
  seq(
    "<",
    alias($._style_start_tag_name, $.html_tag_name),
    repeat($.html_attribute),
    ">"
  ),
```

### 2.3 Update template rule (line 28-38)

```javascript
template: ($) =>
  repeat(
    choice(
      $.statement_directive,
      $.html_element,
      $.style_element,  // NEW
      $.html_doctype,
      $.html_entity,
      $.content,
      $.erroneous_end_tag
    )
  ),
```

### Verification

```bash
cd /Users/N.Haberkamp/Code/tree-sitter-shopware-twig
tree-sitter generate
tree-sitter test
```

---

## Phase 3: Injection Queries

**File**: `queries/injections.scm` (create new)

```scheme
((style_element
  (raw_text) @injection.content)
 (#set! injection.language "css"))
```

### Verification

```bash
ls /Users/N.Haberkamp/Code/tree-sitter-shopware-twig/queries/injections.scm
```

---

## Phase 4: Tests

**File**: `test/corpus/html.txt` (append)

```
==================================
Raw text elements
==================================
<style>
  body { color: red; }
</style>

<style>
  </ </s </st </sty </styl
</style>

---

(template
  (style_element
    (html_start_tag (html_tag_name))
    (raw_text)
    (html_end_tag (html_tag_name)))
  (style_element
    (html_start_tag (html_tag_name))
    (raw_text)
    (html_end_tag (html_tag_name))))
```

### Verification

```bash
cd /Users/N.Haberkamp/Code/tree-sitter-shopware-twig
tree-sitter test
```

---

## Build Commands Summary

| Command | Purpose |
|---------|---------|
| `tree-sitter generate` | Regenerate `src/parser.c` from grammar |
| `tree-sitter test` | Run corpus tests in `test/corpus/` |
| `tree-sitter parse FILE` | Parse a single file |
| `tree-sitter build --wasm && tree-sitter playground` | Interactive testing |

---

## Issue: Windows CI Failure

### Symptom

The "Ruby annotation elements without close tags" test failed only on Windows CI while passing on macOS and Linux:

```
<ruby>東<rb>京<rt>とう<rt>きょう</ruby>
```

### Root Cause

The scanner used `iswalnum()` and `towupper()` from `<wctype.h>`. These functions are **locale-dependent** and behave differently across platforms:

- **Windows**: Classifies Japanese characters (e.g., `東` U+6771) as alphanumeric per Unicode category (Lo = Letter, other)
- **Linux/macOS** (default "C" locale): Only ASCII characters are alphanumeric

This caused `scan_tag_name()` to incorrectly include `東` as part of a tag name on Windows, breaking the parse tree.

### Fix

Replace locale-dependent functions with ASCII-only equivalents:

```c
static inline bool is_ascii_alphanumeric(int32_t c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

static inline int32_t ascii_toupper(int32_t c) {
  if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
  return c;
}
```

HTML tag names are ASCII-only per spec, so this is both correct and cross-platform consistent.

---

## Out of Scope

- `script_element` (can add same way later)
- Highlight queries
- Self-closing style tags (not valid HTML)

