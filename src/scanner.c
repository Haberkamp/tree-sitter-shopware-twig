#include "tree_sitter/parser.h"
#include "tree_sitter/alloc.h"
#include "tree_sitter/array.h"
#include <wctype.h>
#include <string.h>

// ASCII-only character functions for cross-platform consistency
// (iswalnum/towupper are locale-dependent and behave differently on Windows)
static inline bool is_ascii_alphanumeric(int32_t c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

static inline int32_t ascii_toupper(int32_t c) {
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 'A';
  }
  return c;
}

enum TokenType {
  START_TAG_NAME,
  STYLE_START_TAG_NAME,
  END_TAG_NAME,
  ERRONEOUS_END_TAG_NAME,
  SELF_CLOSING_TAG_DELIMITER,
  IMPLICIT_END_TAG,
  RAW_TEXT,
  COMMENT,
};

typedef enum {
  CUSTOM,
  HTML,
  HEAD,
  BODY,
  TABLE,
  TBODY,
  THEAD,
  TFOOT,
  TR,
  TD,
  TH,
  UL,
  OL,
  LI,
  DL,
  DT,
  DD,
  P,
  DIV,
  SPAN,
  H1,
  H2,
  H3,
  H4,
  H5,
  H6,
  SCRIPT,
  STYLE,
  COLGROUP,
  COL,
  RB,
  RT,
  RP,
  RUBY,
} TagType;

typedef struct {
  TagType type;
  Array(char) custom_tag_name;
} Tag;

typedef struct {
  Array(Tag) tags;
} Scanner;

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }
static inline void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

static Tag tag_new() {
  Tag tag;
  tag.type = CUSTOM;
  array_init(&tag.custom_tag_name);
  return tag;
}

static void tag_free(Tag *tag) {
  if (tag->type == CUSTOM) {
    array_delete(&tag->custom_tag_name);
  }
}

static bool tag_eq(const Tag *tag1, const Tag *tag2) {
  if (tag1->type != tag2->type) return false;
  if (tag1->type == CUSTOM) {
    if (tag1->custom_tag_name.size != tag2->custom_tag_name.size) return false;
    return memcmp(tag1->custom_tag_name.contents, tag2->custom_tag_name.contents, tag1->custom_tag_name.size) == 0;
  }
  return true;
}

static TagType tag_type_for_name(const char *name, size_t len) {
  if (len == 2) {
    if (memcmp(name, "TD", 2) == 0) return TD;
    if (memcmp(name, "TH", 2) == 0) return TH;
    if (memcmp(name, "TR", 2) == 0) return TR;
    if (memcmp(name, "LI", 2) == 0) return LI;
    if (memcmp(name, "DT", 2) == 0) return DT;
    if (memcmp(name, "DD", 2) == 0) return DD;
    if (memcmp(name, "RB", 2) == 0) return RB;
    if (memcmp(name, "RT", 2) == 0) return RT;
    if (memcmp(name, "RP", 2) == 0) return RP;
    if (memcmp(name, "H1", 2) == 0) return H1;
    if (memcmp(name, "H2", 2) == 0) return H2;
    if (memcmp(name, "H3", 2) == 0) return H3;
    if (memcmp(name, "H4", 2) == 0) return H4;
    if (memcmp(name, "H5", 2) == 0) return H5;
    if (memcmp(name, "H6", 2) == 0) return H6;
  } else if (len == 1) {
    if (memcmp(name, "P", 1) == 0) return P;
  } else if (len == 3) {
    if (memcmp(name, "COL", 3) == 0) return COL;
    if (memcmp(name, "DIV", 3) == 0) return DIV;
  } else if (len == 4) {
    if (memcmp(name, "HTML", 4) == 0) return HTML;
    if (memcmp(name, "HEAD", 4) == 0) return HEAD;
    if (memcmp(name, "BODY", 4) == 0) return BODY;
    if (memcmp(name, "RUBY", 4) == 0) return RUBY;
  } else if (len == 5) {
    if (memcmp(name, "TABLE", 5) == 0) return TABLE;
    if (memcmp(name, "TBODY", 5) == 0) return TBODY;
    if (memcmp(name, "THEAD", 5) == 0) return THEAD;
    if (memcmp(name, "TFOOT", 5) == 0) return TFOOT;
    if (memcmp(name, "STYLE", 5) == 0) return STYLE;
  } else if (len == 6) {
    if (memcmp(name, "SCRIPT", 6) == 0) return SCRIPT;
  } else if (len == 8) {
    if (memcmp(name, "COLGROUP", 8) == 0) return COLGROUP;
  }
  return CUSTOM;
}

