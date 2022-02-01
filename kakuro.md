# Kakuro

![](https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/kakuro.png)

### Fill the board so that unique numbers sum up to each clue

Author: Anders Holst (anders.holst@ri.se)

This is the traditional Kakuro game, examples of which can be found in
several places. Free and good generators are not easy to find though,
so I had to write one myself. The puzzle has a grid like a crossword,
which should be filled with numbers such that the sum of them match
the horizontal and vertical clues, and the same number must not occur
more than once in each sum.

### Kakuro controls

Kakuro shares much of its control system with other games where you
fill in numbers in squares, such as Solo, Unequal, Towers, Keen, etc.

To play Kakuro, (left) click the mouse in a square and type in a
number. You can erase the number in a selected square by pressing
space.

If you right click in a square, you can add a pencil mark. You can add
several pencil marks in a square, but not more than one of each
number. If you try to add the same number twice, it will erase that
number instead. Space in a selected square erases all pencil marks.

Pressing M will fill in a full set of pencil marks in every square
that does not have a main digit in it.

The game pays no attention to pencil marks, so exactly what you use
them for is up to you: you can use them as reminders that a
particular square needs to be re-examined once you know more about a
particular number, or you can use them as lists of the possible
numbers in a given square, or anything else you feel like.

As for the other similar games, the cursor keys can be used in
conjunction with the digit keys to set numbers or pencil marks. Use
the cursor keys to move a highlight around the grid, and type a digit
to enter it in the highlighted square. Pressing return toggles the
highlight into a mode in which you can enter or remove pencil marks.

(All the actions described in `common-actions` in the SGT puzzles docs
are also available.)

### Kakuro parameters

These parameters are available from the `Custom...` option on the
`Type` menu.

*Grid size*

Size of grid in squares (not counting the pure clue lines at the
left and the top). The grid size can be between 3 and 12.

*Variant*

In addition to the Normal variant, there are two variations that
can be selected: With *Odd/even restriction*, every other square must be
odd (lighter background shade) and every other even (darker
shade). With the variant *No same combinations*, no two sums can be made
up of the same set of numbers. The solution is selected so that this
condition will have to be explicitly considered to be able to solve
the puzzle. This variant is highly recommended! It makes the
puzzle more interesting by adding non-local constraints to it.

*Difficulty*

You can select (approximate) difficulty levels from Trivial to
Extreme. In Trivial there is essentially in each step at least one
sequence that is determined given the previous inputs, whereas in
Extreme you typically will have to guess and backtrack. Somewhere in
between is usually most fun.

*Maximum value*

Specifies the highest number allowed in the squares. The maximum
can vary between 5 and 20.  
