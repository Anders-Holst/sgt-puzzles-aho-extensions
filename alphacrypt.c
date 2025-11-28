/*
 * The alphacrypt puzzle, where letters stand for the numbers 1-26 and
 * the clues are made up of equations.
 *
 * Copyright (C) 2014 Anders Holst (aho@sics.se)
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"
extern bool midend_undo(midend *me);

enum {
    COL_BACKGROUND,
    COL_GRID,
    COL_USER,
    COL_HIGHLIGHT,
    COL_ERROR,
    COL_PENCIL,
    NCOLOURS
};

typedef enum { None, Constant, Plus, Minus, Times, Divide, Square, Sqroot, PythPlus, PythMinus, Modulo, Less, Greater } Operator;

const static int op_nary[] = {0, 0, 2, 2, 2, 2, 1, 1, 2, 2, 2, 1, 1};
const static int op_det[]  = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0};

struct game_params {
  int size;
  int constant_clues;
  int advanced_ops;
  int comparative_ops;
  int diff;
};

struct clues {
  int refcount;
  int num, cols, rows;
  char* letters;
  Operator* ops;
  char* l1vec;
  char* l2vec;
  midend *me;
};

struct game_state {
    const game_params *par;
    struct clues *clues;
    char *grid;
    long *pencil;		       /* bitmaps using bits 1<<1..1<<n */
    int completed, cheated;
};


static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    ret->size = 26;
    ret->constant_clues = 1;
    ret->advanced_ops = 0;
    ret->comparative_ops = 0;
    ret->diff = 2;

    return ret;
}

const static struct game_params alphacrypt_presets[] = {
  {10, 1, 0, 0, 2},
  {10, 0, 0, 0, 2},
  {16, 1, 0, 0, 2},
  {16, 0, 0, 0, 2},
  {26, 1, 0, 0, 1},
  {26, 1, 0, 0, 2},
  {26, 0, 0, 0, 2},
  {26, 0, 1, 0, 2},
  {26, 0, 0, 1, 2},
  {26, 0, 1, 1, 2},
  {26, 1, 0, 0, 3},
  {26, 0, 0, 0, 3}
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i != -1 && (i < 0 || i >= lenof(alphacrypt_presets)))
        return false;

    if (i == -1) {
      ret = *params;
    } else {
      ret = snew(game_params);
      *ret = alphacrypt_presets[i]; /* structure copy */
      *params = ret;
    }
 
    sprintf(buf, "Size %d", ret->size);
    if (!ret->constant_clues)
      sprintf(buf+strlen(buf), ", no constants");
    if (ret->advanced_ops)
      sprintf(buf+strlen(buf), ", advanced ops");
    if (ret->comparative_ops)
      sprintf(buf+strlen(buf), ", comparisons");
    if (ret->diff != 2)
      sprintf(buf+strlen(buf), (ret->diff == 1 ? ", easy" : ret->diff == 3 ? ", hard" : ", extreme"));

    *name = dupstr(buf);
    return true;
}

static void free_params(game_params *params)
{
    sfree(params);
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;		       /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
    char const *p = string;

    params->size = atoi(p);
    while (*p && isdigit((unsigned char)*p)) p++;

    params->constant_clues = 1;
    params->advanced_ops = params->comparative_ops = 0;
    while (*p == 'N' || *p == 'A' || *p == 'C') {
      if (*p == 'N')
        params->constant_clues = 0;
      else if (*p == 'A')
        params->advanced_ops = 1;
      else if (*p == 'C')
        params->comparative_ops = 1;
      p++;
    }
    if (*p == 'D') {
      p++;
      params->diff = atoi(p);
      while (*p && isdigit((unsigned char)*p)) p++;

      if (params->size < 8 && params->diff >= 3)
        params->diff = (params->size == 7 ? 3 : 2);
      else if (params->diff > 4) 
        params->diff = 4;
      else if (params->diff < 1)
        params->diff = 1;
    } else
      params->diff = 2;
}

static char *encode_params(const game_params *params, bool full)
{
    char ret[80];

    sprintf(ret, "%d%s%s%sD%d", params->size,
            (params->constant_clues ? "" : "N"),
            (params->advanced_ops ? "A" : ""),
            (params->comparative_ops ? "C" : ""),
            params->diff);

    return dupstr(ret);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[80];
    int ind = 0;

    ret = snewn(6, config_item);

    ret[ind].name = "Size";
    ret[ind].type = C_STRING;
    sprintf(buf, "%d", params->size);
    ret[ind].u.string.sval = dupstr(buf);

    ind++;
    ret[ind].name = "Constant clues";
    ret[ind].type = C_BOOLEAN;
    ret[ind].u.boolean.bval = (params->constant_clues ? true : false);

    ind++;
    ret[ind].name = "Advanced operators";
    ret[ind].type = C_BOOLEAN;
    ret[ind].u.boolean.bval = (params->advanced_ops ? true : false);

    ind++;
    ret[ind].name = "Comparisons";
    ret[ind].type = C_BOOLEAN;
    ret[ind].u.boolean.bval = (params->comparative_ops ? true : false);

    ind++;
    ret[ind].name = "Difficulty";
    ret[ind].type = C_CHOICES;
    ret[ind].u.choices.choicenames = ":Easy:Medium:Hard:Extreme";
    ret[ind].u.choices.selected = params->diff-1;

    ind++;
    ret[ind].name = NULL;
    ret[ind].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    int ind = 0;
    game_params *ret = snew(game_params);

    ret->size = atoi(cfg[ind++].u.string.sval);

    ret->constant_clues = (cfg[ind++].u.boolean.bval);
    ret->advanced_ops = (cfg[ind++].u.boolean.bval);
    ret->comparative_ops = (cfg[ind++].u.boolean.bval);

    ret->diff = cfg[ind++].u.choices.selected + 1;

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->size < 6 || params->size > 26)
        return "Game size must be between 6 and 26";
/*
    if (params->size == 7 && params->diff > 3)
        return "Game size 7 can not be more difficult than hard";
    else if (params->size == 6 && params->diff > 2)
        return "Game size 6 can not be more difficult than medium";
*/
    return NULL;
}

/* ----------------------------------------------------------------------
 * Game generation code.
 */

typedef struct Equation {
  int index;
  int guess;
  char letter;
  Operator op;
  struct Equation *r1, *r2;
  int nrefs;
  struct Equation** refs;
  long int poss;
  int done;
} Equation;

typedef struct EquationBoard {
  const game_params* par;
  int num;
  Equation** eqs;
} EquationBoard;

const static int sqh[] = {4, 5, 5, 9, 10, 10, 13, 13, 15, 15, 16, 17, 17, 20, 20, 25, 25, 25, 25, 25, 26, 26, -1};
const static int sql[] = {2, 3, 4, 3,  6,  8,  5, 12,  9, 12,  4,  8, 15, 12, 16,  5,  7, 24, 15, 20, 10, 24, -1};

const static float difflevels[] = {0.0, 0.75, 1.75, 4.0, 12.0};

/* --------- Create a random board --------- */



