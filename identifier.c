/*
  Identifier

Author: Anders Holst (anders.holst@ri.se)

This game is similar to Battleship where two players try to sink each others
ships as fast as possible by firing at squares in each others boards.
However, there are two main differences: The "ships" have unknown shapes (in
the default setting), and the goal is to identify their shapes and positions.
I.e. you don't have to explicitly "sink" them by hitting all squares, but as
soon as you know where they are located you are done.

Each shape must be connected, i.e each black square should be possible
to reach from any other black square by moving vertically or
horizontally on black squares. Diagonal touching is not enough to be
connected. Different shapes (of same or different form) placed on the
board may not touch each other, not even diagonally.

Parameters:

There are three game modes: 

In Duel mode you are playing against the computer. First you prepare your
board according to the agreed specification, then you start playing, where
in turn you and the computer guess at each others squares. Whenever one of
you know the other's shapes and locations, the full guess is shown. The other
player will still have a chance to continue playing to see how many extra
moves were needed.

In Single mode you will only guess on the computer generated board. A goal
number of guesses is given, that you could try to beat. 

In Puzzle mode, which is the trickiest one, you get a board with a set of
(empty) squares uncovered, and your task is to find the only way to place
shapes of the agreed specification into that board. There is no way to get
further clues - you have to stare at the board until you know where they are
all located.

There are also three different fleet types to choose from: In the default
setting, "Unknown", you only know the number of black squares in each shape.
In the "Random" setting, some random shapes of the specified size will be
generated, and those are to be used. Finally, the "Standard fleet" setting
is the most boring, and threwn in just in case there is someone who would
prefer the standard straiht line shapes of Battleships instead of arbitrary
shapes.

You can also specify which symmetry transformation are allowed on the shapes:
rotations, mirroring, both, or none. Default is that all transformations are
allowed. 

Finally you can specify the shape configuration, in terms of how many copies
of each shape and of which level, i.e how many black squares they consist of.
For example "2*6,3*4" means 2 copies of a shape of 6 squares and 3 copies of
a shape of 4 squares. Maximum shape size is 12 (for which it takes several
minutes to compile the dictionary of all shapes).

Controls:

Either using the mouse buttons or the arrow keys you can select a square in
any of the two boards. Again pressing the left or right mouse button, or the
space bar, you can toggle a black or white mark on the selected square. For
example, in duel mode it can be used to mark where your ships are on the top
board that the computer will guess on. On the lower board you can use it to
mark the squares you are certain of so far.

To guess on a square in the lower board you use the return key. Also, to
answer a computer guess in the upper board, make sure it is correctly marked
white or black and then press return. 

With the numbers 1-6 you can also give the selected square an auxilary color.
0 erases that color. The auxilary color has no meaning in the game, it can be
used any way you like, for your memory as with "pencil marks". For example, if
you don't trust that the computer will not cheat by looking at your white and
black marks, you can use the colors instead in some obscure way that you decide.
Make sure you remember what the colors mean, because if you answer wrong in
such a way that the result will be inconsistent, you have lost. (Just to make
it perfectly clear, you can safely use the white and black marks, because the
computer will *not* cheat. It does not need to, since it uses an efficient
Bayesian optimization algorithm, maximizing the expected entropy decrease at
each step, which is difficult for anyone to beat.)

Gameplay:

Duel mode is most complex, so it s described here. Much in the other modes
will then be similar. In Duel mode (but not the others) you have to prepare
your own board, for the computer to guess. Prepare it in the upper boars,
either using white and black marks (black for the shape squares and white
outside), or the colors 1 - 6 or arbitrary meaning, or on a paper on the
side if you don't trust the program. At the bottom of the window, the shape
configuration to use is shown. For Unknown fleet type, it only says how
many copies of shapes of how many black squares, and you design your shapes
yourself, and the computer designs its shapes of the same sizes. For Random
fleet type, some random shapes are drawn and displayed at the bottom. You
and the computer will use these same shapes. In Standard fleet type, only
straight line shapes will be used (but you can affect the counts and sizes
of them by setting the parameters). Depending on the symmetry settings the
shapes may be rotated or mirrored.

When you are done preparing, press Start. First it is your turn. Select a
square in the lower board on which you want to ask, and press Return. It
will then be revealed as black (hit) or white (miss), and your turn count
increases.

Then it is the computer's turn. One square in the upper board will be
highlighted. ake sure it has the right black or white mark on it (with the
left or right mouse button, or the space bar), and then press Return. The
computer's turn count increases.

So it continues until either you or the computer is sure of the solution. 
When you are certain what the computer's board look like, make sure that
all squares where you think shapes are located are marked with black.
Whites are not mandatory to mark, thus unmarked squares will be treated as
white. When the right number of black squares are marked, you will be able
to press Guess, and the true answer will be revealed. If you made any
mistake, the difference between your guess and the true answer will be
highlighted with red.

When the computer is certain, it reveals the whole solution. It will not
make a mistake, so if you don't agree, the fault is on your side. You will
have a chance to continue guessing to see how far behind you were. Likewise,
if you are done before the computer, the computer will continue to ask and
if you like you can continue to answer to see how far behind it was.

Play in Single mode is similar, but you need not prepare a board yourself,
but only guess on the computer generated board. There is a goal turn count
provided (which is how long it would have taken the computer to solve it,
plus a handicap of two, since the computer has a very efficient optimizer
which is hard to beat). As soon as you are sure of the solution, make sure
all the black squares are marked, and press Guess.

Puzzle mode is the most tricky, but at the same time the least controls.
You get a board with some white squares revealed, and have to figure out
the only way to fit in the shapes of the given specification on the board.
You can not ask for specific squares. You have to figure it all out, mark
all the shape squares with black, and press Guess to see if you were right.    

Good luck!
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"


/* ---------- Game generation ---------- */

#define ID_ON       2
#define ID_OFF      1
#define ID_UNKNOWN  0
#define ID_BLOCKED  3

#define ID_ROT0        0
#define ID_ROT0_MIR    1
#define ID_ROT90       2
#define ID_ROT90_MIR   3
#define ID_ROT180      4
#define ID_ROT180_MIR  5
#define ID_ROT270      6
#define ID_ROT270_MIR  7
#define ID_REFL_ORIG   1
#define ID_REFL_ROT   85
#define ID_REFL_MIR  153
#define ID_REFL_ALL  255
#define ID_REFL_SWAP 102

#define COMPLEXITY_LIMIT 64
#define MAKE_BOARD_TRIALS 200

#define PI_STRING "243F6A8885A308D313198A2E03707344A4093822299F31D0082EFA98EC4E6C89452821E638D01377BE5466CF34E90C6CC0AC29B7C97C50DD3F84D5B5B54709179216D5D98979FB1BD1310BA698DFB5AC2FFD72DBD01ADFB7B8E1AFED6A267E96BA7C9045F12C7F9924A19947B3916CF70801F2E2858EFC16636920D871574E69"

static int imax(int a, int b) { return (a>b ? a : b); };
static int imin(int a, int b) { return (a<b ? a : b); };

typedef struct Shape {
  int width, height;
  char* pix;
} Shape;

int ShapeWidth(Shape* sh, int reflbit)
{
  return (ID_REFL_SWAP & reflbit ? sh->height : sh->width);
}

int ShapeHeight(Shape* sh, int reflbit)
{
  return (ID_REFL_SWAP & reflbit ? sh->width : sh->height);
}

char ShapePix(Shape* sh, int x, int y, int reflbit)
{
  if (ID_REFL_SWAP & reflbit)
    return sh->pix[(60&reflbit ? sh->height-x : x+1)*(sh->width+2)+(240&reflbit ? sh->width-y : y+1)];
  else
    return sh->pix[(60&reflbit ? sh->width-x : x+1)+(240&reflbit ? sh->height-y : y+1)*(sh->width+2)];
};

void SetShapePix(Shape* sh, int x, int y, int reflbit, char val)
{
  if (ID_REFL_SWAP & reflbit)
    sh->pix[(60&reflbit ? sh->height-x : x+1)*(sh->width+2)+(240&reflbit ? sh->width-y : y+1)] = val;
  else
    sh->pix[(60&reflbit ? sh->width-x : x+1)+(240&reflbit ? sh->height-y : y+1)*(sh->width+2)] = val;
};

typedef struct ShapeConfig {
  int numcomp;
  int symmetry;
  int maxlev;
  int* lev;
  int* mult;
  int* id;
} ShapeConfig;

typedef struct ShapeAnswer {
  int refcount;
  int* shapeind;
  int** shapex;
  int** shapey;
  int** shapeb;
} ShapeAnswer;

typedef struct ShapeDict {
  int maxlevel;
  int toplevel;
  int totnum;
  int reflmask;
  int* len;
  Shape*** shapes;
} ShapeDict;

int ShapeDictNum(ShapeDict* sd, int lev)
{
  return sd->len[lev-1];
}

Shape* ShapeDictGet(ShapeDict* sd, int lev, int ind)
{
  return sd->shapes[lev-1][ind];
}

typedef struct DictStatistics {
  ShapeDict* dict;
  ShapeConfig* conf;
  ShapeAnswer* answer;
  int num; /* Total number of shapes in all comp levels */
  int bsize;
  Shape* board;
  int* lenposs;
  int* numposs;
  int* smask;
  int* np1;
  int* np2;
  char** poss;
  double* entr;
} DictStatistics;

typedef struct DictHyperIndex {
  DictStatistics* stat;
  Shape* origboard;
  Shape* shape;
  int comp;
  int mult;
  int shind0, nshape;
  int* orignumposs;
  int nposs;
  int shind;
  int* pos;
  int* xvec;
  int* yvec;
  int* bvec;
} DictHyperIndex;

Shape* make_unit_shape()
{
  Shape* shape = snew(Shape);
  shape->width = shape->height = 1;
  shape->pix = snewn(9, char);
  for (int i=0; i<9; i++)
    shape->pix[i] = (i==4 ? ID_ON : ID_OFF);
  return shape;
}

Shape* make_empty_board(int w, int h)
{
  Shape* shape = snew(Shape);
  shape->width = w;
  shape->height = h;
  shape->pix = snewn((w+2)*(h+2), char);
  for (int i=0; i<(w+2)*(h+2); i++)
    shape->pix[i] = ID_UNKNOWN;
  return shape;
}

Shape* make_incr_shape(Shape* initial, int addx, int addy)
{
  Shape* shape = snew(Shape);
  int npix;
  if (addx == -1 || addx == initial->width)
    shape->width = initial->width+1, shape->height = initial->height;
  else if (addy == -1 || addy == initial->height)
    shape->width = initial->width, shape->height = initial->height+1;
  else
    shape->width = initial->width, shape->height = initial->height;
  npix = (shape->width+2) * (shape->height+2);
  shape->pix = snewn(npix, char);
  for (int i=0, j=0; j<npix; j++)
    if (addx == -1 ? j%(shape->width+2)==0 :
        addx == initial->width ? j%(shape->width+2)==(shape->width+1) :
        addy == -1 ? j<(shape->width+2) :
        addy == initial->height ? j>=npix-(shape->width+2) :
        0)
      shape->pix[j] = ID_UNKNOWN;
    else
      shape->pix[j] = initial->pix[i++];
  if (addx == -1) addx = 0;
  if (addy == -1) addy = 0;
  for (int y=-1; y<=1; y++)
    for (int x=-1; x<=1; x++) {
      int ind = (addy + 1 + y)*(shape->width+2) + addx + x + 1;
      if (shape->pix[ind] != ID_ON)
        shape->pix[ind] = ID_OFF;
    }
  shape->pix[(addy+1)*(shape->width+2) + (addx+1)] = ID_ON;
  return shape;
}

int can_incr_shape(Shape* initial, int addx, int addy)
{
  return (initial->pix[(addy+1)*(initial->width+2) + (addx+1)] != ID_ON &&
          ((addx < initial->width && initial->pix[(addy+1)*(initial->width+2) + (addx+2)] == ID_ON) ||
           (addy < initial->height && initial->pix[(addy+2)*(initial->width+2) + (addx+1)] == ID_ON) ||
           (addx != -1 && initial->pix[(addy+1)*(initial->width+2) + (addx)] == ID_ON) ||
           (addy != -1 && initial->pix[(addy)*(initial->width+2) + (addx+1)] == ID_ON)));
}

void free_shape(Shape* shape)
{
  sfree(shape->pix);
  sfree(shape);
}

void reset_board(Shape* board, char val)
{
  for (int y=0; y<board->height; y++)
    for (int x=0; x<board->width; x++)
      SetShapePix(board, x, y, 1, val);
}

void copy_board(Shape* shape0, Shape* shape)
{
  if (shape0->width == shape->width && shape0->height == shape->height) {
    int sz = (shape0->width+2)*(shape0->height+2);
    for (int i=0; i<sz; i++)
      shape->pix[i] = shape0->pix[i];
  }
}

int count_board(Shape* board, char val)
{
  int count = 0;
  for (int y=0; y<board->height; y++)
    for (int x=0; x<board->width; x++)
      if (ShapePix(board, x, y, 1) == val)
        count++;
  return count;
}

Shape* copy_shape(Shape* shape)
{
  Shape* ret = snew(Shape);
  int n = (shape->width + 2) * (shape->height + 2);
  ret->width = shape->width;
  ret->height = shape->height;
  ret->pix = snewn(n, char);
  for (int i=0; i<n; i++)
    ret->pix[i] = shape->pix[i];
  return ret;
}

