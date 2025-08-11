/**
 * @file A parser for Shopware 6 Twig
 * @author Nils Haberkamp <haberkamp.n@gmail.com>
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: "shopware_twig",

  extras: () => [/\s/],

  rules: {
    template: ($) =>
      repeat(choice($.statement_directive, $.html_element, $.content)),

    content: () => prec.right(repeat1(/[^\s\{<]+/)),

    html_element: ($) =>
      seq($.html_start_tag, optional($.text), $.html_end_tag),

    html_start_tag: ($) => seq("<", $.html_tag_name, ">"),

    html_end_tag: ($) => seq("</", $.html_tag_name, ">"),

    html_tag_name: () => /[a-zA-Z][a-zA-Z0-9]*/,

    text: () => /[^<{]+/,

    statement_directive: ($) =>
      seq(
        "{%",
        choice($.if_statement, $.tag_statement, $.parent_statement),
        "%}"
      ),

    if_statement: ($) => seq($.conditional, $.variable),

    tag_statement: ($) =>
      choice(seq($.tag, optional($.variable)), $.conditional),

    tag: ($) => choice("block", "endblock"),

    conditional: ($) => choice("if", "endif"),

    variable: ($) => /[a-zA-Z0-9_]+/,

    parent_statement: ($) => seq("parent", "(", ")"),
  },
});
