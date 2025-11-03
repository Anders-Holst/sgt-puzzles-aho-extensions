/*
 * Implementation of a 'factor crossword' puzzle, basically similar to
 * Kakuro except that clues are multiplicative rather than additive
 * and there's no rule against repeating digits.
 *
 * Copyright (C) 2013 Anders Holst (aho@sics.se) and Simon Tatham.
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

#define MULTIDIGIT  /* Define this to allow higher input numbers than 9. And make midend_undo public. */

#define MAXNUM 20
#define NPRIME 9
int primes[] = {0, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};
float logprimes[] = {-1, 0.69314718, 1.98612289, 1.60943791, 1.94591015, 2.39789527, 2.56494936, 2.83321334, 2.94443898, 3.13549422, 3.36729583, 3.43398720, 3.61091791, 3.71357207, 3.76120012, 3.85014760};

#define BAD_GEN_LIMIT(size) (size >= 12 ? 2000 : 1000)
#define ITER_LIMIT(size) (size >= 12 ? 8000 : 5000)

typedef signed char digit;

struct game_params {
    int size;
    int max;
    int smallnum;
    int zero_mode;
    int notone_mode;
    int pmax;
};

struct clues {
    int refcount;
    int w, h;
    unsigned char *playable;
    long *hclues, *vclues;
    midend *me;
};

struct game_state {
    const game_params *par;
    struct clues *clues;
    digit *grid;
    long *pencil;		       /* bitmaps using bits 1<<1..1<<n */
    int completed, cheated;
};

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    ret->size = 7;
    ret->max = 9;
    ret->smallnum = 0;
    ret->zero_mode = 0;
    ret->notone_mode = 0;
    for (ret->pmax=NPRIME-1; ret->pmax>0 && primes[ret->pmax] > ret->max; ret->pmax--);

    return ret;
}

const static struct game_params factor_presets[] = {
  {5, 9, 1, 0, 0, 0},
  {7, 9, 1, 0, 0, 0},
  {5, 9, 0, 0, 0, 0},
  {7, 9, 0, 0, 0, 0},
  {9, 9, 0, 0, 0, 0},
  {12, 9, 0, 0, 0, 0},
  {7, 9, 0, 1, 0, 0},
  {9, 9, 0, 1, 0, 0},
  {7, 9, 0, 0, 1, 0},
  {9, 9, 0, 0, 1, 0},
  {7, 9, 0, 1, 1, 0},
  {9, 9, 0, 1, 1, 0},
#ifdef MULTIDIGIT
  {5, 12, 0, 0, 0, 0},
  {7, 12, 0, 0, 0, 0},
  {9, 12, 0, 0, 0, 0},
  {12, 12, 0, 0, 0, 0},
  {5, 20, 0, 0, 0, 0},
  {7, 20, 0, 0, 0, 0},
  {9, 20, 0, 0, 0, 0},
  {12, 20, 0, 0, 0, 0}
#endif /* MULTIDIGIT */
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(factor_presets))
        return false;

    ret = snew(game_params);
    *ret = factor_presets[i]; /* structure copy */
    for (ret->pmax=NPRIME-1; ret->pmax>0 && primes[ret->pmax] > ret->max; ret->pmax--);

    sprintf(buf, "%dx%d", ret->size, ret->size);
    if (ret->zero_mode) {
      if (ret->notone_mode)
        sprintf(buf+strlen(buf), ", zeroes but no ones");
      else
        sprintf(buf+strlen(buf), ", with zeroes");
    } else if (ret->notone_mode)
      sprintf(buf+strlen(buf), ", no ones");
    if (ret->max != 9) {
      sprintf(buf+strlen(buf), ", up to %d", ret->max);
    }
    if (ret->smallnum) {
      sprintf(buf+strlen(buf), ", only small clues");
    }
    *name = dupstr(buf);
    *params = ret;
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

    params->zero_mode = params->notone_mode = params->smallnum = 0;
    params->max = 9;
    params->pmax = 4;
    if (*p == ',') {
      p++;
      if (*p == '0') {
        params->zero_mode = 1;
        p++;
      } 
      if (*p == '2')
        params->notone_mode = 1;
      while (*p && isdigit((unsigned char)*p)) p++;
      if (*p == '-') {
        p++;
        params->max = atoi(p);
        for (params->pmax=NPRIME-1; params->pmax>0 && primes[params->pmax] > params->max; params->pmax--);
        while (*p && isdigit((unsigned char)*p)) p++;
      }
      if (*p == 's' || *p == 'S') {
        p++;
        params->smallnum = 1;
      }
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char ret[80];

    sprintf(ret, "%d,%s-%d%s", params->size,
            (params->zero_mode ?
             (params->notone_mode ? "02" : "0") :
             (params->notone_mode ? "2" : "1")),
            params->max,
            (params->smallnum ? "s" : ""));

    return dupstr(ret);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[80];
    int ind = 0;

    ret = snewn(6, config_item);

    ret[ind].name = "Grid size";
    ret[ind].type = C_STRING;
    sprintf(buf, "%d", params->size);
    ret[ind].u.string.sval = dupstr(buf);

#ifdef MULTIDIGIT
    ind++;
    ret[ind].name = "Maximum value";
    ret[ind].type = C_STRING;
    sprintf(buf, "%d", params->max);
    ret[ind].u.string.sval = dupstr(buf);
#endif /* MULTIDIGIT */

    ind++;
    ret[ind].name = "Zeroes allowed";
    ret[ind].type = C_BOOLEAN;
    ret[ind].u.boolean.bval = (params->zero_mode ? true : false);

    ind++;
    ret[ind].name = "No ones allowed";
    ret[ind].type = C_BOOLEAN;
    ret[ind].u.boolean.bval = (params->notone_mode ? true : false);

    ind++;
    ret[ind].name = "Limited clue size";
    ret[ind].type = C_BOOLEAN;
    ret[ind].u.boolean.bval = (params->smallnum ? true : false);

    ind++;
    ret[ind].name = NULL;
    ret[ind].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);
    int ind = 0;

    ret->size = atoi(cfg[ind++].u.string.sval);
#ifdef MULTIDIGIT
    ret->max = atoi(cfg[ind++].u.string.sval);
#else
    ret->max = 9;
#endif /* MULTIDIGIT */
    ret->zero_mode = (cfg[ind++].u.boolean.bval);
    ret->notone_mode = (cfg[ind++].u.boolean.bval);
    ret->smallnum = (cfg[ind++].u.boolean.bval);
    for (ret->pmax=NPRIME-1; ret->pmax>0 && primes[ret->pmax] > ret->max; ret->pmax--);

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->size < 2 || params->size > 15)
        return "Grid size must be between 2 and 15";
#ifdef MULTIDIGIT
    if (params->max < 5 || params->max > MAXNUM)
      return "Maximum slot value must be between 5 and " STR(MAXNUM);
#else
    if (params->max != 9)
      return "In this version the maximum slot value must always be 9";
#endif /* MULTIDIGIT */
    return NULL;
}

/* ----------------------------------------------------------------------
 * Game generation code.
 */
typedef struct Slot {
    char n[NPRIME];
    struct Run *run[2];
    int x, y;
} Slot;

typedef struct Run {
    char n[NPRIME];
    char r[NPRIME];
    Slot **slots;
    int nslots;
    int dir; /* 0 vertical, 1 horizontal */
    int srem;
    int done;
} Run;

typedef struct FactorBoard {
    const game_params *par;
    Slot **slots;
    Run **runs;
    int nslots, nruns;
    char *candidate;
    long iter;
    long itermax;
    long quickret;
    long onesol;
    int estimate;
    float estlimit;
} FactorBoard;

static void clean(FactorBoard *fb);