int same_shape(Shape* sh1, Shape* sh2, int reflmask)
{
  int smask = 0;
  if (sh1->width == sh2->width && sh1->height == sh2->height)
    smask |= ID_REFL_MIR;
  if (sh1->width == sh2->height && sh1->height == sh2->width)
    smask |= ID_REFL_SWAP;
  smask &= reflmask;
  if (!smask)
    return 0;
  for (int b=1, i=0; i<8; i++, b<<=1)
    if (smask&b) {
      for (int x=0; x<sh1->width; x++)
        for (int y=0; y<sh1->height; y++)
          if (ShapePix(sh1, x, y, 1) != ShapePix(sh2, x, y, b))
            goto donext;
      return 1;
    donext:
      ;
    }
  return 0;
}

ShapeDict* init_shape_dictionary(int maxlev, int reflmask)
{
  ShapeDict* dict = snew(ShapeDict);
  dict->maxlevel = maxlev;
  dict->toplevel = 1;
  dict->reflmask = reflmask;
  dict->len = snewn(maxlev, int);
  dict->shapes = snewn(maxlev, Shape**);
  dict->shapes[0] = snewn(1, Shape*);
  dict->shapes[0][0] = make_unit_shape();
  dict->len[0] = 1;
  dict->totnum = 1;
  return dict;
}

void extend_shape_dictionary(ShapeDict* dict, int lev)
{
  int worksize = 1000;
  int nr, newsize, i, j, k, x, y;
  Shape** work = snewn(worksize, Shape*);
  Shape** newwork;
  Shape* shape;
  for (i=dict->toplevel; i<lev; i++) {
    nr = 0;
    for (j=0; j<dict->len[i-1]; j++)
      for (x=-1; x<=dict->shapes[i-1][j]->width; x++)
        for (y=-1; y<=dict->shapes[i-1][j]->height; y++)
          if (can_incr_shape(dict->shapes[i-1][j], x, y)) {
            shape = make_incr_shape(dict->shapes[i-1][j], x, y);
            for (k=0; k<nr; k++)
              if (same_shape(work[k], shape, dict->reflmask))
                break;
            if (k<nr) {
              free_shape(shape);
              continue;
            } else {
              if (nr == worksize) {
                newsize = 2*worksize;
                newwork = snewn(newsize, Shape*);
                for (k=0; k<worksize; k++)
                  newwork[k] = work[k];
                sfree(work);
                work = newwork;
                worksize = newsize;
              }
              work[nr++] = shape;
            }
          }
    dict->shapes[i] = snewn(nr, Shape*);
    for (k=0; k<nr; k++)
      dict->shapes[i][k] = work[k];
    dict->len[i] = nr;
    dict->totnum += nr;
  }
  dict->toplevel = lev;
  sfree(work);
}

void free_shape_dictionary(ShapeDict* dict)
{
  int i, j;
  for (i=0; i<dict->maxlevel; i++) {
    for (j=0; j<dict->len[i]; j++)
      free_shape(dict->shapes[i][j]);
    sfree(dict->shapes[i]);
  }
  sfree(dict->shapes);
  sfree(dict->len);
  sfree(dict);
}

/*
Symmetry relations used below

   1 3 5 7
   0 2 4 6    10011001 =153   10101010 =85    11111111 =255

-: B C D A    10001000 =17    10101010 =85    10101010 =85, 11001100
   A B C D

|: D A B C    10001000 =17    10101010 =85    10101010 =85, 11001100
   A B C D

+: B A B A    10000000 =1     10100000 =5     10100000 =5, 11000000
   A B A B

\: C D A B    10011001 =153   10101010 =85   10011001 =153, 10101010 =85, 11110000
   A B C D

/: A B C D    10011001 =153   10101010 =85   10011001 =153, 10101010 =85, 01010101
   A B C D

X: A B A B    10010000 =9     10100000 =5    10010000 =9, 10100000 =5
   A B A B

O: C D C D    10010000 =9     10100000 =5    11110000 =15
   A B A B

C: B B B B    10010000 =9    10000000 =1    11000000 =3
   A A A A

 */

void calc_needed_positions(Shape* shape, Shape* board, int reflmask, int* smask, int* nr1, int* nr2, int* np1, int* np2)
{
  /* Find the symmetry class of the shape */
  int s1=0, s2=0;
  if (reflmask==ID_REFL_ORIG)
    *smask = 1, *nr1 = 1, *nr2 = 0;
  else {
    if (same_shape(shape, shape, 128)) s1++;
    if (same_shape(shape, shape, 8)) s1++;
    if (same_shape(shape, shape, 32)) s2++;
    if (same_shape(shape, shape, 2)) s2++;
    if (s1+s2) {
      if (s1+s2 == 4)
        *smask = 1, *nr1 = 1, *nr2 = 0;
      else if (s1) {
        if (s1 == 2)
          if (reflmask == ID_REFL_MIR)
            *smask = 1, *nr1 = 1, *nr2 = 0;
          else
            *smask = 5, *nr1 = 1, *nr2 = 1;
        else
          if (reflmask == ID_REFL_MIR)
            *smask = 17, *nr1 = 2, *nr2 = 0;
          else
            *smask = 85, *nr1 = 2, *nr2 = 2;
      } else {
        if (s2 == 2)
          if (reflmask == ID_REFL_ROT)
            *smask = 5, *nr1 = 1, *nr2 = 1;
          else
            *smask = 9, *nr1 = 2, *nr2 = 0;
        else
          if (reflmask == ID_REFL_ROT)
            *smask = 85, *nr1 = 2, *nr2 = 2;
          else
            *smask = 153, *nr1 = 4, *nr2 = 0;
      }
    } else if (same_shape(shape, shape, 4)) {
      if (reflmask == ID_REFL_MIR)
        *smask = 9, *nr1 = 2, *nr2 = 0;
      else if (reflmask == ID_REFL_ROT)
        *smask = 1, *nr1 = 1, *nr2 = 0;
      else
        *smask = 9, *nr1 = 2, *nr2 = 0;
    } else if (same_shape(shape, shape, 16)) {
      if (reflmask == ID_REFL_MIR)
        *smask = 9, *nr1 = 2, *nr2 = 0;
      else if (reflmask == ID_REFL_ROT)
        *smask = 5, *nr1 = 1, *nr2 = 1;
      else
        *smask = 15, *nr1 = 2, *nr2 = 2;
    } else {
      *smask = reflmask;
      if (reflmask == ID_REFL_MIR)
        *nr1 = 4, *nr2 = 0;
      else if (reflmask == ID_REFL_ROT)
        *nr1 = 2, *nr2 = 2;
      else
        *nr1 = 4, *nr2 = 4;
    }
  }
  *np1 = (board->width - shape->width + 1)*(board->height - shape->height + 1);
  *np2 = (board->width - shape->height + 1)*(board->height - shape->width + 1);
  if (*np1 < 0) *np1 = 0;
  if (*np2 < 0) *np2 = 0;
}

void mark_inconsistent(Shape* board, Shape* shape, int smask, int np1, int np2, char* poss)
{
  int bpix, spix;
  for (int y=0; y<board->height; y++)
    for (int x=0; x<board->width; x++)
      if ((bpix = ShapePix(board, x, y, 1)) != ID_UNKNOWN) {
        int ir1=0, ir2=0;
        for (int b=1, i=0; i<8; i++, b<<=1)
          if (smask&b) {
            for (int yy = imax(y+ShapeHeight(shape, b)-board->height, -1); yy <= imin(ShapeHeight(shape, b), y); yy++)
              for (int xx = imax(x+ShapeWidth(shape, b)-board->width, -1); xx <= imin(ShapeWidth(shape, b), x); xx++)
                if ((spix = ShapePix(shape, xx, yy, b)) != ID_UNKNOWN && spix != bpix)
                  poss[np1*ir1 + np2*ir2 + (y-yy) * (board->width-ShapeWidth(shape, b)+1) + (x-xx)] = 0;
            if (ID_REFL_SWAP & b)
              ir2++;
            else
              ir1++;
            }
      }
}

void accumulate_possibilities(Shape* board, Shape* shape, int smask, int np1, int np2, char* poss, int* bpos, int* bneg)
{
  char spix;
  for (int y=0; y<board->height; y++)
    for (int x=0; x<board->width; x++)
      if (ShapePix(board, x,y,1) == ID_UNKNOWN) {
        int ir1=0, ir2=0;
        for (int b=1, i=0; i<8; i++, b<<=1)
          if (smask&b) {
            for (int yy = imax(y+ShapeHeight(shape, b)-board->height, -1); yy <= imin(ShapeHeight(shape, b), y); yy++)
              for (int xx = imax(x+ShapeWidth(shape, b)-board->width, -1); xx <= imin(ShapeWidth(shape, b), x); xx++)
                if ((spix = ShapePix(shape, xx,yy,b)) != ID_UNKNOWN &&
                    poss[np1*ir1 + np2*ir2 + (y-yy) * (board->width-ShapeWidth(shape, b)+1) + (x-xx)] != 0) {
                  if (spix == ID_ON)
                    bpos[y*board->width + x]++;
                  else
                    bneg[y*board->width + x]++;
                }
            if (ID_REFL_SWAP & b)
              ir2++;
            else
              ir1++;
          }
      }
}

int check_inconsistent(Shape* board, Shape* shape, int bit, int x, int y)
{
  char px;
  for (int yy=-1; yy<=ShapeHeight(shape, bit); yy++)
    for (int xx=-1; xx<=ShapeWidth(shape, bit); xx++)
      if ((px = ShapePix(shape, xx, yy, bit)) != ID_UNKNOWN)
        if (ShapePix(board, xx+x, yy+y, 1) != px && ShapePix(board, xx+x, yy+y, 1) != ID_UNKNOWN)
          return 1;
  return 0;
}

void copy_to_board(Shape* board, Shape* shape, int bit, int x, int y, char val)
{
  char px;
  for (int yy=-1; yy<=ShapeHeight(shape, bit); yy++)
    for (int xx=-1; xx<=ShapeWidth(shape, bit); xx++)
      if ((px = ShapePix(shape, xx, yy, bit)) != ID_UNKNOWN && xx+x >= 0 && xx+x < ShapeWidth(board, 1) && yy+y >= 0 && yy+y < ShapeHeight(board, 1))
        SetShapePix(board, xx+x, yy+y, 1, (px==ID_ON ? val : px));
}

int add_random_board_shape(Shape* board, Shape* shape, int reflmask, int num, random_state *rs)
{
  int smask, nr1, nr2, np1, np2;
  calc_needed_positions(shape, board, reflmask, &smask, &nr1, &nr2, &np1, &np2);
  
  int b, i, ww;
  int possnum, posslen = nr1*np1+nr2*np2;
  char* poss = snewn(posslen, char);
  int pick;
  for (int i=0; i<posslen; i++) poss[i] = 1;
  while (num--) {
    mark_inconsistent(board, shape, smask, np1, np2, poss);
    for (possnum=0, i=0; i<posslen; i++)
      if (poss[i]) possnum++;
    if (possnum == 0) {
      sfree(poss);
      return 0;
    }
    pick = random_upto(rs, possnum) + 1;
    for (i=0; i<posslen; i++)
      if (poss[i]) {
        pick--;
        if (!pick) break;
      }
    pick=i;
    for (b=1, i=0; i<8; i++, b<<=1)
      if (smask & b) {
        if (pick >= (ID_REFL_SWAP & b ? np2 : np1))
          pick -= (ID_REFL_SWAP & b ? np2 : np1);
        else
          break;
      }
    ww = (board->width - ShapeWidth(shape, b) + 1);
    copy_to_board(board, shape, b, pick%ww, pick/ww, ID_BLOCKED);
  }
  sfree(poss);
  return 1;
}

Shape* make_random_board(ShapeDict* dict, ShapeConfig* conf, int w, int h, int storeshapes, random_state *rs)
{
  int sameshape[conf->numcomp];
  int numsame = 0;
  int ind;
  Shape* shape;
  Shape* board = make_empty_board(w, h);
  for (int i=0; i<conf->numcomp; i++) {
    if (conf->id[i] > -1) {
      ind = conf->id[i];
      sameshape[0] = ind;
      numsame = 0;
    } else if (i>0 && conf->lev[i] == conf->lev[i-1]) {
      /* Prevent the same shape in different components */
      int j;
      numsame++;
      if (ShapeDictNum(dict, conf->lev[i]) <= numsame) {
        free_shape(board);
        return 0;
      }
      ind = random_upto(rs, ShapeDictNum(dict, conf->lev[i])-numsame);
      for (j=0; j<numsame; j++)
        if (ind>=sameshape[j])
          ind++;
      for (j=numsame-1; j>=0 && sameshape[j]>ind; j--)
        sameshape[j+1] = sameshape[j];
      sameshape[j+1] = ind;
    } else {
      ind = random_upto(rs, ShapeDictNum(dict, conf->lev[i]));
      sameshape[0] = ind;
      numsame = 0;
    }
    if (storeshapes)
      conf->id[i] = ind;
    shape = ShapeDictGet(dict, conf->lev[i], ind);
    if (!add_random_board_shape(board, shape, dict->reflmask, conf->mult[i], rs)) {
      free_shape(board);
      return 0;
    }
  }
  for (int i=0; i<(board->width+2)*(board->height+2); i++)
    if (board->pix[i] == ID_BLOCKED)
      board->pix[i] = ID_ON;
    else
      board->pix[i] = ID_OFF;
  return board;
}

/*
Shape* try_make_random_board(ShapeDict* dict, ShapeConfig* conf, int w, int h, int maxiter, random_state *rs)
{
  Shape* board;
  int iter = 0;
  while (!(board = make_random_board(dict, conf, w, h, rs)) && iter++ < maxiter);
  return board;
}
*/

