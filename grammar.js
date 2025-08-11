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

  externals: ($) => [
    $._start_tag_name,
    $._end_tag_name,
    $.erroneous_end_tag_name,
    "/>",
    $._implicit_end_tag,
    $.raw_text,
    $.comment,
  ],

  rules: {
    template: ($) =>
      repeat(
        choice(
          $.statement_directive,
          $.html_element,
          $.html_doctype,
          $.html_entity,
          $.content,
          $.erroneous_end_tag
        )
      ),

    content: () => prec.right(/[^<>&\{\s]([^<>&\{]*[^<>&\{\s])?/),

    html_element: ($) =>
      choice(
        seq(
          $.html_start_tag,
          repeat($._node),
          choice($.html_end_tag, $._implicit_end_tag)
        ),
        $.html_self_closing_tag
      ),

    _node: ($) => choice($.html_element, $.html_entity, $.content),

    html_start_tag: ($) =>
      seq(
        "<",
        alias($._start_tag_name, $.html_tag_name),
        repeat($.html_attribute),
        ">"
      ),

    html_end_tag: ($) =>
      seq("</", alias($._end_tag_name, $.html_tag_name), ">"),

    html_self_closing_tag: ($) =>
      seq(
        "<",
        alias($._start_tag_name, $.html_tag_name),
        repeat($.html_attribute),
        "/>"
      ),

    erroneous_end_tag: ($) => seq("</", $.erroneous_end_tag_name, ">"),

    html_attribute: ($) =>
      seq(
        $.html_attribute_name,
        optional(
          seq(
            "=",
            choice($.html_attribute_value, $.html_quoted_attribute_value)
          )
        )
      ),

    html_attribute_name: () => /[a-zA-Z][a-zA-Z0-9_\-💩]*/,

    html_attribute_value: () => /[^>\s"'=]+/,

    html_quoted_attribute_value: ($) =>
      choice(
        seq('"', alias(/[^"]*/, $.html_attribute_value), '"'),
        seq("'", alias(/[^']*/, $.html_attribute_value), "'")
      ),

    html_doctype: ($) => seq("<!", alias($._doctype, "doctype"), /[^>]+/, ">"),

    _doctype: () => /[Dd][Oo][Cc][Tt][Yy][Pp][Ee]/,

    statement_directive: ($) =>
      seq("{%", choice($.if_statement, $.tag_statement, $.function_call), "%}"),

    if_statement: ($) => seq($.conditional, $.variable),

    tag_statement: ($) =>
      choice(seq($.tag, optional($.variable)), $.conditional),

    tag: ($) => choice("block", "endblock"),

    conditional: ($) => choice("if", "endif"),

    variable: ($) => /[a-zA-Z0-9_]+/,

    function_call: ($) => seq($.function_identifier, $.arguments),

    function_identifier: () => /[a-zA-Z_][a-zA-Z0-9_]*/,

    arguments: ($) => seq("(", ")"),

    html_entity: () =>
      choice(
        /&[a-zA-Z][a-zA-Z0-9]*;/, // Named entities like &nbsp;
        /&#[0-9]+;/, // Numeric entities like &#160;
        /&#[xX][0-9a-fA-F]+;/ // Hex entities like &#xA0;
      ),
  },
});