static int randomize_equation(Equation* eq, EquationBoard* eqb, int mx, random_state *rs)
{
  int n, m1, m2, i, k;
  int tp = random_upto(rs, 4 + (eqb->par->constant_clues && eqb->par->diff == 1 ? 1 : 0)
                           + eqb->par->advanced_ops*3 + eqb->par->comparative_ops);
  if (!eqb->par->constant_clues || eqb->par->diff > 1) tp += 1;
  if (!eqb->par->advanced_ops && tp > 4) tp += 3;
  /* If a selected case below fails, it will continue down to the next */
  switch (tp) {

  case 5:
  case 6: /* Sqr, Sqrt, Pyth */
    for (n=0, i=0; sqh[i] != -1; i++) {
      if (sql[i] == eq->index && sqh[i] <= mx) n++;
      if (sqh[i] == eq->index) n++;
    }
    if (n) {
      n = random_upto(rs, n);
      for (i=0; sqh[i] != -1; i++) {
        if (sql[i] == eq->index) {
          if (n==0) break;
          else n--;
        }
        if (sqh[i] == eq->index) {
          if (n==0) break;
          else n--;
        }
      }
      if (sql[i]*sql[i] == sqh[i]) {
        if (sqh[i] == eq->index) {
          eq->r1 = eqb->eqs[sql[i] -1];
          eq->op = Square;
        } else {
          eq->r1 = eqb->eqs[sqh[i] -1];
          eq->op = Sqroot;
        }
        eq->r2 = 0;
        if (op_nary[eq->r1->op] != 1 || eq->r1->r1 != eq) /* not same relation */
          return 1;
      } else {
        if (sqh[i] == eq->index) {
          eq->r1 = eqb->eqs[sql[i] -1];
          eq->op = PythPlus;
        } else {
          eq->r1 = eqb->eqs[sqh[i] -1];
          eq->op = PythMinus;
        }
        eq->r2 = eqb->eqs[(int)sqrt(sqh[i]*sqh[i] - sql[i]*sql[i]) -1];
        if (!(((eq->r1->op == PythPlus || eq->r1->op == PythMinus) && 
               ((eq->r1->r1 == eq && eq->r1->r2 == eq->r2) ||
                (eq->r1->r2 == eq && eq->r1->r1 == eq->r2))) ||
              ((eq->r2->op == PythPlus || eq->r2->op == PythMinus) &&
               ((eq->r2->r1 == eq && eq->r2->r2 == eq->r1) ||
                (eq->r2->r2 == eq && eq->r2->r1 == eq->r1))))) /* not same relation */
          return 1;
      }
    }

  case 7: /* Modulo */
    if (eq->index*2 < mx) {
      for (n=0, i=eq->index+1; i<=mx-eq->index; i++) {
        k = (mx - eq->index)/i;
        n += k;
      }
      n = random_upto(rs, n);
      for (i=eq->index+1; i<=mx-eq->index; i++) {
        k = (mx - eq->index)/i;
        if (n >= k)
          n -= k;
        else {
          n += 1;
          break;
        }
      }
      eq->r1 = eqb->eqs[eq->index+n*i -1];
      eq->r2 = eqb->eqs[i -1];
      eq->op = Modulo;
      if (!(((eq->r1->op == Plus || eq->r1->op == Minus || eq->r1->op == Modulo) && 
             ((eq->r1->r1 == eq && eq->r1->r2 == eq->r2) ||
              (eq->r1->r2 == eq && eq->r1->r1 == eq->r2))) ||
            ((eq->r2->op == Plus || eq->r2->op == Minus || eq->r2->op == Modulo) &&
             ((eq->r2->r1 == eq && eq->r2->r2 == eq->r1) ||
              (eq->r2->r2 == eq && eq->r2->r1 == eq->r1))))) /* not same relation */
      return 1;
    }

  case 3:
  case 4: /* Times, Divide */
    m1 = 0;
    for (i=(int)sqrt(eq->index-1); i>1; i--)
      if (eq->index % i == 0)
        m1++;
    m2 = (eq->index > 1 && eq->index <= mx/2 ? mx / eq->index - (eq->index * eq->index <= mx ? 2 : 1) : 0);
    if (m1 + m2 > 0) {
      n = random_upto(rs, m1+m1+m2);
      if (n>=m1*2) {
        m2 = n - m1*2 + 2;
        if (m2 >= eq->index) m2++;
        eq->op = Divide;
        eq->r2 = eqb->eqs[m2 -1];
        eq->r1 = eqb->eqs[eq->index * m2 -1];
      } else {
        m2 = n % m1;
        for (i=(int)sqrt(eq->index-1); i>1; i--)
          if (eq->index % i == 0) {
            if (m2==0)
              break;
            else
              m2--;
          }
        eq->op = Times;
        if (n >= m1) {
          eq->r1 = eqb->eqs[i -1];
          eq->r2 = eqb->eqs[eq->index / i -1];
        } else {
          eq->r2 = eqb->eqs[i -1];
          eq->r1 = eqb->eqs[eq->index / i -1];
        }
      }
      if (!(((eq->r1->op == Times || eq->r1->op == Divide) &&  /* not same relation */
             ((eq->r1->r1 == eq && eq->r1->r2 == eq->r2) ||
              (eq->r1->r2 == eq && eq->r1->r1 == eq->r2))) ||
            ((eq->r2->op == Times || eq->r2->op == Divide) &&
             ((eq->r2->r1 == eq && eq->r2->r2 == eq->r1) ||
              (eq->r2->r2 == eq && eq->r2->r1 == eq->r1))) ||
            ((eq->r1->op == Less || eq->r1->op == Greater) && (eq->r1->r1 == eq)) ||
            ((eq->r2->op == Less || eq->r2->op == Greater) && (eq->r2->r1 == eq))))
        return 1;
    }

  case 1:
  case 2: /* Plus, Minus */
    n = random_upto(rs, mx - 1 - (eq->index % 2 ? 0 : 1) - (eq->index * 2 <= mx ? 1 : 0)) + 1;
    if (eq->index % 2 == 0 && n >= eq->index / 2) n++;
    if (n >= eq->index) n++;
    if (n >= eq->index * 2) n++;
    eq->r1 = eqb->eqs[n-1];
    if (n < eq->index) {
      eq->op = Plus;
      eq->r2 = eqb->eqs[eq->index - n -1];
    } else {
      eq->op = Minus;
      eq->r2 = eqb->eqs[n - eq->index -1];
    }
    if (!(((eq->r1->op == Plus || eq->r1->op == Minus || eq->r1->op == Modulo) && 
           ((eq->r1->r1 == eq && eq->r1->r2 == eq->r2) ||   /* not same relation */
            (eq->r1->r2 == eq && eq->r1->r1 == eq->r2))) ||
          ((eq->r2->op == Plus || eq->r2->op == Minus || eq->r2->op == Modulo) &&
           ((eq->r2->r1 == eq && eq->r2->r2 == eq->r1) ||
            (eq->r2->r2 == eq && eq->r2->r1 == eq->r1))) ||
          ((eq->r1->op == Less || eq->r1->op == Greater) && (eq->r1->r1 == eq)) ||
          ((eq->r2->op == Less || eq->r2->op == Greater) && (eq->r2->r1 == eq))))
      return 1;

  case 8: /* Comparison */
    if (eqb->par->comparative_ops) {
      n = random_upto(rs, mx-1) + 1;
      if (n >= eq->index) {
        n++;
        eq->op = Less;
      } else {
        eq->op = Greater;
      }
      eq->r1 = eqb->eqs[n -1];
      eq->r2 = 0;
      if (!(((op_nary[eq->r1->op] == 1) &&
             (eq->r1->r1 == eq)) ||
            ((op_nary[eq->r1->op] == 2) &&
             (eq->r1->r1 == eq || eq->r1->r2 == eq)))) /* not same relation */
        return 1;
    }

  case 0: /* Constant */
    if (eqb->par->constant_clues) {
      eq->op = Constant;
      eq->r1 = 0;
      eq->r2 = 0;
      return 1;
    }

  default:
    eq->op = None;
    eq->r1 = 0;
    eq->r2 = 0;
    return 0;
  }
}

static EquationBoard* randomize_board(const game_params* par, random_state *rs)
{
  int i, j, k, n, minr, nn = 0;
  Equation* eq;
  EquationBoard* eqb = snew(EquationBoard);
  eqb->num = n = par->size;
  eqb->par = par;
  eqb->eqs = snewn(n, Equation*);
  for (i=0; i<n; i++) {
    eqb->eqs[i] = eq = snew(Equation);
    eq->index = i+1;
    eq->op = None;
    eq->nrefs = 0;
    eq->refs = 0;
  }
  for (i=0; i<n; i++) {
    randomize_equation(eqb->eqs[i], eqb, n, rs);
    if (eqb->eqs[i]->op == Constant) nn++;
  }
  /* Setup referenses between equations */
  for (i=0; i<n; i++) {
    eq = eqb->eqs[i];
    if (op_nary[eq->op] == 2) {
      eq->r1->nrefs +=1;
      eq->r2->nrefs +=1;
    } else if (op_nary[eq->op] == 1) {
      eq->r1->nrefs +=1;
    }
  }
  if (par->constant_clues && nn == 0) { /* Make sure there is at least one constant */
    if (par->diff >= 3) { /* select carefully to make it hard */
      minr = n;
      for (i=0; i<n; i++) {
        if (eqb->eqs[i]->nrefs < minr)
          minr = eqb->eqs[i]->nrefs, nn = 1;
        else if (eqb->eqs[i]->nrefs == minr)
          nn++;
      }
    } else { /* just pick any */
      nn = n;
      minr = -1;
    }
    for (j=0; j<(par->diff >= 3 ? 1 : (par->size + 13)/ 10); j++) {
      k = random_upto(rs, nn);
      for (i=0; i<n; i++) {
        if (minr == -1 || eqb->eqs[i]->nrefs == minr) {
          if (k == 0) {
            eq = eqb->eqs[i];
            if (op_nary[eq->op] == 2) {
              eq->r1->nrefs -=1;
              eq->r2->nrefs -=1;
            } else if (op_nary[eq->op] == 1) {
              eq->r1->nrefs -=1;
            }
            eq->op = Constant;
            eq->r1 = 0;
            eq->r2 = 0;
            break;
          }
          k--;
        }
      }
    }
  }
  for (i=0; i<n; i++) {
    eq = eqb->eqs[i];
    eq->refs = snewn(eq->nrefs, Equation*);
    eq->nrefs = 0;
  }
  for (i=0; i<n; i++) {
    eq = eqb->eqs[i];
    if (op_nary[eq->op] == 2) {
      eq->r1->refs[eq->r1->nrefs] = eq;
      eq->r1->nrefs +=1;
      eq->r2->refs[eq->r2->nrefs] = eq;
      eq->r2->nrefs +=1;
    } else if (op_nary[eq->op] == 1) {
      eq->r1->refs[eq->r1->nrefs] = eq;
      eq->r1->nrefs +=1;
    }
  }
  return eqb; 
}