static Tag tag_for_name(const char *name, size_t len) {
  Tag tag = tag_new();
  tag.type = tag_type_for_name(name, len);
  if (tag.type == CUSTOM) {
    array_reserve(&tag.custom_tag_name, len);
    tag.custom_tag_name.size = len;
    memcpy(tag.custom_tag_name.contents, name, len);
  }
  return tag;
}

static bool tag_is_void(const Tag *tag) {
  switch (tag->type) {
    case COL:
      return true;
    default:
      // Check for common void elements by name
      if (tag->type == CUSTOM && tag->custom_tag_name.size > 0) {
        const char *name = tag->custom_tag_name.contents;
        size_t len = tag->custom_tag_name.size;
        
        // Common void elements (case-insensitive check)
        if ((len == 3 && (memcmp(name, "IMG", 3) == 0 || memcmp(name, "img", 3) == 0)) ||
            (len == 2 && (memcmp(name, "BR", 2) == 0 || memcmp(name, "br", 2) == 0)) ||
            (len == 5 && (memcmp(name, "INPUT", 5) == 0 || memcmp(name, "input", 5) == 0)) ||
            (len == 4 && (memcmp(name, "AREA", 4) == 0 || memcmp(name, "area", 4) == 0)) ||
            (len == 4 && (memcmp(name, "BASE", 4) == 0 || memcmp(name, "base", 4) == 0)) ||
            (len == 3 && (memcmp(name, "WBR", 3) == 0 || memcmp(name, "wbr", 3) == 0)) ||
            (len == 5 && (memcmp(name, "EMBED", 5) == 0 || memcmp(name, "embed", 5) == 0)) ||
            (len == 2 && (memcmp(name, "HR", 2) == 0 || memcmp(name, "hr", 2) == 0)) ||
            (len == 4 && (memcmp(name, "LINK", 4) == 0 || memcmp(name, "link", 4) == 0)) ||
            (len == 4 && (memcmp(name, "META", 4) == 0 || memcmp(name, "meta", 4) == 0)) ||
            (len == 5 && (memcmp(name, "PARAM", 5) == 0 || memcmp(name, "param", 5) == 0)) ||
            (len == 6 && (memcmp(name, "SOURCE", 6) == 0 || memcmp(name, "source", 6) == 0)) ||
            (len == 5 && (memcmp(name, "TRACK", 5) == 0 || memcmp(name, "track", 5) == 0)) ||
            (len == 3 && (memcmp(name, "COL", 3) == 0 || memcmp(name, "col", 3) == 0))) {
          return true;
        }
      }
      return false;
  }
}

static bool tag_can_contain(const Tag *parent, const Tag *child) {
  switch (parent->type) {
    case TR:
      return child->type == TD || child->type == TH;
    case TABLE:
      return child->type == TR || child->type == TBODY || child->type == THEAD || child->type == TFOOT || child->type == COLGROUP;
    case UL:
    case OL:
      return child->type == LI;
    case DL:
      return child->type == DT || child->type == DD;
    case RUBY:
      return child->type == RB || child->type == RT || child->type == RP;
    case COLGROUP:
      return child->type == COL;
    case TD:
    case TH:
      // TD and TH cannot contain other TD, TH, or TR elements
      return child->type != TD && child->type != TH && child->type != TR;
    case RB:
    case RT:
    case RP:
      // Ruby annotation elements cannot contain other ruby annotation elements
      return child->type != RB && child->type != RT && child->type != RP;
    case LI:
      // LI cannot contain other LI elements directly
      return child->type != LI;
    case DT:
    case DD:
      // DT and DD cannot contain other DT or DD elements directly
      return child->type != DT && child->type != DD;
    case P:
      // P elements cannot contain block-level elements
      return child->type != P && child->type != DIV && child->type != TABLE && 
             child->type != H1 && child->type != H2 && child->type != H3 &&
             child->type != H4 && child->type != H5 && child->type != H6;
    default:
      return true;
  }
}

static void scan_tag_name(TSLexer *lexer, Array(char) *tag_name) {
  array_clear(tag_name);
  while (is_ascii_alphanumeric(lexer->lookahead) || lexer->lookahead == '-' || lexer->lookahead == ':') {
    array_push(tag_name, ascii_toupper(lexer->lookahead));
    advance(lexer);
  }
}

