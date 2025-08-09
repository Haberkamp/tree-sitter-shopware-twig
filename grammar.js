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
    source_file: ($) => repeat(choice($.statement_directive, $.content)),

    template: ($) => repeat(choice($.statement_directive, $.content)),

    content: () => prec.right(repeat1(/[^\s\{]+/)),

    statement_directive: ($) =>
      seq("{%", choice($.if_statement, $.tag_statement), "%}"),

    if_statement: ($) => seq($.conditional, $.variable),

    tag_statement: ($) =>
      choice(seq($.tag, optional($.variable)), $.conditional),

    tag: ($) => choice("block", "endblock"),

    conditional: ($) => choice("if", "endif"),

    variable: ($) => /[a-zA-Z0-9_]+/,
  },
});
