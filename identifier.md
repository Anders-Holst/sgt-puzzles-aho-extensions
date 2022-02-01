# Identifier

<table>
<tr>
<td><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/identifier1.png"></td>
<td><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/identifier2.png"></td>
<td><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/identifier3.png"></td>
</tr>
</table>

### Identify the forms and positions of a fleet of shapes in a board.

Author: Anders Holst (anders.holst@ri.se)

This game is similar to Battleship where two players try to sink each
others ships as fast as possible by firing at squares in each others
boards. However, there are two main differences: The "ships" have
unknown shapes (in the default setting), and the goal is to identify
their shapes and positions. I.e. you don't have to explicitly "sink"
them by hitting all squares, but as soon as you know where they are
located you are done.

Each shape must be connected, i.e each black square should be possible
to reach from any other black square by moving vertically or
horizontally on black squares. Diagonal touching is not enough to be
connected. Different shapes (of same or different form) placed on the
board may not touch each other, not even diagonally.

This variant of Battleships were invented by me and my brother when we
were young and had some time to kill during a rather event-less summer
vacation.

### Identifier parameters

There are three game modes: 

In *Duel mode* you are playing against the computer. First you prepare
your board according to the agreed specification, then you start
playing, where in turn you and the computer guess at each others
squares. Whenever one of you know the other's shapes and locations,
the full guess is shown. The other player will still have a chance to
continue playing to see how many extra moves were needed.

In *Single mode* you will only guess on the computer generated board. A
goal number of guesses is given, that you could try to beat.

In *Puzzle mode*, which is the trickiest one, you get a board with a set
of (empty) squares uncovered, and your task is to find the only way to
place shapes of the agreed specification into that board. There is no
way to get further clues - you have to stare at the board until you
know where they are all located.

There are also three different fleet types to choose from: In the
default setting, "Unknown", you only know the number of black squares
in each shape. In the "Random" setting, some random shapes of the
specified size will be generated, and those are to be used (by both
you and the computer, in Duel mode). Finally, the "Standard fleet"
setting is the most boring, and threwn in just in case there is
someone who would prefer the standard straiht line shapes of
Battleships instead of arbitrary shapes.

You can also specify which symmetry transformation are allowed on the
shapes: rotations, mirroring, both, or none. Default is that both
mirroring and rotations are allowed.

Finally you can specify the shape configuration, in terms of how many
copies of each shape and of which level, i.e how many black squares
they consist of. For example "2\*6,3\*4" means 2 copies of a shape of 6
squares and 3 copies of a shape of 4 squares. Maximum shape size is 12
(for which it takes several minutes to compile the dictionary of all
shapes).

### Identifier controls

Either using the mouse buttons or the arrow keys you can select a
square in any of the two boards. Again pressing the left or right
mouse button, or the space bar, you can toggle a black or white mark
on the selected square. For example, in duel mode it can be used to
mark where your ships are on the top board that the computer will
guess on. On the lower board you can use it to mark the squares you
are certain of so far.

To guess on a square in the lower board you use the Return key. Also,
to answer a computer guess in the upper board, make sure it is
correctly marked white or black and then press Return.

With the numbers 1-6 you can also give the selected square an auxilary
color. 0 erases that color. The auxilary color has no meaning in the
game, it can be used any way you like, for your memory as with "pencil
marks". For example, if you don't trust that the computer will not
cheat by looking at your white and black marks, you can use the colors
instead in some obscure way that you decide. Make sure you remember
what the colors mean, because if you answer wrong in such a way that
the result will be inconsistent, you have lost. (Just to make it
perfectly clear, you can safely use the white and black marks if you
like, because the computer will *not* cheat. It does not need to,
since it uses an efficient Bayesian optimization algorithm, maximizing
the expected entropy decrease at each step, which is difficult for
anyone to beat.)

### Gameplay

Duel mode is most complex, so it s described here. Much in the other
modes will then be similar. In Duel mode (but not the others) you have
to prepare your own board, for the computer to guess. Prepare it in
the upper board, either using white and black marks (black for the
shape squares and white outside), or the colors 1 - 6 of arbitrary
meaning, or on a paper on the side if you don't trust the program. At
the bottom of the window, the shape configuration to use is shown. For
Unknown fleet type, it only says how many copies of shapes of how many
black squares, and you design your shapes yourself, and the computer
designs its shapes of the same sizes. For Random fleet type, some
random shapes are drawn and displayed at the bottom. You and the
computer will use these same shapes. In Standard fleet type, only
straight line shapes will be used (but you can affect the counts and
sizes of them by setting the parameters). Depending on the symmetry
settings the shapes may be rotated or mirrored.

When you are done preparing, press Start. First it is your
turn. Select a square in the lower board on which you want to ask, and
press Return. It will then be revealed as black (hit) or white (miss),
and your turn count increases.

Then it is the computer's turn. One square in the upper board will be
highlighted. Make sure it has the right black or white mark on it (with
the left or right mouse button, or the space bar), and then press
Return. The computer's turn count increases.

So it continues until either you or the computer is sure of the
solution.  When you are certain what the computer's board look like,
make sure that all squares where you think shapes are located are
marked with black.  Whites are not mandatory to mark, thus unmarked
squares will be treated as white. When the right number of black
squares are marked, you will be able to press Guess, and the true
answer will be revealed. If you made any mistake, the difference
between your guess and the true answer will be highlighted with red.

When the computer is certain, it reveals the whole solution. It will
not make a mistake, so if you don't agree, the fault is on your
side. You will have a chance to continue guessing to see how far
behind you were. Likewise, if you are done before the computer, the
computer will continue to ask and if you like you can continue to
answer to see how far behind it was.

Play in Single mode is similar, but you need not prepare a board
yourself, but only guess on the computer generated board. There is a
goal turn count provided (which is how long it would have taken the
computer to solve it, plus a handicap of two, since the computer has a
very efficient optimizer which is hard to beat). As soon as you are
sure of the solution, make sure all the black squares are marked, and
press Guess.

Puzzle mode is the most tricky, but at the same time has the least
controls. You get a board with some white squares revealed, and have
to figure out the only way to fit in the shapes of the given
specification on the board. You can not ask for specific squares. You
have to figure it all out, mark all the shape squares with black, and
press Guess to see if you were right.