static void factorize(long num, char *n, int pmax)
{
    int i;
    if (num == 0) {
      n[0] = 1;
      for (i=pmax; i>0; i--)
        n[i]=0;
    } else {
      n[0] = 0;
      for (i=pmax; i>0; i--)
        for (n[i]=0; num%primes[i]==0; n[i]++, num/=primes[i]);
    }
}

static void factorize_reset(char *n, int pmax)
{
    int i;
    for (i=pmax; i>=0; i--)
      n[i]=0;
}

static void factorize_incr(char num, char *n, int pmax)
{
    int i;
    if (num == 0) {
      n[0] = 1;
      for (i=pmax; i>0; i--)
        n[i]=0;
    } else if (!n[0]) {
      i = pmax;
      while (num < primes[i]) i--; 
      while (i>0) {
        while (num%primes[i]==0) n[i]++, num/=primes[i];
        do { i--; } while (num < primes[i]); 
      }
    }
}

static long product(char *n, int pmax)
{
    int nn, i;
    long p;
    if (n[0])
      p = 0;
    else {
      p = 1;
      for (i=pmax; i>0; i--)
        for (nn=n[i]; nn; nn--, p*=primes[i]);
    }
    return p;
}

static Slot *new_Slot(int xx, int yy)
{
    int i;
    Slot *sl = snew(Slot);

    sl->x = xx;
    sl->y = yy;
    for (i=0; i<NPRIME; i++)
        sl->n[i] = 0;
    for (i=0; i<2; i++)
        sl->run[i] = 0;

    return sl;
}

static Run *new_Run(int ns, int d, char *nn)
{
    int i;
    Run *r = snew(Run);

    r->srem = r->nslots = ns;
    r->dir = d;
    for (i=0; i<NPRIME; i++)
        r->r[i] = r->n[i] = nn[i];
    r->done = 0;
    r->slots = snewn(ns, Slot *);
    for (i=0; i<ns; i++)
        r->slots[i] = 0;

    return r;
}

static void delete_Run(Run *r)
{
    sfree(r->slots);
    sfree(r);
}

static void mi_setup(Run *r, const game_params* par, int *numind, int **ii, int **bb, char** cache)
{
    Run* r0;
    int i, j, k;
    *numind = 0;
    for (i=0; i<=par->pmax; i++) *numind += r->r[i];
    *ii = snewn(*numind, int);
    *bb = snewn(*numind, int);
    *cache = snewn((par->pmax+1)*r->nslots, char);
    for (i=par->pmax, k=0; i>=0; i--)
        for (j=0; j<r->r[i]; j++, k++)
            (*bb)[k] = i;
    for (i=0; i<r->nslots; i++) {
      (*cache)[i] = (char)product(r->slots[i]->n, par->pmax);
      r0 = r->slots[i]->run[1-r->dir];
      for (j=1; j<=par->pmax; j++)
        (*cache)[j*r->nslots+i] = (!r0 || r0->n[0] ? -1 : r0->done ? 0 : r0->r[j]);
    }
}

static int mi_first(Run *r, const game_params* par, int numind, int *ii, int *bb, char* cache, int k)
{
    int i, bt = 0;
    for (i=k; i<numind; i++) {
        if (bt)
            bt = 0;
        else if (i==0 || bb[i] != bb[i-1])
            ii[i] = 0;
        else
            ii[i] = ii[i-1];
        while (ii[i] < r->nslots && !(cache[bb[i]*r->nslots+ii[i]] && cache[ii[i]]*primes[bb[i]]<=par->max))
            ii[i]++;
        if (ii[i] < r->nslots) {
          r->slots[ii[i]]->n[bb[i]]++;
          if (r->slots[ii[i]]->run[1-r->dir])
            r->slots[ii[i]]->run[1-r->dir]->r[bb[i]]--;
          cache[ii[i]] *= primes[bb[i]];
          cache[bb[i]*r->nslots+ii[i]]--;
        } else {
            /* Backtrack if possible */
            if (i == k)
                return 0;
            else {
                r->slots[ii[i-1]]->n[bb[i-1]]--;
                if (r->slots[ii[i-1]]->run[1-r->dir])
                  r->slots[ii[i-1]]->run[1-r->dir]->r[bb[i-1]]++;
                cache[bb[i-1]*r->nslots+ii[i-1]]++;
                cache[ii[i-1]] /= primes[bb[i-1]];
                ii[i-1]++;
                i-=2;
                bt = 1;
            }
        }
    }
    return 1;
}

static int mi_next(Run *r, const game_params* par, int numind, int *ii, int *bb, char* cache)
{
    int i;
    for (i=numind-1; i>=0; i--) {
        r->slots[ii[i]]->n[bb[i]]--;
        if (r->slots[ii[i]]->run[1-r->dir])
          r->slots[ii[i]]->run[1-r->dir]->r[bb[i]]++;
        cache[bb[i]*r->nslots+ii[i]]++;
        cache[ii[i]] /= primes[bb[i]];
        ii[i]++;
        while (ii[i] < r->nslots) {
            if (cache[bb[i]*r->nslots+ii[i]] && cache[ii[i]]*primes[bb[i]]<=par->max) {
              cache[ii[i]] *= primes[bb[i]];
              cache[bb[i]*r->nslots+ii[i]]--;
              if (mi_first(r, par, numind, ii, bb, cache, i+1)) {
                r->slots[ii[i]]->n[bb[i]]++;
                if (r->slots[ii[i]]->run[1-r->dir])
                  r->slots[ii[i]]->run[1-r->dir]->r[bb[i]]--;
                return 1;
              } else {
                cache[bb[i]*r->nslots+ii[i]]++;
                cache[ii[i]] /= primes[bb[i]];
              }
            }
            ii[i]++;
        }
    }
    return 0;
}

static void mi_abort(Run *r, int numind, int *ii, int *bb)
{
    int i;
    for (i=numind-1; i>=0; i--) {
      r->slots[ii[i]]->n[bb[i]]--;
      if (r->slots[ii[i]]->run[1-r->dir])
        r->slots[ii[i]]->run[1-r->dir]->r[bb[i]]++;
    }
}

static void fix(Run *r)
{
    Run *rr;
    int i;
    r->done = 1;
    for (i=0; i<r->nslots; i++) {
        rr = r->slots[i]->run[1-r->dir];
        if (rr && !rr->done)
            rr->srem--;
    }
}

static void unfix(Run *r)
{
    Run *rr;
    int i;
    for (i=0; i<r->nslots; i++) {
        rr = r->slots[i]->run[1-r->dir];
        if (rr && !rr->done)
            rr->srem++;
    }
    r->done = 0;
}

static FactorBoard *new_FactorBoard(const game_params* p)
{
    FactorBoard *fb = snew(FactorBoard);
    fb->par = p;
    fb->slots = 0;
    fb->runs = 0;
    fb->nslots = 0;
    fb->nruns = 0;
    fb->candidate = 0;
    fb->iter = 0;
    fb->quickret = 0;
    fb->estimate = 0;
    fb->onesol = 0;
    return fb;
}

static void delete_FactorBoard(FactorBoard *fb)
{
    clean(fb);
    if (fb->candidate)
        sfree(fb->candidate);
    sfree(fb);
}

static void clean(FactorBoard *fb)
{
    int i;
    for (i=0; i<fb->nruns; i++)
        delete_Run(fb->runs[i]);
    for (i=0; i<fb->nslots; i++)
        sfree(fb->slots[i]);
    sfree(fb->runs);
    sfree(fb->slots);
    fb->nslots = 0;
    fb->nruns = 0;
    fb->runs = 0;
    fb->slots = 0;
}