static EquationBoard* import_board(const game_params* par, struct clues* cl)
{
  int i, j, n;
  Equation* eq;
  EquationBoard* eqb = snew(EquationBoard);
  eqb->num = n = par->size;
  eqb->par = par;
  eqb->eqs = snewn(n, Equation*);
  for (i=0; i<n; i++) {
    eqb->eqs[i] = eq = snew(Equation);
    eq->index = -1;
    eq->letter = cl->letters[i];
    eq->op = cl->ops[i];
    eq->r1 = eq->r2 = 0;
    eq->nrefs = 0;
    eq->refs = 0;
  }
  for (i=0; i<n; i++) {
    eq = eqb->eqs[i];
    if (op_nary[eq->op] == 2) {
      for (j=0; j<n && eqb->eqs[j]->letter != cl->l1vec[i]; j++);
      eq->r1 = eqb->eqs[j];
      for (j=0; j<n && eqb->eqs[j]->letter != cl->l2vec[i]; j++);
      eq->r2 = eqb->eqs[j];
    } else if (op_nary[eq->op] == 1) {
      for (j=0; j<n && eqb->eqs[j]->letter != cl->l1vec[i]; j++);
      eq->r1 = eqb->eqs[j];
    } else if (eq->op == Constant) {
      eq->index = (int)cl->l1vec[i];
    }
  }
  /* Setup referenses between equations */
  for (i=0; i<n; i++) {
    eq = eqb->eqs[i];
    if (op_nary[eq->op] == 2) {
      eq->r1->nrefs +=1;
      eq->r2->nrefs +=1;
    } else if (op_nary[eq->op] == 1) {
      eq->r1->nrefs +=1;
    }
  }
  for (i=0; i<n; i++) {
    eq = eqb->eqs[i];
    eq->refs = snewn(eq->nrefs, Equation*);
    eq->nrefs = 0;
  }
  for (i=0; i<n; i++) {
    eq = eqb->eqs[i];
    if (op_nary[eq->op] == 2) {
      eq->r1->refs[eq->r1->nrefs] = eq;
      eq->r1->nrefs +=1;
      eq->r2->refs[eq->r2->nrefs] = eq;
      eq->r2->nrefs +=1;
    } else if (op_nary[eq->op] == 1) {
      eq->r1->refs[eq->r1->nrefs] = eq;
      eq->r1->nrefs +=1;
    }
  }
  return eqb; 
}

/* --------- Count solutions --------- */

static int equation_subpoints(Equation* eq)
{
  /* One point for eq, some points for done, many points if just one missing */
  int ndone = 0;
  if (op_nary[eq->op] == 2) {
    ndone = (eq->done == 1) + (eq->r1->done == 1) + (eq->r2->done == 1);
    return (ndone == 2 && op_det[eq->op] ? 1000 : ndone > 0 ? 10 : 1);
  } else if (op_nary[eq->op] == 1) {
    ndone = (eq->done == 1) + (eq->r1->done == 1);
    return (op_det[eq->op] ? (ndone == 1 ? 1000 : 20) : (ndone > 0 ? 2 : 1));
  } else { /* None */
    return 1;
  }
}

static int equation_points(Equation* eq)
{
  /* If done, no points, if determined full points, otherwise sum subpoints */
  int i, tmp, sum = 0;
  if (eq->done == 1)
    return 0;
  sum = equation_subpoints(eq);
  if (sum == 1000) return 1000;
  for (i=0; i<eq->nrefs; i++) {
    tmp = equation_subpoints(eq->refs[i]);
    if (tmp == 1000) return 1000;
    sum += tmp;
  }
  return sum;
}

static Equation* select_equation(EquationBoard* eqb)
{
  int i, tmp, mi = -1, mx = 0;
  for (i=0; i<eqb->num; i++) {
    tmp = equation_points(eqb->eqs[i]);
    if (tmp > mx)
      mx = tmp, mi = i;
  }
  if (mi != -1)
    return eqb->eqs[mi];
  else
    return 0;
}

static int verify_eq2(Operator op, int z, int x, int y)
{
  if (op == Plus)
    return (z == x + y);
  else if (op == Minus)
    return (z == x - y);
  else if (op == Times)
    return (z == x * y);
  else if (op == Divide)
    return (z * y == x);
  else if (op == Modulo)
    return (z == x % y);
  else if (op == PythPlus)
    return (z * z == x * x + y * y);
  else if (op == PythMinus)
    return (z * z == x * x - y * y);
  else
    return 0;
}

static int verify_eq1(Operator op, int z, int x)
{
  if (op == Square)
    return (z == x * x);
  else if (op == Sqroot)
    return (z * z == x);
  else if (op == Less)
    return (z < x);
  else if (op == Greater)
    return (z > x);
  else
    return 0;
}

static int check_subpossible(Equation* eq)
{
  if (eq->done == 1) {
    if (op_nary[eq->op] == 2) {
      if (eq->r1->done == 1 && eq->r2->done == 1) {
        return verify_eq2(eq->op, eq->guess, eq->r1->guess, eq->r2->guess);
      } else
        return 1;
    } else if (op_nary[eq->op] == 1) {
      if (eq->r1->done == 1) {
        return verify_eq1(eq->op, eq->guess, eq->r1->guess);
      } else
        return 1;
    } else
      return 1;
  } else
    return 1;
}

static int check_possible(Equation* eq)
{
  int i;
  if (!check_subpossible(eq))
    return 0;
  for (i=0; i<eq->nrefs; i++)
    if (!check_subpossible(eq->refs[i]))
      return 0;
  return 1;
}

static int count_internal(EquationBoard* eqb, long int pmask, int lim, int* iter, char* ans)
{
  Equation* eq = select_equation(eqb);
  int i, b, n, sum = 0;
  n = eqb->par->size;
  if (!eq) {
    if (ans) {
      for (i=0; i<n; i++) {
        *ans++ = eqb->eqs[i]->letter;
        sprintf(ans, "%d", eqb->eqs[i]->guess);
        ans += strlen(ans);
      }
    }
    return 1;
  }
  (*iter)++;
  eq->done = 1;
  for (i=0, b=1; i<n; i++, b<<=1) {
    eq->guess = i+1;
    if (!(pmask & b) && check_possible(eq)) {
      sum += count_internal(eqb, pmask | b, lim, iter, ans);
      if (sum > lim) break;
    }
  }
  eq->done = 0;
  return sum;
}

static int count_solutions(EquationBoard* eqb, int lim, float* diff, char* ans)
{
  int i, sol, n = eqb->par->size;
  float nn = 0.0;
  int iter = 0;
  for (i=0; i<n; i++)
    if (eqb->eqs[i]->op == Constant) {
      eqb->eqs[i]->guess = eqb->eqs[i]->index;
      eqb->eqs[i]->done = 1;
      nn += 1.0;
/*
      for (j=0; j<eqb->eqs[i]->nrefs; j++)
        nn += (!op_det[eqb->eqs[i]->refs[j]->op] ? 0.0 :
               op_nary[eqb->eqs[i]->refs[j]->op] == 1 ? 1.0 : 0.5);
*/
    } else
      eqb->eqs[i]->done = 0;
  sol = count_internal(eqb, 0, lim, &iter, ans);
  *diff = (sol == 1 ? (nn > n-2 ? 0.0 : ((float)iter)/((n-nn)*12.0)) : -1.0);
  return sol;
}

/* --------- Prune until selected difficulty and still unique --------- */