void print_shape(Shape* shape, int reflbit)
{
  for (int y=0; y<ShapeHeight(shape, reflbit); y++) {
    for (int x=0; x<ShapeWidth(shape, reflbit); x++)
      printf(ShapePix(shape, x,y,reflbit) == ID_ON ? "# " : ShapePix(shape, x,y,reflbit) == ID_BLOCKED ? "X " : "  ");
    printf("\n");
  }
}

void print_dictionary(ShapeDict* dict, int lev)
{
  for (int k=0; k<dict->len[lev-1]; k++) {
    printf("\n%d:\n", k);
    print_shape(dict->shapes[lev-1][k], 1);
  }
}

ShapeAnswer* init_shape_answer(ShapeConfig* conf)
{
  int i;
  ShapeAnswer* ans = snew(ShapeAnswer);
  ans->refcount = 1;
  ans->shapeind = snewn(conf->numcomp, int);
  ans->shapex = snewn(conf->numcomp, int*);
  ans->shapey = snewn(conf->numcomp, int*);
  ans->shapeb = snewn(conf->numcomp, int*);
  for (i=0; i<conf->numcomp; i++) {
    ans->shapeind[i] = -1;
    ans->shapex[i] = snewn(conf->mult[i], int);
    ans->shapey[i] = snewn(conf->mult[i], int);
    ans->shapeb[i] = snewn(conf->mult[i], int);
  }
  return ans;
}

void free_shape_answer(ShapeAnswer* ans, int numcomp)
{
  int i;
  for (i=0; i<numcomp; i++) {
    sfree(ans->shapex[i]);
    sfree(ans->shapey[i]);
    sfree(ans->shapeb[i]);
  }
  sfree(ans->shapex);
  sfree(ans->shapey);
  sfree(ans->shapeb);
  sfree(ans->shapeind);
  sfree(ans);
}

void dict_statistics_constrain_shapes(DictStatistics* stat)
{
  for (int k=0, j=0; k<stat->conf->numcomp; j+=ShapeDictNum(stat->dict, stat->conf->lev[k]), k++)
    if (stat->conf->id[k] != -1) {
      int jk, n = ShapeDictNum(stat->dict, stat->conf->lev[k]);
      for (jk=0; jk<n; jk++)
        if (jk != stat->conf->id[k])
          stat->numposs[j+jk] = 0;
      jk = stat->conf->id[k];
      if (jk < n)
        for (int kk=k+1; kk<stat->conf->numcomp && stat->conf->lev[k] == stat->conf->lev[kk]; kk++)
          stat->numposs[j + n*(kk-k) + jk] = 0;
    }
}

void dict_statistics_break_symmetry(DictStatistics* stat)
{
  /* This takes care of breaking symmetry if two components have same lev and mult */
  for (int k=0, j=0; k<stat->conf->numcomp-1; j+=ShapeDictNum(stat->dict, stat->conf->lev[k]), k++)
    if (stat->conf->lev[k] == stat->conf->lev[k+1] &&
        stat->conf->id[k] == -1 &&
        stat->conf->mult[k] == stat->conf->mult[k+1]) {
      int jk, n = ShapeDictNum(stat->dict, stat->conf->lev[k]);
      for (jk=0; jk<n && !stat->numposs[j+jk]; jk++)
        stat->numposs[j+n+jk] = 0;
      if (jk<n) stat->numposs[j+n+jk] = 0;
    }
  for (int k=stat->conf->numcomp-1, j=stat->num-ShapeDictNum(stat->dict, stat->conf->lev[k]); k>0; k--, j-=ShapeDictNum(stat->dict, stat->conf->lev[k]))
    if (stat->conf->lev[k] == stat->conf->lev[k-1] &&
        stat->conf->id[k-1] == -1 &&
        stat->conf->mult[k] == stat->conf->mult[k-1]) {
      int jk, n = ShapeDictNum(stat->dict, stat->conf->lev[k]);
      for (jk=n-1; jk>=0 && !stat->numposs[j+jk]; jk--)
        stat->numposs[j-n+jk] = 0;
      if (jk>=0) stat->numposs[j-n+jk] = 0;
    }
}

DictStatistics* init_dict_statistics(ShapeDict* dict, ShapeConfig* conf, int w, int h)
{
  int nr1, nr2, np1, np2, tot;
  DictStatistics* stat = snew(DictStatistics);
  stat->dict = dict;
  stat->conf = conf;
  stat->answer = init_shape_answer(conf);
  stat->num = 0;
  for (int i=0; i<conf->numcomp; i++)
    stat->num += ShapeDictNum(dict, conf->lev[i]);
  stat->bsize = w * h;
  stat->board = make_empty_board(w, h);
  stat->lenposs = snewn(stat->num, int);
  stat->numposs = snewn(stat->num, int);
  stat->smask = snewn(stat->num, int);
  stat->np1 = snewn(stat->num, int);
  stat->np2 = snewn(stat->num, int);
  stat->poss = snewn(stat->num, char*);
  for (int j=0, jk=0, k=0; j<stat->num; j++, jk++) {
    if (jk==ShapeDictNum(dict, conf->lev[k]))
      jk = 0, k++;
    calc_needed_positions(ShapeDictGet(dict, conf->lev[k], jk), stat->board, dict->reflmask, &stat->smask[j], &nr1, &nr2, &np1, &np2);
    tot = nr1*np1 + nr2*np2;
    stat->lenposs[j] = stat->numposs[j] = tot;
    stat->np1[j] = np1;
    stat->np2[j] = np2;
    stat->poss[j] = snewn(tot, char);
    for (int i=0; i<tot; i++) stat->poss[j][i] = 1;
  }
  dict_statistics_constrain_shapes(stat);
  stat->entr = snewn(stat->bsize, double);
  for (int i=0; i<stat->bsize; i++) stat->entr[i] = 0.0;
  return stat;
}

void free_dict_statistics(DictStatistics* stat)
{
  stat->answer->refcount--;
  if (!stat->answer->refcount)
    free_shape_answer(stat->answer, stat->conf->numcomp);
  for (int i=0; i<stat->num; i++) {
    sfree(stat->poss[i]);
  }
  sfree(stat->lenposs);
  sfree(stat->numposs);
  sfree(stat->poss);
  sfree(stat->smask);
  sfree(stat->np1);
  sfree(stat->np2);
  sfree(stat->entr);
  free_shape(stat->board);
  sfree(stat);
}

DictStatistics* copy_dict_statistics(DictStatistics* stat0)
{
  int nr1, nr2, np1, np2, tot;
  DictStatistics* stat = snew(DictStatistics);
  stat->dict = stat0->dict;
  stat->conf = stat0->conf;
  stat->answer = stat0->answer;
  stat->answer->refcount++;
  stat->num = stat0->num;
  stat->bsize = stat0->bsize;
  stat->board = copy_shape(stat0->board);
  stat->lenposs = snewn(stat->num, int);
  stat->numposs = snewn(stat->num, int);
  stat->smask = snewn(stat->num, int);
  stat->np1 = snewn(stat->num, int);
  stat->np2 = snewn(stat->num, int);
  stat->poss = snewn(stat->num, char*);
  for (int j=0, jk=0, k=0; j<stat->num; j++, jk++) {
    if (jk==ShapeDictNum(stat->dict, stat->conf->lev[k]))
      jk = 0, k++;
    calc_needed_positions(ShapeDictGet(stat->dict, stat->conf->lev[k], jk), stat->board, stat->dict->reflmask, &stat->smask[j], &nr1, &nr2, &np1, &np2);
    tot = nr1*np1 + nr2*np2;
    stat->lenposs[j] = stat0->lenposs[j];
    stat->numposs[j] = stat0->numposs[j];
    stat->np1[j] = stat0->np1[j];
    stat->np2[j] = stat0->np2[j];
    stat->poss[j] = snewn(tot, char);
    for (int i=0; i<tot; i++) stat->poss[j][i] = stat0->poss[j][i];
  }
  stat->entr = snewn(stat->bsize, double);
  for (int i=0; i<stat->bsize; i++) stat->entr[i] = stat0->entr[i];
  return stat;
}

void dict_statistics_update_poss(DictStatistics* stat, int x, int y, char val)
{
  int spix;
  SetShapePix(stat->board, x, y, 1, val);
  for (int j=0, jk=0, k=0; j<stat->num; j++, jk++) {
    if (jk==ShapeDictNum(stat->dict, stat->conf->lev[k]))
      jk = 0, k++;
    if (stat->numposs[j]) {
      Shape* shape = ShapeDictGet(stat->dict, stat->conf->lev[k], jk);
      int ir1=0, ir2=0;
      for (int b=1, i=0; i<8; i++, b<<=1)
        if (stat->smask[j]&b) {
          for (int yy = imax(y+ShapeHeight(shape, b)-stat->board->height, -1); yy <= imin(ShapeHeight(shape, b), y); yy++)
            for (int xx = imax(x+ShapeWidth(shape, b)-stat->board->width, -1); xx <= imin(ShapeWidth(shape, b), x); xx++)
              if ((spix = ShapePix(shape, xx,yy,b)) != ID_UNKNOWN && spix != val) {
                int ind = stat->np1[j]*ir1 + stat->np2[j]*ir2 + (y-yy) * (stat->board->width-ShapeWidth(shape, b)+1) + (x-xx);
                if (stat->poss[j][ind])
                  stat->poss[j][ind] = 0, stat->numposs[j]--;
              }
          if (stat->numposs[j] < stat->conf->mult[k])
            stat->numposs[j] = 0;
          if (ID_REFL_SWAP & b)
            ir2++;
          else
            ir1++;
        }
    }
  }
  dict_statistics_break_symmetry(stat);
}

static DictHyperIndex* make_hyper_index(DictStatistics* st, int ci)
{
  int i, maxnposs = 0;
  DictHyperIndex* dhi = snew(DictHyperIndex);
  dhi->stat = st;
  dhi->comp = ci;
  dhi->shape = 0;
  dhi->shind = -1;
  dhi->shind0 = 0;
  for (i=0; i<dhi->comp; i++)
    dhi->shind0 += ShapeDictNum(dhi->stat->dict, dhi->stat->conf->lev[i]);
  dhi->nshape = ShapeDictNum(dhi->stat->dict, dhi->stat->conf->lev[dhi->comp]);
  dhi->mult = dhi->stat->conf->mult[dhi->comp];
  dhi->origboard = copy_shape(dhi->stat->board);
  dhi->orignumposs = snewn(dhi->stat->num, int);
  for (i=0; i<dhi->stat->num; i++)
    dhi->orignumposs[i] = dhi->stat->numposs[i];
  dhi->nposs = 0;
  dhi->pos = snewn(dhi->mult, int);
  for (i=0; i<dhi->nshape; i++)
    if (dhi->stat->numposs[dhi->shind0+i] > maxnposs)
      maxnposs = dhi->stat->numposs[dhi->shind0+i];
  dhi->xvec = snewn(maxnposs, int);
  dhi->yvec = snewn(maxnposs, int);
  dhi->bvec = snewn(maxnposs, int);
  return dhi;
}

static void free_hyper_index(DictHyperIndex* dhi)
{
  free_shape(dhi->origboard);
  sfree(dhi->orignumposs);
  sfree(dhi->pos);
  sfree(dhi->xvec);
  sfree(dhi->yvec);
  sfree(dhi->bvec);
  sfree(dhi);
}

