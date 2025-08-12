# tree-sitter-shopware-twig

A tree-sitter grammar for the Shopware 6 Twig admin markup language.

## Installation

```sh
npm install tree-sitter-shopware-twig
```

## Usage

> Note: We expect you to have a basic understanding of tree-sitter. If you don't, please read the [tree-sitter documentation](https://tree-sitter.github.io/tree-sitter).

```ts
const Parser = require("tree-sitter");
const ShopwareTwig = require("tree-sitter-shopware-twig");

const parser = new Parser();
parser.setLanguage(ShopwareTwig);

const tree = parser.parse("{% block my_block %}Hello, world!{% endblock %}");
```

### Author

Hey, I'm Nils. In my spare time [I write about things](https://www.haberkamp.dev/) I learned or I create open source packages, that help me (and hopefully you) to build better apps.

## Feedback and Contributing

I highly appreceate your feedback! Please create an [issue](https://github.com/Haberkamp/tree-sitter-shopware-twig/issues/new), if you've found any bugs or want to request a feature.
