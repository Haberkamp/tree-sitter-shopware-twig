# Twig Comment Support Implementation Plan

## Overview

Add parsing support for Twig comments `{# ... #}` to the tree-sitter-shopware-twig grammar.

## Current State Analysis

- **No twig comment support** - `{# comment #}` produces parse errors
- **`$.comment` external** handles HTML comments (`<!-- -->`) via `scanner.c:237`, completely unrelated to Twig
- **`content` rule** already excludes `{` character, so twig comments won't be consumed as content
- **Regex approach** is appropriate (no scanner.c changes needed)

### Key Files:
- `grammar.js` - Main grammar definition
- `test/corpus/blocks.txt` - Test cases for twig constructs

## Desired End State

```
Input:  {# @deprecated tag:v6.8.0 - Use mt-button instead #}
Output: (template (twig_comment))
```

Twig comments should work:
1. At template root level
2. Inside HTML elements (e.g., `<div>{# comment #}</div>`)
3. With multiline content
4. With special characters

## What We're NOT Doing

- Modifying `scanner.c` - regex in grammar.js is sufficient
- Adding child nodes to `twig_comment` - it's a leaf node
- Supporting comment extraction/content capture - just the delimited block

---

## Phase 1: Add `twig_comment` Rule

### Overview
Define the twig_comment rule using a regex pattern.

### Changes Required:

#### 1. Add rule to `grammar.js`
**File**: `/Users/N.Haberkamp/Code/tree-sitter-shopware-twig/grammar.js`

Add after `html_entity` rule (around line 140):

```javascript
twig_comment: () => /\{#[\s\S]*?#\}/,
```

The regex breakdown:
- `\{#` — literal opening `{#`
- `[\s\S]*?` — any character including newlines, non-greedy
- `#\}` — literal closing `#}`

### Success Criteria:

#### Automated Verification:
- [x] Grammar generates without errors: `npx tree-sitter generate`

**Implementation Note**: Run `npx tree-sitter generate` after this phase to verify the rule syntax is valid.

---

## Phase 2: Add to `template` Choices

### Overview
Allow twig comments at the template root level.

### Changes Required:

#### 1. Update `template` rule in `grammar.js`
**File**: `/Users/N.Haberkamp/Code/tree-sitter-shopware-twig/grammar.js`
**Location**: Lines 29-40

**Before**:
```javascript
template: ($) =>
  repeat(
    choice(
      $.statement_directive,
      $.html_element,
      $.style_element,
      $.html_doctype,
      $.html_entity,
      $.content,
      $.erroneous_end_tag
    )
  ),
```

**After**:
```javascript
template: ($) =>
  repeat(
    choice(
      $.twig_comment,
      $.statement_directive,
      $.html_element,
      $.style_element,
      $.html_doctype,
      $.html_entity,
      $.content,
      $.erroneous_end_tag
    )
  ),
```

### Success Criteria:

#### Automated Verification:
- [x] Grammar generates: `npx tree-sitter generate`
- [x] Parse standalone comment: `echo '{# test #}' | npx tree-sitter parse --stdin -` shows `(template (twig_comment))`

**Implementation Note**: Verify parsing works at root level before proceeding.

---

## Phase 3: Add to `_node` Choices

### Overview
Allow twig comments inside HTML elements.

### Changes Required:

#### 1. Update `_node` rule in `grammar.js`
**File**: `/Users/N.Haberkamp/Code/tree-sitter-shopware-twig/grammar.js`
**Location**: Line 67

**Before**:
```javascript
_node: ($) => choice($.html_element, $.html_entity, $.content),
```

**After**:
```javascript
_node: ($) => choice($.twig_comment, $.html_element, $.html_entity, $.content),
```

### Success Criteria:

#### Automated Verification:
- [x] Grammar generates: `npx tree-sitter generate`
- [x] Parse comment inside element: `echo '<div>{# test #}</div>' | npx tree-sitter parse --stdin -` shows twig_comment as child of html_element

**Implementation Note**: Verify nested parsing before adding tests.

---

## Phase 4: Add Test Cases

### Overview
Add comprehensive test cases to `test/corpus/blocks.txt`.

### Changes Required:

#### 1. Append tests to `test/corpus/blocks.txt`
**File**: `/Users/N.Haberkamp/Code/tree-sitter-shopware-twig/test/corpus/blocks.txt`

Append at end of file:

```
===
Twig comment
===

{# This is a comment #}

---

(template
  (twig_comment))

===
Twig multiline comment
===

{# This is a
   multiline comment #}

---

(template
  (twig_comment))

===
Twig comment inside element
===

<div>{# comment #}</div>

---

(template
  (html_element
    (html_start_tag (html_tag_name))
    (twig_comment)
    (html_end_tag (html_tag_name))))

===
Twig comment with special chars
===

{# @deprecated tag:v6.8.0 - Use `mt-button` instead. #}

---

(template
  (twig_comment))

===
Twig comment before block
===

{# Comment #}
{% block foo %}{% endblock %}

---

(template
  (twig_comment)
  (statement_directive
    (tag_statement
      (tag)
      (variable)))
  (statement_directive
    (tag_statement
      (tag))))

===
Multiple twig comments on same line
===

{# first #}{# second #}

---

(template
  (twig_comment)
  (twig_comment))

===
Twig comment inline with text
===

<p>You can also comment out {# part of a line #}.</p>

---

(template
  (html_element
    (html_start_tag (html_tag_name))
    (content)
    (twig_comment)
    (html_end_tag (html_tag_name))))

===
Twig comment containing twig code
===

{# The following code will not be executed
{% if category.posts %}
  This category has posts
{% endif %}
#}

---

(template
  (twig_comment))

===
Empty twig comment
===

{##}

---

(template
  (twig_comment))

===
Twig comment with hash inside
===

{# This has a # hash inside #}

---

(template
  (twig_comment))
```

### Success Criteria:

#### Automated Verification:
- [x] All tests pass: `npx tree-sitter test`
- [x] No regressions in existing tests

---

## Final Verification Checklist

After all phases complete, verify these parse correctly:

```bash
# Simple comment
echo '{# simple #}' | npx tree-sitter parse --stdin -

# Multiline
echo -e '{# multi\nline #}' | npx tree-sitter parse --stdin -

# With special chars
echo '{# @deprecated tag:v6.8.0 #}' | npx tree-sitter parse --stdin -

# Inside element
echo '<p>{# inline #}text</p>' | npx tree-sitter parse --stdin -

# With block
echo '{% block x %}{# inside #}{% endblock %}' | npx tree-sitter parse --stdin -
```

All should show `(twig_comment)` in the output without errors.

---

## References

- Task specification: `docs/TASK-twig-comments.md`
- Main grammar: `grammar.js`
- Test corpus: `test/corpus/blocks.txt`