static int next_hyper_index(DictHyperIndex* dhi)
{
  int smask, np1, np2, ii;
  while (1) {
    if (dhi->shind != -1)
      for (ii=dhi->mult-1; ii>=0 && dhi->pos[ii]==dhi->nposs+ii-dhi->mult; ii--);
    else
      ii = -1;
    if (ii >= 0) {
      dhi->pos[ii]++;
      for (ii++; ii<dhi->mult; ii++)
        dhi->pos[ii] = dhi->pos[ii-1]+1;
    } else {
      for (int i=0; i<dhi->stat->num; i++)
        dhi->stat->numposs[i] = dhi->orignumposs[i];
      for (dhi->shind=dhi->shind+1; dhi->shind<dhi->nshape && (dhi->stat->numposs[dhi->shind0+dhi->shind] < dhi->mult); dhi->shind++);
      if (dhi->shind==dhi->nshape)
        return 0;
      dhi->shape = ShapeDictGet(dhi->stat->dict, dhi->stat->conf->lev[dhi->comp], dhi->shind);
      np1 = dhi->stat->np1[dhi->shind0+dhi->shind];
      np2 = dhi->stat->np2[dhi->shind0+dhi->shind];
      smask = dhi->stat->smask[dhi->shind0+dhi->shind];
      dhi->nposs = dhi->stat->numposs[dhi->shind0+dhi->shind];
      for (int i=0; i<dhi->stat->lenposs[dhi->shind0+dhi->shind]; i++)
        dhi->stat->poss[dhi->shind0+dhi->shind][i] = 1;
      mark_inconsistent(dhi->origboard, dhi->shape, dhi->stat->smask[dhi->shind0+dhi->shind], dhi->stat->np1[dhi->shind0+dhi->shind], dhi->stat->np2[dhi->shind0+dhi->shind], dhi->stat->poss[dhi->shind0+dhi->shind]);
      for (ii=0; ii<dhi->mult; ii++)
        dhi->pos[ii] = ii;
      ii=0;
      for (int b=1, i=0, j0=0; i<8; i++, b<<=1)
        if (smask&b) {
          for (int j=0, y=0; j<(ID_REFL_SWAP & b ? np2 : np1); y++)
            for (int x=0; x<(dhi->stat->board->width-ShapeWidth(dhi->shape, b)+1); x++, j++)
              if (dhi->stat->poss[dhi->shind0+dhi->shind][j0+j]) {
                dhi->xvec[ii] = x;
                dhi->yvec[ii] = y;
                dhi->bvec[ii] = b;
                ii++;
              }
          j0 += (ID_REFL_SWAP & b ? np2 : np1);
        }
    }
    copy_board(dhi->origboard, dhi->stat->board);
    for (ii=0; ii<dhi->mult; ii++) {
      if (check_inconsistent(dhi->stat->board, dhi->shape, dhi->bvec[dhi->pos[ii]], dhi->xvec[dhi->pos[ii]], dhi->yvec[dhi->pos[ii]]))
        break;
      copy_to_board(dhi->stat->board, dhi->shape, dhi->bvec[dhi->pos[ii]], dhi->xvec[dhi->pos[ii]], dhi->yvec[dhi->pos[ii]], ID_ON);
    }
    if (ii==dhi->mult) {
      for (int k=0, j0=0, n=0; k<dhi->stat->conf->numcomp; j0+=n, k++) {
        n = ShapeDictNum(dhi->stat->dict, dhi->stat->conf->lev[k]);
        if (k == dhi->comp) {
          for (int jk=0; jk<n; jk++)
            dhi->stat->numposs[j0+jk] = (jk==dhi->shind ? dhi->mult : 0);
        } else {
          for (int jk=0; jk<n; jk++)
            if (dhi->orignumposs[j0+jk]) {
              int nn=0;
              for (int i=0; i<dhi->stat->lenposs[j0+jk]; i++)
                dhi->stat->poss[j0+jk][i] = 1;
              mark_inconsistent(dhi->stat->board, ShapeDictGet(dhi->stat->dict, dhi->stat->conf->lev[k], jk), dhi->stat->smask[j0+jk], dhi->stat->np1[j0+jk], dhi->stat->np2[j0+jk], dhi->stat->poss[j0+jk]);
              for (int i=0; i<dhi->stat->lenposs[j0+jk]; i++)
                if (dhi->stat->poss[j0+jk][i])
                  nn++;
              dhi->stat->numposs[j0+jk] = nn;
            }
          if (dhi->stat->conf->lev[k] == dhi->stat->conf->lev[dhi->comp])
            dhi->stat->numposs[j0+dhi->shind] = 0;
        }
      }
      dict_statistics_constrain_shapes(dhi->stat);
      dict_statistics_break_symmetry(dhi->stat);
      return 1;
    }
  }
}

long long over(int n, int m)
{
  long long res = 1;
  for (int k=1; k<=m; n--, k++)
    res = res*n/k;
  return res;
}

int dict_statistics_calc_entropy(DictStatistics* stat, int climit)
{
  DictStatistics** statvec = snewn(stat->conf->numcomp+1, DictStatistics*);
  DictHyperIndex** hindex = snewn(stat->conf->numcomp, DictHyperIndex*);
  int* hicomp = snewn(stat->conf->numcomp, int);
  int curr;
  double* sumpos = snewn(stat->bsize, double);
  double* sumneg = snewn(stat->bsize, double);
  double* prob = snewn(stat->bsize, double);
  double norm;
  norm = 0.0;
  for (int i=0; i<stat->bsize; i++)
    prob[i] = 0.0, sumpos[i] = sumneg[i] = 0;
  curr = 0;
  statvec[0] = stat;
  while(1) {
    if (curr < stat->conf->numcomp) {
      long long mincmpl = -1;
      int mink = -1;
      for (int k=0, j0=0, n=0; k<stat->conf->numcomp; j0+=n, k++) {
        long long cmpl;
        int ii=0;
        n = ShapeDictNum(stat->dict, stat->conf->lev[k]);
        for (ii=0; ii<curr && hicomp[ii] != k; ii++);
        if (ii<curr)
          continue;
        cmpl = 0;
        for (int jk=0; jk<n; jk++) {
          if (statvec[curr]->numposs[j0+jk] >= stat->conf->mult[k])
            cmpl += over(statvec[curr]->numposs[j0+jk], stat->conf->mult[k]);
        }
        if (mink == -1 || mincmpl > cmpl)
          mincmpl = cmpl, mink = k;
      }
      if (mincmpl < climit) {
        statvec[curr+1] = copy_dict_statistics(statvec[curr]);
        hindex[curr] = make_hyper_index(statvec[curr+1], mink);
        hicomp[curr] = mink;
        curr++;
      } else {
        long long tmp;
        int* bpos = snewn(statvec[curr]->bsize, int);
        int* bneg = snewn(statvec[curr]->bsize, int);
        for (int k=0, j0=0, n=0; k<stat->conf->numcomp; j0+=n, k++) {
          int ii=0;
          n = ShapeDictNum(stat->dict, stat->conf->lev[k]);
          for (ii=0; ii<curr && hicomp[ii] != k; ii++);
          if (ii<curr)
            continue;
          for (int jk=0; jk<n; jk++)
            if (statvec[curr]->numposs[j0+jk] >= stat->conf->mult[k]) {
              char px;
              Shape* shape = ShapeDictGet(stat->dict, stat->conf->lev[k], jk);
              for (int i=0; i<statvec[curr]->bsize; i++)
                bpos[i] = bneg[i] = 0;
              accumulate_possibilities(statvec[curr]->board, shape, statvec[curr]->smask[j0+jk], statvec[curr]->np1[j0+jk], statvec[curr]->np2[j0+jk], statvec[curr]->poss[j0+jk], bpos, bneg);
              tmp = over(statvec[curr]->numposs[j0+jk], stat->conf->mult[k]);
              norm += tmp;
              for (int i=0; i<statvec[curr]->bsize; i++) {
                px = ShapePix(stat->board, i%stat->board->width, i/stat->board->width, 1);
                prob[i] += tmp*imin(statvec[curr]->numposs[j0+jk], stat->conf->mult[k]*bpos[i])/((double)statvec[curr]->numposs[j0+jk]);
                sumpos[i] += (bpos[i]==0 ? 0 : over(statvec[curr]->numposs[j0+jk] - bneg[i], stat->conf->mult[k]));
                sumneg[i] += (px==ID_ON ? 0 : over(statvec[curr]->numposs[j0+jk] - bpos[i], stat->conf->mult[k]));
              }
            }
        }
        sfree(bpos);
        sfree(bneg);
      }
    } else {
      int brnumon = 0, exnumon = 0;
      for (int i=0; i<stat->conf->numcomp; i++)
        exnumon += stat->conf->lev[i]*stat->conf->mult[i];
      for (int i=0, y=0; i<statvec[curr]->bsize; y++)
        for (int x=0; x<statvec[curr]->board->width; x++, i++) {
          if (ShapePix(statvec[curr]->board, x,y,1) == ID_ON)
            brnumon += 1;
        }
      if (exnumon == brnumon) {
        norm += 1;
        for (int i=0, y=0; i<statvec[curr]->bsize; y++)
          for (int x=0; x<statvec[curr]->board->width; x++, i++) {
            if (ShapePix(statvec[curr]->board, x,y,1) == ID_ON) {
              prob[i] += 1;
              sumpos[i] += 1;
            } else {
              sumneg[i] += 1;
            }
          }
        if (norm == 1.0) {
          for (int i=0; i<curr; i++) {
            stat->answer->shapeind[hicomp[i]] = hindex[i]->shind;
            for (int j=0; j<stat->conf->mult[hicomp[i]]; j++) {
              stat->answer->shapex[hicomp[i]][j] = hindex[i]->xvec[hindex[i]->pos[j]];
              stat->answer->shapey[hicomp[i]][j] = hindex[i]->yvec[hindex[i]->pos[j]];
              stat->answer->shapeb[hicomp[i]][j] = hindex[i]->bvec[hindex[i]->pos[j]];
            }
          }
        } else {
          for (int i=0; i<curr; i++)
            stat->answer->shapeind[hicomp[i]] = -1;
        }
      }
    }
    while (curr > 0 && !next_hyper_index(hindex[curr-1])) {
      free_dict_statistics(statvec[curr]);
      curr--;
      free_hyper_index(hindex[curr]);
    }
    if (curr == 0)
      break;
  }
  if (norm > 0.0) {
    for (int i=0; i<stat->bsize; i++) {
      prob[i] /= norm;
      if (prob[i]==0.0 || prob[i]==1.0 || sumpos[i]==0 || sumneg[i]==0)
        stat->entr[i] = 0.0;
      else
        stat->entr[i] = log(norm) - prob[i]*log(sumpos[i]) - (1.0-prob[i])*log(sumneg[i]);
    }
  }
  sfree(sumpos);
  sfree(sumneg);
  sfree(prob);
  sfree(hicomp);
  sfree(statvec);
  sfree(hindex);
  return (norm == 0.0 ? -1 : norm == 1.0 ? 1 : 0);
}

void dict_statistics_pick_best_entropy(DictStatistics* stat, Shape* solboard, char solval, random_state *rs, double* entr, int* x, int* y)
{
  double maxentr;
  int cntmaxentr, pick;
  maxentr = 0.0;
  cntmaxentr = 0;
  if (solboard && solval) { /* Solitaire game - only consider OFF or ON squares */
    for (int i=0; i<stat->bsize; i++) {
      if (ShapePix(solboard, i%solboard->width, i/solboard->width, 1) != solval)
        continue;
      if (stat->entr[i] >= maxentr) {
        if (stat->entr[i] == maxentr)
          cntmaxentr++;
        else
          maxentr = stat->entr[i], cntmaxentr = 1;
      }
    }
    pick = random_upto(rs, cntmaxentr);
    for (int i=0; i<stat->bsize; i++)
      if (stat->entr[i] == maxentr) {
        if (ShapePix(solboard, i%solboard->width, i/solboard->width, 1) != solval)
          continue;
        if (pick == 0) {
          *entr = maxentr;
          *x = i%stat->board->width;
          *y = i/stat->board->width;
          break;
        } else
          pick--;
      }
  } else {
    for (int i=0; i<stat->bsize; i++) {
      if (stat->entr[i] >= maxentr) {
        if (stat->entr[i] == maxentr)
          cntmaxentr++;
        else
          maxentr = stat->entr[i], cntmaxentr = 1;
      }
    }
    pick = random_upto(rs, cntmaxentr);
    for (int i=0; i<stat->bsize; i++)
      if (stat->entr[i] == maxentr) {
        if (pick == 0) {
          *entr = maxentr;
          *x = i%stat->board->width;
          *y = i/stat->board->width;
          break;
        } else
          pick--;
      }
  }
}

void dict_statistics_fill_board(DictStatistics* stat)
{
  int j, k, b, x, y;
  reset_board(stat->board, ID_OFF);
  for (k=0; k<stat->conf->numcomp; k++) {
    if (stat->answer->shapeind[k] != -1) {
      Shape* shape = ShapeDictGet(stat->dict, stat->conf->lev[k], stat->answer->shapeind[k]);
      for (j=0; j<stat->conf->mult[k]; j++) {
        x = stat->answer->shapex[k][j];
        y = stat->answer->shapey[k][j];
        b = stat->answer->shapeb[k][j];
        copy_to_board(stat->board, shape, b, x, y, ID_ON);
      }
    }
  }
}

void dict_statistics_prune_superfluous(DictStatistics* stat, random_state *rs)
{
  int sz = stat->board->width * stat->board->height;
  int* order = snewn(sz, int);
  for (int i=0; i<sz; i++)
    order[i]=i;
  for (int i=0; i<sz; i++) {
    int tmp, j = random_upto(rs, sz-i) + i;
    tmp = order[i], order[i] = order[j], order[j] = tmp;
  }
  for (int i=0; i<sz; i++) {
    int x = order[i] % stat->board->width;
    int y = order[i] / stat->board->width;
    char px = ShapePix(stat->board, x, y, 1);
    if (px != ID_UNKNOWN) {
      SetShapePix(stat->board, x, y, 1, ID_UNKNOWN);
      for (int k=0, j0=0, n=0; k<stat->conf->numcomp; j0+=n, k++) {
        n = ShapeDictNum(stat->dict, stat->conf->lev[k]);
        for (int jk=0; jk<n; jk++) {
          int nn=0;
          for (int ii=0; ii<stat->lenposs[j0+jk]; ii++)
            stat->poss[j0+jk][ii] = 1;
          mark_inconsistent(stat->board, ShapeDictGet(stat->dict, stat->conf->lev[k], jk), stat->smask[j0+jk], stat->np1[j0+jk], stat->np2[j0+jk], stat->poss[j0+jk]);
          for (int ii=0; ii<stat->lenposs[j0+jk]; ii++)
            if (stat->poss[j0+jk][ii])
              nn++;
          stat->numposs[j0+jk] = nn;
        }
      }
      dict_statistics_constrain_shapes(stat);
      dict_statistics_break_symmetry(stat);
      if (dict_statistics_calc_entropy(stat, COMPLEXITY_LIMIT) != 1) {
        SetShapePix(stat->board, x, y, 1, px);
      }
    }
  }
  sfree(order);
}

ShapeConfig* make_shape_config(int ncomp)
{
  ShapeConfig* conf = snew(ShapeConfig);
  conf->numcomp = ncomp;
  conf->lev = snewn(ncomp, int);
  conf->mult = snewn(ncomp, int);
  conf->id = snewn(ncomp, int);
  return conf;
}

