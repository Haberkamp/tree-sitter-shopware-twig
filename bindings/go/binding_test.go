package tree_sitter_shopware_twig_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_shopware_twig "www.github.com/haberkamp/shopware-twig-parser/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_shopware_twig.Language())
	if language == nil {
		t.Errorf("Error loading ShopwareTwig grammar")
	}
}