static void import_answer(FactorBoard *fb, const char *str)
{
    int n = fb->par->size*fb->par->size;
    Slot **sgrid = snewn(n, Slot *);
    Run *r;
    char vv[NPRIME];
    int i, j, k;
    int cnt=0, cnt2;
    for (i=0; i<n; i++) {
        if (str[i] == '#') {
            sgrid[i] = 0;
        } else {
            sgrid[i] = new_Slot(i%fb->par->size, i/fb->par->size);
            cnt++;
        }
    }
    fb->nslots = cnt;
    fb->slots = snewn(fb->nslots, Slot *);
    cnt = 0;
    for (i=0; i<n; i++)
        if (sgrid[i])
            fb->slots[cnt++] = sgrid[i];
    fb->runs = snewn(2*(n-fb->nslots+fb->par->size), Run *);
    cnt = 0;
    for (j=0; j<fb->par->size; j++) {
        cnt2 = 0;
        factorize_reset(vv, fb->par->pmax);
        for (i=0; i<=fb->par->size; i++) {
            if (i==fb->par->size || str[i+fb->par->size*j] == '#') {
                if (cnt2>1) {
                    fb->runs[cnt++] = r = new_Run(cnt2, 1, vv);
                    for (k=0; k<cnt2; k++) {
                        r->slots[k] = sgrid[i+fb->par->size*j-cnt2+k];
                        sgrid[i+fb->par->size*j-cnt2+k]->run[1] = r;
                    }
                }
                cnt2 = 0;
                factorize_reset(vv, fb->par->pmax);
            } else {
                factorize_incr(str[i+fb->par->size*j] - '0', vv, fb->par->pmax);
                cnt2++;
            }
        }
    }
    for (i=0; i<fb->par->size; i++) {
        cnt2 = 0;
        factorize_reset(vv, fb->par->pmax);
        for (j=0; j<=fb->par->size; j++) {
            if (j==fb->par->size || str[i+fb->par->size*j] == '#') {
                if (cnt2>1) {
                    fb->runs[cnt++] = r = new_Run(cnt2, 0, vv);
                    for (k=0; k<cnt2; k++) {
                        r->slots[k] = sgrid[i+fb->par->size*(j-cnt2+k)];
                        sgrid[i+fb->par->size*(j-cnt2+k)]->run[0] = r;
                    }
                }
                cnt2 = 0;
                factorize_reset(vv, fb->par->pmax);
            } else {
                factorize_incr(str[i+fb->par->size*j] - '0', vv, fb->par->pmax);
                cnt2++;
            }
        }
    }
    fb->nruns = cnt;
    if (fb->candidate)
        sfree(fb->candidate);
    fb->candidate = snewn(n+(fb->par->size*2)+3, char);
    fb->candidate[n] = 0;
    sfree(sgrid);
}

static void export_answer(FactorBoard *fb, char *str)
{
  int i;
    Slot *s;
    str[0] = 'S';
    for (i=1; i<=(fb->par->size+1)*(fb->par->size+1); i++)
        str[i] = '\\';
    str[i] = 0;
    for (i=0; i<fb->nslots; i++) {
        s = fb->slots[i];
        str[(s->x+1)+(s->y+1)*(fb->par->size+1)+1] = '0' + product(s->n, fb->par->pmax);
    }
}

typedef struct { long v, h; } pair;

static pair *get_clues(FactorBoard *fb)
{
    int i, j;
    pair *clues = snewn((fb->par->size+1)*(fb->par->size+1), pair);
    for (i=0; i<(fb->par->size+1)*(fb->par->size+1); i++)
        clues[i].v = -1, clues[i].h = -1;
    for (i=0; i<fb->nslots; i++)
        if (fb->slots[i])
            j=fb->slots[i]->x+1+(fb->slots[i]->y+1)*(fb->par->size+1), clues[j].v = -2, clues[j].h = -2;
    for (i=0; i<fb->nruns; i++)
        if (fb->runs[i]) {
            if (fb->runs[i]->dir) 
                j = fb->runs[i]->slots[0]->x + (fb->runs[i]->slots[0]->y+1)*(fb->par->size+1), clues[j].h = product(fb->runs[i]->n, fb->par->pmax);
            else
                j = fb->runs[i]->slots[0]->x+1 + (fb->runs[i]->slots[0]->y)*(fb->par->size+1), clues[j].v = product(fb->runs[i]->n, fb->par->pmax);
        }
    return clues;
}

static void set_clues(FactorBoard *fb, struct clues* cl)
{
    int n = fb->par->size*fb->par->size;
    int sz = fb->par->size+1;
    Slot **sgrid = snewn(n, Slot *);
    Run *r;
    char vv[NPRIME];
    int i, j, k;
    int cnt=0, cnt2;
    for (j=sz, i=0; i<n; i++, j++) {
        if (j%sz == 0) j++;
        if (!cl->playable[j]) {
            sgrid[i] = 0;
        } else {
            sgrid[i] = new_Slot(i%fb->par->size, i/fb->par->size);
            cnt++;
        }
    }
    fb->nslots = cnt;
    fb->slots = snewn(fb->nslots, Slot *);
    cnt = 0;
    for (i=0; i<n; i++)
        if (sgrid[i])
            fb->slots[cnt++] = sgrid[i];
    fb->runs = snewn(2*(n-fb->nslots+fb->par->size), Run *);
    cnt = 0;
    for (j=0; j<sz; j++) {
      for (i=0; i<sz; i++) {
        if (cl->hclues[j*sz+i] != -1 && j>0) {
          factorize(cl->hclues[j*sz+i], vv, fb->par->pmax);
          for (k=i+1, cnt2=0; k<sz && cl->playable[j*sz+k]; k++, cnt2++);
          fb->runs[cnt++] = r = new_Run(cnt2, 1, vv);
          for (k=0; k<cnt2; k++) {
            r->slots[k] = sgrid[(j-1)*(sz-1) + (i+k)];
            r->slots[k]->run[1] = r;
          }
        }
        if (cl->vclues[j*sz+i] != -1 && i>0) {
          factorize(cl->vclues[j*sz+i], vv, fb->par->pmax);
          for (k=j+1, cnt2=0; k<sz && cl->playable[k*sz+i]; k++, cnt2++);
          fb->runs[cnt++] = r = new_Run(cnt2, 0, vv);
          for (k=0; k<cnt2; k++) {
            r->slots[k] = sgrid[(j+k)*(sz-1) + (i-1)];
            r->slots[k]->run[0] = r;
          }
        }
      }
    }          
    fb->nruns = cnt;
    if (fb->candidate)
        sfree(fb->candidate);
    fb->candidate = snewn(n+(fb->par->size*2)+3, char);
    fb->candidate[n] = 0;
    sfree(sgrid);
}

static char *randomize_answer(random_state *rs, const game_params* par)
{
    int i, j, ncl = 0;
    int n = par->size*par->size;
    char *str = snewn(n+1, char);
    str[n] = 0;
    for (i=0; i<n; i++) {
      if (random_upto(rs, 20) < 3) {
        ncl++;
        str[i] = '#';
      } else if (par->notone_mode)
        str[i] = '2' + random_upto(rs, par->max-1);
      else
        str[i] = '1' + random_upto(rs, par->max);
    }
    if (ncl < par->size*2-9) {
      for (j=ncl; j<par->size*2-9; j++) {
        i = random_upto(rs, n);
        str[i] = '#';
      }
    }
    if (par->zero_mode) {
      ncl = 1 + random_upto(rs, (par->size < 3 ? 1 : par->size/3));
      for (j=0; j<ncl; j++) {
        i = random_upto(rs, n);
        str[i] = '0';
      }
    }
    return str;
}

static char *mutate_answer(random_state *rs, const game_params* par, char *str, int m)
{
    int i, j, ncl = 0;
    int n = par->size*par->size;
    for (j=0; j<m; j++) {
      i = random_upto(rs, n);
      if (par->zero_mode && str[i]=='0') {
        j--;
        continue;
      }
      if (random_upto(rs, 20) < 3)
        str[i] = '#';
      else if (par->notone_mode)
        str[i] = '2' + random_upto(rs, par->max-1);
      else
        str[i] = '1' + random_upto(rs, par->max);
    }
    for (i=0; i<n; i++)
      if (str[i] == '#')
        ncl++;
    if (ncl < par->size*2-9) {
      for (j=ncl; j<par->size*2-9; j++) {
        i = random_upto(rs, n);
        if (par->zero_mode && str[i]=='0') {
          j--;
          continue;
        }
        str[i] = '#';
      }
    }
    return str;
}