void free_shape_config(ShapeConfig* conf)
{
  sfree(conf->lev);
  sfree(conf->mult);
  sfree(conf->id);
  sfree(conf);
}

ShapeConfig* copy_shape_config(ShapeConfig* conf)
{
  ShapeConfig* ret = snew(ShapeConfig);
  ret->numcomp = conf->numcomp;
  ret->lev = snewn(conf->numcomp, int);
  ret->mult = snewn(conf->numcomp, int);
  ret->id = snewn(conf->numcomp, int);
  for (int i=0; i<conf->numcomp; i++) {
    ret->lev[i] = conf->lev[i];
    ret->mult[i] = conf->mult[i];
    ret->id[i] = conf->id[i];
  }
  return ret;
}

void insert_shape_config_item(ShapeConfig* conf, int ind, int lev, int mult, int id)
{
  int i;
  for (i=ind; i>0 && (lev > conf->lev[i-1] || (lev == conf->lev[i-1] && (id != -1 || mult > conf->mult[i-1]))); i--) {
    conf->lev[i] = conf->lev[i-1];
    conf->mult[i] = conf->mult[i-1];
    conf->id[i] = conf->id[i-1];
  }
  conf->lev[i] = lev;
  conf->mult[i] = mult;
  conf->id[i] = id;
}

ShapeDict* global_dict_a = 0;
ShapeDict* global_dict_m = 0;
ShapeDict* global_dict_r = 0;
ShapeDict* global_dict_i = 0;

/* ---------- Game configuration ---------- */


/*
  <mode><size><ftype><refl><conf>:<bits>
  <D,S,P>10x10<U,R,B><,m,r,i>2*6.30,3*4.4:hex
 */
struct game_params {
  int mode; /* duel, user, solitaire */
  int ftype; /* unknown, random, standard */
  int refl;
  int bwidth, bheight;
  ShapeConfig* conf;
  char* confstr;
};

struct clues {
  int refcount;
  Shape* given;
  Shape* groundtruth;
  ShapeConfig* conf;
  int goal;
  random_state* drs;
  DictStatistics* dstat;
};

struct game_state {
  const game_params *par;
  struct clues *clues;
  char *pencil;
  Shape* guess;
  Shape* reveal;
  int turn;
  int completed, cheated, errors;
  /* Some extra for Duel mode */
  char *dpencil;
  Shape* dguess;
  Shape* dreveal;
  int dturn;
  int dstate; /* prepare, user, computer, only user, only computer, done */
  int dx, dy;
  int derrors;
};

ShapeConfig* interpret_fleet_config(const char* conf, int sym)
{
  const char *p;
  int i, mul, lev, mlev, id, count = 1;
  for (p=conf; *p && *p != ':' && *p != ';'; p++)
    if (*p == ',')
      count += 1;
  ShapeConfig* ret = make_shape_config(count);
  i = 0;
  mlev = 0;
  for (i=0, p=conf; i<count; i++) {
    while (*p==' ') p++;
    if (*p < '0' || *p > '9') {
      free_shape_config(ret);
      return 0;
    }
    mul = atoi(p);
    while (*p >= '0' && *p <= '9') p++;
    while (*p==' ') p++;
    if (*p != '*' && *p != 'x') {
      free_shape_config(ret);
      return 0;
    }
    p++;
    while (*p==' ') p++;
    if (*p < '0' || *p > '9') {
      free_shape_config(ret);
      return 0;
    }
    lev = atoi(p);
    while (*p >= '0' && *p <= '9') p++;
    if (*p == '.') {
      p++;
      if (*p < '0' || *p > '9') {
        free_shape_config(ret);
        return 0;
      }
      id = atoi(p);
      while (*p >= '0' && *p <= '9') p++;
    } else {
      id = -1;
    }
    while (*p==' ') p++;
    if (*p != 0 && *p != ',' && *p != ':' && *p != ';') {
      free_shape_config(ret);
      return 0;
    }
    p++;
    insert_shape_config_item(ret, i, lev, mul, id);
    if (lev > mlev)
      mlev = lev;
  }
  ret->maxlev = mlev;
  ret->symmetry = sym;
  return ret;
}

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);
    ret->mode = 2;
    ret->ftype = 0;
    ret->refl = ID_REFL_ALL;
    ret->bwidth = ret->bheight = 8;
    ret->confstr = dupstr("2*6,3*4");
    ret->conf = interpret_fleet_config(ret->confstr, ret->refl);
    return ret;
}

const static struct game_params factor_presets[] = {
  {2, 0, ID_REFL_ALL, 8, 8, NULL, "2*6,3*4"},
  {1, 0, ID_REFL_ALL, 8, 8, NULL, "2*6,3*4"},
  {2, 1, ID_REFL_ALL, 8, 8, NULL, "2*6,3*4"},
  {1, 1, ID_REFL_ALL, 8, 8, NULL, "2*6,3*4"},
  {2, 2, ID_REFL_ALL, 9, 9, NULL, "2*4,3*3,4*2"},
  {1, 2, ID_REFL_ALL, 9, 9, NULL, "2*4,3*3,4*2"},
  {0, 0, ID_REFL_ALL, 8, 8, NULL, "3*9"},
  {0, 1, ID_REFL_ALL, 8, 8, NULL, "3*9"},
  {0, 0, ID_REFL_ALL, 10, 10, NULL, "2*10,3*7"},
  {0, 1, ID_REFL_ALL, 10, 10, NULL, "2*10,3*7"},
  {0, 0, ID_REFL_ALL, 12, 12, NULL, "2*10,3*7,4*5"},
  {0, 1, ID_REFL_ALL, 12, 12, NULL, "2*10,3*7,4*5"},
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(factor_presets))
      return false;

    ret = snew(game_params);
    *ret = factor_presets[i]; /* structure copy */
    ret->confstr = dupstr(ret->confstr);
    ret->conf = interpret_fleet_config(ret->confstr, ret->refl);

    if (name) {
      sprintf(buf, "%s %dx%d, %s %s", 
              (ret->mode==2 ? "Duel" : ret->mode==1 ? "Single" : "Puzzle"),
              ret->bwidth, ret->bheight,
              (ret->ftype==0 ? "unknown" : ret->ftype==1 ? "random" : "standard fleet"),
              (ret->ftype==2 ? "" : ret->confstr));
      *name = dupstr(buf);
    }
    *params = ret;
    return true;
}

static void free_params(game_params *params)
{
    free_shape_config(params->conf);
    sfree(params->confstr);
    sfree(params);
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;		       /* structure copy */
    ret->confstr = dupstr(params->confstr);
    ret->conf = interpret_fleet_config(ret->confstr, ret->refl);
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
    char const *p = string;
    int refl;

    params->mode = (*p == 'D' ? 2 : *p == 'S' ? 1 : 0);
    p++;
    params->bwidth = atoi(p);
    while (*p && isdigit((unsigned char)*p)) p++;
    p++;
    params->bheight = atoi(p);
    while (*p && isdigit((unsigned char)*p)) p++;
    params->ftype = (*p == 'U' ? 0 : *p == 'R' ? 1 : 2);
    p++;
    if (*p && isdigit((unsigned char)*p))
      refl = ID_REFL_ALL;
    else {
      refl = (*p == 'm' ? ID_REFL_MIR : *p == 'r' ? ID_REFL_ROT : ID_REFL_ORIG);
      p++;
    }
    params->conf = interpret_fleet_config(p, refl);
    params->confstr = dupstr(p);
}

static char *encode_params(const game_params *params, bool full)
{
    char ret[80];

    sprintf(ret, "%c%dx%d%c%s%s",
            (params->mode == 2 ? 'D' : params->mode == 1 ? 'S' : 'P'),
            params->bwidth, params->bheight,
            (params->ftype == 0 ? 'U' : params->ftype == 1 ? 'R' : 'B'),
            (params->refl == ID_REFL_MIR ? "m" : params->refl == ID_REFL_ROT ? "r" : params->refl == ID_REFL_ORIG ? "i" : ""),
            params->confstr);
    return dupstr(ret);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[80];
    int ind = 0;

    ret = snewn(7, config_item);

    ret[ind].name = "Game mode";
    ret[ind].type = C_CHOICES;
    ret[ind].u.choices.choicenames = ":Duel:Single:Puzzle";
    ret[ind].u.choices.selected = 2 - params->mode;

    ind++;
    ret[ind].name = "Grid width";
    ret[ind].type = C_STRING;
    sprintf(buf, "%d", params->bwidth);
    ret[ind].u.string.sval = dupstr(buf);

    ind++;
    ret[ind].name = "Grid height";
    ret[ind].type = C_STRING;
    sprintf(buf, "%d", params->bheight);
    ret[ind].u.string.sval = dupstr(buf);

    ind++;
    ret[ind].name = "Fleet type";
    ret[ind].type = C_CHOICES;
    ret[ind].u.choices.choicenames = ":Unknown:Random:Standard";
    ret[ind].u.choices.selected = params->ftype;

    ind++;
    ret[ind].name = "Symmetry";
    ret[ind].type = C_CHOICES;
    ret[ind].u.choices.choicenames = ":All:Mirror:Rotation:None";
    ret[ind].u.choices.selected = (params->refl == ID_REFL_ALL ? 0 : params->refl == ID_REFL_MIR ? 1 : params->refl == ID_REFL_ROT ? 2 : 3);

    ind++;
    ret[ind].name = "Shape config (num*level, ...)";
    ret[ind].type = C_STRING;
    ret[ind].u.string.sval = dupstr(params->confstr);

    ind++;
    ret[ind].name = NULL;
    ret[ind].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);
    int tmp, ind = 0;

    ret->mode = 2 - cfg[ind++].u.choices.selected;
    ret->bwidth = atoi(cfg[ind++].u.string.sval);
    ret->bheight = atoi(cfg[ind++].u.string.sval);
    ret->ftype = cfg[ind++].u.choices.selected;
    tmp = cfg[ind++].u.choices.selected;
    ret->refl = (tmp == 0 ? ID_REFL_ALL : tmp == 1 ? ID_REFL_MIR : tmp == 2 ? ID_REFL_ROT : ID_REFL_ORIG);
    ret->confstr = dupstr(cfg[ind++].u.string.sval);
    ret->conf = interpret_fleet_config(ret->confstr, ret->refl);

    return ret;
}

static int shape_config_count(ShapeConfig* conf)
{
  int sum = 0;
  for (int i=0; i<conf->numcomp; i++) {
    sum += conf->lev[i] * conf->mult[i];
  }
  return sum;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->bwidth < 3 || params->bwidth > 15 || params->bheight < 3 || params->bheight > 15)
        return "Grid size must be between 3 and 15";
    if (!params->conf) {
      return "Malformed configuration string";
    }
    if (shape_config_count(params->conf) * 2 > params->bwidth * params->bheight)
        return "Too dense configuration";
    if (params->conf->maxlev > 12)
        return "Maximum level is 12";
    return NULL;
}


/* ---------- Game interface ---------- */


static unsigned char hextobits(unsigned char ch)
{
  unsigned char ret = 0;
  if (ch>='0' && ch <='9')
    ret = ch-'0';
  else if (ch>='A' && ch <='F')
    ret = ch-'A'+10;
  return ret;
}

static unsigned char bitstohex(unsigned char ch)
{
  unsigned char bits = ch & 15;
  if (bits < 10)
    return bits + '0';
  else
    return bits - 10 + 'A';
}

