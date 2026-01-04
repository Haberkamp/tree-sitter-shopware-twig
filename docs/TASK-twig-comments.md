# Task: Add Twig Comment Support to tree-sitter-shopware-twig

## Objective

Add parsing support for Twig comments `{# ... #}` to the tree-sitter grammar.

## Current State

- Twig comments are **not supported**
- Input `{# comment #}` produces parse errors
- The existing `$.comment` external handles HTML comments (`<!-- -->`), not Twig

## Expected Behavior

```
Input:  {# @deprecated tag:v6.8.0 - Use mt-button instead #}
Output: (template (twig_comment))
```

```
Input:  {# Multi
        line
        comment #}
Output: (template (twig_comment))
```

```
Input:  <div>{# inline #}</div>
Output: (template (html_element ... (twig_comment) ...))
```

---

## Files to Modify

### 1. `grammar.js`

**Location:** `/Users/N.Haberkamp/Code/tree-sitter-shopware-twig/grammar.js`

#### Change 1: Add the `twig_comment` rule

Add this new rule (after `html_entity` around line 140):

```javascript
twig_comment: () => /\{#[\s\S]*?#\}/,
```

The regex:
- `\{#` — literal opening `{#`
- `[\s\S]*?` — any character including newlines, non-greedy
- `#\}` — literal closing `#}`

#### Change 2: Add to `template` choices

Current (lines 29-40):
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

Change to:
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

#### Change 3: Add to `_node` choices

Current (line 67):
```javascript
_node: ($) => choice($.html_element, $.html_entity, $.content),
```

Change to:
```javascript
_node: ($) => choice($.twig_comment, $.html_element, $.html_entity, $.content),
```

This allows comments inside HTML elements like `<div>{# comment #}</div>`.

---

## Build & Test

After making changes:

```bash
cd /Users/N.Haberkamp/Code/tree-sitter-shopware-twig
npx tree-sitter generate
npx tree-sitter test
```

---

## Add Test Cases

**File:** `test/corpus/blocks.txt`

Add these test cases:

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
```

---

## Verification

After implementation, these should all parse correctly:

```twig
{# simple #}
{# multi
   line #}
{# @deprecated tag:v6.8.0 #}
<p>{# inline #}text</p>
{% block x %}{# inside block #}{% endblock %}
```

---

## Notes

- Do NOT modify `scanner.c` — the regex approach in grammar.js is sufficient
- The existing `$.comment` external is for HTML comments and is unrelated
- The `twig_comment` node will be a leaf node (no children)
- Make sure the regex is non-greedy (`*?`) to handle multiple comments on same line