static bool scan_comment(TSLexer *lexer) {
  if (lexer->lookahead != '-') return false;
  advance(lexer);
  if (lexer->lookahead != '-') return false;
  advance(lexer);

  unsigned dashes = 0;
  while (lexer->lookahead) {
    switch (lexer->lookahead) {
      case '-':
        ++dashes;
        break;
      case '>':
        if (dashes >= 2) {
          lexer->result_symbol = COMMENT;
          advance(lexer);
          lexer->mark_end(lexer);
          return true;
        }
      default:
        dashes = 0;
    }
    advance(lexer);
  }
  return false;
}

static void pop_tag(Scanner *scanner) {
  if (scanner->tags.size > 0) {
    Tag popped_tag = array_pop(&scanner->tags);
    tag_free(&popped_tag);
  }
}

static bool scan_implicit_end_tag(Scanner *scanner, TSLexer *lexer) {
  Tag *parent = scanner->tags.size == 0 ? NULL : array_back(&scanner->tags);

  bool is_closing_tag = false;
  if (lexer->lookahead == '/') {
    is_closing_tag = true;
    advance(lexer);
  } else {
    if (parent && tag_is_void(parent)) {
      pop_tag(scanner);
      lexer->result_symbol = IMPLICIT_END_TAG;
      return true;
    }
  }

  Array(char) tag_name = array_new();
  scan_tag_name(lexer, &tag_name);
  if (tag_name.size == 0) {
    array_delete(&tag_name);
    // At EOF, close any open element
    if (lexer->eof(lexer) && parent) {
      pop_tag(scanner);
      lexer->result_symbol = IMPLICIT_END_TAG;
      return true;
    }
    return false;
  }

  Tag next_tag = tag_for_name(tag_name.contents, tag_name.size);
  array_delete(&tag_name);

  if (is_closing_tag) {
    // The tag correctly closes the topmost element on the stack
    if (scanner->tags.size > 0 && tag_eq(array_back(&scanner->tags), &next_tag)) {
      tag_free(&next_tag);
      return false;
    }

    // Otherwise, dig deeper and queue implicit end tags
    for (unsigned i = scanner->tags.size; i > 0; i--) {
      if (tag_eq(&scanner->tags.contents[i - 1], &next_tag)) {
        pop_tag(scanner);
        lexer->result_symbol = IMPLICIT_END_TAG;
        tag_free(&next_tag);
        return true;
      }
    }
  } else if (parent && !tag_can_contain(parent, &next_tag)) {
    pop_tag(scanner);
    lexer->result_symbol = IMPLICIT_END_TAG;
    tag_free(&next_tag);
    return true;
  }

  tag_free(&next_tag);
  return false;
}

