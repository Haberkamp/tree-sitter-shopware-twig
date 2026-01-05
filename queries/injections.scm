((style_element
  (raw_text) @injection.content)
 (#set! injection.language "css"))

((vue_interpolation
  (interpolation_content) @injection.content)
 (#set! injection.language "javascript"))