int too_big(const game_params* par, Run* run)
{
  int j;
  float bn = 0.0;
  for (j=1; j<=par->pmax; j++) {
    bn += run->n[j]*logprimes[j];
  }
  if (par->smallnum)
    return (bn > (par->size <= 5 ? 5.2984 : (par->size/2)*2.302585));
  /* log(200), size/2*log(10) */
  else
    return (bn > 20.72326583);  /* log(1000000000) */
}

static void check_connected_mark(Run* r)
{
  int i;
  r->done = 2;
  for (i=0; i<r->nslots; i++) {
    if (r->slots[i]->run[1-r->dir] && !r->slots[i]->run[1-r->dir]->done)
      check_connected_mark(r->slots[i]->run[1-r->dir]);
  }
}

static int check_correct_form(FactorBoard* fb)
{
  int i, ok;
  if (fb->nslots == 0 || fb->nruns <= 1)
    return 0;
  for (i=0; i<fb->nslots; i++)
    if (fb->slots[i]->run[0] == 0 && fb->slots[i]->run[1] == 0)
      return 0;
  check_connected_mark(fb->runs[0]);
  ok = 1;
  for (i=0; i<fb->nruns; i++) {
    if (!fb->runs[i]->done)
      ok = 0;
    fb->runs[i]->done = 0;
    if (!fb->estimate && too_big(fb->par, fb->runs[i]))
      ok = 0;
  }
  if (!ok) return 0;
  if (fb->par->zero_mode) {
    /* Check that there is no ambiguity in the zeroes positions */
    for (i=0; i<fb->nruns; i++)
      if (fb->runs[i]->n[0]) {
        int j, n1=0, n2=0;
        Run* r = fb->runs[i];
        for (j=0; j<r->nslots; j++) {
          if (!r->slots[j]->run[1-r->dir]) {
            n1++;
            /* Set the slot to 0 as we go, solver wont reach it anyway */
            r->slots[j]->n[0] = 1;
          } else if (r->slots[j]->run[1-r->dir]->n[0]) {
            if (r->slots[j]->run[1-r->dir]->done == 2)
              n1++;
            else
              n2++;
            /* Set the slot to 0 in either case */
            r->slots[j]->n[0] = 1;
          }
        }
        if (n1+n2>1) { 
          if (n1>0) {
            ok = 0;
            break;
          } else 
            r->done = 2;
        }
      }
    for (i=0; i<fb->nruns; i++)
      if (fb->runs[i]->done == 2)
        fb->runs[i]->done = 0;
  }
  return ok;
}

static long count_possibilities(const game_params* par, Run* run, long limit)
{
  int numind;
  int* ii;
  int* bb;
  char* cache;
  long count = 0;
  mi_setup(run, par, &numind, &ii, &bb, &cache);
  if (!mi_first(run, par, numind, ii, bb, cache, 0)) {
    sfree(ii);
    sfree(bb);
    sfree(cache);
    return count;
  }
  do {
    count++;
    if (count > limit) {
      mi_abort(run, numind, ii, bb);
      break;
    }
  } while (mi_next(run, par, numind, ii, bb, cache));
  sfree(ii);
  sfree(bb);
  sfree(cache);
  return count;
}
  
float estimate_possibilities(const game_params* par, Run* run)
{
  /* Rough estimate of possible placements of factors */
  float lp, bn;
  int i, j, m, n;
  Run* r0;
  lp = 0.0, bn = 0.0;
  for (j=1; j<=par->pmax; j++) {
    n = run->n[j];
    if (!n) continue;
    for (i=0, m=0; i<run->nslots; i++) {
      r0 = run->slots[i]->run[1-run->dir];
      if (!r0 || r0->n[0] || (!r0->done && r0->r[j]))
        m++;
    }
    bn += n*logprimes[j];
    if (j <= (par->pmax+1)/2) {
      if (m > 1)
        lp += (m+n-0.5)*log(m+n-1) - (n+0.5)*log(n) - (m-0.5)*log(m-1) - 1;
    } else {
      if (m > n)
        lp += (m+0.5)*log(m) - (n+0.5)*log(n) - (m-n+0.5)*log(m-n) - 1;
    }
  }
  return (lp + (bn>20 ? (bn-20) : 0.0));
}

static Run *select_run(FactorBoard *fb)
{
    int i, best = -1;
    for (i=0; i<fb->nruns; i++) {
        if (!fb->runs[i]->done &&
            !fb->runs[i]->n[0] &&
            (best == -1 || fb->runs[i]->srem < fb->runs[best]->srem))
            best = i;
    }
    if (best == -1)
        return 0;
    else {
      int b, i;
      long m, mn;
      b = best;
      mn = m = count_possibilities(fb->par, fb->runs[best], 10000);
      for (i=0; i<fb->nruns; i++) {
        if (i == best) continue;
        if (fb->runs[i]->done || fb->runs[i]->n[0]) continue;
        m = count_possibilities(fb->par, fb->runs[i], mn);
        if (m < mn) mn = m, b = i;
      }
      return fb->runs[b];
    }
}

static int contains_one(FactorBoard* fb)
{
    int ok, i, j;
    for (i=0; i<fb->nslots; i++) {
      ok = 0;
      for (j=0; j<=fb->par->pmax; j++)
        if (fb->slots[i]->n[j]) {
          ok = 1;
          break;
        }
      if (!ok)
        return 1;
    }
    return 0;
}

static long count_internal(FactorBoard *fb)
{
    int numind;
    int *ii;
    int *bb;
    char *cache;
    long sol, s;
    Run *run = select_run(fb);
    if (!run) {
      if (fb->par->notone_mode && contains_one(fb)) {
        /* invalid solution with ones */
        fb->onesol++;
        return 0;
      } else {
        /* we have a solution and are done */
        export_answer(fb, fb->candidate);
        return 1;
      }
    }
    mi_setup(run, fb->par, &numind, &ii, &bb, &cache);
    if (!mi_first(run, fb->par, numind, ii, bb, cache, 0)) {
        sfree(ii);
        sfree(bb);
        sfree(cache);
        return 0;
    }
    sol = 0;
    fix(run);
    do {
        s = count_internal(fb);
        fb->iter++;
        if (s >= fb->itermax || fb->iter >= fb->itermax) {
          sol = fb->itermax;
          mi_abort(run, numind, ii, bb);
          break;
        } else
          sol += s;
        if (fb->quickret && sol > fb->quickret) {
          mi_abort(run, numind, ii, bb);
          break;
        }
    } while (mi_next(run, fb->par, numind, ii, bb, cache));
    unfix(run);
    sfree(ii);
    sfree(bb);
    sfree(cache);
    return sol;
}