static bool scan_raw_text(Scanner *scanner, TSLexer *lexer) {
  if (scanner->tags.size == 0) return false;

  Tag *tag = array_back(&scanner->tags);
  if (tag->type != STYLE) return false;

  lexer->mark_end(lexer);
  const char *end_delimiter = "</STYLE";
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

static bool scan_end_tag_name(Scanner *scanner, TSLexer *lexer) {
  Array(char) tag_name = array_new();
  scan_tag_name(lexer, &tag_name);
  if (tag_name.size == 0) {
    array_delete(&tag_name);
    return false;
  }

  Tag tag = tag_for_name(tag_name.contents, tag_name.size);
  if (scanner->tags.size > 0 && tag_eq(array_back(&scanner->tags), &tag)) {
    pop_tag(scanner);
    lexer->result_symbol = END_TAG_NAME;
  } else {
    lexer->result_symbol = ERRONEOUS_END_TAG_NAME;
  }

  tag_free(&tag);
  array_delete(&tag_name);
  return true;
}

static bool scan_self_closing_tag_delimiter(Scanner *scanner, TSLexer *lexer) {
  advance(lexer);
  if (lexer->lookahead == '>') {
    advance(lexer);
    if (scanner->tags.size > 0) {
      pop_tag(scanner);
      lexer->result_symbol = SELF_CLOSING_TAG_DELIMITER;
    }
    return true;
  }
  return false;
}

static bool scan(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  if (valid_symbols[RAW_TEXT] && !valid_symbols[START_TAG_NAME] && !valid_symbols[END_TAG_NAME]) {
    return scan_raw_text(scanner, lexer);
  }

  while (iswspace(lexer->lookahead)) {
    skip(lexer);
  }

  switch (lexer->lookahead) {
    case '<':
      lexer->mark_end(lexer);
      advance(lexer);

      if (lexer->lookahead == '!') {
        advance(lexer);
        return scan_comment(lexer);
      }

      if (valid_symbols[IMPLICIT_END_TAG]) {
        return scan_implicit_end_tag(scanner, lexer);
      }
      break;

    case '\0':
      if (valid_symbols[IMPLICIT_END_TAG]) {
        return scan_implicit_end_tag(scanner, lexer);
      }
      break;

    case '/':
      if (valid_symbols[SELF_CLOSING_TAG_DELIMITER]) {
        return scan_self_closing_tag_delimiter(scanner, lexer);
      }
      break;

    default:
      if ((valid_symbols[START_TAG_NAME] || valid_symbols[END_TAG_NAME])) {
        return valid_symbols[START_TAG_NAME] ? scan_start_tag_name(scanner, lexer)
                                             : scan_end_tag_name(scanner, lexer);
      }
      if (valid_symbols[IMPLICIT_END_TAG]) {
        return scan_implicit_end_tag(scanner, lexer);
      }
  }

  return false;
}

static unsigned serialize(Scanner *scanner, char *buffer) {
  uint16_t tag_count = scanner->tags.size > UINT16_MAX ? UINT16_MAX : scanner->tags.size;
  uint16_t serialized_tag_count = 0;

  unsigned size = sizeof(tag_count);
  memcpy(&buffer[size], &tag_count, sizeof(tag_count));
  size += sizeof(tag_count);

  for (; serialized_tag_count < tag_count; serialized_tag_count++) {
    Tag tag = scanner->tags.contents[serialized_tag_count];
    if (tag.type == CUSTOM) {
      unsigned name_length = tag.custom_tag_name.size;
      if (name_length > UINT8_MAX) {
        name_length = UINT8_MAX;
      }
      if (size + 2 + name_length >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
        break;
      }
      buffer[size++] = (char)tag.type;
      buffer[size++] = (char)name_length;
      strncpy(&buffer[size], tag.custom_tag_name.contents, name_length);
      size += name_length;
    } else {
      if (size + 1 >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
        break;
      }
      buffer[size++] = (char)tag.type;
    }
  }

  memcpy(&buffer[0], &serialized_tag_count, sizeof(serialized_tag_count));
  return size;
}

static void deserialize(Scanner *scanner, const char *buffer, unsigned length) {
  for (unsigned i = 0; i < scanner->tags.size; i++) {
    tag_free(&scanner->tags.contents[i]);
  }
  array_clear(&scanner->tags);

  if (length > 0) {
    unsigned size = 0;
    uint16_t tag_count = 0;
    uint16_t serialized_tag_count = 0;

    memcpy(&serialized_tag_count, &buffer[size], sizeof(serialized_tag_count));
    size += sizeof(serialized_tag_count);

    memcpy(&tag_count, &buffer[size], sizeof(tag_count));
    size += sizeof(tag_count);

    array_reserve(&scanner->tags, tag_count);
    if (tag_count > 0) {
      unsigned iter = 0;
      for (iter = 0; iter < serialized_tag_count; iter++) {
        Tag tag = tag_new();
        tag.type = (TagType)buffer[size++];
        if (tag.type == CUSTOM) {
          uint16_t name_length = (uint8_t)buffer[size++];
          array_reserve(&tag.custom_tag_name, name_length);
          tag.custom_tag_name.size = name_length;
          memcpy(tag.custom_tag_name.contents, &buffer[size], name_length);
          size += name_length;
        }
        array_push(&scanner->tags, tag);
      }
      // add zero tags if we didn't read enough
      for (; iter < tag_count; iter++) {
        array_push(&scanner->tags, tag_new());
      }
    }
  }
}

void *tree_sitter_shopware_twig_external_scanner_create() {
  Scanner *scanner = ts_calloc(1, sizeof(Scanner));
  return scanner;
}

bool tree_sitter_shopware_twig_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  Scanner *scanner = (Scanner *)payload;
  return scan(scanner, lexer, valid_symbols);
}

unsigned tree_sitter_shopware_twig_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *scanner = (Scanner *)payload;
  return serialize(scanner, buffer);
}

void tree_sitter_shopware_twig_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  deserialize(scanner, buffer, length);
}

void tree_sitter_shopware_twig_external_scanner_destroy(void *payload) {
  Scanner *scanner = (Scanner *)payload;
  for (unsigned i = 0; i < scanner->tags.size; i++) {
    tag_free(&scanner->tags.contents[i]);
  }
  array_delete(&scanner->tags);
  ts_free(scanner);
}