static char *new_game_desc(const game_params *params, random_state *rs,
			   char **aux, bool interactive)
{
    char *buf;
    char tbuf[120];
    char* p;
    ShapeDict* dict;
    Shape* board;
    ShapeConfig* conf;
    DictStatistics* stat = 0; /* Clues to the human */
    double entr;
    int x, y, done, count;
    int n, i, off, nc;
    if (!global_dict_a) {
      global_dict_a = init_shape_dictionary(12, ID_REFL_ALL);
      global_dict_r = init_shape_dictionary(12, ID_REFL_ROT);
      global_dict_m = init_shape_dictionary(12, ID_REFL_MIR);
      global_dict_i = init_shape_dictionary(12, ID_REFL_ORIG);
    }
    dict = (params->refl == ID_REFL_ALL ? global_dict_a :
            params->refl == ID_REFL_ROT ? global_dict_r :
            params->refl == ID_REFL_MIR ? global_dict_m :
            global_dict_i);
    extend_shape_dictionary(dict, params->conf->maxlev);

    conf = copy_shape_config(params->conf);
    if (params->ftype == 2) { /* standard fleet, only straight line shapes */
      for (int k=0; k<conf->numcomp; k++)
        if (conf->id[k] == -1)
          conf->id[k] = 0;
    }
    if (params->mode == 0) {
      while (1) {
        board = make_random_board(dict, conf, params->bwidth, params->bheight, (params->ftype == 1 ? 1 : 0), rs);
        if (!board) {
          if (params->ftype == 1) {
            free_shape_config(conf);
            conf = copy_shape_config(params->conf);
          }
          continue;
        }
        done = 0;
        stat = init_dict_statistics(dict, conf, params->bwidth, params->bheight);
        while (1) {
          done = dict_statistics_calc_entropy(stat, COMPLEXITY_LIMIT);
          if (done) break;
          dict_statistics_pick_best_entropy(stat, board, ID_OFF, rs, &entr, &x, &y);
          if (entr == 0.0) break;
          dict_statistics_update_poss(stat, x, y, ShapePix(board, x, y, 1));
        }
        if (done == 1) {
          dict_statistics_prune_superfluous(stat, rs);
          break;
        } else if (params->ftype == 1) {
          free_shape_config(conf);
          conf = copy_shape_config(params->conf);
        }
      }
    } else {
      while (1) {
        board = make_random_board(dict, conf, params->bwidth, params->bheight, (params->ftype == 1 ? 1 : 0), rs);
        if (board)
          break;
        else if (params->ftype == 1) {
          free_shape_config(conf);
          conf = copy_shape_config(params->conf);
        }
      }
      if (params->mode == 1) {
        count = 0;
        done = 0;
        stat = init_dict_statistics(dict, conf, params->bwidth, params->bheight);
        while (1) {
          done = dict_statistics_calc_entropy(stat, COMPLEXITY_LIMIT);
          if (done) break;
          dict_statistics_pick_best_entropy(stat, 0, 0, rs, &entr, &x, &y);
          if (entr == 0.0) break;
          dict_statistics_update_poss(stat, x, y, ShapePix(board, x, y, 1));
          count++;
        }
      }
    }

    p = tbuf;
    for (int k=0; k<conf->numcomp; k++) {
      if (conf->id[k] != -1)
        sprintf(p, "%d*%d.%d,", conf->mult[k], conf->lev[k], conf->id[k]);
      else
        sprintf(p, "%d*%d,", conf->mult[k], conf->lev[k]);
      p += strlen(p);
    }
    *(p-1) = ';';
    if (params->mode == 1) {
      sprintf(p, "%d;", count + 2); 
    }

    nc = strlen(tbuf);
    n = (params->bwidth + 2) * (params->bheight + 2);
    off = (n + 1)/2;
    if (params->mode == 0)
      buf = snewn(nc+2*off+1, char);
    else
      buf = snewn(nc+off+1, char);
    sprintf(buf, "%s", tbuf);
    for (i=0; i<n-1; i+=2)
      buf[nc + i/2] = bitstohex(((3 & board->pix[i])<<2 | (3 & board->pix[i+1])) ^ hextobits(PI_STRING[(i/2)%256]));
    if (i<n)
      buf[nc + i/2] = bitstohex(((3 & board->pix[i])<<2) ^ hextobits(PI_STRING[(i/2)%256]));
    if (params->mode == 0) {
      for (i=0; i<n-1; i+=2)
        buf[nc + off + i/2] = bitstohex((3 & stat->board->pix[i])<<2 | (3 & stat->board->pix[i+1]));
      if (i<n)
        buf[nc + off + i/2] = bitstohex(((3 & stat->board->pix[i])<<2));
      buf[nc + 2*off] = 0;
    } else
      buf[nc + off] = 0;

    if (params->mode == 0 || params->mode == 1)
      free_dict_statistics(stat);
    free_shape(board);
    free_shape_config(conf);
    *aux = dupstr("S");
    return buf;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    int i, n = (params->bwidth + 2) * (params->bheight + 2);
    int wanted = (params->mode == 0 ? 2*((n + 1)/2) : (n + 1)/2);

    for (; *desc && *desc != ';'; desc++)
      if (!(*desc >= '0' && *desc <= '9') && *desc != '*' && *desc != ',' && *desc != '.')
        return "Bad configuration specification";
    if (*desc != ';')
      return "Too short description";
    desc++;
    if (params->mode == 1) {
      for (; *desc && *desc != ';'; desc++)
        if (!(*desc >= '0' && *desc <= '9'))
          return "Bad configuration specification";
      if (*desc != ';')
        return "Too short description";
      desc++;
    }
    for (i=0; i<wanted && *desc; i++, desc++)
      if (!(*desc >= '0' && *desc <= '9') && !(*desc >= 'A' && *desc <= 'F'))
        return "Expected hexadecimal digit";
    if (i < wanted)
      return "Too short description";
    if (*desc)
    return "Too long description";
  
  return NULL;
}

static game_state *new_game(midend *me, const game_params *params, const char *desc)
{
    int i, n, off;
    game_state *state = snew(game_state);
    Shape* board;
    char ch;
    state->par = params;
    state->clues = snew(struct clues);
    state->clues->refcount = 1;

    state->clues->conf = interpret_fleet_config(desc, params->refl);
    while (*desc && *desc != ';') desc++; 
    if (*desc) desc++;
    if (params->mode == 1) {
      state->clues->goal = atoi(desc);
      while (*desc && *desc != ';') desc++; 
      if (*desc) desc++;
    } else
      state->clues->goal = -1;
    n = (params->bwidth + 2) * (params->bheight + 2);
    off = (n + 1)/2;
    board = make_empty_board(params->bwidth, params->bheight);
    for (i=0; i<n-1; i+=2) {
      ch = hextobits(desc[i/2]) ^ hextobits(PI_STRING[(i/2)%256]);
      board->pix[i] = 3 & (ch >> 2);
      board->pix[i+1] = 3 & ch;
    }
    if (i<n) {
      ch = hextobits(desc[i/2]) ^ hextobits(PI_STRING[(i/2)%256]);
      board->pix[i] = 3 & (ch >> 2);
    }
    state->clues->groundtruth = board;

    if (params->mode == 0) {
      board = make_empty_board(params->bwidth, params->bheight);
      for (i=0; i<n-1; i+=2) {
        ch = hextobits(desc[off + i/2]);
        board->pix[i] = 3 & (ch >> 2);
        board->pix[i+1] = 3 & ch;
      }
      if (i<n) {
        ch = hextobits(desc[off + i/2]);
        board->pix[i] = 3 & (ch >> 2);
      }
      state->clues->given = board;
    } else
      state->clues->given = 0;

    if (params->mode == 2) {
      void *randseed;
      int randseedsize;
      ShapeDict* dict;
      dict = (params->refl == ID_REFL_ALL ? global_dict_a :
              params->refl == ID_REFL_ROT ? global_dict_r :
              params->refl == ID_REFL_MIR ? global_dict_m :
              global_dict_i);
      state->clues->dstat = init_dict_statistics(dict, state->clues->conf, params->bwidth, params->bheight);
      get_random_seed(&randseed, &randseedsize);
      state->clues->drs = random_new(randseed, randseedsize);
      sfree(randseed);
    } else {
      state->clues->drs = 0;
      state->clues->dstat = 0;
    }

    n = params->bwidth * params->bheight;
    state->guess = make_empty_board(params->bwidth, params->bheight);
    if (params->mode == 0)
      state->reveal = copy_shape(state->clues->given);
    else
      state->reveal = make_empty_board(params->bwidth, params->bheight);
    state->pencil = snewn(n, char);
    for (i = 0; i < n; i++) {
        state->pencil[i] = 0;
    }
    if (params->mode == 2) {
      state->dguess = make_empty_board(params->bwidth, params->bheight);
      state->dreveal = make_empty_board(params->bwidth, params->bheight);
      state->dpencil = snewn(n, char);
      for (i = 0; i < n; i++) {
        state->dpencil[i] = 0;
      }
      state->dstate = 0;
    } else {
      state->dguess = 0;
      state->dreveal = 0;
      state->dpencil = 0;
      state->dstate = 5;
    }
    state->turn = 0;
    state->dturn = 0;
    state->dx = state->dy = -1;
    state->completed = state->cheated = state->errors = state->derrors = false;

    return state;
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = snew(game_state);
    int i, n;

    ret->par = state->par;
    ret->clues = state->clues;
    ret->clues->refcount++;

    n = state->par->bwidth * state->par->bheight;
    ret->guess = copy_shape(state->guess);
    ret->reveal = copy_shape(state->reveal);
    ret->pencil = snewn(n, char);
    for (i = 0; i < n; i++) {
        ret->pencil[i] = state->pencil[i];
    }
    if (state->par->mode == 2) {
      ret->dguess = copy_shape(state->dguess);
      ret->dreveal = copy_shape(state->dreveal);
      ret->dpencil = snewn(n, char);
      for (i = 0; i < n; i++) {
        ret->dpencil[i] = state->dpencil[i];
      }
    } else {
      ret->dguess = 0;
      ret->dreveal = 0;
      ret->dpencil = 0;
    }
    ret->turn = state->turn;
    ret->dturn = state->dturn;
    ret->dstate = state->dstate;
    ret->dx = state->dx;
    ret->dy = state->dy;
    ret->completed = state->completed;
    ret->cheated = state->cheated;
    ret->errors = state->errors;
    ret->derrors = state->derrors;

    return ret;
}

static void free_game(game_state *state)
{
    if (--state->clues->refcount <= 0) {
        free_shape(state->clues->groundtruth);
        if (state->clues->given)
          free_shape(state->clues->given);
        if (state->clues->drs)
          random_free(state->clues->drs);
        if (state->clues->dstat)
          free_dict_statistics(state->clues->dstat);
        free_shape_config(state->clues->conf);
        sfree(state->clues);
    }
    free_shape(state->guess);
    free_shape(state->reveal);
    sfree(state->pencil);
    if (state->dpencil) {
      free_shape(state->dguess);
      free_shape(state->dreveal);
      sfree(state->dpencil);
    }
    sfree(state);
}

static char *solve_game(const game_state *state, const game_state *currstate,
			const char *aux, const char **error)
{
  if (aux)
    return dupstr(aux);
  else {
    *error = "Not implemented";
    return NULL;
  }
}

static bool game_can_format_as_text_now(const game_params *params)
{
    return true;
}

static char *game_text_format(const game_state *state)
{
    return NULL;
}


struct game_ui {
    int hx, hy;
    int hshow;
    int hpanel;
    int hcursor;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);

    ui->hx = ui->hy = 0;
    ui->hpanel = ui->hshow = ui->hcursor = 0;
    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static char *encode_ui(const game_ui *ui)
{
    return NULL;
}

static void decode_ui(game_ui *ui, const char *encoding)
{
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
}

#define PREFERRED_TILESIZE 32

#define FLASH_TIME 0.6F

struct game_drawstate {
    int tilesize;
    int gridsize;
    int wdt, hgt;
    int xoff, yoffd, yoffn, yoffb, yoffl;
    int started;
};

static int check_can_guess(const game_state* state)
{
  return shape_config_count(state->par->conf) == count_board(state->guess, ID_ON) + count_board(state->reveal, ID_ON);
}

static char *interpret_move(const game_state *state, game_ui *ui, const game_drawstate *ds,
			    int x, int y, int button)
{
  int w = state->par->bwidth;
  int h = state->par->bheight;
  char pix;
  int tpanel, tx, ty;
  char* retstr = UI_UPDATE;
  char buf[20];

  button &= ~MOD_MASK;

  if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
    tpanel = (y < ds->yoffn ? 2 : 1);
    tx = (x + ds->tilesize - ds->xoff) / ds->tilesize - 1;
    ty = (y + ds->tilesize - (tpanel==2 ? ds->yoffd : ds->yoffn)) / ds->tilesize -1;

    if (tpanel == 1 && tx >= 0 && tx < w && ty >= 0 && ty < h) {
      ui->hcursor = 0;
      if (ui->hpanel == 1 && tx == ui->hx && ty == ui->hy && ui->hshow) {
        /* button-click on the already active square */
        if (ShapePix(state->reveal, ui->hx, ui->hy, 1) != ID_UNKNOWN)
          return retstr;
        pix = ShapePix(state->guess, ui->hx, ui->hy, 1);
        if (button == LEFT_BUTTON)
          sprintf(buf, "Tn%d,%d,%d", ui->hx, ui->hy, (pix==ID_ON ? 2 : pix==ID_OFF ? 0 : 1));
        else
          sprintf(buf, "Tn%d,%d,%d", ui->hx, ui->hy, (pix==ID_OFF ? 1 : pix==ID_ON ? 0 : 2));
        return dupstr(buf);
      } else {
        /* button-click on a new square */
        ui->hx = tx;
        ui->hy = ty;
        ui->hshow = 1;
        ui->hpanel = 1;
      }
      return retstr;
    } else if (tpanel == 2 && tx >= 0 && tx < w && ty >= 0 && ty < h) {
      ui->hcursor = 0;
      if (ui->hpanel == 2 && tx == ui->hx && ty == ui->hy && ui->hshow) {
        /* button-click on the already active square */
        if (ShapePix(state->dreveal, ui->hx, ui->hy, 1) != ID_UNKNOWN)
          return retstr;
        pix = ShapePix(state->dguess, ui->hx, ui->hy, 1);
        if (button == LEFT_BUTTON)
          sprintf(buf, "Td%d,%d,%d", ui->hx, ui->hy, (pix==ID_ON ? 2 : pix==ID_OFF ? 0 : 1));
        else
          sprintf(buf, "Td%d,%d,%d", ui->hx, ui->hy, (pix==ID_OFF ? 1 : pix==ID_ON ? 0 : 2));
        return dupstr(buf);
      } else {
        /* button-click on a new square */
        ui->hx = tx;
        ui->hy = ty;
        ui->hshow = 1;
        ui->hpanel = 2;
      }
      return retstr;
    } else if (x >= (ds->wdt - ds->tilesize)/2 && x <= (ds->wdt + ds->tilesize)/2 &&
               y >= ds->yoffb && y <= ds->yoffb + ds->tilesize) {
      ui->hshow = 0;
      if (state->par->mode == 2 && state->dstate == 0)
        return dupstr("D");
      else if ((state->dstate == 1 || state->dstate == 5) && check_can_guess(state))
        return dupstr("G");
      return retstr;
    }
  }

  if (IS_CURSOR_MOVE(button)) {
    int np = (ds->yoffd >= 0 ? 2 : 1);
    int tmpy = ui->hy + (np - ui->hpanel) * h;
    move_cursor(button, &ui->hx, &tmpy, w, np*h + 1, 0);
    ui->hy = tmpy % h;
    ui->hpanel = np - tmpy / h;
    ui->hshow = ui->hcursor = 1;
    return retstr;
  }

  if (button == CURSOR_SELECT2 || button == '\b') {
    if (state->dstate == 2 || state->dstate == 6) {
      pix = ShapePix(state->dguess, state->dx, state->dy, 1);
      sprintf(buf, "Td%d,%d,%d", state->dx, state->dy, (pix==ID_ON ? 2 : pix==ID_OFF ? 0 : 1));
    } else if (ui->hpanel == 2 && ui->hshow) {
      if (ShapePix(state->reveal, ui->hx, ui->hy, 1) != ID_UNKNOWN)
        return retstr;
      pix = ShapePix(state->dguess, ui->hx, ui->hy, 1);
      sprintf(buf, "Td%d,%d,%d", ui->hx, ui->hy, (pix==ID_ON ? 2 : pix==ID_OFF ? 0 : 1));
    } else if (ui->hpanel == 1 && ui->hshow) {
      if (ShapePix(state->reveal, ui->hx, ui->hy, 1) != ID_UNKNOWN)
        return retstr;
      pix = ShapePix(state->guess, ui->hx, ui->hy, 1);
      sprintf(buf, "Tn%d,%d,%d", ui->hx, ui->hy, (pix==ID_ON ? 2 : pix==ID_OFF ? 0 : 1));
    } else
      return retstr;
    return dupstr(buf);
  }

  if (button == CURSOR_SELECT) {
    if (state->dstate == 2 || state->dstate == 6) {
      pix = ShapePix(state->dguess, state->dx, state->dy, 1);
      if (pix==ID_ON || pix==ID_OFF) {
        sprintf(buf, "R%d,%d,%d", state->dx, state->dy, (pix==ID_ON ? 1 : 2));
        return dupstr(buf);
      }
    } else if (ui->hpanel == 1 && ui->hshow && state->dstate != 0 && state->par->mode != 0) {
      if (!ui->hcursor) ui->hshow = 0;
      if (ShapePix(state->reveal, ui->hx, ui->hy, 1) == ID_UNKNOWN) {
        sprintf(buf, "Q%d,%d", ui->hx, ui->hy);
        return dupstr(buf);
      }
    } else if (ui->hpanel == 0 && ui->hshow) {
      ui->hshow = 0;
      if (state->par->mode == 2 && state->dstate == 0)
        return dupstr("D");
      else if ((state->dstate == 1 || state->dstate == 5) && check_can_guess(state))
        return dupstr("G");
      return retstr;
    }
  }

  if (button >= '0' && button <= '6') {
    if (ui->hpanel == 1 && ui->hshow) {
      sprintf(buf, "Pn%d,%d,%d", ui->hx, ui->hy, button - '0');
      return dupstr(buf);
    } else if (ui->hpanel == 2 && ui->hshow) {
      sprintf(buf, "Pd%d,%d,%d", ui->hx, ui->hy, button - '0');
      return dupstr(buf);
    }
  }
  
  return NULL;
}