static float try_prune_equation(Equation* eq, EquationBoard* eqb, float dlim)
{
  Operator oop;
  int i, sol;
  float diff;
  if (eq->op == None)
    return -1;
  oop = eq->op;
  eq->op = None;
  if (op_nary[oop] >= 1) {
    for (i=eq->r1->nrefs-1; i>=0 && eq->r1->refs[i] != eq; i--);
    eq->r1->nrefs -= 1;
    for (; i<eq->r1->nrefs; i++)
      eq->r1->refs[i] = eq->r1->refs[i+1];
    if (op_nary[oop] == 2) {
      for (i=eq->r2->nrefs-1; i>=0 && eq->r2->refs[i] != eq; i--);
      eq->r2->nrefs -= 1;
      for (; i<eq->r2->nrefs; i++)
        eq->r2->refs[i] = eq->r2->refs[i+1];
    }
  }

  sol = count_solutions(eqb, 1, &diff, 0);

  if (sol != 1 || diff > dlim) {
    eq->op = oop;
    if (op_nary[oop] >= 1) {
      eq->r1->refs[eq->r1->nrefs] = eq;
      eq->r1->nrefs++;
      if (op_nary[oop] == 2) {
        eq->r2->refs[eq->r2->nrefs] = eq;
        eq->r2->nrefs++;
      }
    }
    return -1.0;
  } else {
    eq->r1 = 0;
    eq->r2 = 0;
    return diff;
  }
}

static void prune_equations(EquationBoard* eqb, random_state* rs, float dlim1, float dlim2, float* diff0, int* feat)
{
  long int mask = 0;
  long int b;
  int i, j, k, n;
  int na, nc, nn, da, dc, dn;
  float diff;
  Operator op;
  n = eqb->par->size;
  
  for (na=0, nc=0, nn=0, i=0; i<n; i++) {
    op = eqb->eqs[i]->op;
    if (op == Constant) nn++;
    if (op == Square || op == Sqroot || op == PythPlus || op == PythMinus || op == Modulo) na++;
    if (op == Less || op == Greater) nc++;
  }
  for (i=n; i>0 && *diff0 < dlim1; i--) {
    k = random_upto(rs, i);
    for (b=1, j=0; (b&mask) || (k>0); j++, k-=(!(b&mask)), b<<=1);
    mask |= b;
    op = eqb->eqs[j]->op;
    da = dc = dn = 0;
    if (op == Constant) {
      if (nn == 1)
        continue;
      else
        dn = 1;
    } else if (op == Square || op == Sqroot || op == PythPlus || op == PythMinus || op == Modulo) {
      if (na <= 2)
        continue;
      else
        da = 1;
    } else if (op == Less || op == Greater) {
      if (nc <= 2)
        continue;
      else
        dc = 1;
    }
    diff = try_prune_equation(eqb->eqs[j], eqb, dlim2);
    if (diff >= 0.0) {
      *diff0 = diff;
      nn -= dn;
      na -= da;
      nc -= dc;
    }
  }
  *feat = ((!eqb->par->constant_clues || nn) &&
           (!eqb->par->advanced_ops || na) &&
           (!eqb->par->comparative_ops || nc));
}

/*
void print_board(EquationBoard* eqb)
{
  int i;
  Equation* eq;
  for (i=0; i<eqb->par->size; i++) {
    eq = eqb->eqs[i];
    if (op_nary[eq->op] == 2)
      printf("%c = %c %c %c\n", eq->letter, eq->r1->letter, (eq->op == Plus ? '+' : eq->op == Minus ? '-' : eq->op == Times ? '*' : '/'), eq->r2->letter);
//      printf("%c (%d) = %c %c %c\n", eq->letter, eq->index, eq->r1->letter, (eq->op == Plus ? '+' : eq->op == Minus ? '-' : eq->op == Times ? '*' : '/'), eq->r2->letter);
    else if (op_nary[eq->op] == 1) {
    } else if (eq->op == Constant) {
    } else
      printf("%c \n", eq->letter);
//      printf("%c (%d)\n", eq->letter, eq->index);
  }
}
*/

static void free_board(EquationBoard* eqb)
{
  int i, n = eqb->par->size;
  for (i=0; i<n; i++) {
    if (eqb->eqs[i]->refs)
      sfree(eqb->eqs[i]->refs);
    sfree(eqb->eqs[i]);
  }
  sfree(eqb->eqs);
  sfree(eqb);
}

static EquationBoard* construct_board(random_state* rs, const game_params* par, char** aux)
{
  long int mask = 0;
  long int b;
  float diff, dmin, dmax;
  int i, j, k, n, sol, feat;
  char* p;
  EquationBoard* eqb;
  Equation** oldeqs;
  n = par->size;
  k = 1;
  dmin = difflevels[par->diff-1];
  dmax = difflevels[par->diff];
  while (1) {
    eqb = randomize_board(par, rs);
    sol = count_solutions(eqb, 1, &diff, 0);
    if (sol == 1) {
      prune_equations(eqb, rs, dmin, dmax, &diff, &feat);
      if (feat && diff >= dmin && diff <= dmax)
        break;
    }
    if (k > 10) {
      dmin -= 0.05;
      dmax += 0.05;
    }
    k += 1;
    free_board(eqb);
  }
  /* assign letters */
  mask = 0;
  oldeqs = eqb->eqs;
  eqb->eqs = snewn(n, Equation*);
  for (i=n; i>0; i--) {
    k = random_upto(rs, i);
    for (b=1, j=0; (b&mask) || (k>0); j++, k-=(!(b&mask)), b<<=1);
    mask |= b;
    eqb->eqs[n-i] = oldeqs[j];
    eqb->eqs[n-i]->letter = 'A' + (n-i);
  }
  /* fill in answer */
  *aux = p = snewn(3*n+2, char);
  *p++ = 's';
  for (i=0; i<n; i++) {
    *p++ = eqb->eqs[i]->letter;
    sprintf(p, "%d", eqb->eqs[i]->index);
    p += strlen(p);
  }
  sfree(oldeqs);
  return eqb;
}

static char *new_game_desc(const game_params *params, random_state *rs,
			   char **aux, bool interactive)
{
    char *buf, *p;
    int n, i;
    EquationBoard* eqb;
    Equation* eq;
    eqb = construct_board(rs, params, aux);

    n = params->size;
    p = buf = snewn(n * 9 + 10, char);

    for (i=0; i<n; i++) {
      eq = eqb->eqs[i];
      *p++ = eq->letter;
      if (op_nary[eq->op] == 2) {
        *p++ = '=';
        if (eq->op == PythPlus || eq->op == PythMinus)
          *p++ = 'r', *p++ = 's';
        *p++ = eq->r1->letter;
        *p++ = (eq->op == Plus || eq->op == PythPlus ? '+' :
                eq->op == Minus || eq->op == PythMinus ? '-' :
                eq->op == Times ? '*' : eq->op == Divide ? '/' :
                eq->op == Modulo ? '%' : '?');
        if (eq->op == PythPlus || eq->op == PythMinus)
          *p++ = 's';
        *p++ = eq->r2->letter;
      } else if (op_nary[eq->op] == 1) {
        if (eq->op == Less)
          *p++ = '<';
        else if (eq->op == Greater)
          *p++ = '>';
        else if (eq->op == Square)
          *p++ = '=', *p++ = 's';
        else if (eq->op == Sqroot)
          *p++ = '=', *p++ = 'r';
        *p++ = eq->r1->letter;
      } else if (eq->op == Constant) {
        *p++ = '=';
        sprintf(p, "%d", eq->index);
        p += strlen(p);
      }
      *p++ = (i==n-1 ? '.' : ',');
    }
    *p = '\0';

    free_board(eqb);
    return buf;
}

/* ----------------------------------------------------------------------
 * Main game UI.
 */

static const char *validate_desc(const game_params *params, const char *desc)
{
  int wanted = params->size;
  int n = 0;

  while (n < wanted && *desc) {
    char c = *desc++;
    if (c < 'A' || c > 'Z')  /* should also check that they are unique... */
      return "Expected letter";
    c = *desc++;
    if (c == '=') {
      c = *desc++;
      if (c == 'r') {
        c = *desc++;
        if (c == 's') {
          c = *desc++;
          if (c < 'A' || c > 'Z')
            return "Expected first operand letter";
          c = *desc++;
          if (c != '+' && c != '-')
            return "Expected + or - operator";
          c = *desc++;
          if (c != 's')
            return "Expected an 's' before second operand letter";
          c = *desc++;
          if (c < 'A' || c > 'Z')
            return "Expected second operand letter";
        } else {
          if (c < 'A' || c > 'Z')
            return "Expected operand letter";
        }
      } else if (c == 's') {
        c = *desc++;
        if (c < 'A' || c > 'Z')
          return "Expected operand letter";
      } else if (c >= '1' && c <= '9') {
        while (*desc >= '0' && *desc <= '9') desc++;
      } else {
        if (c < 'A' || c > 'Z')
          return "Expected first operand letter";
        c = *desc++;
        if (c != '+' && c != '-' && c != '*' && c != '/' && c != '%')
          return "Expected operator";
        c = *desc++;
        if (c < 'A' || c > 'Z')
          return "Expected second operand letter";
      }
      c = *desc++;
    } else if (c == '<' || c == '>') {
      c = *desc++;
      if (c < 'A' || c > 'Z')
        return "Expected operand letter";
      c = *desc++;
    }
    if (c != ',' && c != '.')
      return "Expected separator (comma or period)";
    n++;
  }

  if (n > wanted || *desc)
    return "Too long description";
  else if (n < wanted)
    return "Too short description";
  else
    return NULL;
}

