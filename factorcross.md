# Factorcross

![](https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/factorcross.png)

### Fill the board so that the numbers' products match the clues.

Author: Anders Holst (anders.holst@ri.se)

This game is a factor crossword, i.e. the grid should be filled with
numbers such that the products of them match the horizontal and
vertical clues. It is similar to kakuro, but with multiplication
instead of addition, and there is no rule against repetition of
numbers, so the same factor can occur several times in a product.

I encountered this puzzle in a book on number games when I was a kid
(Carl-Otto Johansen, "37+53 number games", 1980, in Swedish). The book
also contained some tricks for mental arithmetic, such as how to check
divisibility with different numbers. I have never seen this puzzle
anywhere else. But if you are not scared of numbers and math, it is
quite enjoyable. And as a side effect, you will quickly learn how to
do prime factorization of large numbers in the head.

### Factorcross controls

Factorcross shares much of its control system with other games where you
fill in numbers in squares, such as Solo, Unequal, Towers, Keen, etc.

To play Factorcross, (left) click the mouse in a square and type in a
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

(All the actions described in `common-actions` the SGT puzzles docs
are also available.)

In addition to the above controls, you can also use H to toggle hints
mode. If you select a clue square in hints mode, the game will show
you the prime factorizations of both the vertical and horizontal clue
in that square. Use restrictively, since it may remove some of the fun
of the game...

### Factorcross parameters

These parameters are available from the `Custom...` option on the
`Type` menu.

*Grid size*

Size of grid in squares (not counting the pure clue lines at the
left and the top). The grid size can be between 2 and 15.

*Zeroes allowed*

Allow zeroes in the puzzle. This makes it a little trickier. Not
in finding the positions of the zeroes, because that is trivial, but
because the slots in the same rows/columns will lose one of their
clues.

*No ones allowed*

No ones are allowed in the puzzle. You must household with your
factors so you have them for all squares. The board is generated so
that this condition will have to be explicitly considered to be able
to find the unique solution.

*Limited clue size*

For beginners, make sure that the clues are not too large
numbers. The exact limit depends on the size of the board.

*Maximum value*

Specifies the highest number allowed in the squares. Using higher
numbers than 9 will introduce more prime factors, and also more ways
to factor old familiar products. The maximum can vary between 5 and
20.