static void execute_computer_guess(game_state* ret)
{
  double entr;
  int x, y;
  int done = dict_statistics_calc_entropy(ret->clues->dstat, COMPLEXITY_LIMIT);
  if (done) {
    dict_statistics_fill_board(ret->clues->dstat);
    copy_board(ret->clues->dstat->board, ret->dreveal);
    ret->dstate = (ret->dstate == 1 ? 5 : 4);
  } else {
    dict_statistics_pick_best_entropy(ret->clues->dstat, 0, 0, ret->clues->drs, &entr, &x, &y);
    if (entr > 0.0) {
      ret->dx = x;
      ret->dy = y;
      ret->dstate = (ret->dstate == 1 ? 2 : 6);
    } else {
      ret->dstate = (ret->dstate == 1 ? 5 : 4);
      ret->derrors = 1;
    }
  }
}

static game_state *execute_move(const game_state *from0, const char *move)
{
    game_state* from = (game_state*) from0;
    int w = from->par->bwidth;
    int h = from->par->bheight;
    game_state *ret;
    char ch;
    int x, y, n;
    if (move[0] == 'S') {
	ret = dup_game(from);
	ret->completed = ret->cheated = true;
        ret->errors = false;
	for (x = 0; x < w; x++)
          for (y = 0; y < h; y++) {
            ch = ShapePix(ret->clues->groundtruth, x, y, 1);
            SetShapePix(ret->reveal, x, y, 1, ch);
            SetShapePix(ret->guess, x, y, 1, ID_UNKNOWN);
          }
        ret->dstate = (ret->dstate == 5 ? 4 : 6);
        if (ret->dstate == 6)
          execute_computer_guess(ret);
	return ret;
    } else if (move[0] == 'T' &&
	sscanf(move+2, "%d,%d,%d", &x, &y, &n) == 3 &&
	x >= 0 && x < w && y >= 0 && y < h && n >= 0 && n <= 2) {
	ret = dup_game(from);
        if (move[1] == 'd') 
          SetShapePix(ret->dguess, x, y, 1, (n==1 ? ID_ON : n==2 ? ID_OFF : ID_UNKNOWN));
        else
          SetShapePix(ret->guess, x, y, 1, (n==1 ? ID_ON : n==2 ? ID_OFF : ID_UNKNOWN));
	return ret;
    } else if (move[0] == 'P' &&
	sscanf(move+2, "%d,%d,%d", &x, &y, &n) == 3 &&
	x >= 0 && x < w && y >= 0 && y < h && n >= 0 && n <= 6) {
	ret = dup_game(from);
        if (move[1] == 'd') 
          ret->dpencil[y*w + x] = (char)n;
        else
          ret->pencil[y*w + x] = (char)n;
	return ret;
    } else if (move[0] == 'Q' &&
	sscanf(move+1, "%d,%d", &x, &y) == 2 &&
	x >= 0 && x < w && y >= 0 && y < h) {
	ret = dup_game(from);
        ch = ShapePix(ret->clues->groundtruth, x, y, 1);
        SetShapePix(ret->reveal, x, y, 1, ch);
        SetShapePix(ret->guess, x, y, 1, ID_UNKNOWN);
        ret->turn += 1;
        if (ret->turn == w*h) {
          ret->completed = true;
          ret->dstate = (ret->dstate == 5 ? 4 : 6);
        }
        if (from->par->mode == 2 && (from->dstate == 1 || from->dstate == 6)) {
          execute_computer_guess(ret);
       }
	return ret;
    } else if (move[0] == 'R' &&
	sscanf(move+1, "%d,%d,%d", &x, &y, &n) == 3 &&
	x >= 0 && x < w && y >= 0 && y < h && n >= 1 && n <= 2) {
      if (from->par->mode == 2 && (from->dstate == 2 || from->dstate == 6)) {
        ret = dup_game(from);
        SetShapePix(ret->dreveal, x, y, 1, (n==1 ? ID_ON : ID_OFF));
        ret->dturn += 1;
        dict_statistics_update_poss(ret->clues->dstat, x, y, (n==1 ? ID_ON : ID_OFF));
        ret->dstate = (ret->dstate == 2 ? 1 : 6);
        if (ret->dstate == 6)
          execute_computer_guess(ret);
        return ret;
      } else
        return from;
    } else if (move[0] == 'G') {
      int err = 0;
      ret = dup_game(from);
      ret->completed = true;
      for (x = 0; x < w; x++)
        for (y = 0; y < h; y++) {
          ch = ShapePix(ret->clues->groundtruth, x, y, 1);
          if (ShapePix(ret->guess, x, y, 1) == ID_UNKNOWN)
            SetShapePix(ret->guess, x, y, 1, (ShapePix(ret->reveal, x, y, 1) == ID_ON ? ID_ON : ID_OFF));
          if (ShapePix(ret->guess, x, y, 1) != ch)
            err = 1;
          else
            SetShapePix(ret->guess, x, y, 1, ID_UNKNOWN);
          SetShapePix(ret->reveal, x, y, 1, ch);
        }
      ret->errors = (err ? true : false);
      ret->dstate = (ret->dstate == 5 ? 4 : 6);
      if (ret->dstate == 6)
        execute_computer_guess(ret);
      return ret;
    } else if (move[0] == 'D') {
      if (from->par->mode == 2 && from->dstate == 0) {
        ret = dup_game(from);
        ret->dstate = 1;
        return ret;
      } else
        return from;
    } else
      return from;		       /* couldn't parse move string */
}


/* ---------- Drawing routines ---------- */

enum {
    COL_BACKGROUND,
    COL_HIGHLIGHT,
    COL_BLACK,
    COL_WHITE,
    COL_LIGHT,
    COL_DARK,
    COL_ERROR,
    COL_HUE1,
    COL_HUE1_L,
    COL_HUE1_D,
    COL_HUE2,
    COL_HUE2_L,
    COL_HUE2_D,
    COL_HUE3,
    COL_HUE3_L,
    COL_HUE3_D,
    COL_HUE4,
    COL_HUE4_L,
    COL_HUE4_D,
    COL_HUE5,
    COL_HUE5_L,
    COL_HUE5_D,
    COL_HUE6,
    COL_HUE6_L,
    COL_HUE6_D,
    NCOLOURS
};


static void game_compute_size(const game_params *params, int tilesize,
			      int *x, int *y)
{
  int half = tilesize / 2;
  int grsz = tilesize / 32 + 1;
  *x = tilesize * params->bwidth + grsz + 2 * half;
  if (params->mode == 2) {
    *y = 2 * tilesize * params->bheight + 2 * grsz + 4 * tilesize + 4 * half;
  } else {
    *y = tilesize * params->bheight + grsz + 3 * tilesize + 3 * half;
  }
}

static void game_set_size(drawing *dr, game_drawstate *ds,
			  const game_params *params, int tilesize)
{
  int half = tilesize / 2;
  int grsz = tilesize / 32 + 1;
  ds->tilesize = tilesize;
  ds->gridsize = grsz;
  ds->wdt = tilesize * params->bwidth + grsz + 2 * half;
  ds->xoff = half + grsz;
  if (params->mode == 2) {
    ds->hgt = 2 * tilesize * params->bheight + 2 * grsz + 4 * tilesize + 4 * half;
    ds->yoffd = half + grsz;
    ds->yoffn = ds->yoffd + tilesize * params->bheight + tilesize + half + grsz;
  } else {
    ds->hgt = tilesize * params->bheight + grsz + 3 * tilesize + 3 * half;
    ds->yoffd = -1;
    ds->yoffn = half + grsz;
  }
  ds->yoffb = ds->yoffn + tilesize * params->bheight + half + grsz;
  ds->yoffl = ds->yoffb + tilesize + half;
}

/* simple lightness changing routines, use sqrt for approx gamma compensation */
static void darken_colour(float* arr, int dind, int sind, float prop)
{
  int i;
  for (i=0; i<3; i++)
    arr[dind * 3 + i] = arr[sind * 3 + i] * sqrt(1.0-prop); 
}

static void lighten_colour(float* arr, int dind, int sind, float prop)
{
  int i;
  for (i=0; i<3; i++)
    arr[dind * 3 + i] = sqrt(1.0 - (1.0 - arr[sind * 3 + i]*arr[sind * 3 + i]) * (1.0-prop));
}

static void set_colour(float* arr, int ind, float r, float g, float b)
{
  arr[ind * 3] = sqrt(r), arr[ind * 3 + 1] = sqrt(g), arr[ind * 3 + 2] = sqrt(b);
}

static float *game_colours(frontend *fe, int *ncolours)
{
  float *ret = snewn(3 * NCOLOURS, float);

  frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

  darken_colour(ret, COL_HIGHLIGHT, COL_BACKGROUND, 0.5);
  darken_colour(ret, COL_DARK, COL_BACKGROUND, 0.3);
  lighten_colour(ret, COL_LIGHT, COL_BACKGROUND, 0.4);
  set_colour(ret, COL_BLACK, 0.0, 0.0, 0.0);
  set_colour(ret, COL_WHITE, 1.0, 1.0, 1.0);
  set_colour(ret, COL_ERROR, 1.0, 0.0, 0.0);
  set_colour(ret, COL_HUE1, 0.95, 0.95, 0.0);
  lighten_colour(ret, COL_HUE1_L, COL_HUE1, 0.4);
  darken_colour(ret, COL_HUE1_D, COL_HUE1, 0.3);
  set_colour(ret, COL_HUE2, 1.0, 0.68, 0.24);
  lighten_colour(ret, COL_HUE2_L, COL_HUE2, 0.4);
  darken_colour(ret, COL_HUE2_D, COL_HUE2, 0.3);
  set_colour(ret, COL_HUE3, 1.0, 0.41, 0.36);
  lighten_colour(ret, COL_HUE3_L, COL_HUE3, 0.4);
  darken_colour(ret, COL_HUE3_D, COL_HUE3, 0.3);
  set_colour(ret, COL_HUE4, 0.93, 0.25, 1.0);
  lighten_colour(ret, COL_HUE4_L, COL_HUE4, 0.4);
  darken_colour(ret, COL_HUE4_D, COL_HUE4, 0.3);
  set_colour(ret, COL_HUE5, 0.0, 0.74, 1.0);
  lighten_colour(ret, COL_HUE5_L, COL_HUE5, 0.4);
  darken_colour(ret, COL_HUE5_D, COL_HUE5, 0.3);
  set_colour(ret, COL_HUE6, 0.31, 1.0, 0.11);
  lighten_colour(ret, COL_HUE6_L, COL_HUE6, 0.4);
  darken_colour(ret, COL_HUE6_D, COL_HUE6, 0.3);

  *ncolours = NCOLOURS;
  return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = 0;
    ds->gridsize = 0;
    ds->wdt = 0;
    ds->hgt = 0;
    ds->xoff = 0;
    ds->yoffd = 0;
    ds->yoffn = 0;
    ds->yoffb = 0;
    ds->yoffl = 0;
    ds->started = false;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds);
}