static game_state *new_game(midend *me, const game_params *params, const char *desc)
{
    game_state *state = snew(game_state);
    int i, n, k;
    char c;

    n = params->size;

    state->par = params;
    state->grid = snewn(n, char);
    state->pencil = snewn(n, long);
    for (i=0; i<n; i++) {
        state->grid[i] = -1;
        state->pencil[i] = 0;
    }
    state->completed = state->cheated = false;

    state->clues = snew(struct clues);
    state->clues->refcount = 1;
    state->clues->num = n;
    if (n > 12) {
      state->clues->cols = 2;
      state->clues->rows = (n+1)/2;
    } else {
      state->clues->cols = 1;
      state->clues->rows = n;
    }
    state->clues->letters = snewn(n, char);
    state->clues->ops = snewn(n, Operator);
    state->clues->l1vec = snewn(n, char);
    state->clues->l2vec = snewn(n, char);
    state->clues->me = me;

    for (i=0; i<n; i++) {
      state->clues->letters[i] = *desc++;
      c = *desc++;
      if (c == '=') {
        c = *desc++;
        if (c == 'r') {
          c = *desc++;
          if (c == 's') {
            c = *desc++;
            state->clues->l1vec[i] = c;
            c = *desc++;
            state->clues->ops[i] = (c == '+' ? PythPlus : PythMinus);
            desc++;
            state->clues->l2vec[i] = *desc++;
            desc++;
          } else {
            state->clues->ops[i] = Sqroot;
            state->clues->l1vec[i] = c;
            state->clues->l2vec[i] = ' ';
            desc++;
          }
        } else if(c == 's') {
          state->clues->ops[i] = Square;
          c = *desc++;
          state->clues->l1vec[i] = c;
          state->clues->l2vec[i] = ' ';
          desc++;
        } else if (c >= '1' && c <= '9') {
          sscanf(desc-1, "%d", &k);
          state->grid[i] = k;
          state->clues->l1vec[i] = (char)k;
          state->clues->l2vec[i] = ' ';
          state->clues->ops[i] = Constant;
          while (*desc >= '0' && *desc <= '9') desc++;
          desc++;
        } else {
          state->clues->l1vec[i] = c;
          c = *desc++;
          state->clues->ops[i] = (c == '+' ? Plus : c == '-' ? Minus : c == '*' ? Times : c == '/' ? Divide : c == '%' ? Modulo : None);
          state->clues->l2vec[i] = *desc++;
          desc++;
        }
      } else if (c == '<' || c == '>') {
        state->clues->ops[i] = (c == '<' ? Less : Greater);
        state->clues->l1vec[i] = *desc++;
        state->clues->l2vec[i] = ' ';
        desc++;
      } else {
        state->clues->l1vec[i] = state->clues->l2vec[i] = 0;
        state->clues->ops[i] = None;
      }
    }

    assert(!*desc);

    return state;
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = snew(game_state);
    int n, i;

    n = state->par->size;
    ret->par = state->par;
    ret->grid = snewn(n, char);
    ret->pencil = snewn(n, long);
    for (i=0; i<n; i++) {
        ret->grid[i] = state->grid[i];
        ret->pencil[i] = state->pencil[i];
    }
    ret->completed = state->completed;
    ret->cheated = state->cheated;

    ret->clues = state->clues;
    ret->clues->refcount++;

    return ret;
}

static void free_game(game_state *state)
{
    if (--state->clues->refcount <= 0) {
        sfree(state->clues->letters);
        sfree(state->clues->l1vec);
        sfree(state->clues->l2vec);
        sfree(state->clues->ops);
        sfree(state->clues);
    }
    sfree(state->grid);
    sfree(state->pencil);
    sfree(state);
}

static char *solve_game(const game_state *state, const game_state *currstate,
			const char *aux, const char **error)
{
  if (aux)
    return dupstr(aux);
  else {
    EquationBoard *eqb = import_board(state->par, state->clues);
    int sol;
    float diff;
    char* ans = snewn(3*state->par->size+2, char);
    *ans = 's';
    sol = count_solutions(eqb, 1000, &diff, ans + 1);
    free_board(eqb);
    if (sol>0) {
      return ans;
    } else {
      sfree(ans);
      *error = "No solution found";
      return NULL;
    }
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
    /*
     * These are the coordinates of the currently highlighted
     * square on the grid, if hshow = 1.
     */
    int hx, hy;
    /*
     * This indicates whether the current highlight is a
     * pencil-mark one or a real one.
     */
    int hpencil;
    /*
     * This indicates whether or not we're showing the highlight
     * (used to be hx = hy = -1); important so that when we're
     * using the cursor keys it doesn't keep coming back at a
     * fixed position. When hshow = 1, pressing a valid number
     * or letter key or Space will enter that number or letter in the grid.
     */
    int hshow;
    /*
     * This indicates whether we're using the highlight as a cursor;
     * it means that it doesn't vanish on a keypress, and that it is
     * allowed on immutable squares.
     */
    int hcursor;
    /*
     * To allow for two-digit inputs, save one unfinished input char.
     */
    int pending;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);

    ui->hx = ui->hy = 0;
    ui->hpencil = ui->hshow = ui->hcursor = ui->pending = 0;
    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
    /*
     * We prevent pencil-mode highlighting of a filled square, unless
     * we're using the cursor keys. So if the user has just filled in
     * a square which we had a pencil-mode highlight in (by Undo, or
     * by Redo, or by Solve), then we cancel the highlight.
     */
    if (ui->hshow && ui->hpencil && !ui->hcursor &&
        newstate->grid[ui->hx * newstate->clues->rows + ui->hy] != -1) {
        ui->hshow = 0;
    }
}

#define PREFERRED_TILESIZE 48
#define TILESIZEX(size) (4*size)
#define TILESIZEY(size) (size)
#define BORDER(size) (size/2)
#define GRIDEXTRA(size) max((size/32),1)

#define TOTSIZEX(w, size) ((w) * TILESIZEX(size) + 2*BORDER(size))
#define TOTSIZEY(h, size) ((h) * TILESIZEY(size) + 2*BORDER(size))
#define INNERSIZEX(size) (TILESIZEX(size) - 1 - 2*GRIDEXTRA(size))
#define INNERSIZEY(size) (TILESIZEY(size) - 1 - 2*GRIDEXTRA(size))
#define SUBTILEOFFX(size) TILESIZEY(size)
#define SUBTILESIZEX(size) INNERSIZEY(size)

#define COORDX(x, size) ((x)*TILESIZEX(size) + BORDER(size) + 1 + GRIDEXTRA(size))
#define COORDY(y, size) ((y)*TILESIZEY(size) + BORDER(size) + 1 + GRIDEXTRA(size))
#define FROMCOORDX(x, size) (((x)+(TILESIZEX(size) - BORDER(size))) / TILESIZEX(size) - 1)
#define FROMCOORDY(y, size) (((y)+(TILESIZEY(size) - BORDER(size))) / TILESIZEY(size) - 1)

#define FLASH_TIME 0.4F

#define DF_ERR_NUMBER 0x40
#define DF_ERR_EQUATION 0x20
#define DF_HIGHLIGHT 0x04
#define DF_HIGHLIGHT_PENCIL 0x08
#define DF_PENDING_INPUT 0x10
#define DF_HAS_DIGIT 0x01
#define DF_HAS_PENCIL 0x02

struct game_drawstate {
    int tilesize;
    int w, h;
    int started;
    char *status;
    char *numbers;
    long *pencils;
    long *errors;
};

static void abort_pending(const game_state *state0, game_ui *ui)
{
  ui->pending = 0;
  if (!ui->hcursor)
    ui->hshow = 0;
  midend_undo(state0->clues->me); /* Remove the tentative pending state */
}

static void finish_pending(const game_ui *ui0)
{
  game_ui *ui = (game_ui*)ui0;
  ui->pending = 0;
  if (!ui->hcursor)
    ui->hshow = 0;
}

static char* make_move_string(const game_params *par, game_ui *ui, struct clues* cl, int n)
{
  char buf[80];
  if ((n != -1 && 
       (n > par->size || n == 0)))
    return (ui->pending ? dupstr("o") : MOVE_UI_UPDATE);
  sprintf(buf, "%c%c%d", (char)(ui->hpencil ? 'p' : 'r'),
          cl->letters[ui->hx*cl->rows + ui->hy], n);
  return dupstr(buf);
}