static void count_solutions(FactorBoard *fb, const char *str, long limit, long *sol, long *it)
{
    int i, ok;
    float lp = 0.0;
    clean(fb);
    fb->iter = 0;
    fb->quickret = limit;
    fb->itermax = 50000;
    fb->estlimit = 10.0*fb->par->size;
    import_answer(fb, str);
    if (fb->par->notone_mode)
      fb->onesol = 0;
    if (!check_correct_form(fb)) {
      *sol = -1;
    } else if (fb->estimate) {
      ok = 1;
      for (i=0; i<fb->nruns; i++)
        if (!fb->runs[i]->n[0]) {
          lp += estimate_possibilities(fb->par, fb->runs[i]);
          if (too_big(fb->par, fb->runs[i])) ok = 0;
        }
      if (lp < fb->estlimit && lp*100 + fb->itermax < limit && ok) {
        *sol = count_internal(fb);
        if (*sol < fb->itermax) {
          if (fb->par->notone_mode && fb->par->max > 3) {
            if (fb->onesol < *sol) *sol = 2 * *sol - fb->onesol;
            if (*sol > fb->itermax/2) *sol = (*sol+fb->itermax)/3;
          }
          fb->estimate = 0;
        } else
          *sol = fb->itermax + (int)(lp*100);
      } else
        *sol = fb->itermax + (int)(lp*100);
    } else {
      *sol = count_internal(fb);
      if (*sol < fb->itermax && fb->par->notone_mode && fb->par->max > 3) {
        if (fb->onesol < *sol) *sol = 2 * *sol - fb->onesol;
        if (*sol > fb->itermax/2) *sol = (*sol+fb->itermax)/3;
      }
    }
    *it = fb->iter;
}

static pair *simple_evolve(random_state *rs, const game_params *par, char** answer)
{
    FactorBoard *fb = new_FactorBoard(par);
    char *vec1, *vec2;
    long val1, val2;
    long hard1, hard2;
    int gen, genbad, itertot, tmp;
    pair *ret;

    vec1 = randomize_answer(rs, par);
    fb->estimate = 1;
    count_solutions(fb, vec1, 0, &val1, &hard1);
    gen = 1;
    genbad = 0;
    itertot = 0;
    while (val1 != 1) {
        gen++;
        if (val1 <= 0)
            vec2 = randomize_answer(rs, par);
        else
            vec2 = mutate_answer(rs, par, strcpy(snewn(par->size*par->size+1, char), vec1), (val1 > 250 ? 10 : 5));
        count_solutions(fb, vec2, (val1 < 0 ? 0 : val1), &val2, &hard2);
        if (val2 > 0 && (val1 < 0 || val2 < val1)) {
            sfree(vec1);
            val1 = val2, hard1 = hard2;
            vec1 = vec2;
            genbad = 0;
        } else {
            sfree(vec2);
            if (val2 > 0)
              genbad++;
        }
        tmp = (int)(hard2 >> 8);
        itertot += tmp;
        if (fb->estimate ? genbad >= BAD_GEN_LIMIT(par->size) : itertot >= ITER_LIMIT(par->size)) {
          /* restart */
          gen++;
          genbad = 0;
          itertot = 0;
          sfree(vec1);
          vec1 = randomize_answer(rs, par);
          fb->estimate = 1;
          count_solutions(fb, vec1, 0, &val1, &hard1);
        }
    }

    ret = get_clues(fb);
    *answer = dupstr(fb->candidate);
    sfree(vec1);
    delete_FactorBoard(fb);
    return ret;
}

static char *new_game_desc(const game_params *params, random_state *rs,
			   char **aux, bool interactive)
{
    pair *clues;
    char *buf, *p, *ret;
    int n, i, run;

    clues = simple_evolve(rs, params, aux);

    n = (params->size+1) * (params->size+1);
    buf = snewn(n * 24, char);

    p = buf;
    run = 0;
    for (i = 0; i < n+1; i++) {
        if (i < n && clues[i].h == -2 && clues[i].v == -2) {
            run++;
        } else {
            while (run > 0) {
                int thisrun = run > 26 ? 26 : run;
                *p++ = 'a' + (thisrun-1);
                run -= thisrun;
            }
            if (i < n) {
                if (clues[i].h != -1 && clues[i].v != -1)
                    p += sprintf(p, "B%ld.%ld", clues[i].h, clues[i].v);
                else if (clues[i].h != -1)
                    p += sprintf(p, "H%ld", clues[i].h);
                else if (clues[i].v != -1)
                    p += sprintf(p, "V%ld", clues[i].v);
                else
                    *p++ = 'X';
            }
        }
    }
    *p = '\0';

    ret = dupstr(buf);
    sfree(buf);
    sfree(clues);
    return ret;
}

/* ----------------------------------------------------------------------
 * Main game UI.
 */

static const char *validate_desc(const game_params *params, const char *desc)
{
    int wanted = (params->size+1) * (params->size+1);
    int n = 0;

    while (n < wanted && *desc) {
        char c = *desc++;
        if (c == 'X') {
            n++;
        } else if (c >= 'a' && c < 'z') {
            n += 1 + (c - 'a');
        } else if (c == 'B') {
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            if (*desc != '.')
                return "Expected a '.' after number following 'B'";
            desc++;
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            n++;
        } else if (c == 'H' || c == 'V') {
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            n++;
        } else {
            return "Unexpected character in grid description";
        }
    }

    if (n > wanted || *desc)
        return "Too much data to fill grid";
    else if (n < wanted)
        return "Not enough data to fill grid";

    return NULL;
}

