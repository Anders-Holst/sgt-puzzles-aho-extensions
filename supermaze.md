# Supermaze

<table>
<tr>
<td><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/supermaze1.png"></td>
<td><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/supermaze2.png"></td>
</tr>
<tr>
<td><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/supermaze3.png"></td>
<td><img src="https://raw.githubusercontent.com/Anders-Holst/sgt-puzzles-aho-extensions/main/supermaze4.png"></td>
</tr>
</table>

### Several types of auto-generated mazes

Author: Anders Holst (anders.holst@ri.se)

This game contains six variants of super-mazes and one ordinary
two-dimensional type of maze. A super-maze, as introduced by Robert
Abbot, is a stateful maze, such that the possible moves depend on the
current state. Mathematically, it is equivalent to moving around in a
maze of high dimension, where the state can be seen as one or several
additional dimensions - instead of moving through an ordinary door in
the plane, you can change state, thereby moving in another dimension,
potentially changing the available doors in the visible plane. If this
is abstract, it will be clearer when you see the examples below.

### Supermaze parameters

There are seven variants of mazes to choose from:

*Basic*: This is an ordinary two-dimensional maze, just to see that
the generator works. Boring and simple. To make it marginally more
challenging, the open doors will reveal themselves only when you are
next to them. You can temporarily reveal them all with the right
button or space bar, but they will close again as soon as you start
moving.

*Tandem*: This was the first super-maze variant I implemented,
freely inspired by Robert Abbot's maze "Where are the cows" (search
for it on the web), in which you have two pencils pointing at
different rooms in a maze, and at each room there are instructions on
where you may move that pencil, depending on *the other pencil's
position*. It is such a fantastic and challenging maze that I can
recommend it for everyone to try. I wanted to see if I could
automatically generate a stylized version of that type of maze. I
could, it turned out, although it may not be as much fun as the
original: There are two balls that need to be moved through the maze,
but the doors depend on in which rooms the balls are. It is
deterministic, but may seem quite perplexing. (Each room has an
xor-mask to change the status of the doors.) Also note that it is a
quite valid (and sometimes necessary) move, to move one ball back out
of the entrance, or back in from the exit, to enable the other ball to
move. It is rather difficult but quite possible to solve. (Note that
effectively this is a four-dimensional maze, since the "state" depends
on the position of both balls, and each has two coordinates.)

*3D*: As is evident from the name, this is an ordinary 3-dimensional
maze, with stairs going up and down between the floors. The entrance
is on the lowest floor and the exit on the highest floor. You only
see one floor at a time. No perplexing state changes.

*Floors*: Similar to 3D, but instead of stairs there are "direct
portals" between different floors. (Or are they trampolines?) Each
floor has its own color, which is also shown in the portals that go
there.

*Keys*: Here there are doors of different colors, and you need a key
of the corresponding color to go through. Once you find a key it can
be used repeatedly. This is an attempt to make a non-reversible maze
(you can only get a key, not lose it), with a generator designed for
reversible mazes... Unfortunately it does not work perfectly, so the
mazes tend to be either too easy, or impossible for the generator to
find.

*Levers*: Here too the doors are colored, and they can be operated
by levers of the matching color. However, each lever of a certain
color opens some and closes some doors. This is the most puzzle-like
variant, since you see the full board and need to figure out which
levers to pull in which order to make it through. In "extra tricky"
mode, which is recommended, this can be quite challenging.

*Combined*: This variant combines 3D, keys and levers. Unfortunately
it is not possible to use "extra tricky" mode, since it is too hard
for the generator to find a suitable maze then. It can be quite
entertaining nevertheless.

Other options are: The size (width and height) of the maze, the number
of Floors or Keys or Levers in the corresponding variants (or all
three in Combined), and "Extra tricky" mode, which means that there is
a bottleneck somewhere that has to be passed. This is recommended for
all the variants where the generator can manage it (not Combined, or
Keys of too high key count), since it makes it more challenging and
interesting.

### Supermaze controls

The objective is to move the ball (or two balls in one variant) from
the entrance to the exit of the maze. The controls have slighty
different meaning in the variants but essentially it works the same.

Using the mouse, you can left-click on a square next to the ball where
you want to move. If there is an open door between the squares, the
ball will move. When there are two balls, if one of them is in a
nearby square and can move, it will move. If it is ambiguous, the one
that moved last time will move. If you want to change which to move,
left-click on the other ball. To operate a lever, or take a key, or
move through a portal, you use eiher the left or right button. To move
in stairs, use the left button for up and the right button for down.
Also, in the Basic variant you can use the right button to temporarily
reveal all doors, and in the Tandem variant you can use the right
button to temporarily test what would happen with the doors if you put
a ball in a room (or lift an existing ball up from the magic floor
tiles which operate the doors).

You an also operate the game with the keys. Use arrows to move in a
direction possible to move. When there are two balls, use Return to
switch which one to move. Either Return or Space is used to operate
levers, keys and portals. Use Return to move upwards in stairs and
Space to move downwards. In Basic mode, Space temorarily reveals the
doors, and in Tandem mode Space will temporarily lift the active ball
to see what happens with the doors.

If you get stuck in some variant, you can get some hints, by selecting
Solve in the menu. Nothing will change visualy in the board, but from
now on, every time you press "s", the ball will move one step closer
towards the goal. You can mix walking yourself and pressing "s" -
wherever you are in the maze it will take you one step in the right
direction. (After having solved a maze myself, I usually do Restart
and Solve, and step through with "s", to check if I had found the
fastest solution.)
