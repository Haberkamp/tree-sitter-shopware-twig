((style_element
  (raw_text) @injection.content)
 (#set! injection.language "css"))

((script_element
  (raw_text) @injection.content)
 (#set! injection.language "javascript"))

((vue_interpolation
  (interpolation_content) @injection.content)
 (#set! injection.language "javascript"))