static void draw_tile(drawing *dr, game_drawstate *ds, const game_params *par,
		      int dpanel, int x, int y, Shape* reveal, Shape* guess,
                      char pencil, int highlight, int errors, int flash)
{
    int cx, cy, cw, ch;
    char pix1, pix2;
    int marg = max(ds->tilesize / 10, 1);

    cx = ds->xoff + x * ds->tilesize;
    cy = (dpanel==1 ? ds->yoffd : ds->yoffn) + y * ds->tilesize;
    ch = cw = ds->tilesize - ds->gridsize;

    clip(dr, cx, cy, cw, ch);

    pix1 = ShapePix(reveal, x, y, 1);
    if (pix1 == ID_ON || pix1 == ID_OFF) {
      if (!errors && flash) {
        draw_rect(dr, cx, cy, cw, ch, (pix1 == ID_ON ? COL_HUE1 + (flash+2)%6 * 3: COL_HUE1 + (flash+5)%6 * 3));
      } else {
        draw_rect(dr, cx, cy, cw, ch, (pix1 == ID_ON ? COL_BLACK : COL_WHITE));
        if (highlight)
          draw_rect(dr, cx+marg, cy+marg, cw-2*marg, ch-2*marg, (pix1 == ID_ON ? COL_HIGHLIGHT : COL_BACKGROUND)); 
      }
    } else {
      int col, dark, light; 
      int coords[6];
      if (pencil <= 0 || pencil > 6) {
        col = (highlight ? COL_HIGHLIGHT : COL_BACKGROUND);
        light = COL_LIGHT;
        dark = COL_DARK;
      } else {
        col = COL_HUE1 + pencil * 3 - 3;
        light = col + 1;
        dark = col + 2;
        if (highlight) col = col + 2;
      }
      coords[0] = cx+cw-1;
      coords[1] = cy+ch-1;
      coords[2] = cx+cw-1;
      coords[3] = cy;
      coords[4] = cx;
      coords[5] = cy+ch-1;
      draw_polygon(dr, coords, 3, dark, dark);
      coords[0] = cx;
      coords[1] = cy;
      draw_polygon(dr, coords, 3, light, light);
      draw_rect(dr, cx+marg, cy+marg, cw-2*marg, ch-2*marg, col); 
    }

    pix2 = ShapePix(guess, x, y, 1);
    if (pix2 == ID_ON || pix2 == ID_OFF) {
      if (errors && pix1 != pix2 && pix1 != ID_UNKNOWN)
        draw_rect(dr, cx+2*marg-ds->gridsize, cy+2*marg-ds->gridsize, cw-4*marg+2*ds->gridsize, ch-4*marg+2*ds->gridsize, COL_ERROR);
      draw_rect(dr, cx+2*marg, cy+2*marg, cw-4*marg, ch-4*marg, (pix2 == ID_ON ? COL_BLACK : COL_WHITE));
    }

    unclip(dr);
}

static void draw_button(drawing *dr, game_drawstate *ds, const game_params *par, const char* txt, int highlight, int gray)
{
  int rad = (ds->tilesize + 1) / 2;
  int x1 = ds->wdt / 2 - rad;
  int x2 = ds->wdt - x1;
  int y = ds->yoffb + rad;
  int marg = max(ds->tilesize / 10, 1);
  int light = COL_LIGHT;
  int dark = COL_DARK;
  int col = (highlight ? COL_HIGHLIGHT : COL_BACKGROUND);
  int tcol = (gray ? COL_HIGHLIGHT : COL_BLACK);
  draw_circle(dr, x1, y, rad, light, light); 
  draw_circle(dr, x2, y, rad, dark, dark); 
  draw_circle(dr, x2, y, rad - marg, col, col); 
  draw_circle(dr, x1, y, rad - marg, col, col);
  draw_rect(dr, x1, y - rad, x2 - x1, marg, light);
  draw_rect(dr, x1, y + rad - marg + 1, x2 - x1, marg, dark);
  draw_rect(dr, x1, y - rad + marg, x2 - x1, 2 * rad - 2 * marg + 1, col);
  draw_text(dr, ds->wdt / 2, y, FONT_VARIABLE, rad,
            ALIGN_HCENTRE | ALIGN_VCENTRE, tcol, txt);
}

static void draw_legend(drawing *dr, game_drawstate *ds, const game_params *par, const ShapeConfig* conf)
{
  char buf[8];
  int i;
  int x = ds->xoff;
  int y = ds->yoffl;
  int sqsz = ds->tilesize - ds->gridsize;
  ShapeDict* dict;
  dict = (par->refl == ID_REFL_ALL ? global_dict_a :
          par->refl == ID_REFL_ROT ? global_dict_r :
          par->refl == ID_REFL_MIR ? global_dict_m :
          global_dict_i);
  for (i=0; i<conf->numcomp; i++) {
    sprintf(buf, "%d *", conf->mult[i]);
    draw_text(dr, x, y + sqsz / 2, FONT_VARIABLE, ds->tilesize / 2, ALIGN_HLEFT | ALIGN_VCENTRE, COL_BLACK, buf);
    x += ds->tilesize;
    if (conf->id[i] == -1) {
      draw_rect(dr, x, y, sqsz, sqsz, COL_BLACK);
      sprintf(buf, "%d", conf->lev[i]);
      draw_text(dr, x + sqsz / 2, y + sqsz / 2, FONT_VARIABLE, ds->tilesize / 2, ALIGN_HCENTRE | ALIGN_VCENTRE, COL_WHITE, buf);
      x += ds->tilesize + ds->tilesize / 2;
    } else {
      Shape* sh = ShapeDictGet(dict, conf->lev[i], conf->id[i]);
      int xx, yy;
      int sqsz2 = max(ds->tilesize *2 / 5, 3) + 1;
      int yextra = (sh->height == 1 ? (sqsz - sqsz2) / 2 : sh->height == 2 ? (sqsz - 2*sqsz2) / 2 : 0);
      for (yy=0; yy<sh->height; yy++)
        for (xx=0; xx<sh->width; xx++)
          if (ShapePix(sh, xx, yy, 1) == ID_ON)
            draw_rect(dr, x + xx*sqsz2, y + yextra + yy*sqsz2, sqsz2-1, sqsz2-1, COL_BLACK);
      x += sh->width * sqsz2 + ds->tilesize / 2;
    }
  }
}

static void game_redraw(drawing *dr, game_drawstate *ds, const game_state *oldstate,
			const game_state *state, int dir, const game_ui *ui,
			float animtime, float flashtime)
{
    int w = state->par->bwidth;
    int h = state->par->bheight;
    char buf[20];
    int x, y, k;

    if (!ds->started) {
	/*
	 * The initial contents of the window are not guaranteed and
	 * can vary with front ends. To be on the safe side, all
	 * games should start by drawing a big background-colour
	 * rectangle covering the whole window.
	 */
	draw_rect(dr, 0, 0, ds->wdt, ds->hgt, COL_BACKGROUND);

        if (ds->yoffd >= 0) {
          draw_rect(dr, ds->xoff - ds->gridsize, ds->yoffd - ds->gridsize,
                    w*ds->tilesize+ds->gridsize, h*ds->tilesize+ds->gridsize,
                    COL_HIGHLIGHT);
        }

        draw_rect(dr, ds->xoff - ds->gridsize, ds->yoffn - ds->gridsize,
                  w*ds->tilesize+ds->gridsize, h*ds->tilesize+ds->gridsize,
                  COL_HIGHLIGHT);

        draw_legend(dr, ds, state->par, state->clues->conf);

	ds->started = true;
    }

    if (animtime)
      return;

    if (state->par->mode == 2) {
      for (k = 0, y = 0; y < h; y++) {
	for (x = 0; x < w; x++, k++) {
          draw_tile(dr, ds, state->par, 1, x, y, state->dreveal, state->dguess, state->dpencil[k],
                    ((state->dstate == 2 || state->dstate == 6) ?
                     (state->dx == x && state->dy == y ? 1 : 0) :
                     (ui->hpanel == 2 && ui->hshow && ui->hx == x && ui->hy == y ? 1 : 0)),
                    0, 0);
	}
      }
      if (state->dstate > 0) {
        draw_rect(dr, ds->xoff, ds->yoffn - ds->tilesize - ds->tilesize/2, ds->wdt - ds->xoff * 2, ds->tilesize, COL_BACKGROUND);
        sprintf(buf, "Turns: %d", state->dturn);
        draw_text(dr, ds->xoff, ds->yoffn - ds->tilesize, FONT_VARIABLE, ds->tilesize / 2, ALIGN_HLEFT | ALIGN_VCENTRE, COL_BLACK, buf);
      }
      if (state->dstate == 2 || state->dstate == 6) {
        sprintf(buf, "Computer's turn");
        draw_text(dr, ds->wdt - ds->xoff, ds->yoffn - ds->tilesize, FONT_VARIABLE, ds->tilesize / 2, ALIGN_HRIGHT | ALIGN_VCENTRE, COL_BLACK, buf);
      } else if (state->derrors) {
        sprintf(buf, "Inconsistent!");
        draw_text(dr, ds->wdt - ds->xoff, ds->yoffn - ds->tilesize, FONT_VARIABLE, ds->tilesize / 2, ALIGN_HRIGHT | ALIGN_VCENTRE, COL_ERROR, buf);
      }
    }

    for (k = 0, y = 0; y < h; y++) {
      for (x = 0; x < w; x++, k++) {
        draw_tile(dr, ds, state->par, 0, x, y, state->reveal, state->guess, state->pencil[k],
                  ((state->dstate == 2 || state->dstate == 6) ? 0 :
                   (ui->hpanel == 1 && ui->hshow && ui->hx == x && ui->hy == y ? 1 : 0)),
                  (state->completed && state->errors ? 1 : 0),
                  (flashtime ? (int)(flashtime * 6 / FLASH_TIME + 1) : 0));
      }
    }
    if (state->par->mode == 1 || (state->par->mode == 2 && state->dstate > 0)) {
      sprintf(buf, "Turns: %d", state->turn);
      draw_rect(dr, ds->xoff, ds->yoffb, ds->wdt - ds->xoff * 2, ds->tilesize, COL_BACKGROUND);
      draw_text(dr, ds->xoff, ds->yoffb + ds->tilesize/2, FONT_VARIABLE, ds->tilesize / 2, ALIGN_HLEFT | ALIGN_VCENTRE, COL_BLACK, buf);
    }
    if (state->par->mode == 2 && (state->dstate == 1 || state->dstate == 5)) {
      sprintf(buf, "Your turn");
      draw_text(dr, ds->wdt - ds->xoff, ds->yoffb + ds->tilesize/2, FONT_VARIABLE, ds->tilesize / 2, ALIGN_HRIGHT | ALIGN_VCENTRE, COL_BLACK, buf);
    } else if (state->par->mode == 1 && state->clues->goal != -1) {
      sprintf(buf, "Goal: %d", state->clues->goal);
      draw_text(dr, ds->wdt - ds->xoff, ds->yoffb + ds->tilesize/2, FONT_VARIABLE, ds->tilesize / 2, ALIGN_HRIGHT | ALIGN_VCENTRE, COL_BLACK, buf);
    }

    draw_button(dr, ds, state->par,
                (state->par->mode == 2 && state->dstate == 0 ? "Start" :
                 !state->completed ? "Guess" :
                 state->errors ? "Wrong!" :
                 state->cheated ? "" : "Right!"),
                (ui->hpanel == 0 && ui->hshow ? 1 : 0),
                (state->dstate == 0 || state->completed || check_can_guess(state) ? 0 : 1));

    draw_update(dr, 0, 0, ds->wdt, ds->hgt);
}

static float game_anim_length(const game_state *oldstate, const game_state *newstate,
			      int dir, game_ui *ui)
{
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate, const game_state *newstate,
			       int dir, game_ui *ui)
{
    if (!oldstate->completed && !oldstate->cheated &&
        !newstate->cheated && newstate->completed &&
        !newstate->errors && !newstate->derrors) {
      return FLASH_TIME;
    }
    return 0.0F;
}

static int game_status(const game_state *state)
{
    return state->completed ? +1 : 0;
}

static bool game_timing_state(const game_state *state, game_ui *ui)
{
    if (state->completed)
	return false;
    return true;
}

static void game_print_size(const game_params *params, float *x, float *y)
{
}

static void game_print(drawing *dr, const game_state *state, int tilesize)
{
}


#ifdef COMBINED
#define thegame identifier
#endif

const struct game thegame = {
    "Identifier", NULL, NULL,
    default_params,
    game_fetch_preset,
    NULL,
    decode_params,
    encode_params,
    free_params,
    dup_params,
    true, game_configure, custom_params,
    validate_params,
    new_game_desc,
    validate_desc,
    new_game,
    dup_game,
    free_game,
    true, solve_game,
    false, game_can_format_as_text_now, game_text_format,
    new_ui,
    free_ui,
    encode_ui,
    decode_ui,
    NULL,
    game_changed_state,
    interpret_move,
    execute_move,
    PREFERRED_TILESIZE, game_compute_size, game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    NULL,
    game_status,
    false, false, game_print_size, game_print,
    false,			       /* wants_statusbar */
    false, game_timing_state,
    0,				       /* flags */
};