static char *interpret_move(const game_state *state, game_ui *ui, const game_drawstate *ds,
			    int x, int y, int button)
{
    int num = state->par->size;
    int rows = state->clues->rows;
    int cols = state->clues->cols;
    int n;
    int tx, ty;
    char* retstr = "";

    button &= ~MOD_MASK;

    tx = FROMCOORDX(x, ds->tilesize);
    ty = FROMCOORDY(y, ds->tilesize);

    if (tx >= 0 && tx < cols && ty >= 0 && ty < rows) {
        if (button == LEFT_BUTTON) {
            ui->hcursor = 0;
            if (ui->pending) {
              retstr = make_move_string(state->par, ui, state->clues, ui->pending - '0');
              abort_pending(state, ui);
            }
            if (tx == ui->hx && ty == ui->hy && ui->hshow && ui->hpencil == 0) {
              /* left-click on the already active square */
              ui->hshow = 0;
            } else {
              /* left-click on a new square */
              ui->hx = tx;
              ui->hy = ty;
              ui->hshow = (ty + tx*rows < num && state->clues->ops[ty + tx*rows] != Constant);
              ui->hpencil = 0;
            }
            return retstr;
        }
        if (button == RIGHT_BUTTON) {
            /*
             * Pencil-mode highlighting for non filled squares.
             */
            ui->hcursor = 0;
            if (ui->pending) {
              retstr = make_move_string(state->par, ui, state->clues, ui->pending - '0');
              abort_pending(state, ui);
            }
            if (tx == ui->hx && ty == ui->hy && ui->hshow && ui->hpencil) {
              /* right-click on the already active square */
              ui->hshow = 0;
              ui->hpencil = 0;
            } else {
              /* right-click on a new square */
              ui->hx = tx;
              ui->hy = ty;
              if (ty + tx*rows >= num || state->grid[ty + tx*rows] != -1) {
                ui->hshow = 0;
                ui->hpencil = 0;
              } else {
                ui->hshow = 1;
                ui->hpencil = 1;
              }
            }
            return retstr;
        }
    }
    if (IS_CURSOR_MOVE(button)) {
      if (ui->pending) {
        retstr = make_move_string(state->par, ui, state->clues, ui->pending - '0');
        abort_pending(state, ui);
      }
      move_cursor(button, &ui->hx, &ui->hy, cols, rows, 0, NULL);
      ui->hshow = ui->hcursor = 1;
      return retstr;
    }
    if (ui->hshow &&
        (button == CURSOR_SELECT)) {
      if (ui->pending) {
        retstr = make_move_string(state->par, ui, state->clues, ui->pending - '0');
        abort_pending(state, ui);
      } else {
        ui->hpencil = 1 - ui->hpencil;
        ui->hcursor = 1;
      }
      return retstr;
    }

    if (ui->hshow &&
	((button >= '0' && button <= '9') ||
	 button == CURSOR_SELECT2 || button == '\b')) {
        /*
         * Can't make pencil marks in a filled square. This can only
         * become highlighted if we're using cursor keys.
         */
        if (ui->hpencil && (ui->hy + ui->hx*rows >= num || state->grid[ui->hy + ui->hx*rows] != -1))
          return NULL;

        if (!ui->hcursor) ui->hshow = 0;

        if (ui->pending) {
          if (button == CURSOR_SELECT2)
            n = ui->pending - '0';
          else if (button == '\b') {
            n = -1;
          } else 
            n = (ui->pending - '0')*10 + (button - '0');
          retstr = make_move_string(state->par, ui, state->clues, n);
          abort_pending(state, ui);
        } else {
          if (button == CURSOR_SELECT2 || button == '\b')
            n = -1;
          else if (state->par->size > 9 && button >= '1' && button <= '0' + state->par->size/10) {
            ui->pending = button;
            ui->hshow = 1;
            n = button - '0';
            retstr = make_move_string(state->par, ui, state->clues, n);
            return retstr;
          } else {
            n = button - '0';
          }
          retstr = make_move_string(state->par, ui, state->clues, n);
        }
	return retstr;
    }

    return NULL;
}

static bool check_errors(const game_state *state, long *errors)
{
    int num = state->par->size;
    int i, j1, j2;
    int ret = false;
    long b, mask = 0;

    if (errors)
        for (i=0; i<num; i++) errors[i] = 0;

    for (i=0; i<num; i++) {
      if (state->grid[i] == -1) {
        ret = true;
      } else {
        b = 1L << (state->grid[i] - 1);
        if (mask & b) {
          if (errors) {
            for (j1=0; j1<i && state->grid[j1] != state->grid[i]; j1++);
            errors[i] |= DF_ERR_NUMBER;
            errors[j1] |= DF_ERR_NUMBER;
          }
          ret = true;
        } else
          mask |= b;
        if (op_nary[state->clues->ops[i]] == 2) {
          for (j1=0; j1<num && state->clues->letters[j1] != state->clues->l1vec[i]; j1++);
          for (j2=0; j2<num && state->clues->letters[j2] != state->clues->l2vec[i]; j2++);
          if (state->grid[j1] != -1 && state->grid[j2] != -1 &&
              !verify_eq2(state->clues->ops[i], state->grid[i], state->grid[j1], state->grid[j2])) {
            if (errors)
              errors[i] |= DF_ERR_EQUATION;
            ret = true;
          }
        } else if (op_nary[state->clues->ops[i]] == 1) {
          for (j1=0; j1<num && state->clues->letters[j1] != state->clues->l1vec[i]; j1++);
          if (state->grid[j1] != -1 &&
              !verify_eq1(state->clues->ops[i], state->grid[i], state->grid[j1])) {
            if (errors)
              errors[i] |= DF_ERR_EQUATION;
            ret = true;
          }
        }
      }
    }

    return ret;
}

static game_state *execute_move(const game_state *from0, const char *move)
{
    game_state* from = (game_state*) from0;
    int num = from->par->size;
    game_state *ret;
    int i, n;
    char l;

    if (move[0] == 'o') { /* Null operation for delayed moves */
      ret = dup_game(from);
      return ret;
    } else if (move[0] == 's') {
        const char* p = move + 1;
	ret = dup_game(from);
	ret->completed = ret->cheated = true;

        while (*p) {
          if (sscanf(p, "%c%d", &l, &n) == 2 &&
              l >= 'A' && l < 'A'+num && n >= -1 && n <= num) {
            for (p++; *p >= '0' && *p <= '9'; p++);
            for (i=0; i<num && from->clues->letters[i] != l; i++);
            if (i==num) {
              free_game(ret);
              return from;
            }
            ret->grid[i] = n;
            ret->pencil[i] = 0;
          } else {
	    free_game(ret);
            return from;
          }
        }

	return ret;
    } else if (move[0] == 'p') {
      if (sscanf(move+1, "%c%d", &l, &n) == 2 &&
          l >= 'A' && l < 'A'+num && n >= -1 && n <= num) {
        for (i=0; i<num && from->clues->letters[i] != l; i++);
        if (i==num)
          return from;
	ret = dup_game(from);
        if (n == -1)
          ret->pencil[i] = 0;
        else if (n > 0)
          ret->pencil[i] ^= 1L << (n-1);
	return ret;
      } else
        return from;
    } else if (move[0] == 'r') {
      if (sscanf(move+1, "%c%d", &l, &n) == 2 &&
          l >= 'A' && l < 'A'+num && n >= -1 && n <= num) {
        for (i=0; i<num && from->clues->letters[i] != l; i++);
        if (i==num)
          return from;
	ret = dup_game(from);
        ret->grid[i] = n;
        ret->pencil[i] = 0;
	return ret;
      } else
        return from;
    } else 
      return from;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
			      const game_ui *ui, int *x, int *y)
{
  int cols = (params->size > 12 ? 2 : 1);
  int rows = (params->size > 12 ? (params->size+1)/2 : params->size);
  *x = TOTSIZEX(cols, tilesize);
  *y = TOTSIZEY(rows, tilesize);
}

