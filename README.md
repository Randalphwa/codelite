What is CodeLite?
====

[CodeLite][1] is a free, open source, cross platform IDE specialized in C, C++, PHP and JavaScript (mainly for backend developers using Node.js) programming languages, which runs best on all major platforms (Windows, macOS and Linux).

You can download pre-built binaries for Windows, macOS and Linux from our **[Download Page][2]**.

More information can be found here:

 - [Official website][3]
 - [Download page][4]
 - [Our Documentation page][5]

Building and installation
===

- [Windows][9]
- [Linux][10]
- [macOS][11]

  [1]: https://codelite.org
  [2]: https://codelite.org/support.php
  [3]: https://codelite.org
  [4]: https://codelite.org/support.php
  [5]: https://docs.codelite.org/
  [9]: https://docs.codelite.org/build/build_from_sources/#windows
  [10]: https://docs.codelite.org/build/build_from_sources/#linux
  [11]: https://docs.codelite.org/build/build_from_sources/#macos

# _Fork Notes_

This fork is built as if wxSnapshot had been added as a sub-module. It's not actually added so if you want to build from this fork, you will need to a) clone the wxSnapshot directory somewhere and b) create a symbolic link in the root of this repository that points to your own version of wxSnapshot. Currently, the only portion of this fork that uses this is the wxcrafter folder.
