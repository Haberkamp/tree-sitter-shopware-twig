import XCTest
import SwiftTreeSitter
import TreeSitterShopwareTwig

final class TreeSitterShopwareTwigTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_shopware_twig())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading ShopwareTwig grammar")
    }
}