static void game_set_size(drawing *dr, game_drawstate *ds,
			  const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);

    frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

    ret[COL_GRID * 3 + 0] = 0.0F;
    ret[COL_GRID * 3 + 1] = 0.0F;
    ret[COL_GRID * 3 + 2] = 0.0F;

    ret[COL_USER * 3 + 0] = 0.0F;
    ret[COL_USER * 3 + 1] = 0.6F * ret[COL_BACKGROUND * 3 + 1];
    ret[COL_USER * 3 + 2] = 0.0F;

    ret[COL_HIGHLIGHT * 3 + 0] = 0.78F * ret[COL_BACKGROUND * 3 + 0];
    ret[COL_HIGHLIGHT * 3 + 1] = 0.78F * ret[COL_BACKGROUND * 3 + 1];
    ret[COL_HIGHLIGHT * 3 + 2] = 0.78F * ret[COL_BACKGROUND * 3 + 2];

    ret[COL_ERROR * 3 + 0] = 1.0F;
    ret[COL_ERROR * 3 + 1] = 0.0F;
    ret[COL_ERROR * 3 + 2] = 0.0F;

    ret[COL_PENCIL * 3 + 0] = 0.5F * ret[COL_BACKGROUND * 3 + 0];
    ret[COL_PENCIL * 3 + 1] = 0.5F * ret[COL_BACKGROUND * 3 + 1];
    ret[COL_PENCIL * 3 + 2] = ret[COL_BACKGROUND * 3 + 2];

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);
    int i, a;
    int n = state->par->size;

    ds->tilesize = 0;
    ds->w = (n > 12 ? 2 : 1);
    ds->h = (n > 12 ? (n+1)/2 : n);
    a = ds->w*ds->h;
    ds->started = false;
    ds->status = snewn(a, char);
    ds->numbers = snewn(a, char);
    ds->pencils = snewn(a, long);
    ds->errors = snewn(a, long);
    for (i=0; i<a; i++)
      ds->status[i] = -1, ds->numbers[i] = 0, ds->pencils[i] = 0, ds->errors[i] = 0;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->status);
    sfree(ds->numbers);
    sfree(ds->pencils);
    sfree(ds->errors);
    sfree(ds);
}

static void draw_tile(drawing *dr, game_drawstate *ds, const game_params *par,
		      struct clues *clues, int x, int y, char status, char number, long pencil)
{
    int tx, ty, tw, th;
    int cx, cy, cw, ch;
    char str[64];

    cx = tx = COORDX(x, ds->tilesize);
    cy = ty = COORDY(y, ds->tilesize);
    cw = tw = INNERSIZEX(ds->tilesize);
    ch = th = INNERSIZEY(ds->tilesize);

    clip(dr, cx, cy, cw, ch);

    /* background needs erasing */
    draw_rect(dr, cx, cy, cw, ch, COL_BACKGROUND);

    if ((status & DF_HIGHLIGHT) && (status & DF_HIGHLIGHT_PENCIL)) { /* flash */
      draw_rect(dr, cx, cy, cw, ch, COL_HIGHLIGHT);
    } else if (status & DF_HIGHLIGHT_PENCIL) {   /* pencil-mode highlight */
        int coords[6];
        coords[0] = cx + SUBTILEOFFX(ds->tilesize);
        coords[1] = cy;
        coords[2] = coords[0] + SUBTILESIZEX(ds->tilesize)/2;
        coords[3] = cy;
        coords[4] = coords[0];
        coords[5] = cy+ch/2;
        draw_polygon(dr, coords, 3, COL_HIGHLIGHT, COL_HIGHLIGHT);
    } else if (status & DF_HIGHLIGHT) {
      draw_rect(dr, cx + SUBTILEOFFX(ds->tilesize), cy, 
                SUBTILESIZEX(ds->tilesize), ch, COL_HIGHLIGHT);
    }
    if (y + x*clues->rows <= clues->num) { /* Equation square */
      Operator op = clues->ops[y + x*clues->rows];
      sprintf(str, "%c", clues->letters[y + x*clues->rows]);
      draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)/2, ty + TILESIZEY(ds->tilesize)/2,
                FONT_VARIABLE, TILESIZEY(ds->tilesize)*2/5, ALIGN_VCENTRE | ALIGN_HCENTRE,
                COL_GRID, str);
      sprintf(str, ":");
      draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)-2, ty + TILESIZEY(ds->tilesize)/2,
                FONT_VARIABLE, TILESIZEY(ds->tilesize)*2/5, ALIGN_VCENTRE | ALIGN_HRIGHT,
                COL_GRID, str);
      if (op_nary[op] == 2) {
        int pyth = (op == PythPlus || op == PythMinus);
        sprintf(str, "=");
        draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*27/12, ty + TILESIZEY(ds->tilesize)/2,
                  FONT_VARIABLE, TILESIZEY(ds->tilesize)*2/5, ALIGN_VCENTRE | ALIGN_HCENTRE,
                  (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID), str);
        sprintf(str, "%c", clues->l1vec[y + x*clues->rows]);
        draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*33/12, ty + TILESIZEY(ds->tilesize)/2,
                  FONT_VARIABLE, TILESIZEY(ds->tilesize)*2/5, ALIGN_VCENTRE | ALIGN_HCENTRE,
                  (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID), str);
        sprintf(str, "%c", (op == Plus || op == PythPlus ? '+' : op == Minus || op == PythMinus ? '-' : op == Times ? '*' : op == Divide ? '/' : op == Modulo ? '%' : ' '));
        draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*39/12, ty + TILESIZEY(ds->tilesize)/2,
                  FONT_VARIABLE, TILESIZEY(ds->tilesize)*2/5, ALIGN_VCENTRE | ALIGN_HCENTRE,
                  (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID), str);
        sprintf(str, "%c", clues->l2vec[y + x*clues->rows]);
        draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*(pyth ? 43 : 45)/12, ty + TILESIZEY(ds->tilesize)/2,
                  FONT_VARIABLE, TILESIZEY(ds->tilesize)*2/5, ALIGN_VCENTRE | ALIGN_HCENTRE,
                  (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID), str);
        if (pyth) {
          sprintf(str, "2");
          draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*35/12, ty + TILESIZEY(ds->tilesize)/3,
                    FONT_VARIABLE, TILESIZEY(ds->tilesize)*3/10, ALIGN_VCENTRE | ALIGN_HLEFT,
                    (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID), str);
          draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*45/12, ty + TILESIZEY(ds->tilesize)/3,
                    FONT_VARIABLE, TILESIZEY(ds->tilesize)*3/10, ALIGN_VCENTRE | ALIGN_HLEFT,
                    (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID), str);
          draw_line(dr, tx + SUBTILESIZEX(ds->tilesize)*29/12, ty + TILESIZEY(ds->tilesize)*7/12, 
                    tx + SUBTILESIZEX(ds->tilesize)*30/12, ty + TILESIZEY(ds->tilesize)*9/12,
                    (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID));
          draw_line(dr, tx + SUBTILESIZEX(ds->tilesize)*30/12, ty + TILESIZEY(ds->tilesize)*9/12,
                    tx + SUBTILESIZEX(ds->tilesize)*31/12, ty + TILESIZEY(ds->tilesize)/5, 
                    (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID));
          draw_line(dr, tx + SUBTILESIZEX(ds->tilesize)*31/12, ty + TILESIZEY(ds->tilesize)/5, 
                    tx + SUBTILESIZEX(ds->tilesize)*48/12, ty + TILESIZEY(ds->tilesize)/5, 
                    (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID));
        }
      } else if (op_nary[op] == 1) {
        sprintf(str, (op == Less ? "<" : op == Greater ? ">" : "="));
        draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*27/12, ty + TILESIZEY(ds->tilesize)/2,
                  FONT_VARIABLE, TILESIZEY(ds->tilesize)*2/5, ALIGN_VCENTRE | ALIGN_HCENTRE,
                  (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID), str);
        sprintf(str, "%c", clues->l1vec[y + x*clues->rows]);
        draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*33/12, ty + TILESIZEY(ds->tilesize)/2,
                  FONT_VARIABLE, TILESIZEY(ds->tilesize)*2/5, ALIGN_VCENTRE | ALIGN_HCENTRE,
                  (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID), str);
        if (op == Square) {
          sprintf(str, "2");
          draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*35/12, ty + TILESIZEY(ds->tilesize)/3,
                    FONT_VARIABLE, TILESIZEY(ds->tilesize)*3/10, ALIGN_VCENTRE | ALIGN_HLEFT,
                    (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID), str);
        } else if (op == Sqroot) {
          draw_line(dr, tx + SUBTILESIZEX(ds->tilesize)*29/12, ty + TILESIZEY(ds->tilesize)*7/12, 
                    tx + SUBTILESIZEX(ds->tilesize)*30/12, ty + TILESIZEY(ds->tilesize)*9/12,
                    (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID));
          draw_line(dr, tx + SUBTILESIZEX(ds->tilesize)*30/12, ty + TILESIZEY(ds->tilesize)*9/12,
                    tx + SUBTILESIZEX(ds->tilesize)*31/12, ty + TILESIZEY(ds->tilesize)/5, 
                    (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID));
          draw_line(dr, tx + SUBTILESIZEX(ds->tilesize)*31/12, ty + TILESIZEY(ds->tilesize)/5, 
                    tx + SUBTILESIZEX(ds->tilesize)*36/12, ty + TILESIZEY(ds->tilesize)/5, 
                    (status & DF_ERR_EQUATION ? COL_ERROR : COL_GRID));
        }
      }
      if (status & DF_HAS_DIGIT) {
        sprintf(str, "%d", number);
        draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*3/2, ty + TILESIZEY(ds->tilesize)/2,
                  FONT_VARIABLE, TILESIZEY(ds->tilesize)/2, ALIGN_VCENTRE | ALIGN_HCENTRE,
                  (status & DF_ERR_NUMBER ? COL_ERROR : op == Constant ? COL_GRID : COL_USER), str);
      } else if (number && !(status & DF_HIGHLIGHT_PENCIL)) {
        sprintf(str, "%d_", number);
        draw_text(dr, tx + SUBTILESIZEX(ds->tilesize)*3/2, ty + TILESIZEY(ds->tilesize)/2,
                  FONT_VARIABLE, TILESIZEY(ds->tilesize)/2, ALIGN_VCENTRE | ALIGN_HCENTRE,
                  COL_USER, str);
      } else {
            long rev;
            int i, j, npencil;
            int pl, pr, pt, pb;
            float vhprop;
            int ok, pw, ph, minph, minpw, pgsizex, pgsizey, fontsize;

            /* Count the pencil marks required. */
            if (number) {
              npencil = 1, rev = 1L << (number-1);
            } else
              npencil = 0, rev = 0;
            for (i = 1; i <= par->size; i++)
              if ((pencil ^ rev) & (1L << (i-1)))
                npencil++;
            if (npencil) {
                minph = 2;
                minpw = 2;
                vhprop = 1.5;
                /*
                 * Determine the bounding rectangle within which we're going
                 * to put the pencil marks.
                 */
                pl = tx + SUBTILESIZEX(ds->tilesize);
                pr = pl + SUBTILESIZEX(ds->tilesize);
                pt = ty + GRIDEXTRA(ds->tilesize);
                pb = pt + TILESIZEY(ds->tilesize) - GRIDEXTRA(ds->tilesize); 

                /*
                 * We arrange our pencil marks in a grid layout, with
                 * the number of rows and columns adjusted to allow the
                 * maximum font size.
                 *
                 * So now we work out what the grid size ought to be.
                 */
                pw = minpw, ph = minph;
                ok = 0;
                for (fontsize=(pb-pt)/minph; fontsize>1 && !ok; fontsize--) {
                  pw = (pr - pl)/(int)(fontsize*vhprop+0.5);
                  ph = (pb - pt)/fontsize;
                  ok = (pw >= minpw && ph >= minph && npencil <= pw*ph && pw*vhprop>=ph);
                }
                pgsizey = fontsize;
                pgsizex = (int)(fontsize*vhprop+0.5);

                /*
                 * Centre the resulting figure in the square.
                 */
                pl = tx + SUBTILESIZEX(ds->tilesize) * 3/2 - pgsizex * pw/2;
                pt = ty + (TILESIZEY(ds->tilesize) - pgsizey * ph - 2) / 2;

                /*
                 * Now actually draw the pencil marks.
                 */
                for (i = 1, j = 0; i <= par->size; i++)
                  if ((pencil ^ rev) & (1L << (i-1))) {
                    int dx = j % pw, dy = j / pw;
                    sprintf(str, "%d", i);
                    draw_text(dr, pl + pgsizex * (2*dx+1) / 2,
                              pt + pgsizey * (2*dy+1) / 2,
                              FONT_VARIABLE, fontsize,
                              ALIGN_VCENTRE | ALIGN_HCENTRE, COL_PENCIL,
                              str);
                    j++;
                    }
                if (number) {
                  int dx = j % pw, dy = j / pw;
                  sprintf(str, "%d_", (int)(number));                  
                  draw_text(dr, pl + pgsizex * (2*dx+1) / 2,
                            pt + pgsizey * (2*dy+1) / 2,
                            FONT_VARIABLE, fontsize,
                            ALIGN_VCENTRE | ALIGN_HCENTRE, COL_PENCIL,
                            str);
                }
            }
      }
    }

    unclip(dr);

    draw_update(dr, cx, cy, cw, ch);
}

