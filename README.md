sgt-puzzles-aho-extensions
==========================

### Anders Holst's extensions to Simon Tatham's puzzle collection

This package contains a number of puzzles I have written for [Simon Tatham's Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/) but which have not made it into the official package. Nevertheless, they are fully playable and complete. 

## The puzzles:


Click on a puzzle below for the full description of it.

<table>
<tr>
<td align="center"><b><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/kakuro.md">Kakuro</a></b><br/>Fill the board so that unique numbers sum up to each clue</td>
<td align="center"><b><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/factorcross.md">Factorcross</a></b><br/>Fill the board so that the numbers' products match the clues</td>
<td align="center"><b><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/alphacrypt.md">Alphacrypt</a></b><br/>Assign a number to each letter such that the equations match</td>
</tr>
<tr>
<td><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/kakuro.md"><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/kakuro.png"></a></td>
<td><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/factorcross.md"><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/factorcross.png"></a></td>
<td><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/alphacrypt.md"><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/alphacrypt.png"></a></td>
</tr>
<tr>
<td align="center"><b><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/identifier.md">Identifier</a></b><br/>Identify the forms and positions of shapes in a board</td>
<td align="center"><b><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/supermaze.md">Supermaze</a></b><br/>Find your way through the Supermazes, i.e mazes with states</td>
<td align="center"><b> </b></td>
</tr>
<tr>
<td><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/identifier.md"><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/identifier3.png"></a></td>
<td><a href="https://github.com/Anders-Holst/sgt-puzzles-aho-extensions/blob/main/supermaze.md"><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/supermaze2.png"></a></td>
<td> </td>
</tr>
</table>

## How to install:

You will need both the source code in this package, and the code of
the SGT puzzle collection. This extension package was recently
compiled together with a version of the SGT puzzles from January 2022,
but sometimes the interface changes, so give me a notice if I need to
update my code.

To download and install, follow these steps:

* Download this package from Github.
  For example, on Linux you can do this by going into (with "cd") the
  directory where you want your source code, and running:
```
git clone git@github.com:Anders-Holst/sgt-puzzles-aho-extensions.git
```

* If you have not done so already, also get the source code for the
  SGT Portable Puzzle Collection from
  [the official site](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/).
  Download it, unpack it, and move into the resulting directory.
```
mv -f puzzles.tar.gz puzzles.tar.gz.old
wget https://www.chiark.greenend.org.uk/~sgtatham/puzzles/puzzles.tar.gz
tar xzf puzzles.tar.gz
cd  `tar tzf puzzles.tar.gz | head -1` 
```

* Copy thie extension package directory into the SGT puzzles directory as a subdirectory called `aho-extensions`:
  Assuming that you followed the above commands, you can do:
```
cp -a ../sgt-puzzles-aho-extensions ./aho-extensions
```

* In the SGT puzzles repository's `CMakeLists.txt`, add the line
  `add_subdirectory(aho-extensions)` to include the extensions in the
  compilation:
```
mv CMakeLists.txt CMakeLists.txt.orig
sed 's|add_subdirectory(unfinished)|add_subdirectory(unfinished)\nadd_subdirectory(aho-extensions)|' < CMakeLists.txt.orig > CMakeLists.txt
```

* You also need to apply a small modification to one file in the SGT
  code: Remove the word "static" in front of the function `midend_undo`
  in the file "midend.c". This is needed to be able to use multi-digit
  numbers. For example, you can do this by the commands:
```
mv midend.c midend.c.orig
sed 's|static bool midend_undo(|bool midend_undo(|' < midend.c.orig > midend.c
```

* Run CMake and make in the SGT puzzles folder:
```
cmake .
make clean
make
```

* If you have super-user rights on your computer, you can now install all
  the puzzles with:
```
sudo make install
```
Otherwise the extension binaries are located in the aho-extensions
subdirectory in the SGT puzzles source directory.

* You are done. Enjoy!


## Related packages

Other nice additions to the SGT puzzle collection can be found here:

[Lennard Sprong's extensions](https://github.com/x-sheep/puzzles-unreleased)

[Steffen Bauer's extensions](https://github.com/SteffenBauer/sgtpuzzles-extended)


## LICENSE

Copyright (c) 2004-2021 Simon Tatham, 2012-2022 Anders Holst

Distributed under the MIT license, see LICENSE