static game_state *new_game(midend *me, const game_params *params, const char *desc)
{
    game_state *state = snew(game_state);
    int w, h, wh, i, n;

    w = h = params->size + 1;
    wh = w*h;
    state->par = params;
    state->grid = snewn(wh, digit);
    state->pencil = snewn(wh, long);
    for (i = 0; i < wh; i++) {
        state->grid[i] = -1;
        state->pencil[i] = 0;
    }
    state->completed = state->cheated = false;

    state->clues = snew(struct clues);
    state->clues->refcount = 1;
    state->clues->w = w;
    state->clues->h = h;
    state->clues->playable = snewn(wh, unsigned char);
    state->clues->hclues = snewn(wh, long);
    state->clues->vclues = snewn(wh, long);
    state->clues->me = me;

    n = 0;

    while (n < wh && *desc) {
        char c = *desc++;
        if (c == 'X') {
            state->clues->playable[n] = false;
            state->clues->hclues[n] = state->clues->vclues[n] = -1;
            n++;
        } else if (c >= 'a' && c < 'z') {
            int k = 1 + (c - 'a');
            while (k-- > 0) {
                state->clues->playable[n] = true;
                state->clues->hclues[n] = state->clues->vclues[n] = -1;
                n++;
            }
        } else if (c == 'B') {
            state->clues->playable[n] = false;
            state->clues->hclues[n] = atol(desc);
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            assert(*desc == '.');
            desc++;
            state->clues->vclues[n] = atol(desc);
            while (*desc && isdigit((unsigned char)*desc))
                desc++;            
            n++;
        } else if (c == 'H') {
            state->clues->playable[n] = false;
            state->clues->hclues[n] = atol(desc);
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            state->clues->vclues[n] = -1;
            n++;
        } else if (c == 'V') {
            state->clues->playable[n] = false;
            state->clues->hclues[n] = -1;
            state->clues->vclues[n] = atol(desc);
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            n++;
        } else {
            assert(!"This should never happen");
        }
    }

    assert(!*desc);
    assert(n == wh);

    return state;
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = snew(game_state);
    int w, wh, i;

    w = state->par->size+1;
    wh = w*w;
    ret->par = state->par;
    ret->grid = snewn(wh, digit);
    ret->pencil = snewn(wh, long);
    for (i = 0; i < wh; i++) {
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
        sfree(state->clues->playable);
        sfree(state->clues->hclues);
        sfree(state->clues->vclues);
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
    FactorBoard *fb = new_FactorBoard(state->par);
    long sol;
    fb->iter = 0;
    fb->quickret = 2;
    fb->itermax = 10000000;
    set_clues(fb, state->clues);
    if (!check_correct_form(fb)) {
      *error = "Game is not correctly formed";
      return NULL;
    }
    sol = count_internal(fb);
    if (sol>0) {
      return dupstr(fb->candidate);
    } else {
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
#ifdef MULTIDIGIT
    /*
     * To allow for two-digit inputs, save one unfinished input char.
     */
    int pending;
#endif /* MULTIDIGIT */
    /*
     * Hint mode, toggles with 'H' and makes clue factorizations show
     * in the message area.
     */
    int showhint;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);

    ui->hx = ui->hy = 0;
    ui->hpencil = ui->hshow = ui->hcursor = ui->showhint = 0;
#ifdef MULTIDIGIT
    ui->pending = 0;
#endif /* MULTIDIGIT */
    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
    int w = newstate->par->size + 1;
    /*
     * We prevent pencil-mode highlighting of a filled square, unless
     * we're using the cursor keys. So if the user has just filled in
     * a square which we had a pencil-mode highlight in (by Undo, or
     * by Redo, or by Solve), then we cancel the highlight.
     */
    if (ui->hshow && ui->hpencil && !ui->hcursor &&
        newstate->grid[ui->hy * w + ui->hx] != -1) {
        ui->hshow = 0;
    }
}

#define PREFERRED_TILESIZE 48
#define TILESIZE (ds->tilesize)
#define BORDER (TILESIZE / 2)
#define GRIDEXTRA max((TILESIZE / 32),1)
#define COORD(x) ((x)*TILESIZE + BORDER)
#define FROMCOORD(x) (((x)+(TILESIZE-BORDER)) / TILESIZE - 1)

#define FLASH_TIME 0.4F

#define DF_PENCIL_SHIFT 11
#define DF_ERR_HCLUE 0x0800
#define DF_ERR_VCLUE 0x0400
#define DF_HIGHLIGHT 0x0200
#define DF_HIGHLIGHT_PENCIL 0x0100
#define DF_DIGIT_MASK 0x007F
#define DF_HAS_DIGIT_MASK 0x0080

struct game_drawstate {
    int tilesize;
    int w, h;
    int started;
    long *tiles;
    long *errors;
};

static char* make_move_string(const game_params *par, game_ui *ui, int n)
{
  char buf[80];
  if ((n != -1 && 
       (n > par->max || 
        (n == 0 && !par->zero_mode) || 
        (n == 1 && par->notone_mode))) ||
      (ui->hpencil && n == 0))
#ifdef MULTIDIGIT
    return (ui->pending ? dupstr("O") : MOVE_UI_UPDATE);
#else
    return MOVE_UI_UPDATE;
#endif /* MULTIDIGIT */
  sprintf(buf, "%c%d,%d,%d", (char)(ui->hpencil ? 'P' : 'R'), ui->hx, ui->hy, n);
  return dupstr(buf);
}

#ifdef MULTIDIGIT
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
#endif /* MULTIDIGIT */

static char *interpret_move(const game_state *state, game_ui *ui, const game_drawstate *ds,
			    int x, int y, int button)
{
    int sz = state->par->size + 1;
    int n;
    int tx, ty;
    char* retstr = MOVE_UI_UPDATE;

    button &= ~MOD_MASK;

    tx = FROMCOORD(x);
    ty = FROMCOORD(y);

    if (tx >= 0 && tx < sz && ty >= 0 && ty < sz) {
        if (button == LEFT_BUTTON) {
            ui->hcursor = 0;
#ifdef MULTIDIGIT
            if (ui->pending) {
              retstr = make_move_string(state->par, ui, ui->pending - '0');
              abort_pending(state, ui);
            }
#endif /* MULTIDIGIT */
            if (tx == ui->hx && ty == ui->hy && ui->hshow && ui->hpencil == 0) {
              /* left-click on the already active square */
              ui->hshow = 0;
            } else {
              /* left-click on a new square */
              ui->hx = tx;
              ui->hy = ty;
              ui->hshow = (ui->showhint || state->clues->playable[ty*sz+tx]);
              ui->hpencil = 0;
            }
            return retstr;
        }
        if (button == RIGHT_BUTTON) {
            /*
             * Pencil-mode highlighting for non filled squares.
             */
            ui->hcursor = 0;
#ifdef MULTIDIGIT
            if (ui->pending) {
              retstr = make_move_string(state->par, ui, ui->pending - '0');
              abort_pending(state, ui);
            }
#endif /* MULTIDIGIT */
            if (tx == ui->hx && ty == ui->hy && ui->hshow && ui->hpencil) {
              /* right-click on the already active square */
              ui->hshow = 0;
              ui->hpencil = 0;
            } else {
              /* right-click on a new square */
              ui->hx = tx;
              ui->hy = ty;
              if (state->grid[ty*sz+tx] != -1 ||
                  (!ui->showhint && !state->clues->playable[ty*sz+tx])) {
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
#ifdef MULTIDIGIT
      if (ui->pending) {
        retstr = make_move_string(state->par, ui, ui->pending - '0');
        abort_pending(state, ui);
      }
#endif /* MULTIDIGIT */
      move_cursor(button, &ui->hx, &ui->hy, sz, sz, 0, NULL);
      ui->hshow = ui->hcursor = 1;
      return retstr;
    }
    if (ui->hshow &&
        (button == CURSOR_SELECT)) {
#ifdef MULTIDIGIT
      if (ui->pending) {
        retstr = make_move_string(state->par, ui, ui->pending - '0');
        abort_pending(state, ui);
      } else 
#endif /* MULTIDIGIT */
      {
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
        if (ui->hpencil && state->grid[ui->hy*sz+ui->hx] != -1)
            return NULL;

	/*
	 * Can't do anything to an immutable square.
	 */
        if (!state->clues->playable[ui->hy*sz+ui->hx])
            return NULL;

        if (!ui->hcursor) ui->hshow = 0;

#ifdef MULTIDIGIT
        if (ui->pending) {
          if (button == CURSOR_SELECT2)
            n = ui->pending - '0';
          else if (button == '\b') {
            n = -1;
          } else 
            n = (ui->pending - '0')*10 + (button - '0');
          retstr = make_move_string(state->par, ui, n);
          abort_pending(state, ui);
        } else 
#endif /* MULTIDIGIT */
        {
          if (button == CURSOR_SELECT2 || button == '\b')
            n = -1;
          else 
#ifdef MULTIDIGIT
          if (state->par->max > 9 && button >= '1' && button <= '0' + state->par->max/10) {
            ui->pending = button;
            ui->hshow = 1;
            n = button - '0';
            retstr = make_move_string(state->par, ui, n);
            return retstr;
          } else
#endif /* MULTIDIGIT */
          {
            n = button - '0';
          }
          retstr = make_move_string(state->par, ui, n);
        }
	return retstr;
    }

    if (button == 'M' || button == 'm') {
#ifdef MULTIDIGIT
        if (ui->pending) {
          finish_pending(ui);
        }
#endif /* MULTIDIGIT */
        return dupstr("M");
    }

    if (button == 'H' || button == 'h') {
#ifdef MULTIDIGIT
      if (ui->pending) {
        retstr = make_move_string(state->par, ui, ui->pending - '0');
        abort_pending(state, ui);
      }
#endif /* MULTIDIGIT */
      ui->showhint = 1 - ui->showhint;
      return retstr;
    }

    return NULL;
}

static bool check_errors(const game_state *state, long *errors)
{
    int sz = state->par->size + 1, a = sz*sz;
    int x, y;
    int ret = false;

    if (errors)
        for (x = 0; x < a; x++) errors[x] = 0;

    for (y = 0; y < sz; y++) {
        for (x = 0; x < sz; x++) {
            if (!state->clues->playable[y*sz+x] &&
                state->clues->hclues[y*sz+x] >= 0) {
                long clue = state->clues->hclues[y*sz+x];
                int error = false;
                int zero = false;
                int unfilled = false;
                int xx;
                for (xx = x+1; xx<sz && state->clues->playable[y*sz+xx]; xx++) {
                    long d = state->grid[y*sz+xx];
                    if (d == -1) {
                        unfilled = true;    /* an unfilled square exists */
                    } else if (d == 0) {
                        zero = true;
                    } else if (clue % d) {
                        error = true;
                        break;
                    } else {
                        clue /= d;
                    }
                }
                if (error || (zero ? clue != 0 : !unfilled && clue != 1)) {
                    ret = true;
                    if (errors)
                        errors[y*sz+x] |= DF_ERR_HCLUE;
                } else if (unfilled)
                  ret = true;
            }
        }
    }

    for (x = 0; x < sz; x++) {
        for (y = 0; y < sz; y++) {
            if (!state->clues->playable[y*sz+x] &&
                state->clues->vclues[y*sz+x] >= 0) {
                long clue = state->clues->vclues[y*sz+x];
                int error = false;
                int zero = false;
                int unfilled = false;
                int yy;
                for (yy = y+1; yy<sz && state->clues->playable[yy*sz+x]; yy++) {
                    long d = state->grid[yy*sz+x];
                    if (d == -1) {
                        unfilled = true;    /* an unfilled square exists */
                    } else if (d == 0) {
                        zero = true;
                    } else if (clue % d) {
                        error = true;
                        break;
                    } else {
                        clue /= d;
                    }
                }
                if (error || (zero ? clue != 0 : !unfilled && clue != 1)) {
                    ret = true;
                    if (errors)
                        errors[y*sz+x] |= DF_ERR_VCLUE;
                } else if (unfilled)
                  ret = true;
            }
        }
    }

    return ret;
}

static game_state *execute_move(const game_state *from0, const char *move)
{
    game_state* from = (game_state*) from0;
    int sz = from->par->size + 1, a = sz*sz;
    game_state *ret;
    int x, y, i, n;

    if (move[0] == 'O') { /* Null operation for delayed moves */
      ret = dup_game(from);
      return ret;
    } else if (move[0] == 'S') {
	ret = dup_game(from);
	ret->completed = ret->cheated = true;

	for (i = 0; i < a; i++) {
            if (!from->clues->playable[i])
                continue;
	    if (move[i+1] < '0' || move[i+1] > '0'+MAXNUM) {
		free_game(ret);
		return from;
	    }
	    ret->grid[i] = move[i+1] - '0';
	    ret->pencil[i] = 0;
	}

	if (move[a+1] != '\0') {
	    free_game(ret);
	    return from;
	}

	return ret;
    } else if ((move[0] == 'P' || move[0] == 'R') &&
	sscanf(move+1, "%d,%d,%d", &x, &y, &n) == 3 &&
	x >= 0 && x < sz && y >= 0 && y < sz && n >= -1 && n <= MAXNUM) {
	if (!from->clues->playable[y*sz+x])
	    return from;

	ret = dup_game(from);
        if (move[0] == 'P') {
          if (n == -1)
            ret->pencil[y*sz+x] = 0;
          else if (n > 0)
            ret->pencil[y*sz+x] ^= 1L << n;
        } else {
            ret->grid[y*sz+x] = n;
            ret->pencil[y*sz+x] = 0;
        }
	return ret;
    } else if (move[0] == 'M') {
	/*
	 * Fill in absolutely all pencil marks everywhere.
	 */
        long mask = (2L<<from->par->max) - (from->par->notone_mode ? 4 : 2);
	ret = dup_game(from);
	for (i = 0; i < a; i++) {
	    if (ret->grid[i] == -1)
                ret->pencil[i] = mask;
	}
	return ret;
    } else
	return from;		       /* couldn't parse move string */
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

#define SIZE(w) (((w)+1) * TILESIZE + 2*BORDER)

static void game_compute_size(const game_params *params, int tilesize,
			      const game_ui *ui, int *x, int *y)
{
    /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    struct { int tilesize; } ads, *ds = &ads;
    ads.tilesize = tilesize;

    *x = *y = SIZE(params->size);
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
    int sz = state->par->size + 1, a = sz*sz;
    int i;

    ds->tilesize = 0;
    ds->w = sz;
    ds->h = sz;
    ds->started = false;
    ds->tiles = snewn(a, long);
    for (i = 0; i < a; i++)
	ds->tiles[i] = -1;
    ds->errors = snewn(a, long);

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->tiles);
    sfree(ds->errors);
    sfree(ds);
}

static void draw_tile(drawing *dr, game_drawstate *ds, const game_params *par,
		      struct clues *clues, int x, int y, long tile)
{
    int sz = ds->w /*, h = ds->h, a = w*h */;
    int tx, ty, tw, th;
    int cx, cy, cw, ch;
    char str[64];

    tx = BORDER + x * TILESIZE + 1 + GRIDEXTRA;
    ty = BORDER + y * TILESIZE + 1 + GRIDEXTRA;

    cx = tx;
    cy = ty;
    cw = tw = TILESIZE-1-2*GRIDEXTRA;
    ch = th = TILESIZE-1-2*GRIDEXTRA;

    clip(dr, cx, cy, cw, ch);

    /* background needs erasing */
    draw_rect(dr, cx, cy, cw, ch,
	      (tile & DF_HIGHLIGHT) ? COL_HIGHLIGHT : COL_BACKGROUND);

    /* pencil-mode highlight */
    if (tile & DF_HIGHLIGHT_PENCIL) {
        int coords[6];
        coords[0] = cx;
        coords[1] = cy;
        coords[2] = cx+cw/2;
        coords[3] = cy;
        coords[4] = cx;
        coords[5] = cy+ch/2;
        draw_polygon(dr, coords, 3, COL_HIGHLIGHT, COL_HIGHLIGHT);
    }

    if (!clues->playable[y*sz+x]) {
        /*
         * This is an unplayable square, so draw clue(s) or
         * crosshatching.
         */
        long hclue = clues->hclues[y*sz+x], vclue = clues->vclues[y*sz+x];
        char hcbuf[20], vcbuf[20];
        int i, hfs, vfs;
        sprintf(hcbuf, "%ld", hclue);
        sprintf(vcbuf, "%ld", vclue);
        hfs = ((i=strlen(hcbuf)) < 5 ? ch/4 : i<7 ? ch/5 : ch/6);
        vfs = ((i=strlen(vcbuf)) < 5 ? ch/4 : i<7 ? ch/5 : ch/6);
        if (hclue < 0 && vclue < 0) {
            /* unplayable square without clue - crosshatch it */
            for (i = cw/8-2; i < 2*cw; i += cw/8)
                draw_line(dr, cx+i, cy, cx+i-ch, cy+ch, COL_GRID);
        } else if (x == 0 && hclue >= 0) {
            /* left edge horizontal clue, center it */
            draw_text(dr, cx+cw/2, cy+ch/2, FONT_VARIABLE, hfs,
                      ALIGN_VCENTRE | ALIGN_HCENTRE,
                      (tile & DF_ERR_HCLUE ? COL_ERROR : COL_GRID), hcbuf);
        } else if (y == 0 && vclue >= 0) {
            /* top edge vertical clue, center it */
            draw_text(dr, cx+cw/2, cy+ch/2, FONT_VARIABLE, vfs,
                      ALIGN_VCENTRE | ALIGN_HCENTRE,
                      (tile & DF_ERR_VCLUE ? COL_ERROR : COL_GRID), vcbuf);
        } else {
          /* Divide square in two parts, horizontal clue upper and vertical lower */
          draw_line(dr, cx, cy, cx+cw, cy+ch, COL_GRID);
          if (hclue >= 0) {
            draw_text(dr, cx+(cw+hfs)/2, cy+hfs, FONT_VARIABLE, hfs,
                      ALIGN_VCENTRE | ALIGN_HCENTRE,
                      (tile & DF_ERR_HCLUE ? COL_ERROR : COL_GRID), hcbuf);
          } else {
            for (i = cw/8 - 2; i < 2*cw; i += cw/8)
                draw_line(dr, cx+i, cy, cx+i/2, cy+i-i/2, COL_GRID);
          }
          if (vclue >= 0) {
            draw_text(dr, cx+(cw-vfs)/2, cy+ch-vfs, FONT_VARIABLE, vfs,
                      ALIGN_VCENTRE | ALIGN_HCENTRE,
                      (tile & DF_ERR_VCLUE ? COL_ERROR : COL_GRID), vcbuf);
          } else {
            for (i = cw/8 - 2; i < 2*cw; i += cw/8)
                draw_line(dr, cx, cy+i, cx+i-i/2, cy+i/2, COL_GRID);
          }
        }
    } else {
        /*
         * This is a playable square, so draw a user-entered number or
         * pencil mark.
         */

        /* new number needs drawing? */
        if (tile & DF_HAS_DIGIT_MASK) {
            sprintf(str, "%d", (int)(tile & DF_DIGIT_MASK));
            draw_text(dr, tx + TILESIZE/2, ty + TILESIZE/2,
                      FONT_VARIABLE, TILESIZE/2, ALIGN_VCENTRE | ALIGN_HCENTRE,
                      COL_USER, str);
        } else if ((tile & DF_DIGIT_MASK) && !(tile & DF_HIGHLIGHT_PENCIL)) {
            sprintf(str, "%d_", (int)(tile & DF_DIGIT_MASK));
            draw_text(dr, tx + TILESIZE/2, ty + TILESIZE/2,
                      FONT_VARIABLE, TILESIZE/2, ALIGN_VCENTRE | ALIGN_HCENTRE,
                      COL_USER, str);
        } else {
            long rev;
            int i, j, npencil;
            int pl, pr, pt, pb;
            float vhprop;
            int ok, pw, ph, minph, minpw, pgsizex, pgsizey, fontsize;

            /* Count the pencil marks required. */
            if ((tile & DF_DIGIT_MASK) && ((tile & DF_DIGIT_MASK) != 1 || !par->notone_mode)) {
                rev = 1L << ((tile & DF_DIGIT_MASK) + DF_PENCIL_SHIFT);
            } else
                rev = 0;
            for (i = 1, npencil = 0; i <= MAXNUM; i++)
                if ((tile ^ rev) & (1L << (i + DF_PENCIL_SHIFT)))
                    npencil++;
            if (tile & DF_DIGIT_MASK)
                npencil++;
            if (npencil) {

                minph = 2;
                minpw = (par->max > 9 ? 2 : 3);
                vhprop = (par->max > 9 ? 1.5 : 1.0);
                /*
                 * Determine the bounding rectangle within which we're going
                 * to put the pencil marks.
                 */
                pl = tx + GRIDEXTRA;
                pr = pl + TILESIZE - GRIDEXTRA;
                pt = ty + GRIDEXTRA;
                pb = pt + TILESIZE - GRIDEXTRA - 2; 

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
                pl = tx + (TILESIZE - pgsizex * pw) / 2;
                pt = ty + (TILESIZE - pgsizey * ph - 2) / 2;

                /*
                 * Now actually draw the pencil marks.
                 */
                for (i = 1, j = 0; i <= MAXNUM; i++)
                    if ((tile ^ rev) & (1L << (i + DF_PENCIL_SHIFT))) {
                        int dx = j % pw, dy = j / pw;
                        sprintf(str, "%d", i);
                        draw_text(dr, pl + pgsizex * (2*dx+1) / 2,
                                  pt + pgsizey * (2*dy+1) / 2,
                                  FONT_VARIABLE, fontsize,
                                  ALIGN_VCENTRE | ALIGN_HCENTRE, COL_PENCIL,
                                  str);
                        j++;
                    }
                if (tile & DF_DIGIT_MASK) {
                  int dx = j % pw, dy = j / pw;
                  sprintf(str, "%d_", (int)(tile & DF_DIGIT_MASK));                  
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
    int sz = state->par->size + 1;
    int x, y;

    if (!ds->started) {
	/*
	 * The initial contents of the window are not guaranteed and
	 * can vary with front ends. To be on the safe side, all
	 * games should start by drawing a big background-colour
	 * rectangle covering the whole window.
	 */
	draw_rect(dr, 0, 0, SIZE(sz), SIZE(sz), COL_BACKGROUND);

	/*
	 * Big containing rectangle.
	 */
	draw_rect(dr, COORD(0) - GRIDEXTRA, COORD(0) - GRIDEXTRA,
		  sz*TILESIZE+1+GRIDEXTRA*2, sz*TILESIZE+1+GRIDEXTRA*2,
		  COL_GRID);

	draw_update(dr, 0, 0, SIZE(sz), SIZE(sz));

	ds->started = true;
    }

    if (animtime)
      return;
#ifdef MULTIDIGIT
    if (ui->pending && !oldstate) {      
      finish_pending(ui);
    }
#endif /* MULTIDIGIT */
    
#ifdef MULTIDIGIT
    if (!ui->pending)
#endif /* MULTIDIGIT */
      check_errors(state, ds->errors);

    status_bar(dr, "");
    for (y = 0; y < sz; y++) {
	for (x = 0; x < sz; x++) {
	    long tile = 0L;

            tile = (long)state->pencil[y*sz+x] << DF_PENCIL_SHIFT;
#ifdef MULTIDIGIT
            if (ui->pending && ui->hx == x && ui->hy == y)
                tile |= ui->pending - '0';
	    else 
#endif /* MULTIDIGIT */
            if (state->grid[y*sz+x] != -1)
              tile = state->grid[y*sz+x] | DF_HAS_DIGIT_MASK;

	    if (ui->hshow && ui->hx == x && ui->hy == y) {
		tile |= (ui->hpencil ? DF_HIGHLIGHT_PENCIL : DF_HIGHLIGHT);
                if (ui->showhint && 
                    (state->clues->vclues[y*sz+x] != -1 ||
                     state->clues->hclues[y*sz+x] != -1)) {
                  int i, j;
                  char vv[NPRIME];
                  char *bufp;
                  char buf[400];
                  bufp = buf;
                  if (state->clues->hclues[y*sz+x] != -1) {
                    factorize(state->clues->hclues[y*sz+x], vv, state->par->pmax);
                    sprintf(bufp, "H %ld: ", state->clues->hclues[y*sz+x]);
                    bufp += strlen(bufp);
                    for (i=0; i<=state->par->pmax; i++)
                      for (j=0; j<vv[i]; j++) {
                        sprintf(bufp, "%d ", primes[i]);
                        bufp += strlen(bufp);
                      }
                    sprintf(bufp, "     ");
                    bufp += strlen(bufp);
                  }
                  if (state->clues->vclues[y*sz+x] != -1) {
                    factorize(state->clues->vclues[y*sz+x], vv, state->par->pmax);
                    sprintf(bufp, "V %ld: ", state->clues->vclues[y*sz+x]);
                    bufp += strlen(bufp);
                    for (i=0; i<=state->par->pmax; i++)
                      for (j=0; j<vv[i]; j++) {
                        sprintf(bufp, "%d ", primes[i]);
                        bufp += strlen(bufp);
                      }
                  }
                  status_bar(dr, buf);
                }
            }

            if (flashtime > 0 &&
                (flashtime <= FLASH_TIME/3 ||
                 flashtime >= FLASH_TIME*2/3))
                tile |= DF_HIGHLIGHT;  /* completion flash */

	    tile |= ds->errors[y*sz+x];

	    if (ds->tiles[y*sz+x] != tile) {
		ds->tiles[y*sz+x] = tile;
		draw_tile(dr, ds, state->par, state->clues, x, y, tile);
	    }
	}
    }
}

static float game_anim_length(const game_state *oldstate, const game_state *newstate,
			      int dir, game_ui *ui)
{
#ifdef MULTIDIGIT
  if (ui->pending)
    return 1.0;
  else
#endif /* MULTIDIGIT */
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
#define thegame factorcross
#endif

const struct game thegame = {
    "Factorcross", NULL, NULL,
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
    true,			       /* wants_statusbar */
    false, game_timing_state,
    0,				       /* flags */
};