static void game_redraw(drawing *dr, game_drawstate *ds, const game_state *oldstate,
			const game_state *state, int dir, const game_ui *ui,
			float animtime, float flashtime)
{
    int n = state->par->size;
    int rows = state->clues->rows;
    int cols = state->clues->cols;
    int x, y, i;

    if (!ds->started) {
	/*
	 * The initial contents of the window are not guaranteed and
	 * can vary with front ends. To be on the safe side, all
	 * games should start by drawing a big background-colour
	 * rectangle covering the whole window.
	 */
      draw_rect(dr, 0, 0, TOTSIZEX(cols, ds->tilesize), TOTSIZEY(rows, ds->tilesize), COL_BACKGROUND);

	/*
	 * Big containing rectangle.
	 */
      draw_rect(dr, BORDER(ds->tilesize) - GRIDEXTRA(ds->tilesize),
                BORDER(ds->tilesize) - GRIDEXTRA(ds->tilesize),
                COORDX(cols, ds->tilesize) - BORDER(ds->tilesize) + GRIDEXTRA(ds->tilesize),
                COORDY(rows, ds->tilesize) - BORDER(ds->tilesize) + GRIDEXTRA(ds->tilesize),
                COL_GRID);

      draw_update(dr, 0, 0, TOTSIZEX(cols, ds->tilesize), TOTSIZEY(rows, ds->tilesize));
      
      ds->started = true;
    }

    if (animtime)
      return;
    if (ui->pending && !oldstate) {      
      finish_pending(ui);
    }
    
    if (!ui->pending)
      check_errors(state, ds->errors);

    for (x=0, i=0 ; x<cols; x++) {
      for (y=0; y<rows; y++, i++) {
        char status = 0;
        char number;
        long pencil;
        
        if (i<n) {
          pencil = state->pencil[i];
          if (pencil)
            status |= DF_HAS_PENCIL;
          if (ui->pending && ui->hx == x && ui->hy == y)
            number = ui->pending - '0', status |= DF_PENDING_INPUT;
          else if (state->grid[i] != -1)
            number = state->grid[i], status |= DF_HAS_DIGIT;
          else
            number = 0;
          if (ui->hshow && ui->hx == x && ui->hy == y) {
            status |= (ui->hpencil ? DF_HIGHLIGHT_PENCIL : DF_HIGHLIGHT);
          }
          status |= (char) (0xFF & ds->errors[i]);
        } else {
          number = 0, status = 0, pencil = 0;
        }

        if (flashtime > 0 &&
            (flashtime <= FLASH_TIME/3 ||
             flashtime >= FLASH_TIME*2/3))
          status |= DF_HIGHLIGHT | DF_HIGHLIGHT_PENCIL;  /* completion flash */

        if (ds->status[i] != status || ds->numbers[i] != number || ds->pencils[i] != pencil) {
          ds->status[i] = status;
          ds->numbers[i] = number;
          ds->pencils[i] = pencil;
          draw_tile(dr, ds, state->par, state->clues, x, y, status, number, pencil);
        }
      }
    }
}

static float game_anim_length(const game_state *oldstate, const game_state *newstate,
			      int dir, game_ui *ui)
{
  if (ui->pending)
    return 1.0;
  else
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate, const game_state *newstate,
			       int dir, game_ui *ui)
{
    if (!oldstate->completed && !oldstate->cheated &&
        !newstate->cheated && !newstate->completed &&
        !check_errors(newstate, NULL)) {
      ((game_state*)newstate)->completed = true;
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

#ifdef COMBINED
#define thegame alphacrypt
#endif

const struct game thegame = {
    "Alphacrypt", NULL, NULL,
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
    NULL, /* set_public_desc */
    dup_game,
    free_game,
    true, solve_game,
    false, game_can_format_as_text_now, game_text_format,
    NULL, NULL, /* get_prefs, set_prefs */
    new_ui,
    free_ui,
    NULL, /* encode_ui */
    NULL, /* decode_ui */
    NULL,
    game_changed_state,
    NULL,
    interpret_move,
    execute_move,
    PREFERRED_TILESIZE, 
    game_compute_size,
    game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    NULL,
    game_status,
    false, false, NULL, NULL, /* print_size, print */
    false,			       /* wants_statusbar */
    false, game_timing_state,
    0,				       /* flags */
};
