/*
 * Copyright (C) 2015 Anders Holst (aho@sics.se)
 *
 * Several types of auto-generated super mazes, freely inspired by Robert
 * Abbot's stateful and high-dimensional super-mazes.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

#define MAXCOORD 4
#define MAXDOMAIN 6

typedef enum { Basic, Tandem, ThreeD, Floors, Keys, Levers, Combo }  SmStyle;

enum {
    COL_BACKGROUND,
    COL_GRID,
    COL_DOOR,
    COL_0_D,
    COL_0_N,
    COL_0_L,
    COL_1_D,
    COL_1_N,
    COL_1_L,
    COL_2_D,
    COL_2_N,
    COL_2_L,
    COL_3_D,
    COL_3_N,
    COL_3_L,
    COL_4_D,
    COL_4_N,
    COL_4_L,
    COL_5_D,
    COL_5_N,
    COL_5_L,
    COL_6_D,
    COL_6_N,
    COL_6_L,
    COL_7_D,
    COL_7_N,
    COL_7_L,
    COL_8_D,
    COL_8_N,
    COL_8_L,
    COL_9_D,
    COL_9_N,
    COL_9_L,
    COL_SHADE0,
    COL_SHADE1,
    COL_SHADE2,
    COL_SHADE3,
    COL_SHADE4,
    NCOLOURS
};

struct game_params {
  SmStyle style;
  int size;
  int floors;
  int keys;
  int levers;
  int difficult;
};

struct clues {
  int refcount;
  int size;
  int nswitches;
  unsigned char* doorvector;
  unsigned char** doorswitches;
  int* doorprop;
  int* roomvector;
  char* sol;
};

struct game_state {
  const game_params *par;
  struct clues *clues;
  int* coord;
  int completed, cheated;
};


static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    ret->style = Basic;
    ret->size = 10;
    ret->floors = 0;
    ret->keys = 0;
    ret->levers = 0;
    ret->difficult = 1;

    return ret;
}

const static struct game_params supermaze_presets[] = {
  {0, 10, 0, 0, 0, 1},
  {0, 16, 0, 0, 0, 1},
  {0, 25, 0, 0, 0, 1},
  {1, 4, 0, 0, 0, 1},
  {1, 6, 0, 0, 0, 1},
  {2, 4, 0, 0, 0, 1},
  {2, 6, 0, 0, 0, 1},
  {2, 8, 0, 0, 0, 1},
  {2, 10, 0, 0, 0, 1},
  {3, 6, 3, 0, 0, 1},
  {3, 8, 5, 0, 0, 1},
  {3, 10, 10, 0, 0, 1},
  {4, 6, 0, 3, 0, 1},
  {4, 10, 0, 5, 0, 0},
  {4, 10, 0, 9, 0, 0},
  {5, 6, 0, 0, 3, 1},
  {5, 10, 0, 0, 5, 1},
  {5, 10, 0, 0, 9, 0},
  {6, 8, 3, 3, 3, 0},
  {6, 10, 4, 4, 4, 0}
};


static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i != -1 && (i < 0 || i >= lenof(supermaze_presets)))
        return false;

    if (i == -1) {
      ret = *params;
    } else {
      ret = snew(game_params);
      *ret = supermaze_presets[i];
      *params = ret;
    }
 
    sprintf(buf, "Size %d ", ret->size);

    if (ret->style == Basic)
      sprintf(buf+strlen(buf), "basic");
    else if (ret->style == Tandem)
      sprintf(buf+strlen(buf), "tandem");
    else if (ret->style == ThreeD)
      sprintf(buf+strlen(buf), "3D");
    else if (ret->style == Floors)
      sprintf(buf+strlen(buf), "with %d floors", ret->floors);
    else if (ret->style == Keys)
      sprintf(buf+strlen(buf), "with %d keys", ret->keys);
    else if (ret->style == Levers)
      sprintf(buf+strlen(buf), "with %d levers", ret->levers);
    else if (ret->style == Combo)
      sprintf(buf+strlen(buf), "with %d keys, levers and floors", ret->levers);
    else 
      sprintf(buf+strlen(buf), "undefined style");
    if (ret->difficult)
      sprintf(buf+strlen(buf), ", extra tricky");
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

    params->floors = params->keys = params->levers = 0;
    params->size = atoi(p);
    while (*p && isdigit((unsigned char)*p)) p++;

    if (*p == 'N') {
      params->style = Basic;
      p++;
    } else if (*p == 'T') {
      params->style = Tandem;
      p++;
    } else if (*p == 'D') {
      params->style = ThreeD;
      p++;
    } else if (*p == 'F') {
      params->style = Floors;
      p++;
      params->floors = atoi(p);
      while (*p && isdigit((unsigned char)*p)) p++;
    } else if (*p == 'K') {
      params->style = Keys;
      p++;
      params->keys = atoi(p);
      while (*p && isdigit((unsigned char)*p)) p++;
    } else if (*p == 'L') {
      params->style = Levers;
      p++;
      params->levers = atoi(p);
      while (*p && isdigit((unsigned char)*p)) p++;
    } else if (*p == 'C') {
      params->style = Combo;
      p++;
      params->floors = params->keys = params->levers = atoi(p);
      while (*p && isdigit((unsigned char)*p)) p++;
    }

    params->difficult = (*p == 'E');
}

static char *encode_params(const game_params *params, bool full)
{
    char buf[20];

    sprintf(buf, "%d", params->size);
    if (params->style == Basic)
      sprintf(buf+strlen(buf), "N");
    else if (params->style == Tandem)
      sprintf(buf+strlen(buf), "T");
    else if (params->style == ThreeD)
      sprintf(buf+strlen(buf), "D");
    else if (params->style == Floors)
      sprintf(buf+strlen(buf), "F%d", params->floors);
    else if (params->style == Keys)
      sprintf(buf+strlen(buf), "K%d", params->keys);
    else if (params->style == Levers)
      sprintf(buf+strlen(buf), "L%d", params->levers);
    else if (params->style == Combo)
      sprintf(buf+strlen(buf), "C%d", params->keys);
    else
      sprintf(buf+strlen(buf), "U");

    if (params->difficult)
      sprintf(buf+strlen(buf), "E");

    return dupstr(buf);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[20];
    int ind = 0;

    ret = snewn(5, config_item);

    ret[ind].name = "Variant";
    ret[ind].type = C_CHOICES;
    ret[ind].u.choices.choicenames = ":Basic:Tandem:3D:Floors:Keys:Levers:Combined";
    ret[ind].u.choices.selected = params->style;

    ind++;
    ret[ind].name = "Size";
    ret[ind].type = C_STRING;
    sprintf(buf, "%d", params->size);
    ret[ind].u.string.sval = dupstr(buf);

    ind++;
    ret[ind].name = "Floors/Keys/Levers";
    ret[ind].type = C_STRING;
    if (params->style == Floors)
      sprintf(buf, "%d", params->floors);
    else if (params->style == Keys)
      sprintf(buf, "%d", params->keys);
    else if (params->style == Levers)
      sprintf(buf, "%d", params->levers);
    else if (params->style == Combo)
      sprintf(buf, "%d", params->keys);
    else
      sprintf(buf, "--");
    ret[ind].u.string.sval = dupstr(buf);

    ind++;
    ret[ind].name = "Extra tricky";
    ret[ind].type = C_BOOLEAN;
    ret[ind].u.boolean.bval = (params->difficult ? 1 : 0);

    ind++;
    ret[ind].name = NULL;
    ret[ind].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    int ind = 0;
    game_params *ret = snew(game_params);
    ret->floors = ret->keys = ret->levers = 0;

    ret->style = cfg[ind++].u.choices.selected;
    ret->size = atoi(cfg[ind++].u.string.sval);
    if (ret->style == Floors)
      ret->floors = atoi(cfg[ind++].u.string.sval);
    else if (ret->style == Keys)
      ret->keys = atoi(cfg[ind++].u.string.sval);
    else if (ret->style == Levers)
      ret->levers = atoi(cfg[ind++].u.string.sval);
    else if (ret->style == Combo)
      ret->floors = ret->keys = ret->levers = atoi(cfg[ind++].u.string.sval);
    else
      ind++;
    ret->difficult = cfg[ind++].u.boolean.bval;
    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
  if (params->style == Tandem) {
    if (params->size < 3 || params->size > 12)
      return "Game size of Tandem mode must be between 3 and 12";
  } else if (params->style == ThreeD) {
    if (params->size < 3 || params->size > 12)
      return "Game size of 3D mode must be between 3 and 12";
  } else {
    if (params->size < 3 || params->size > 25)
      return "Game size must be between 3 and 25";
  }

  if (params->style == Floors) {
    if (params->floors < 2 || params->floors > 10)
      return "Number of floors must be between 2 and 10";
  } else if (params->style == Keys) {
    if (params->keys < 1 || params->keys > 9)
      return "Number of keys must be between 1 and 9";
  } else if (params->style == Levers) {
    if (params->levers < 1 || params->levers > 9)
      return "Number of levers must be between 1 and 9";
  } else if (params->style == Combo) {
    if (params->keys < 1 || params->keys > 5)
      return "Number of keys, levers and floors must be between 1 and 5";
  }
 
  return NULL;
}

/* ----------------------------------------------------------------------
 * Game generation code.

Variants:

Basic: Ordinary 2D labyrinth, hidden until start walking it.
  State is just x and y, no mirror states, 4 directions.

Tandem: Two balls, doors depend on positions.
  State is both positions (in normalized order), mirror is that a ball on
  first and last square has same doors, 2*4 dirs 

3D: Several floors, ordinary geometry.
  State is x,y,z position, no mirror, 6 dirs.

Floors: Several floors with portals.
  State is x,y,z position, no mirror, 4 plus one out of (num floors - 1) dirs.

Keys: Doors that open if you have the right key.
  State is position and key set (2^k), mirror is that every keysets has at least
  the same doors open as any smaller key set, 4 + 1 out of num keys dirs.

Levers: Levers control doors
  State is position and lever states (2^k), mirror is all ordinary doors,
  4 + 1 dirs.

 */

typedef enum { Impossible, Open, Closed, Unset }  SmPowerDoor;

typedef enum { Unallocated, Connected, Island, Complete } SmRoomStatus;

typedef struct SmPowerRoom {
  int* coord;
  struct SmPowerRoom** trans;
  SmPowerDoor* door;
  int* recdir;
  SmRoomStatus status;
  int domain;
  int dist;
} SmPowerRoom;

typedef struct SuperMaze {
  int size;
  int nswitches;
  unsigned char* doorvector;
  unsigned char** doorswitches;
  int* roomvector;
} SuperMaze;


static int calchexlen(int sz, int fl)
{
  return ((sz*(sz-1)*2+3)/4) * fl;
}

static int calcdoorveclen(int sz, int fl)
{
  return (calchexlen(sz, fl) + 1)/2;
}

static unsigned char* makedoorvector(int sz, int fl)
{
  int i;
  int len = calcdoorveclen(sz, fl);
  unsigned char* res = snewn(len, unsigned char);
  for (i=0; i<len; i++)
    res[i] = 0xFF;
  return res;
}

static int doorbitpos(int sz, int x, int y, int z, int dir)
{
  int zoff = 4 * calchexlen(sz, z);
  if (dir==0) {
    if (x >= sz-1 || x < 0 || y >= sz || y < 0) return -1;
    return (y*(sz-1) + x) + zoff;
  } else if (dir==1) {
    if (x >= sz || x <= 0 || y >= sz || y < 0) return -1;
    return (y*(sz-1) + (x-1)) + zoff;
  } else if (dir==2) {
    if (x >= sz || x < 0 || y >= sz-1 || y < 0) return -1;
    return (y*sz + x + sz*(sz-1)) + zoff;
  } else if (dir==3) {
    if (x >= sz || x < 0 || y >= sz || y <= 0) return -1;
    return ((y-1)*sz + x + sz*(sz-1)) + zoff;
  } else
    return -1;
}

static int getdoor(unsigned char* doors, int sz, int x, int y, int z, int dir)
{
  int pos;
  if (x==-1 && y==0)
    return (dir==0 ? 1 : 0);
  else if (x==sz && y==sz-1)
    return (dir==1 ? 1 : 0);
  else {
    pos = doorbitpos(sz, x, y, z, dir);
    if (pos == -1)
      return -1;
    else
      return (doors[pos/8]&(1<<(pos%8)) ? 1 : 0);
  }
}

static void setdoor(unsigned char* doors, int sz, int x, int y, int z, int dir, int bit)
{
  int pos;
  pos = doorbitpos(sz, x, y, z, dir);
  if (pos != -1) {
    if (bit)
      doors[pos/8] |= ((unsigned char)1<<(pos%8));
    else
      doors[pos/8] &= ~((unsigned char)1<<(pos%8));
  }
}

static int getindex(int* coord, const game_params *params)
{
  int sz = params->size;
  if (params->style == Basic) {
    return (coord[0]==-1 ? 0 : coord[0]==sz ? sz*sz+1 : coord[1]*sz + coord[0] + 1);
  } else if (params->style == Tandem) {
    int tmp;
    int p1 = (coord[0]==-1 ? 0 : coord[0]==sz ? sz*sz+1 : coord[1]*sz + coord[0] + 1);
    int p2 = (coord[2]==-1 ? 0 : coord[2]==sz ? sz*sz+1 : coord[3]*sz + coord[2] + 1);
    if (p1 > p2) tmp=p1, p1=p2, p2=tmp;
    return (p2==0 ? 0 : p2*(p2-1)/2 + p1 + 1);
  } else if (params->style == ThreeD) {
    return (coord[0]==-1 ? 0 : coord[0]==sz ? sz*sz*sz+1 : coord[2]*sz*sz + coord[1]*sz + coord[0] + 1);
  } else if (params->style == Floors) {
    return (coord[0]==-1 ? 0 : coord[0]==sz ? sz*sz*params->floors+1 : coord[2]*sz*sz + coord[1]*sz + coord[0] + 1);
  } else if (params->style == Keys) {
    int ls = 1 << params->keys;
    return (coord[0]==-1 ? coord[2] : coord[0]==sz ? sz*sz*ls + ls + coord[2] : coord[1]*sz*ls + coord[0]*ls + coord[2] + ls);
  } else if (params->style == Levers) {
    int ls = 1 << params->levers;
    return (coord[0]==-1 ? coord[2] : coord[0]==sz ? sz*sz*ls + ls + coord[2] : coord[1]*sz*ls + coord[0]*ls + coord[2] + ls);
  } else if (params->style == Combo) {
    int ls = 1 << (params->keys + params->levers);
    int fl = params->floors;
    return (coord[0]==-1 ? coord[3] : coord[0]==sz ? sz*sz*fl*ls + ls + coord[3] : (coord[2]*sz*sz + coord[1]*sz + coord[0] + 1)*ls + coord[3]);
  } else
    return -1;
}

static int numindex(const game_params *params)
{
  if (params->style == Basic) {
    return params->size * params->size + 2;
  } else if (params->style == Tandem) {
    int sz2 = params->size * params->size + 2;
    return (sz2*sz2-sz2)/2+2;
  } else if (params->style == ThreeD) {
    return params->size * params->size * params->size + 2;
  } else if (params->style == Floors) {
    return params->size * params->size * params->floors + 2;
  } else if (params->style == Keys) {
    int ls = 1 << params->keys;
    return (params->size * params->size + 2) * ls;
  } else if (params->style == Levers) {
    int ls = 1 << params->levers;
    return (params->size * params->size + 2) * ls;
  } else if (params->style == Combo) {
    int ls = 1 << (params->keys + params->levers);
    return (params->size * params->size * params->floors + 2) * ls;
  } else
    return -1;
}

static int numcoord(const game_params *params)
{
  if (params->style == Basic) {
    return 2;
  } else if (params->style == Tandem) {
    return 4;
  } else if (params->style == ThreeD) {
    return 3;
  } else if (params->style == Floors) {
    return 3;
  } else if (params->style == Keys) {
    return 3;
  } else if (params->style == Levers) {
    return 3;
  } else if (params->style == Combo) {
    return 4;
  } else
    return -1;
}

static int numdoors(const game_params *params)
{
  if (params->style == Basic) {
    return 4;
  } else if (params->style == Tandem) {
    return 8;
  } else if (params->style == ThreeD) {
    return 6;
  } else if (params->style == Floors) {
    return 4 + params->floors - 1;
  } else if (params->style == Keys) {
    return 4 * (params->keys + 1) + params->keys;
  } else if (params->style == Levers) {
    return 4 * (params->levers + 1) + params->levers;
  } else if (params->style == Combo) {
    return 6 + (4 + 1) * (params->keys + params->levers);
  } else
    return -1;

}

/* 
   Directions:
   All styles: First four directions right, left, down, up.
   Tandem: First ball (upper left) dirs first, then second (lower right).
   3d: higher, lower.
   floors: to each floor, except to the current floor.
   levers & keys: all dirs with each color, plus switch lever/take key of each color
   combo: all dirs with each color, plus up/down, switch levers, and take keys
 */

static int getnearbyindex(const game_params *params, int* coord, int dir, int* recdir)
{
  int rescoord[MAXCOORD];
  int delta, cind, ok, tmp;
  int sz = params->size;
  if (params->style == Basic) {
    rescoord[0] = coord[0];
    rescoord[1] = coord[1];
    cind = (dir&2)>>1;
    delta = (dir&1 ? -1 : 1);
    rescoord[cind] += delta;
    if ((rescoord[cind] <= -1 || rescoord[cind] >= sz || rescoord[cind^1] <= -1 || rescoord[cind^1] >= sz) &&
        !(cind == 0 && ((rescoord[0] == -1 && rescoord[1] == 0) ||
                        (rescoord[0] == sz && rescoord[1] == sz-1)))) {
      *recdir = -1;
      return -1;
    }
    *recdir = dir^1;
    return (rescoord[0]==-1 ? 0 : rescoord[0]==sz ? sz*sz+1 : rescoord[1]*sz + rescoord[0] + 1);
  } else if (params->style == Tandem) {
    int p1 = (coord[0]==-1 ? 0 : coord[0]==sz ? sz*sz+1 : coord[1]*sz + coord[0] + 1);
    int p2 = (coord[2]==-1 ? 0 : coord[2]==sz ? sz*sz+1 : coord[3]*sz + coord[2] + 1);
    if (p1 > p2)
      rescoord[0]=coord[2], rescoord[1]=coord[3], rescoord[2]=coord[0], rescoord[3]=coord[1];
    else
      rescoord[0]=coord[0], rescoord[1]=coord[1], rescoord[2]=coord[2], rescoord[3]=coord[3];
    cind = (dir&6)>>1;
    delta = (dir&1 ? -1 : 1);
    rescoord[cind] += delta;
    if (rescoord[cind] <= -1 || rescoord[cind] >= sz || rescoord[cind^1] <= -1 || rescoord[cind^1] >= sz)
      ok = (rescoord[cind] == -1 && (cind&1) == 0 && rescoord[cind|1] == 0) ||
        (rescoord[cind] == sz && (cind&1) == 0 && rescoord[cind|1] == sz-1);
    else
      ok = (rescoord[0] != rescoord[2] || rescoord[1] != rescoord[3]);
    if (!ok) {
      *recdir = -1;
      return -1;
    } else {
      p1 = (rescoord[0]==-1 ? 0 : rescoord[0]==sz ? sz*sz+1 : rescoord[1]*sz + rescoord[0] + 1);
      p2 = (rescoord[2]==-1 ? 0 : rescoord[2]==sz ? sz*sz+1 : rescoord[3]*sz + rescoord[2] + 1);
      *recdir = (p1 > p2 ? dir^5 : dir^1);    
      if (p1 > p2) tmp=p1, p1=p2, p2=tmp;
      return (p2==0 ? 0 : p2*(p2-1)/2 + p1 + 1);
    }
  } else if (params->style == ThreeD) {
    rescoord[0] = coord[0];
    rescoord[1] = coord[1];
    rescoord[2] = coord[2];
    cind = (dir&6)>>1;
    delta = (dir&1 ? -1 : 1);
    rescoord[cind] += delta;
    if ((rescoord[cind] <= -1 || rescoord[cind] >= sz || rescoord[0] <= -1 || rescoord[0] >= sz) &&
        !(cind == 0 && ((rescoord[0] == -1 && rescoord[1] == 0 && rescoord[2] == 0) ||
                        (rescoord[0] == sz && rescoord[1] == sz-1 && rescoord[2] == sz-1)))) {
      *recdir = -1;
      return -1;
    }
    *recdir = dir^1;
    return (rescoord[0]==-1 ? 0 : rescoord[0]==sz ? sz*sz*sz+1 : rescoord[2]*sz*sz + rescoord[1]*sz + rescoord[0] + 1);
  } else if (params->style == Floors) {
    rescoord[0] = coord[0];
    rescoord[1] = coord[1];
    rescoord[2] = coord[2];
    if (dir < 4) {
      cind = (dir&2)>>1;
      delta = (dir&1 ? -1 : 1);
      rescoord[cind] += delta;
      if ((rescoord[0] <= -1 || rescoord[0] >= sz || rescoord[1] <= -1 || rescoord[1] >= sz) &&
          !(cind == 0 && ((rescoord[0] == -1 && rescoord[1] == 0 && rescoord[2] == 0) ||
                          (rescoord[0] == sz && rescoord[1] == sz-1 && rescoord[2] == params->floors-1)))) {
        *recdir = -1;
        return -1;
      }
      *recdir = dir^1;
      return (rescoord[0]==-1 ? 0 : rescoord[0]==sz ? sz*sz*params->floors+1 : rescoord[2]*sz*sz + rescoord[1]*sz + rescoord[0] + 1);
    } else {
      if (rescoord[2] <= -1 || rescoord[2] >= params->floors || rescoord[0] <= -1 || rescoord[0] >= sz) {
        *recdir = -1;
        return -1;
      }
      rescoord[2] = (dir-4 < coord[2] ? dir-4 : dir-3);
      *recdir = (dir-4 < coord[2] ? coord[2] - 1 : coord[2]) + 4;
      return rescoord[2]*sz*sz + rescoord[1]*sz + rescoord[0] + 1;
    }
  } else if (params->style == Keys) {
    int ls = 1 << params->keys;
    rescoord[0] = coord[0];
    rescoord[1] = coord[1];
    rescoord[2] = coord[2];
    if (dir < 4*params->keys + 4) {
      cind = (dir&2)>>1;
      delta = (dir&1 ? -1 : 1);
      rescoord[cind] += delta;
      if ((rescoord[cind] <= -1 || rescoord[cind] >= sz || rescoord[cind^1] <= -1 || rescoord[cind^1] >= sz) &&
          !(cind == 0 && ((rescoord[0] == -1 && rescoord[1] == 0) ||
                          (rescoord[0] == sz && rescoord[1] == sz-1)))) {
        *recdir = -1;
        return -1;
      }
      if (dir >= 4 && !(coord[2] & (1<<(dir/4-1)))) {
        *recdir = -1;
        return -1;
      }
      *recdir = dir^1;
      return (rescoord[0]==-1 ? rescoord[2] : rescoord[0]==sz ? sz*sz*ls + ls + rescoord[2] : rescoord[1]*sz*ls + rescoord[0]*ls + rescoord[2] + ls);
    } else {
      if (rescoord[2] <= -1 || rescoord[2] >= ls || rescoord[0] <= -1 || rescoord[0] >= sz) {
        *recdir = -1;
        return -1;
      }
      rescoord[2] = coord[2] ^ (1<<(dir - 4*params->keys - 4));
      *recdir = dir;
      return rescoord[1]*sz*ls + rescoord[0]*ls + rescoord[2] + ls;
    }
  } else if (params->style == Levers) {
    int ls = 1 << params->levers;
    rescoord[0] = coord[0];
    rescoord[1] = coord[1];
    rescoord[2] = coord[2];
    if (dir < 4*params->levers + 4) {
      cind = (dir&2)>>1;
      delta = (dir&1 ? -1 : 1);
      rescoord[cind] += delta;
      if ((rescoord[cind] <= -1 || rescoord[cind] >= sz || rescoord[cind^1] <= -1 || rescoord[cind^1] >= sz) &&
          !(cind == 0 && ((rescoord[0] == -1 && rescoord[1] == 0) ||
                          (rescoord[0] == sz && rescoord[1] == sz-1)))) {
        *recdir = -1;
        return -1;
      }
      *recdir = dir^1;
      return (rescoord[0]==-1 ? rescoord[2] : rescoord[0]==sz ? sz*sz*ls + ls + rescoord[2] : rescoord[1]*sz*ls + rescoord[0]*ls + rescoord[2] + ls);
    } else {
      if (rescoord[2] <= -1 || rescoord[2] >= ls || rescoord[0] <= -1 || rescoord[0] >= sz) {
        *recdir = -1;
        return -1;
      }
      rescoord[2] = coord[2] ^ (1<<(dir - 4*params->levers - 4));
      *recdir = dir;
      return rescoord[1]*sz*ls + rescoord[0]*ls + rescoord[2] + ls;
    }
  } else if (params->style == Combo) {
    int ls = 1 << (params->keys + params->levers);
    rescoord[0] = coord[0];
    rescoord[1] = coord[1];
    rescoord[2] = coord[2];
    rescoord[3] = coord[3];
    if (dir < 4*(params->keys + params->levers + 1) + 2) {
      if (dir < 4*(params->keys + params->levers + 1)) {
        cind = (dir&2)>>1;
        delta = (dir&1 ? -1 : 1);
        rescoord[cind] += delta;
        if ((rescoord[cind] <= -1 || rescoord[cind] >= sz || rescoord[0] <= -1 || rescoord[0] >= sz) &&
            !(cind == 0 && ((rescoord[0] == -1 && rescoord[1] == 0 && rescoord[2] == 0) ||
                            (rescoord[0] == sz && rescoord[1] == sz-1 && rescoord[2] == params->floors-1)))) {
          *recdir = -1;
          return -1;
        }
        if (dir >= 4 + 4*params->levers && !(coord[3] & (1<<(dir/4-1)))) {
          *recdir = -1;
          return -1;
        }
      } else {
        cind = 2;
        delta = (dir&1 ? -1 : 1);
        rescoord[cind] += delta;
        if (rescoord[cind] <= -1 || rescoord[cind] >= params->floors || rescoord[0] <= -1 || rescoord[0] >= sz) {
          *recdir = -1;
          return -1;
        }
      }
      *recdir = dir^1;
      return (rescoord[0]==-1 ? rescoord[3] : rescoord[0]==sz ? sz*sz*params->floors*ls + ls + rescoord[3] : (rescoord[2]*sz*sz + rescoord[1]*sz + rescoord[0] + 1)*ls + rescoord[3]);
    } else {
      if (rescoord[3] <= -1 || rescoord[3] >= ls || rescoord[0] <= -1 || rescoord[0] >= sz) {
        *recdir = -1;
        return -1;
      }
      rescoord[3] = coord[3] ^ (1<<(dir - 4*(params->keys + params->levers + 1) - 2));
      *recdir = dir;
      return  (rescoord[2]*sz*sz + rescoord[1]*sz + rescoord[0] + 1)*ls + rescoord[3];
    }
  } else {
    *recdir = -1;
    return -1;
  }
}

static int firstmirrorstate(const game_params *params, int ind, int dir)
{
  if (params->style == Basic || params->style == ThreeD || params->style == Floors)
    return ind;
  else if (params->style == Tandem) {
    int sz = params->size;
    int p2 = (ind == 0 ? 0 : floor(sqrt(2*ind - 1.75) + 0.5));
    int p1 = (ind == 0 ? 0 : ind - p2*(p2-1)/2 - 1);
    if (p1 >= 2 && p2 <= sz*sz-1)
      return ind;
    if ((p1 == 0 && dir == 0) || (p2 == 0 && dir == 4))
      return 0;
    if ((p1 == 1 && dir == 1) || (p2 == 1 && dir == 5))
      return 1;
    if ((p2 == sz*sz+1 && dir == 5) || (p1 == sz*sz+1 && dir == 1))
      return p2*(p2-1)/2 + 1;
    if ((p2 == sz*sz && dir == 4) || (p1 == sz*sz && dir == 0))
      return sz*sz*(sz*sz-1)/2 + 1;
    if (p2 == sz*sz+1)
      return p1*(p1-1)/2 + 1;
    else
      return ind;
  } else if (params->style == Keys) {
    int mask = (1 << params->keys) - 1;
    if (dir >= 4 && dir < 4*params->keys + 4)
      mask &= ~(1 << (dir/4-1));
    return ind & ~mask;
  } else if (params->style == Levers) {
    int mask = (1 << params->levers) - 1;
    if (dir >= 4 && dir < 4*params->levers + 4)
      mask &= ~(1 << (dir/4-1));
    return ind & ~mask;
  } else if (params->style == Combo) {
    int mask = (1 << (params->keys + params->levers)) - 1;
    if (dir >= 4 && dir < 4*(params->keys + params->levers + 1))
      mask &= ~(1 << (dir/4-1));
    return ind & ~mask;
  } else
    return ind;
}

static int getmirrordoors(const game_params *params, int* coord, int dir, int** indices, int** dirs)
{
  static int* mirrorstatevec = 0;
  static int* mirrordoorvec = 0;
  static int lastvecsize = 0;
  /* For a coordinate and direction, list all doors from states that must be the same */
  int sz = params->size;
  int i, k, vecsize;
  if (params->style == Tandem) {
    int tmp;
    int p1 = (coord[0]==-1 ? 0 : coord[0]==sz ? sz*sz+1 : coord[1]*sz + coord[0] + 1);
    int p2 = (coord[2]==-1 ? 0 : coord[2]==sz ? sz*sz+1 : coord[3]*sz + coord[2] + 1);
    vecsize = sz*sz+2;
    if (vecsize > lastvecsize) {
      if (mirrorstatevec)
        sfree(mirrorstatevec);
      if (mirrordoorvec)
        sfree(mirrordoorvec);
      lastvecsize = vecsize;
      mirrorstatevec = snewn(vecsize, int);
      mirrordoorvec = snewn(vecsize, int);
    }
    *indices = mirrorstatevec;
    *dirs = mirrordoorvec;
    if (p1 > p2) tmp=p1, p1=p2, p2=tmp;
    if ((p1 == 0 && dir == 0) || (p2 == 0 && dir == 4)) {
      k = 0;
      for (i=0; i<sz*sz+2; i++)
        if (i != 1) {
          mirrorstatevec[k] = (i==0 ? 0 : i*(i-1)/2 + 1);
          mirrordoorvec[k] = 0;
          k++;
        }
      mirrorstatevec[k] = 0;
      mirrordoorvec[k] = 4;
      k++;
      return k;
    }
    if ((p1 == 1 && dir == 1) || (p2 == 1 && dir == 5)) {
      k = 0;
      mirrorstatevec[k] = 1;
      mirrordoorvec[k] = 5;
      k++;
      for (i=2; i<sz*sz+2; i++) {
        mirrorstatevec[k] = (i*(i-1)/2 + 2);
        mirrordoorvec[k] = 1;
        k++;
      }
      return k;
    }
    if ((p2 == sz*sz+1 && dir == 5) || (p1 == sz*sz+1 && dir == 1)) {
      k = 0;
      for (i=0; i<sz*sz+2; i++)
        if (i != sz*sz) {
          mirrorstatevec[k] = (sz*sz*(sz*sz+1)/2 + i + 1);
          mirrordoorvec[k] = 5;
          k++;
        }
      mirrorstatevec[k] = sz*sz*(sz*sz+1)/2 + sz*sz + 2;
      mirrordoorvec[k] = 1;
      k++;
      return k;
    }
    if ((p2 == sz*sz && dir == 4) || (p1 == sz*sz && dir == 0)) {
      k = 0;
      mirrorstatevec[k] = ((sz*sz+1)*(sz*sz)/2 + sz*sz + 1);
      mirrordoorvec[k] = 0;
      k++;
      for (i=0; i<sz*sz; i++) {
        mirrorstatevec[k] = (sz*sz*(sz*sz-1)/2 + i + 1);
        mirrordoorvec[k] = 4;
        k++;
      }
      return k;
    }
    if (p1 == 0) {
      mirrorstatevec[0] = (p2*(p2-1)/2 + p1 + 1);
      mirrordoorvec[0] = dir;
      mirrorstatevec[1] = (sz*sz*(sz*sz+1)/2 + p2 + 1);
      mirrordoorvec[1] = dir^4;
      return 2;
    } else if (p2 == sz*sz+1) {
      mirrorstatevec[1] = (p2*(p2-1)/2 + p1 + 1);
      mirrordoorvec[1] = dir;
      mirrorstatevec[0] = (p1*(p1-1)/2 + 1);
      mirrordoorvec[0] = dir^4;
      return 2;
    } else {
      mirrorstatevec[0] = (p2*(p2-1)/2 + p1 + 1);
      mirrordoorvec[0] = dir;
      return 1;
    }
  } else if (params->style == Keys) {
    int p = (coord[0]==-1 ? 0 : coord[0]==sz ? sz*sz+1 : coord[1]*sz + coord[0] + 1);
    int ls = 1 << params->keys;
    int lbit;
    vecsize = ls;
    if (vecsize > lastvecsize) {
      if (mirrorstatevec)
        sfree(mirrorstatevec);
      if (mirrordoorvec)
        sfree(mirrordoorvec);
      lastvecsize = vecsize;
      mirrorstatevec = snewn(vecsize, int);
      mirrordoorvec = snewn(vecsize, int);
    }
    *indices = mirrorstatevec;
    *dirs = mirrordoorvec;
    if (dir >= 4 && dir < 4*params->keys + 4) {
      lbit = 1 << (dir/4-1);
      k = 0;
      for (i=0; i<ls; i++)
        if ((coord[2]&lbit) == (i&lbit)) {
          mirrorstatevec[k] = p*ls + i;
          mirrordoorvec[k] = dir;
          k++;
        }
      return k;
    } else {
      k = 0;
      for (i=0; i<ls; i++) {
        mirrorstatevec[k] = p*ls + i;
        mirrordoorvec[k] = dir;
        k++;
      }
      return k;
    }
  } else if (params->style == Levers) {
    int p = (coord[0]==-1 ? 0 : coord[0]==sz ? sz*sz+1 : coord[1]*sz + coord[0] + 1);
    int ls = 1 << params->levers;
    int lbit;
    vecsize = ls;
    if (vecsize > lastvecsize) {
      if (mirrorstatevec)
        sfree(mirrorstatevec);
      if (mirrordoorvec)
        sfree(mirrordoorvec);
      lastvecsize = vecsize;
      mirrorstatevec = snewn(vecsize, int);
      mirrordoorvec = snewn(vecsize, int);
    }
    *indices = mirrorstatevec;
    *dirs = mirrordoorvec;
    if (dir >= 4 && dir < 4*params->levers + 4) {
      lbit = 1 << (dir/4-1);
      k = 0;
      for (i=0; i<ls; i++)
        if ((coord[2]&lbit) == (i&lbit)) {
          mirrorstatevec[k] = p*ls + i;
          mirrordoorvec[k] = dir;
          k++;
        }
      return k;
    } else {
      k = 0;
      for (i=0; i<ls; i++) {
        mirrorstatevec[k] = p*ls + i;
        mirrordoorvec[k] = dir;
        k++;
      }
      return k;
    }
  } else if (params->style == Combo) {
    int p = (coord[0]==-1 ? 0 : coord[0]==sz ? sz*sz*params->floors+1 : coord[2]*sz*sz + coord[1]*sz + coord[0] + 1);
    int ls = 1 << (params->keys + params->levers);
    int lbit;
    vecsize = ls;
    if (vecsize > lastvecsize) {
      if (mirrorstatevec)
        sfree(mirrorstatevec);
      if (mirrordoorvec)
        sfree(mirrordoorvec);
      lastvecsize = vecsize;
      mirrorstatevec = snewn(vecsize, int);
      mirrordoorvec = snewn(vecsize, int);
    }
    *indices = mirrorstatevec;
    *dirs = mirrordoorvec;
    if (dir >= 4 && dir < 4*(params->keys + params->levers + 1)) {
      lbit = 1 << (dir/4-1);
      k = 0;
      for (i=0; i<ls; i++)
        if ((coord[3]&lbit) == (i&lbit)) {
          mirrorstatevec[k] = p*ls + i;
          mirrordoorvec[k] = dir;
          k++;
        }
      return k;
    } else {
      k = 0;
      for (i=0; i<ls; i++) {
        mirrorstatevec[k] = p*ls + i;
        mirrordoorvec[k] = dir;
        k++;
      }
      return k;
    }
  } else {
    if (lastvecsize == 0) {
      lastvecsize = 1;
      mirrorstatevec = snewn(1, int);
      mirrordoorvec = snewn(1, int);
    }
    *indices = mirrorstatevec;
    *dirs = mirrordoorvec;
    mirrorstatevec[0] = getindex(coord, params);
    mirrordoorvec[0] = dir;
    return 1;
  }
}

static int getcontradoors(const game_params *params, int* coord, int dir, int** indices, int** dirs)
{
  static int* contrastatevec = 0;
  static int* contradoorvec = 0;
  static int lastvecsize = 0;
  int i, k, vecsize;
  int s = getindex(coord, params);
  /* For a coordinate and direction, list all doors from states that must not be open at the same time */
  if (params->style == Floors) {
    vecsize = params->floors*2;
    if (vecsize > lastvecsize) {
      if (contrastatevec)
        sfree(contrastatevec);
      if (contradoorvec)
        sfree(contradoorvec);
      lastvecsize = vecsize;
      contrastatevec = snewn(vecsize, int);
      contradoorvec = snewn(vecsize, int);
    }
    if (dir < 4)
      return 0;
    else {
      *indices = contrastatevec;
      *dirs = contradoorvec;
      k = 0;
      for (i=0; i<params->floors-1; i++)
        if (i + 4 != dir) {
          contrastatevec[k] = s;
          contradoorvec[k] = i + 4;
          k++;
        }
      s = getnearbyindex(params, coord, dir, &dir);
      for (i=0; i<params->floors-1; i++)
        if (i + 4 != dir) {
          contrastatevec[k] = s;
          contradoorvec[k] = i + 4;
          k++;
        }
      return k;
    }
  } else if (params->style == Keys) {
    int sz2 = params->size*params->size;
    int ls = 1 << params->keys;
    vecsize = sz2 + params->keys;
    if (vecsize > lastvecsize) {
      if (contrastatevec)
        sfree(contrastatevec);
      if (contradoorvec)
        sfree(contradoorvec);
      lastvecsize = vecsize;
      contrastatevec = snewn(vecsize, int);
      contradoorvec = snewn(vecsize, int);
    }
    *indices = contrastatevec;
    *dirs = contradoorvec;
    if (dir < 4*params->keys + 4) {
      k = 0;
      for (i=0; i<params->keys+1; i++)
        if (4*i != (dir & ~3)) {
          contrastatevec[k] = (i==0 || s & (1<<(i-1)) ? s : s ^ (ls-1));
          contradoorvec[k] = 4*i + (dir & 3);
          k++;
        }
      return k;
    } else {
      k = 0;
      for (i=0; i<params->keys; i++)
        if (i + 4*params->keys + 4 != dir) {
          contrastatevec[k] = s;
          contradoorvec[k] = i + 4*params->keys + 4;
          k++;
        }
      for (i=(s&(ls-1))+ls; i<(sz2+1)*ls; i+=ls)
        if (i != s) {
          contrastatevec[k] = i;
          contradoorvec[k] = dir;
          k++;
        }
      return k;
    }
  } else if (params->style == Levers) {
    int ls = 1 << params->levers;
    vecsize = 2*params->levers;
    if (vecsize > lastvecsize) {
      if (contrastatevec)
        sfree(contrastatevec);
      if (contradoorvec)
        sfree(contradoorvec);
      lastvecsize = vecsize;
      contrastatevec = snewn(vecsize, int);
      contradoorvec = snewn(vecsize, int);
    }
    *indices = contrastatevec;
    *dirs = contradoorvec;
    if (dir < 4*params->levers + 4) {
      k = 0;
      for (i=0; i<params->levers+1; i++)
        if (4*i != (dir & ~3)) {
          contrastatevec[k] = s;
          contradoorvec[k] = 4*i + (dir & 3);
          k++;
        }
      for (i=1; i<params->levers+1; i++) {
        contrastatevec[k] = s ^ (ls-1);
        contradoorvec[k] = 4*i + (dir & 3);
        k++;
      }
      return k;
    } else {
      k = 0;
      for (i=0; i<params->levers; i++)
        if (i + 4*params->levers + 4 != dir) {
          contrastatevec[k] = s;
          contradoorvec[k] = i + 4*params->levers + 4;
          k++;
        }
      return k;
    }
  } else if (params->style == Combo) {
    int sz2 = params->size*params->size;
    int ls = 1 << (params->keys + params->levers);
    vecsize = sz2*params->floors + 2*(params->keys + params->levers + 1);
    if (vecsize > lastvecsize) {
      if (contrastatevec)
        sfree(contrastatevec);
      if (contradoorvec)
        sfree(contradoorvec);
      lastvecsize = vecsize;
      contrastatevec = snewn(vecsize, int);
      contradoorvec = snewn(vecsize, int);
    }
    *indices = contrastatevec;
    *dirs = contradoorvec;
    if (dir < 4*(params->keys + params->levers + 1)) {
      k = 0;
      i = 0;
      if (4*i != (dir & ~3)) {
        contrastatevec[k] = s;
        contradoorvec[k] = 4*i + (dir & 3);
        k++;
      }
      i++;
      for (; i<params->levers+1; i++) {
        if (4*i != (dir & ~3)) {
          contrastatevec[k] = s;
          contradoorvec[k] = 4*i + (dir & 3);
          k++;
        }
        contrastatevec[k] = s ^ ((1<<params->levers)-1);
        contradoorvec[k] = 4*i + (dir & 3);
        k++;
      }
      for (; i<params->keys+params->levers+1; i++)
        if (4*i != (dir & ~3)) {
          contrastatevec[k] = (s & (1<<(i-1)) ? s : s ^ (((1<<params->keys)-1)<<params->levers));
          contradoorvec[k] = 4*i + (dir & 3);
          k++;
        }
      return k;
    } else {
      k = 0;
      if (dir >= 4*(params->keys + params->levers+1) + 2) {
        for (i=0; i<2; i++)
          if (coord[2] != (i==0 ? params->floors-1 : 0)) {
            contrastatevec[k] = s;
            contradoorvec[k] = i + 4*(params->keys + params->levers + 1);
            k++;
          }
      } else {
        for (i=2; i<(params->keys+params->levers+2); i++) {
          contrastatevec[k] = s + ((dir&1) ? -sz2*ls : sz2*ls);
          contradoorvec[k] = i + 4*(params->keys + params->levers + 1);
          k++;
        }
      }
      for (i=2; i<(params->keys+params->levers+2); i++)
        if (i + 4*(params->keys + params->levers+1) != dir) {
          contrastatevec[k] = s;
          contradoorvec[k] = i + 4*(params->keys + params->levers + 1);
          k++;
        }
      if (dir >= 4*(params->keys + params->levers + 1) + 2 + params->levers)
        for (i=(s&(ls-1))+ls; i<(params->size*params->size*params->floors+1)*ls; i+=ls)
          if (i != s) {
            contrastatevec[k] = i;
            contradoorvec[k] = dir;
            k++;
          }
      return k;
    }
  } else
    return 0;
}

static int binary(float p, random_state *rs)
{
  return (random_bits(rs, 10) < 1024*p);
}

static float uniform(random_state *rs)
{
  return (random_bits(rs, 22) / (float)(1<<22));
}

static int bottleneckscore(SmPowerRoom* rstate, int dir)
{
  return (rstate->dist + 1)*(rstate->trans[dir]->dist + 1);
}



static void floodfillisland(SmPowerRoom* rstate, int dom, int ndoors, SmPowerRoom** pool, int* numpool)
{
  int j;
  int startpool = *numpool;
  rstate->status = Connected;
  rstate->domain = dom;
  pool[*numpool] = rstate;
  *numpool += 1;
  while (startpool < *numpool) {
    rstate = pool[startpool];
    for (j=0; j<ndoors; j++) 
      if (rstate->door[j] == Open && rstate->trans[j]->status == Island) {
        rstate->trans[j]->status = Connected;
        rstate->trans[j]->domain = dom;
        pool[*numpool] = rstate->trans[j];
        *numpool += 1;
      }
    startpool++;
  }
}

static int canopendoor(const game_params *params, SmPowerRoom** states, SmPowerRoom* rstate, int dir)
{
  /* This is called to see if we can open an Unset door. What can prevent this is
     that we have bottlenecks and this would open up between two different domains
     (directly or in any mirror). */
  int i, n, *ms, *md;
  n = getmirrordoors(params, rstate->coord, dir, &ms, &md);
  for (i=0; i<n; i++)
    if (states[ms[i]]->status == Connected &&
        states[ms[i]]->trans[md[i]]->status == Connected &&
        states[ms[i]]->domain != states[ms[i]]->trans[md[i]]->domain)
      return 0;
  return 1;
}

static void opendoor(const game_params *params, SmPowerRoom** states, SmPowerRoom* rstate, int dir, int ndoors, SmPowerRoom** pool, int* numpool)
{
  int i, j, c1, c2, nm, nc, *ms, *md, *cs, *cd;
  nm = getmirrordoors(params, rstate->coord, dir, &ms, &md);
  for (i=0; i<nm; i++) {
    states[ms[i]]->door[md[i]] = Open;
    states[ms[i]]->trans[md[i]]->door[states[ms[i]]->recdir[md[i]]] = Open;
    c1 = (states[ms[i]]->status == Complete || states[ms[i]]->status == Connected);
    c2 = (states[ms[i]]->trans[md[i]]->status == Complete || states[ms[i]]->trans[md[i]]->status == Connected);
    /* Fix the mirror states. */
    if (c1 && c2) {
    } else if (c1 || c2) {
      /* One is connected, connect and domain-tag the other */
      if (c1) {
        rstate = states[ms[i]];
        dir = md[i];
      } else {
        int tmp = md[i];
        dir = states[ms[i]]->recdir[tmp];
        rstate = states[ms[i]]->trans[tmp];
      }
      if (rstate->trans[dir]->status == Unallocated) {
        rstate->trans[dir]->status = Connected;
        rstate->trans[dir]->domain = rstate->domain;
        pool[*numpool] = rstate->trans[dir];
        *numpool += 1;
      } else if (rstate->trans[dir]->status == Island) {
        /* Flood fill with Connected and domain */
        floodfillisland(rstate->trans[dir], rstate->domain, ndoors, pool, numpool);
      }
    } else {
      /* Make an island */
      states[ms[i]]->status = Island;
      states[ms[i]]->trans[md[i]]->status = Island;
    }
  }

  nc = getcontradoors(params, rstate->coord, dir, &cs, &cd);
  for (j=0; j<nc; j++)
    if (states[cs[j]]->door[cd[j]] != Closed) {
      nm = getmirrordoors(params, states[cs[j]]->coord, cd[j], &ms, &md);
      for (i=0; i<nm; i++) {
        states[ms[i]]->door[md[i]] = Closed;
        states[ms[i]]->trans[md[i]]->door[states[ms[i]]->recdir[md[i]]] = Closed;
      }
    }
}

static void closedoor(const game_params *params, SmPowerRoom** states, SmPowerRoom* rstate, int dir)
{
  int i, n, *ms, *md;
  n = getmirrordoors(params, rstate->coord, dir, &ms, &md);
  for (i=0; i<n; i++) {
    states[ms[i]]->door[md[i]] = Closed;
    states[ms[i]]->trans[md[i]]->door[states[ms[i]]->recdir[md[i]]] = Closed;
  }
}

static int canbreakupdoor(const game_params *params, SmPowerRoom** states, SmPowerRoom* rstate, int dir, int* dom)
{
  int i, j, n, c1, c2, ok, nm, nc, *ms, *md, *cs, *cd, *numdom;
  if (rstate->door[dir] != Closed)
    return 0;

  nc = getcontradoors(params, rstate->coord, dir, &cs, &cd);
  for (j=0; j<nc; j++)
    if (states[cs[j]]->door[cd[j]] == Open)
      return 0;
    else if (states[cs[j]]->door[cd[j]] == Unset)
      break;

  numdom = snewn(MAXDOMAIN+1, int);
  for (i=0; i<MAXDOMAIN+1; i++)
    numdom[i] = 0;
  ok = 0;
  nm = getmirrordoors(params, rstate->coord, dir, &ms, &md);
  for (i=0; i<nm; i++) {
    c1 = (states[ms[i]]->status == Complete || states[ms[i]]->status == Connected);
    c2 = (states[ms[i]]->trans[md[i]]->status == Complete || states[ms[i]]->trans[md[i]]->status == Connected);
    if (c1 && c2 && states[ms[i]]->domain != states[ms[i]]->trans[md[i]]->domain) {
      sfree(numdom);
      return 0; /* There should be no transition between different domains */
    }
    if (c1 ^ c2) { /* There exist a transition between conn and non-conn */
      numdom[c1 ? states[ms[i]]->domain : states[ms[i]]->trans[md[i]]->domain]++;
      ok = 1;
    }
  }
  if (ok) {
    n = 0;
    for (i=1; i<MAXDOMAIN+1; i++)
      if (numdom[i] > numdom[n])
        n = i;
    if (n>0)
      *dom = n;
    else
      ok = 0;
  }
  sfree(numdom);

  return ok;
}

static int canbreakupdoor_conn(const game_params *params, SmPowerRoom** states, SmPowerRoom* rstate, int dir, int* score, int* dom1, int* dom2)
{
  int i, j, nm, nc, *ms, *md, *cs, *cd;
  int d1, d2, c1, c2;
  int minscore, tmpscore;
  if (rstate->door[dir] != Closed) {
    *score = -1;
    return 0;
  }

  nc = getcontradoors(params, rstate->coord, dir, &cs, &cd);
  for (j=0; j<nc; j++)
    if (states[cs[j]]->door[cd[j]] == Open) {
      *score = -1;
      return 0;
    } else if (states[cs[j]]->door[cd[j]] == Unset)
      break;

  d1 = d2 = -1;
  minscore = -1;
  nm = getmirrordoors(params, rstate->coord, dir, &ms, &md);
  for (i=0; i<nm; i++) {
    c1 = (states[ms[i]]->status == Complete || states[ms[i]]->status == Connected);
    c2 = (states[ms[i]]->trans[md[i]]->status == Complete || states[ms[i]]->trans[md[i]]->status == Connected);
    if (c1 != c2) { /* do not create new reachable states */
      *score = -1;
      return 0;
    } else if (c1 && c2 && states[ms[i]]->domain != states[ms[i]]->trans[md[i]]->domain) {
      if (d1 == -1) {
        d1 = states[ms[i]]->domain;
        d2 = states[ms[i]]->trans[md[i]]->domain;
        minscore = bottleneckscore(states[ms[i]], md[i]);
      } else if ((states[ms[i]]->domain != d1 && states[ms[i]]->domain != d2) ||    
                 (states[ms[i]]->trans[md[i]]->domain != d1 && states[ms[i]]->trans[md[i]]->domain != d2)) {  /* and no other domains involved */
        *score = -1;
        return 0;
      } else {
        tmpscore = bottleneckscore(states[ms[i]], md[i]);
        if (tmpscore < minscore)
          minscore = tmpscore;
      }
    }
  }
  *dom1 = (d1 < d2 ? d1 : d2);
  *dom2 = (d1 < d2 ? d2 : d1);
  *score = minscore;
  return (d1<=0 || d2<=0 ? 0 : 1);
}

static int calcdistance(SmPowerRoom** pool, int npool, int ndoors)
{
  int start, end, max, j;
  start = 0;
  end = npool;
  for (j=0; j<npool; j++)
    pool[j]->dist = 0;
  max = 0;
  while (start < end) {
    for (j=0; j<ndoors; j++) {
      if (pool[start]->door[j] == Open && pool[start]->trans[j]->dist == -1) {
        pool[start]->trans[j]->dist = pool[start]->dist + 1;
        if (pool[start]->trans[j]->dist > max) max = pool[start]->trans[j]->dist;
        pool[end++] = pool[start]->trans[j];
      }
    }
    start++;
  }
  return max;
}

static void free_powerstates(SmPowerRoom** states, const game_params *params)
{
  int i, num;
  num = numindex(params);
  for (i=0; i<num; i++) {
    sfree(states[i]->coord);
    sfree(states[i]->trans);
    sfree(states[i]->door);
    sfree(states[i]->recdir);
    sfree(states[i]);
  }
  sfree(states);
}

static float doorprobability(const game_params *params, SmPowerRoom** states, SmPowerRoom* rstate, int dir, int breakup)
{
  float probdoor;
  if ((params->style == Tandem ? rstate->coord[2] : rstate->coord[0]) == -1) { /* First state in all styles */
    if (dir == 0)
      return 1.0;
    else
      return 0.0;
  } else if (rstate->coord[0] == params->size) { /* Goal state in all styles */
    if (dir == 1)
      return 1.0;
    else
      return 0.0;
  } else if (params->style == Floors) {
    probdoor = 0.3;
    if (dir >= 4)
      return probdoor / params->floors;
    else
      return probdoor;
  } else if (params->style == Keys) {
    probdoor = 0.4;
    if (dir >= 4*(params->keys+1)) {
      if (params->difficult)
        return (rstate->domain == 2 ? 0.002 : 0.000001)/params->keys;
/*        return (breakup ? (rstate->domain == 2 ? 0.2 : 0.001)/params->keys : 0.0);*/
/*        return (rstate->domain == 2 ? 0.2 : 0.05)/params->keys; */
      else
        return 0.04 / params->keys;
    } else if (dir >= 4) {
      if (params->difficult)
        return (rstate->domain == 2 ? 0.000001 : 0.08)/params->keys;
/*        return (rstate->domain == 2 ? (breakup ? 0.001 : 0.0) : 0.08)/params->keys;*/
/*        return (rstate->domain == 2 ? 0.05 : 0.2)/params->keys; */
      else
        return 0.1 / params->keys;
    } else {
      if (params->difficult)
        return (rstate->domain == 2 ? 0.16 : 0.01);
      else
        return 0.08;
    }
  } else if (params->style == Levers) {
    probdoor = 0.4;
    if (dir >= 4*(params->levers+1))
      return 0.1 * probdoor / params->levers;
    else if (dir >= 4)
      return 0.5 * probdoor / params->levers;
    else
      return 0.5 * probdoor;
  } else if (params->style == Combo) {
    probdoor = 0.3;
    if (dir >= 4*(params->keys+params->levers+1)) {
      if (params->difficult && dir >= 4*(params->keys+params->levers+1) + 2 + params->levers)
        return (rstate->domain == 2 ? 0.15 : 0.03)/params->keys;
      else
        return 0.2 * probdoor / (params->keys+params->levers+2);
    } else if (dir >= 4) {
      if (params->difficult && dir >= 4*(params->levers+1))
        return (rstate->domain == 2 ? 0.03 : 0.15)/params->keys;
      else
        return 0.5 * probdoor / (params->keys+params->levers+2);
    } else
      return 0.5 * probdoor;
  } else {
    probdoor = (params->style == Basic ? 0.4 : params->style == Tandem ? 0.25 : 0.3);
    return probdoor;
  }
}

static void initializestates(const game_params *params, int num, SmPowerRoom**states)
{
  int ind, x1, y1, x2, y2, z1, h;
  int size = params->size;
  if (params->style == Basic) {
    for (ind=0, y1=0; y1<size; y1++)
      for (x1=(y1==0 ? -1 : 0); x1<(y1==size-1 ? size+1 : size); x1++, ind++)
        states[ind]->coord[0] = x1, states[ind]->coord[1] = y1; 
  }
  else if (params->style == Tandem) {
    states[0]->coord[0] = -1, states[0]->coord[1] = 0, states[0]->coord[2] = -1, states[0]->coord[3] = 0; 
    for (ind=1, y2=0; y2<size; y2++)
      for (x2=(y2==0 ? -1 : 0); x2<(y2==size-1 ? size+1 : size); x2++)
        for (y1=0; y1<=y2; y1++)
          for (x1=(y1==0 ? -1 : 0); x1<(y1==y2 ? x2 : size); x1++, ind++)
            states[ind]->coord[0] = x1, states[ind]->coord[1] = y1, states[ind]->coord[2] = x2, states[ind]->coord[3] = y2; 
    states[num-1]->coord[0] = size, states[num-1]->coord[1] = size-1, states[num-1]->coord[2] = size, states[num-1]->coord[3] = size-1; 
  }
  else if (params->style == ThreeD) {
    for (ind=0, z1=0; z1<size; z1++)
      for (y1=0; y1<size; y1++)
        for (x1=((y1==0 && z1==0) ? -1 : 0); x1<((y1==size-1 && z1==size-1) ? size+1 : size); x1++, ind++)
          states[ind]->coord[0] = x1, states[ind]->coord[1] = y1, states[ind]->coord[2] = z1; 
  }
  else if (params->style == Floors) {
    int fl = params->floors;
    for (ind=0, z1=0; z1<fl; z1++)
      for (y1=0; y1<size; y1++)
        for (x1=((y1==0 && z1==0) ? -1 : 0); x1<((y1==size-1 && z1==fl-1) ? size+1 : size); x1++, ind++)
          states[ind]->coord[0] = x1, states[ind]->coord[1] = y1, states[ind]->coord[2] = z1; 
  }
  else if (params->style == Keys) {
    int ls = 1 << params->keys;
    for (ind=0, y1=0; y1<size; y1++)
      for (x1=((y1==0) ? -1 : 0); x1<(y1==size-1 ? size+1 : size); x1++)
        for (h=0; h<ls; h++, ind++)
          states[ind]->coord[0] = x1, states[ind]->coord[1] = y1, states[ind]->coord[2] = h; 
  }
  else if (params->style == Levers) {
    int ls = 1 << params->levers;
    for (ind=0, y1=0; y1<size; y1++)
      for (x1=((y1==0) ? -1 : 0); x1<(y1==size-1 ? size+1 : size); x1++)
        for (h=0; h<ls; h++, ind++)
          states[ind]->coord[0] = x1, states[ind]->coord[1] = y1, states[ind]->coord[2] = h; 
  }
  else if (params->style == Combo) {
    int ls = 1 << (params->keys + params->levers);
    int fl = params->floors;
    for (ind=0, z1=0; z1<fl; z1++)
      for (y1=0; y1<size; y1++)
        for (x1=((y1==0 && z1==0) ? -1 : 0); x1<(y1==size-1 && z1==fl-1 ? size+1 : size); x1++)
          for (h=0; h<ls; h++, ind++)
            states[ind]->coord[0] = x1, states[ind]->coord[1] = y1, states[ind]->coord[2] = z1, states[ind]->coord[3] = h; 
  }
}

/* 
A room has a status and a domain. 
The domain is to separate connected regions and create bottlenecks
between them to make it harder, but also to make sure that we have a
path at all from start to end.
The status can be: Unallocated when the room is not reached at all;
Connected when it is reached by one good domain and put in queue;
Island when connected to nearby rooms but not yet to a good domain;
and Complete when all doors are set and the room has a final domain.
*/
static SmPowerRoom** makepowerstates(const game_params *params, random_state *rs)
{
  int ind, dom, rdir, count, dom1, dom2;
  int bnind, bndir;
  int tmpscore, *maxscore, *maxind, *maxdir;
  int i, j, nn;
  float prob;
  int size = params->size;
  int ncoord = numcoord(params);
  int ndoors = numdoors(params);
  int num = numindex(params);
  int numpool = 0;
  int* pooldir = snewn(num*(ndoors-2), int);
  int* pooldom = snewn(num*(ndoors-2), int);
  float* poolprob = snewn(num*(ndoors-2), float);
  float* domprob = snewn(MAXDOMAIN+1, float);
  SmPowerRoom** pool = snewn(num*(ndoors-2), SmPowerRoom*);
  SmPowerRoom** states = snewn(num, SmPowerRoom*);
  SmPowerRoom* rstate;
  for (ind=0; ind<num; ind++) {
    states[ind] = snew(SmPowerRoom);
    states[ind]->status = Unallocated;
    states[ind]->domain = -1;
    states[ind]->coord = snewn(ncoord, int);
    states[ind]->trans = snewn(ndoors, SmPowerRoom*);
    states[ind]->door = snewn(ndoors, SmPowerDoor);
    states[ind]->recdir = snewn(ndoors, int);
  }

  initializestates(params, num, states);
  for (i=0; i<num; i++)
    for (j=0; j<ndoors; j++) {
      ind = getnearbyindex(params, states[i]->coord, j, &rdir);
      if (ind > -1) {
        states[i]->trans[j] = states[ind];
        states[i]->door[j] = Unset;
        states[i]->recdir[j] = rdir;
      } else {
        states[i]->trans[j] = 0;
        states[i]->door[j] = Impossible;
        states[i]->recdir[j] = -1;
      }
    }
  /* Put first and last room in pool */
  states[0]->domain = 1;
  states[0]->status = Connected;
  pool[numpool++] = states[0];
  states[num-1]->domain = 2;
  states[num-1]->status = Connected;
  pool[numpool++] = states[num-1];

  if (params->style == Keys || params->style == Levers || params->style == Combo) {
    nn = (params->style == Keys ? 1<<params->keys : params->style == Levers ? 1<<params->levers : 1<<(params->keys + params->levers));
    for (i=0; i<nn-1; i++) {
      pool[numpool] = states[num-nn+i];
      if (params->difficult &&
          (params->style == Keys ||
           (params->style == Combo && i <= (1<<(params->keys + params->levers)) - (1<<(params->levers)))))
        pool[numpool]->domain = 0;
      else
        pool[numpool]->domain = 2;
      pool[numpool]->status = Connected;
      numpool++;
    }
  }

  if (params->difficult && /* when to use extra bottleneck */
      (params->style != Keys && params->style != Combo && params->style != Basic)) {
    int nm, *ms, *md;
    while (1) {
      ind = random_upto(rs, num-2) + 1;
      j = random_upto(rs, ndoors);
      if (states[ind]->status == Unallocated && states[ind]->door[j] == Unset)
        break;
    }
    nm = getmirrordoors(params, states[ind]->coord, j, &ms, &md);
    for (i=0; i<nm; i++) {
      states[ms[i]]->status = Connected;
      states[ms[i]]->trans[md[i]]->status = Connected;
/*      if ((params->style == Keys || params->style == Levers) &&
          (states[ms[i]]->coord[2]&3) != 3) {
        states[ms[i]]->domain = 0;
        states[ms[i]]->trans[md[i]]->domain = 0;
      } else {
*/
      states[ms[i]]->domain = 3;
      states[ms[i]]->trans[md[i]]->domain = 4;
      pool[numpool++] = states[ms[i]];
      pool[numpool++] = states[ms[i]]->trans[md[i]];
    }
    bnind = ind;
    bndir = j;
    opendoor(params, states, states[ind], j, ndoors, pool, &numpool);
  } else {
    bnind = -1;
    bndir = -1;
  }

  count = 0;
  while (1) {
    while (numpool) { /* As long as there are rooms in the pool */
      /* Draw a room, determine doors, put new rooms in pool */
      ind = random_upto(rs, numpool);
      rstate = pool[ind];
      pool[ind] = pool[--numpool];
      for (j=0; j<ndoors; j++)
        if (rstate->door[j] == Unset) {
          if (binary(doorprobability(params, states, rstate, j, 0), rs) && canopendoor(params, states, rstate, j))
            opendoor(params, states, rstate, j, ndoors, pool, &numpool);
          else
            closedoor(params, states, rstate, j);
        }
      rstate->status = Complete;
      count++;
    }
    /* Break through some walls */
    numpool = 0;
    for (i=0; i<MAXDOMAIN+1; i++) {
      domprob[i] = 0.0;
    }
    for (i=0; i<num; i++)
      for (j=0; j<ndoors; j++)
        if (firstmirrorstate(params, i, j) == i &&
            canbreakupdoor(params, states, states[i], j, &dom)) {
          pool[numpool] = states[i];
          pooldir[numpool] = j;
          pooldom[numpool] = dom;
          domprob[dom] += poolprob[numpool] = doorprobability(params, states, states[i], j, 1);
          numpool++;
        }

    if (numpool == 0) { /* If impossible to break up more doors, we are done */
      break;
    }

    /* Even breakup between domains to avoid starvation of some domain */
    for (dom=1, ind=0; dom<MAXDOMAIN+1; dom++)
      if (domprob[dom] > 0.0)
        ind++;
    ind = random_upto(rs, ind);
    for (dom=1; dom<MAXDOMAIN+1; dom++)
      if (domprob[dom] > 0.0) {
        if (ind==0)
          break;
        else
          ind--;
      }
    prob = uniform(rs)*domprob[dom];
    for (j=0; pooldom[j] != dom || prob > poolprob[j]; j++)
      if (pooldom[j] == dom)
        prob -= poolprob[j];
    /* no equalization of breakup
    for (i=1; i<MAXDOMAIN+1; i++) {
      domprob[0] += domprob[i];
    }
    prob = uniform(rs)*domprob[0];
    for (j=0; prob > poolprob[j]; j++)
      prob -= poolprob[j];
    */

    rstate = pool[j];
    j = pooldir[j];
    numpool = 0;
    opendoor(params, states, rstate, j, ndoors, pool, &numpool);
  }

  /* Open up the bottleneck */
  nn = MAXDOMAIN*(MAXDOMAIN-1)/2;
  maxscore = snewn(nn, int);
  maxind = snewn(nn, int);
  maxdir = snewn(nn, int);
  for (i=0; i<nn; i++)
    maxscore[i] = 0, maxind[i] = -1, maxdir[i] = -1;
  for (i=0; i<num; i++)
    states[i]->dist = -1;
  numpool = (params->style == Keys ? 1<<params->keys : params->style == Levers ? 1<<params->levers : params->style == Combo ? 1<<(params->keys + params->levers) : 1);
  for (i=0; i<numpool; i++) {
    pool[i] = states[num-numpool+i];
  }
  calcdistance(pool, numpool, ndoors);
  if (states[0]->dist != -1) {
    /* There is a leak - abort and try again */
    printf("Found a leak, restarting.\n");
    goto failure;
  }
  pool[0] = states[0];
  calcdistance(pool, 1, ndoors);

  if (bnind != -1) {
    if (states[bnind]->dist != -1) {
      /* There is a leak - abort and try again */
      printf("Found a leak (BN), restarting.\n");
      goto failure;
    }
    pool[0] = states[bnind];
    pool[1] = states[bnind]->trans[bndir];
    calcdistance(pool, 2, ndoors);
  }
  for (i=0; i<num; i++)
    for (j=0; j<ndoors; j++)
      if (firstmirrorstate(params, i, j) == i &&
          canbreakupdoor_conn(params, states, states[i], j, &tmpscore, &dom1, &dom2))
        if (tmpscore > maxscore[(dom2-1)*(dom2-2)/2 + (dom1-1)]) {
          maxscore[(dom2-1)*(dom2-2)/2 + (dom1-1)] = tmpscore;
          maxind[(dom2-1)*(dom2-2)/2 + (dom1-1)] = i;
          maxdir[(dom2-1)*(dom2-2)/2 + (dom1-1)] = j;
        }
  if (bnind != -1) {
    /* 1-2: 0, 1-3: 1, 1-4: 3, 2-3: 2, 2-4: 4 */
    int s1342 = (maxscore[1] && maxscore[4] ? maxscore[1] + maxscore[4] : 0);
    int s1432 = (maxscore[3] && maxscore[2] ? maxscore[3] + maxscore[2] : 0);

    /* printf("Possible bottlenecks: %d, %d - %d, %d - %d\n", maxscore[0], maxscore[1], maxscore[4], maxscore[3], maxscore[2]); */
    if (s1342 || s1432) {
      numpool = 0;
      if (s1342 > s1432) {
        opendoor(params, states,  states[maxind[1]], maxdir[1], ndoors, pool, &numpool);
        opendoor(params, states,  states[maxind[4]], maxdir[4], ndoors, pool, &numpool);
      } else {
        opendoor(params, states,  states[maxind[3]], maxdir[3], ndoors, pool, &numpool);
        opendoor(params, states,  states[maxind[2]], maxdir[2], ndoors, pool, &numpool);
      }
      /* printf("Bottleneck opened\n"); */
    } else {
      /* Failed to open bottleneck */
      printf("Failed to open bottleneck, restarting.\n");
      goto failure;
    }
  } else {
    if (maxscore[0]) {
      numpool = 0;
      opendoor(params, states,  states[maxind[0]], maxdir[0], ndoors, pool, &numpool);
      /*
      if (states[maxind[0]]->domain != states[maxind[0]]->trans[maxdir[0]]->domain)
        printf("Bottleneck dist: %d + %d  { ", states[maxind[0]]->dist, states[maxind[0]]->trans[maxdir[0]]->dist);
      else
        printf("Bottleneck dist: approx %d  { ", 2*(int)sqrt(maxscore[0]) -2);
      for (i=0; i<ncoord; i++)
        printf("%d ", states[maxind[0]]->coord[i]);
      printf("} <-> { ");
      for (i=0; i<ncoord; i++)
        printf("%d ", states[maxind[0]]->trans[maxdir[0]]->coord[i]);
      printf("}\n");
      */
    } else {
      /* Failed to open bottleneck */
      printf("Failed to open bottleneck, restarting.\n");
      goto failure;
    }
  }

  /* Check for trivial solutions */
  if (params->style == Tandem) {
    for (i=0; i<num; i++)
      states[i]->dist = -1;
    pool[0] = states[num - size*size - 2];
    calcdistance(pool, 1, 4);
    if (states[num-1]->dist != -1) {
      printf("Trivial solution, restarting.\n");
      goto failure;
    }
  } else if (params->style == Keys || params->style == Levers || params->style == Combo) {
    int trivialend = (params->style == Keys ? num - (1<<params->keys) :
                      params->style == Levers ? num - (1<<params->levers) :
                      num - (1<<(params->keys + params->levers)));
    int trivialdoors = (params->style == Keys ? 4 * (1 + params->keys) :
                        params->style == Levers ? 4 * (1 + params->levers) :
                        4 * (params->keys+params->levers+1));
    for (i=0; i<num; i++)
      states[i]->dist = -1;
    pool[0] = states[0];
    calcdistance(pool, 1, trivialdoors);
    if (states[trivialend]->dist != -1) {
      printf("Trivial solution, restarting.\n");
      goto failure;
    }
  }
  /* And nonconnected mazes */
  for (i=0; i<num; i++)
    states[i]->dist = -1;
  pool[0] = states[0];
  calcdistance(pool, 1, ndoors);
  if (states[num-1]->dist == -1) {
    printf("Not connected, restarting.\n");
    goto failure;
  }

  sfree(pool);
  sfree(pooldir);
  sfree(pooldom);
  sfree(poolprob);
  sfree(domprob);
  sfree(maxscore);
  sfree(maxind);
  sfree(maxdir);
  return states;

 failure:

  free_powerstates(states, params);
  sfree(pool);
  sfree(pooldir);
  sfree(pooldom);
  sfree(poolprob);
  sfree(domprob);
  sfree(maxscore);
  sfree(maxind);
  sfree(maxdir);
  return 0;
}

static void desc_dimensions(const game_params *params, int* nfloors, int* nrooms, int* nswitches, int* hasdoorprop)
{
  if (params->style == Basic) {
    *nfloors = 1, *nrooms = 0, *nswitches = 0, *hasdoorprop = 0;
  } else if (params->style == Tandem) {
    *nfloors = 1, *nrooms = 0, *nswitches = params->size*params->size, *hasdoorprop = 0;
  } else if (params->style == ThreeD) {
    *nfloors = params->size, *nrooms = params->size*params->size*params->size, *nswitches = 0, *hasdoorprop = 0;
  } else if (params->style == Floors) {
    *nfloors = params->floors, *nrooms = params->size*params->size*params->floors, *nswitches = 0, *hasdoorprop = 0;
  } else if (params->style == Keys) {
    *nfloors = 1, *nrooms = params->size*params->size, *nswitches = params->keys, *hasdoorprop = 1;
  } else if (params->style == Levers) {
    *nfloors = 1, *nrooms = params->size*params->size, *nswitches = params->levers, *hasdoorprop = 1;
  } else if (params->style == Combo) {
    *nfloors = params->floors, *nrooms = params->size*params->size*params->floors, *nswitches = params->keys + params->levers, *hasdoorprop = 1;
  }
}

static void initializemaze(SuperMaze* maze, const game_params *params)
{
  int sz = params->size;
  int i, fl, nswitches, nrooms, dprop;
  desc_dimensions(params, &fl, &nrooms, &nswitches, &dprop);
  maze->size = sz;
  maze->nswitches = nswitches;
  maze->doorvector = makedoorvector(sz, fl);
  maze->doorswitches = (nswitches ? snewn(nswitches, unsigned char*) : 0);
  for (i=0; i<nswitches; i++)
    maze->doorswitches[i] = makedoorvector(sz, fl);
  maze->roomvector = (nrooms ? snewn(nrooms, int) : 0);
}

static SuperMaze* makesupermaze(SmPowerRoom** states, const game_params *params, random_state *rs)
{
  int ind, sw, ord;
  SmPowerRoom* rstate;
  SuperMaze* maze = snew(SuperMaze);
  int size = params->size;
  int coord[MAXCOORD];
  float probdoor = 0.3; /* Only user for nonreachable doors */
  initializemaze(maze, params);

  if (params->style == Basic) {
    for (coord[1]=0; coord[1]<size; coord[1]++)
      for (coord[0]=0; coord[0]<size; coord[0]++) {
        ind = getindex(coord, params);
        if (ind > -1) {
          rstate = states[ind];
          setdoor(maze->doorvector, size, coord[0], coord[1], 0, 0, 
                  ((rstate->door[0] == Open || (rstate->door[0] == Unset && binary(probdoor, rs))) ? 1 : 0));
          setdoor(maze->doorvector, size, coord[0], coord[1], 0, 2,
                  ((rstate->door[2] == Open || (rstate->door[2] == Unset && binary(probdoor, rs))) ? 1 : 0));
        }
      }
  } else if (params->style == Tandem) {
    coord[0] = -1, coord[1] = 0;
    for (coord[3]=0; coord[3]<size; coord[3]++)
      for (coord[2]=0; coord[2]<size; coord[2]++) {
        ind = getindex(coord, params);
        if (ind > -1) {
          rstate = states[ind];
          setdoor(maze->doorvector, size, coord[2], coord[3], 0, 0, 
                  ((rstate->door[4] == Open || (rstate->door[4] == Unset && binary(probdoor, rs))) ? 1 : 0));
          setdoor(maze->doorvector, size, coord[2], coord[3], 0, 2,
                  ((rstate->door[6] == Open || (rstate->door[6] == Unset && binary(probdoor, rs))) ? 1 : 0));
        }
      }
    for (sw=0, coord[1]=0; coord[1]<size; coord[1]++)
      for (coord[0]=0; coord[0]<size; coord[0]++, sw++) {
        for (ord=0, coord[3]=0; coord[3]<size; coord[3]++)
          for (coord[2]=0; coord[2]<size; coord[2]++) {
            if (coord[0]==coord[2] && coord[1]==coord[3]) {
              ord=1;
              setdoor(maze->doorswitches[sw], size, coord[2], coord[3], 0, 0, 0);
              setdoor(maze->doorswitches[sw], size, coord[2], coord[3], 0, 2, 0);
            } else {
              ind = getindex(coord, params);
              if (ind > -1) {
                rstate = states[ind];
                setdoor(maze->doorswitches[sw], size, coord[2], coord[3], 0, 0, 
                        (rstate->door[ord?4:0] == Impossible ? 0 :
                         rstate->door[ord?4:0] == Unset ? (binary(probdoor, rs) ? 1 : 0) :
                         (rstate->door[ord?4:0] == Open ? 1 : 0) ^ getdoor(maze->doorvector, size, coord[2], coord[3], 0, 0)));
                setdoor(maze->doorswitches[sw], size, coord[2], coord[3], 0, 2, 
                        (rstate->door[ord?6:2] == Impossible ? 0 :
                         rstate->door[ord?6:2] == Unset ? (binary(probdoor, rs) ? 1 : 0) :
                         (rstate->door[ord?6:2] == Open ? 1 : 0) ^ getdoor(maze->doorvector, size, coord[2], coord[3], 0, 2)));
              }
            }
          }
      }
  } else if (params->style == ThreeD) {
    int up, down, k;
    for (coord[2]=0, k=0; coord[2]<size; coord[2]++)
      for (coord[1]=0; coord[1]<size; coord[1]++)
        for (coord[0]=0; coord[0]<size; coord[0]++, k++) {
          ind = getindex(coord, params);
          if (ind > -1) {
            rstate = states[ind];
            setdoor(maze->doorvector, size, coord[0], coord[1], coord[2], 0, 
                    ((rstate->door[0] == Open || (rstate->door[0] == Unset && binary(probdoor, rs))) ? 1 : 0));
            setdoor(maze->doorvector, size, coord[0], coord[1], coord[2], 2,
                    ((rstate->door[2] == Open || (rstate->door[2] == Unset && binary(probdoor, rs))) ? 1 : 0));
            up = ((rstate->door[4] == Open || (rstate->door[4] == Unset && binary(probdoor, rs))) ? 1 : 0);
            down = ((rstate->door[5] == Open || (rstate->door[5] == Unset && states[ind-size*size]->door[4] == Open)) ? 1 : 0);
            maze->roomvector[k] = up + 2*down;
        }
      }
  } else if (params->style == Floors) {
    int fl, i, k;
    for (coord[2]=0, k=0; coord[2]<params->floors; coord[2]++)
      for (coord[1]=0; coord[1]<size; coord[1]++)
        for (coord[0]=0; coord[0]<size; coord[0]++, k++) {
          ind = getindex(coord, params);
          if (ind > -1) {
            rstate = states[ind];
            setdoor(maze->doorvector, size, coord[0], coord[1], coord[2], 0, 
                    ((rstate->door[0] == Open || (rstate->door[0] == Unset && binary(probdoor, rs))) ? 1 : 0));
            setdoor(maze->doorvector, size, coord[0], coord[1], coord[2], 2,
                    ((rstate->door[2] == Open || (rstate->door[2] == Unset && binary(probdoor, rs))) ? 1 : 0));
            fl = -1;
            for (i=0; i<params->floors-1; i++)
              if (rstate->door[4+i] == Open)
                fl = (i<coord[2] ? i : i+1);
            maze->roomvector[k] = fl;
        }
      }
  } else if (params->style == Keys) {
    SmPowerRoom* rstate2;
    int ind2;
    int o1, o2, i, k, key, done_e, done_s;
    for (coord[1]=0, k=0; coord[1]<size; coord[1]++)
      for (coord[0]=0; coord[0]<size; coord[0]++, k++) {
        coord[2] = 0;
        ind = getindex(coord, params);
        coord[2] = (1 << params->keys)-1;
        ind2 = getindex(coord, params);
        if (ind > -1) {
          rstate = states[ind];
          rstate2 = states[ind2];
          done_e = done_s = 0;
          for (i=0; i<params->keys; i++) {
            o1 = (rstate->door[4 + 4*i] == Open);
            o2 = (rstate2->door[4 + 4*i] == Open);
            if (o1 != o2) {
              setdoor(maze->doorswitches[i], size, coord[0], coord[1], 0, 0, 1);
              setdoor(maze->doorvector, size, coord[0], coord[1], 0, 0, 0);
              done_e = 1;
            } else
              setdoor(maze->doorswitches[i], size, coord[0], coord[1], 0, 0, 0);
            o1 = (rstate->door[6 + 4*i] == Open);
            o2 = (rstate2->door[6 + 4*i] == Open);
            if (o1 != o2) {
              setdoor(maze->doorswitches[i], size, coord[0], coord[1], 0, 2, 1);
              setdoor(maze->doorvector, size, coord[0], coord[1], 0, 2, 0);
              done_s = 1;
            } else
              setdoor(maze->doorswitches[i], size, coord[0], coord[1], 0, 2, 0);
          }
          if (!done_e)
            setdoor(maze->doorvector, size, coord[0], coord[1], 0, 0, 
                    ((rstate->door[0] == Open || (rstate->door[0] == Unset && binary(probdoor, rs))) ? 1 : 0));
          if (!done_s)
            setdoor(maze->doorvector, size, coord[0], coord[1], 0, 2,
                    ((rstate->door[2] == Open || (rstate->door[2] == Unset && binary(probdoor, rs))) ? 1 : 0));
          key = -1;
          for (i=0; i<params->keys; i++)
            if (rstate->door[4+4*params->keys+i] == Open)
              key = i;
          maze->roomvector[k] = key;
        }
      }
  } else if (params->style == Levers) {
    SmPowerRoom* rstate2;
    int ind2;
    int o1, o2, i, k, lev, done_e, done_s;
    for (coord[1]=0, k=0; coord[1]<size; coord[1]++)
      for (coord[0]=0; coord[0]<size; coord[0]++, k++) {
        coord[2] = 0;
        ind = getindex(coord, params);
        coord[2] = (1 << params->levers)-1;
        ind2 = getindex(coord, params);
        if (ind > -1) {
          rstate = states[ind];
          rstate2 = states[ind2];
          done_e = done_s = 0;
          for (i=0; i<params->levers; i++) {
            o1 = (rstate->door[4 + 4*i] == Open);
            o2 = (rstate2->door[4 + 4*i] == Open);
            if (o1 != o2) {
              setdoor(maze->doorswitches[i], size, coord[0], coord[1], 0, 0, 1);
              setdoor(maze->doorvector, size, coord[0], coord[1], 0, 0, (o1 ? 1 : 0));
              done_e = 1;
            } else
              setdoor(maze->doorswitches[i], size, coord[0], coord[1], 0, 0, 0);
            o1 = (rstate->door[6 + 4*i] == Open);
            o2 = (rstate2->door[6 + 4*i] == Open);
            if (o1 != o2) {
              setdoor(maze->doorswitches[i], size, coord[0], coord[1], 0, 2, 1);
              setdoor(maze->doorvector, size, coord[0], coord[1], 0, 2, (o1 ? 1 : 0));
              done_s = 1;
            } else
              setdoor(maze->doorswitches[i], size, coord[0], coord[1], 0, 2, 0);
          }
          if (!done_e)
            setdoor(maze->doorvector, size, coord[0], coord[1], 0, 0, 
                    ((rstate->door[0] == Open || (rstate->door[0] == Unset && binary(probdoor, rs))) ? 1 : 0));
          if (!done_s)
            setdoor(maze->doorvector, size, coord[0], coord[1], 0, 2,
                    ((rstate->door[2] == Open || (rstate->door[2] == Unset && binary(probdoor, rs))) ? 1 : 0));
          lev = -1;
          for (i=0; i<params->levers; i++)
            if (rstate->door[4+4*params->levers+i] == Open)
              lev = i;
          maze->roomvector[k] = lev;
        }
      }
  } else if (params->style == Combo) {
    SmPowerRoom* rstate2;
    int ind2;
    int o1, o2, i, k, up, down, rp, done_e, done_s;
    for (coord[2]=0, k=0; coord[2]<params->floors; coord[2]++)
      for (coord[1]=0; coord[1]<size; coord[1]++)
        for (coord[0]=0; coord[0]<size; coord[0]++, k++) {
          coord[3] = 0;
          ind = getindex(coord, params);
          coord[3] = (1 << (params->keys + params->levers))-1;
          ind2 = getindex(coord, params);
          if (ind > -1) {
            rstate = states[ind];
            rstate2 = states[ind2];
            done_e = done_s = 0;
            for (i=0; i<(params->keys+params->levers); i++) {
              o1 = (rstate->door[4 + 4*i] == Open);
              o2 = (rstate2->door[4 + 4*i] == Open);
              if (o1 != o2) {
                setdoor(maze->doorswitches[i], size, coord[0], coord[1], coord[2], 0, 1);
                setdoor(maze->doorvector, size, coord[0], coord[1], coord[2], 0, (o1 && i<params->levers ? 1 : 0));
                done_e = 1;
              } else
                setdoor(maze->doorswitches[i], size, coord[0], coord[1], coord[2], 0, 0);
              o1 = (rstate->door[6 + 4*i] == Open);
              o2 = (rstate2->door[6 + 4*i] == Open);
              if (o1 != o2) {
                setdoor(maze->doorswitches[i], size, coord[0], coord[1], coord[2], 2, 1);
                setdoor(maze->doorvector, size, coord[0], coord[1], coord[2], 2, (o1 && i<params->levers ? 1 : 0));
                done_s = 1;
              } else
                setdoor(maze->doorswitches[i], size, coord[0], coord[1], coord[2], 2, 0);
            }
            if (!done_e)
              setdoor(maze->doorvector, size, coord[0], coord[1], coord[2], 0, 
                      ((rstate->door[0] == Open || (rstate->door[0] == Unset && binary(probdoor, rs))) ? 1 : 0));
            if (!done_s)
              setdoor(maze->doorvector, size, coord[0], coord[1], coord[2], 2,
                      ((rstate->door[2] == Open || (rstate->door[2] == Unset && binary(probdoor, rs))) ? 1 : 0));
            up = ((rstate->door[4*(params->keys+params->levers+1)] == Open || (rstate->door[4*(params->keys+params->levers+1)] == Unset && binary(probdoor, rs))) ? 1 : 0);
            down = ((rstate->door[4*(params->keys+params->levers+1)+1] == Open || (rstate->door[4*(params->keys+params->levers+1)+1] == Unset && states[ind-size*size]->door[4*(params->keys+params->levers+1)] == Open)) ? 1 : 0);
            if (up || down)
              maze->roomvector[k] = up + 2*down;
            else {
              rp = 0;
              for (i=0; i<(params->keys+params->levers); i++)
                if (rstate->door[4*(params->keys+params->levers+1)+2+i] == Open)
                  rp = 4+i;
              maze->roomvector[k] = rp;
            }
          }
        }
  }

  return maze;
}

static int countsolutionstates(SmPowerRoom** states, const game_params *params, char** aux)
{
  int par, ok, solcount;
  int i, j;
  char* p;
  char* tmpaux;
  char last, curr;
  int ndoors, npool;
  int num = numindex(params);
  SmPowerRoom** pool = snewn(num, SmPowerRoom*);
  SmPowerRoom* rstate;
  ndoors = numdoors(params);
  for (i=0; i<num; i++)
    states[i]->dist = -1;
  npool = (params->style == Keys ? 1<<params->keys : params->style == Levers ? 1<<params->levers : params->style == Combo ? 1<<(params->keys + params->levers) : 1);
  for (i=0; i<npool; i++) {
    pool[i] = states[num-npool+i];
  }
  calcdistance(pool, npool, ndoors);
  solcount = states[0]->dist;
  if (solcount == -1) {
    *aux = 0;
    return -1;
  }
  npool = 0;
  tmpaux = p = snewn(solcount*2+2, char);
  *p++ = 'S';

  rstate = states[0];
  par = 0;
  last = 'A';
  *p++ = last;
  while (rstate->dist != 0) {
    ok = 0;
    for (j=0; j<ndoors; j++)
      if (rstate->door[j] == Open && rstate->trans[j]->dist == rstate->dist - 1) {
        pool[npool++] = rstate;
        if (params->style == Basic) {
          *p++ = (j == 0 ? 'e' : j == 1 ? 'w' : j == 2 ? 's' : 'n');
        } else if (params->style == Tandem) {
          curr = (((j&4)==4) == par ? 'A' : 'B');
          if (last != curr)
            *p++ = last = curr;
          *p++ = ((j&3) == 0 ? 'e' : (j&3) == 1 ? 'w' : (j&3) == 2 ? 's' : 'n');
          par = (((rstate->recdir[j]^j)&4) ? !par : par);
        } else if (params->style == ThreeD) {
          *p++ = (j == 0 ? 'e' : j == 1 ? 'w' : j == 2 ? 's' : j == 3 ? 'n' : j == 4 ? 'u' : 'd');
        } else if (params->style == Floors) {
          *p++ = (j == 0 ? 'e' : j == 1 ? 'w' : j == 2 ? 's' : j == 3 ? 'n' : 't');
        } else if (params->style == Keys) {
          *p++ = (j >= 4 + 4*params->keys ? 't' : (j&3) == 0 ? 'e' : (j&3) == 1 ? 'w' : (j&3) == 2 ? 's' : 'n');
        } else if (params->style == Levers) {
          *p++ = (j >= 4 + 4*params->levers ? 't' : (j&3) == 0 ? 'e' : (j&3) == 1 ? 'w' : (j&3) == 2 ? 's' : 'n');
        } else if (params->style == Combo) {
          *p++ = (j >= 4*(params->keys+params->levers+1)+2 ? 't' : 
                  j == 4*(params->keys+params->levers+1) ? 'u' :
                  j == 4*(params->keys+params->levers+1) + 1 ? 'd' :
                  (j&3) == 0 ? 'e' : (j&3) == 1 ? 'w' : (j&3) == 2 ? 's' : 'n');
        } 
        rstate = rstate->trans[j];
        ok = 1;
        break;
      }
    if (!ok) {
      sfree(*aux);
      sfree(tmpaux);
      *aux = 0;
      break;
    }
  }
  *p = 0;

  *aux = p = snewn(num+2, char);
  *p++ = 'S';
  for (i=0; i<num; i++) {
    rstate = states[i];
    ok = 0;
    for (j=0; j<ndoors; j++)
      if (rstate->door[j] == Open && rstate->trans[j]->dist == rstate->dist - 1) {
        if (params->style == Basic) {
          *p++ = (j == 0 ? 'e' : j == 1 ? 'w' : j == 2 ? 's' : 'n');
        } else if (params->style == Tandem) {
          if ((j&4)==4)
            *p++ = ((j&3) == 0 ? 'E' : (j&3) == 1 ? 'W' : (j&3) == 2 ? 'S' : 'N');
          else
            *p++ = ((j&3) == 0 ? 'e' : (j&3) == 1 ? 'w' : (j&3) == 2 ? 's' : 'n');
        } else if (params->style == ThreeD) {
          *p++ = (j == 0 ? 'e' : j == 1 ? 'w' : j == 2 ? 's' : j == 3 ? 'n' : j == 4 ? 'u' : 'd');
        } else if (params->style == Floors) {
          *p++ = (j == 0 ? 'e' : j == 1 ? 'w' : j == 2 ? 's' : j == 3 ? 'n' : 't');
        } else if (params->style == Keys) {
          *p++ = (j >= 4 + 4*params->keys ? 't' : (j&3) == 0 ? 'e' : (j&3) == 1 ? 'w' : (j&3) == 2 ? 's' : 'n');
        } else if (params->style == Levers) {
          *p++ = (j >= 4 + 4*params->levers ? 't' : (j&3) == 0 ? 'e' : (j&3) == 1 ? 'w' : (j&3) == 2 ? 's' : 'n');
        } else if (params->style == Combo) {
          *p++ = (j >= 4*(params->keys+params->levers+1)+2 ? 't' : 
                  j == 4*(params->keys+params->levers+1) ? 'u' :
                  j == 4*(params->keys+params->levers+1) + 1 ? 'd' :
                  (j&3) == 0 ? 'e' : (j&3) == 1 ? 'w' : (j&3) == 2 ? 's' : 'n');
        }
        ok = 1;
        break;
      }
    if (!ok)
      *p++ = '-';
  }
  *p = 0;

  for (i=0; i<num; i++)
    states[i]->dist = -1;
  calcdistance(pool, npool, ndoors);
  /* printf("Steps: %d  Longest: %d   Sol: %s\n", solcount, max, tmpaux+1); */
  sfree(tmpaux);

  sfree(pool);
  return solcount;
}

static void free_supermaze(SuperMaze* maze)
{
  int i;
  sfree(maze->doorvector);
  for (i=0; i<maze->nswitches; i++)
    sfree(maze->doorswitches[i]);
  if (maze->doorswitches)
    sfree(maze->doorswitches);
  if (maze->roomvector)
    sfree(maze->roomvector);
  sfree(maze);
}


/* ----------------------------------------------------------------------
 * Main game UI.
 */

static unsigned char hextobits(unsigned char ch, int shift)
{
  unsigned char ret = 0;
  if (ch>='0' && ch <='9')
    ret = ch-'0';
  else if (ch>='A' && ch <='F')
    ret = ch-'A'+10;
  return (shift ? ret << 4 : ret);
}

static unsigned char bitstohex(unsigned char ch, int shift)
{
  unsigned char bits = (shift ? ch >> 4 : ch & 15);
  if (bits < 10)
    return bits + '0';
  else
    return bits - 10 + 'A';
}

static void transhextobits(int nhex, const unsigned char* hex, unsigned char* bits)
{
  int i;
  for (i=0; i<nhex; i++)
    if (i&1)
      bits[i/2] |= hextobits(hex[i], 1);
    else
      bits[i/2] = hextobits(hex[i], 0);
}

static void transbitstohex(int nhex, const unsigned char* bits, unsigned char* hex)
{
  int i;
  for (i=0; i<nhex; i++)
    hex[i] = bitstohex(bits[i/2], i&1);
}

static void transhextoints(int nints, const unsigned char* hex, int* ints)
{
  int i;
  for (i=0; i<nints; i++) {
    ints[i] = hextobits(hex[2*i], 1);
    ints[i] |= hextobits(hex[2*i+1], 0);
    if (ints[i] & 0x80)
      ints[i] -= 256;
  }
}

static void transintstohex(int nints, const int* ints, unsigned char* hex)
{
  int i;
  unsigned char ch;
  for (i=0; i<nints; i++) {
    ch = (unsigned char)(ints[i] & 0xff);
    hex[2*i] = bitstohex(ch, 1);
    hex[2*i+1] = bitstohex(ch, 0);
  }
}


static char *new_game_desc(const game_params *params, random_state *rs,
			   char **aux, bool interactive)
{
  char *buf = 0, *p;
  int i, sz, hexlen, nfloors, nrooms, nswitches, dprop, solcount;
  SuperMaze* maze;
  SmPowerRoom** states;
  do {
    states = makepowerstates(params, rs);
  } while (!states);
  maze = makesupermaze(states, params, rs);
  solcount = countsolutionstates(states, params, aux);
  if (!solcount) return 0; /* dummy statement to satisfy picky compiler... */

  sz = params->size;
  desc_dimensions(params, &nfloors, &nrooms, &nswitches, &dprop);
  hexlen = calchexlen(sz, nfloors);
  p = buf = snewn((hexlen+1)*(nswitches+1) + 2*nrooms + 1, char);
  transbitstohex(hexlen, maze->doorvector, (unsigned char*)p);
  for (i=0; i<nswitches; i++) {
    p += hexlen;
    *p++ = ',';
    transbitstohex(hexlen, maze->doorswitches[i], (unsigned char*)p);
  }
  p += hexlen;
  if (nrooms) {
    *p++ = ',';
    transintstohex(nrooms, maze->roomvector, (unsigned char*)p);
    p += 2*nrooms;
  }
  *p = '\0';

  free_powerstates(states, params);
  free_supermaze(maze);
  return buf;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
  int i, hexlen, nfloors, nrooms, nswitches, dprop;
  int n = 0;
  desc_dimensions(params, &nfloors, &nrooms, &nswitches, &dprop);
  hexlen = calchexlen(params->size, nfloors);
  /* We expect 1 + switches hexadecimal sequences of bitlen bits each, comma separated, followed by one hexadecimal sequence of length 2*rooms */
  for (i=0; i<hexlen; i++, desc++)
    if (!(*desc >= '0' && *desc <= '9') && !(*desc >= 'A' && *desc <= 'F'))
      return "Expected hexadecimal digit";
  for (n=0; n < nswitches; n++) {
    if (!*desc)
      return "Too short description";
    if (*desc++ != ',')
      return "Expected comma between hexadecimal numbers";
    for (i=0; i<hexlen; i++, desc++)
      if (!(*desc >= '0' && *desc <= '9') && !(*desc >= 'A' && *desc <= 'F'))
        return "Expected hexadecimal digit";
  }
  if (nrooms) {
    if (!*desc)
      return "Too short description";
    if (*desc++ != ',')
      return "Expected comma between hexadecimal numbers";
    for (i=0; i<2*nrooms; i++, desc++)
      if (!(*desc >= '0' && *desc <= '9') && !(*desc >= 'A' && *desc <= 'F'))
        return "Expected hexadecimal digit";
  }
  if (*desc)
    return "Too long description";
  
  return NULL;
}

static void set_initial_state(const game_params *params, int* coord)
{
  if (params->style == Basic) {
    coord[0] = -1;
    coord[1] = 0;
  } else if (params->style == Tandem) {
    coord[0] = coord[2] = -1; /* Both balls in starting position */
    coord[1] = coord[3] = 0;
  } else if (params->style == Combo) {
    coord[0] = -1;
    coord[1] = 0;
    coord[2] = 0;
    coord[3] = 0;
  } else {
    coord[0] = -1;
    coord[1] = 0;
    coord[2] = 0;
  }
}

static game_state *new_game(midend *me, const game_params *params, const char *desc)
{
    game_state *state = snew(game_state);
    int i, sz, hexlen, nfloors, nrooms, nswitches, dprop, ncoord;

    sz = params->size;
    desc_dimensions(params, &nfloors, &nrooms, &nswitches, &dprop);
    hexlen = calchexlen(sz, nfloors);
    ncoord = numcoord(params);

    state->par = params;
    state->coord = snewn(ncoord, int);
    set_initial_state(params, state->coord);
    state->completed = state->cheated = false;

    state->clues = snew(struct clues);
    state->clues->refcount = 1;
    state->clues->size = sz;
    state->clues->nswitches = nswitches;
    state->clues->doorvector = makedoorvector(sz, nfloors);
    state->clues->doorswitches = (nswitches ? snewn(nswitches, unsigned char*) : 0);
    for (i=0; i<nswitches; i++)
      state->clues->doorswitches[i] = makedoorvector(sz, nfloors);
    state->clues->roomvector = (nrooms ? snewn(nrooms, int) : 0);
    state->clues->sol = 0;

    transhextobits(hexlen, (unsigned char*)desc, state->clues->doorvector);
    for (i=0; i<nswitches; i++) {
      desc += hexlen+1;
      transhextobits(hexlen, (unsigned char*)desc, state->clues->doorswitches[i]);
    }
    if (nrooms) {
      desc += hexlen+1;
      transhextoints(nrooms, (unsigned char*)desc, state->clues->roomvector);
    }
    if (dprop) {
      int j, dlen = sz*(sz-1)*2*nfloors;
      state->clues->doorprop = snewn(dlen, int);
      for (i=0; i<dlen; i++) {
        state->clues->doorprop[i] = -1;
        for (j=0; j<nswitches; j++)
          if (state->clues->doorswitches[j][i/8] & (1<<(i%8))) {
            /* if (state->clues->doorprop[i] != -1)
               printf("Error: multi-colored door (%d : %d & %d)\n", i, state->clues->doorprop[i], j); */
            state->clues->doorprop[i] = j;
          }
      }
    } else
      state->clues->doorprop = 0;
    return state;
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = snew(game_state);
    int i, ncoord;

    ret->par = state->par;
    ncoord = numcoord(state->par);
    ret->coord = snewn(ncoord, int);
    for (i=0; i<ncoord; i++)
      ret->coord[i] = state->coord[i];
    ret->completed = state->completed;
    ret->cheated = state->cheated;

    ret->clues = state->clues;
    ret->clues->refcount++;

    return ret;
}

static void free_game(game_state *state)
{
  int i;
  if (--state->clues->refcount <= 0) {
    sfree(state->clues->doorvector);
    for (i=0; i<state->clues->nswitches; i++)
      sfree(state->clues->doorswitches[i]);
    sfree(state->clues->doorswitches);
    sfree(state->clues->roomvector);
    if (state->clues->doorprop)
      sfree(state->clues->doorprop);
    if (state->clues->sol)
      sfree(state->clues->sol);
    sfree(state->clues);
  }
  sfree(state->coord);
  sfree(state);
}

static char *solve_game(const game_state *state, const game_state *currstate,
			const char *aux, const char **error)
{
  if (aux)
    return dupstr(aux);
  else
    return NULL;
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
  /* The ball which is the default to be moved in the next step, if ambiguous. */
  int ballnr;
  /* Coordinates of active test square */
  int tshow;
  int tpos[2];
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);

    ui->ballnr = 1;
    ui->tshow = 0;
    ui->tpos[0] = ui->tpos[1] = -1;
    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
  int i;
  int change = 0;
  int ncoord = numcoord(oldstate->par);
  for (i=0; i<ncoord; i++)
    if (oldstate->coord[i] != newstate->coord[i])
      change = 1;
  if (change) {
    ui->tshow = 0;
    ui->tpos[0] = ui->tpos[1] = -1;
  }
}

#define PREFERRED_TILESIZE 48
#define TILESIZE(size) (size)
#define BORDER(size) (size/2)

#define TOTSIZEX(w, size) ((w+2) * TILESIZE(size) + 8)
#define TOTSIZEY(h, size) ((h) * TILESIZE(size) + 2*BORDER(size))

#define COORDX(x, size) ((x+1)*TILESIZE(size) + 4)
#define COORDY(y, size) ((y)*TILESIZE(size) + BORDER(size))
#define FROMCOORDX(x, size) (((x) - 8) / TILESIZE(size) - 1)
#define FROMCOORDY(y, size) (((y)+(TILESIZE(size) - BORDER(size))) / TILESIZE(size) - 1)

#define FLASH_TIME 0.6F
#define ANIM_TIME 0.5F

struct game_drawstate {
  int tilesize;
  int w, h;
  int started;
  int flash, anim;
  int* pos;
  int* lastpos;
  int testpos[2];
  int lasttestpos[2];
  unsigned char *doors;
  unsigned char *lastdoors;
};

static int combinedoor(const game_params *params, const struct clues *clues, const int* coord, int x, int y, int dir)
{
  int z = (params->style == ThreeD || params->style == Floors || params->style == Combo ? coord[2] : 0);
  int pos = doorbitpos(params->size, x, y, z, dir);
  unsigned char bits;
  if (pos == -1)
    return 0;
  bits = clues->doorvector[pos/8];

  if (params->style == Tandem) {
    int sz = params->size;
    int b1 = (coord[0]==-1 || coord[0]==sz ? -1 : coord[1]*sz + coord[0]);
    int b2 = (coord[2]==-1 || coord[2]==sz ? -1 : coord[3]*sz + coord[2]);
    if (b1 != -1)
      bits ^= clues->doorswitches[b1][pos/8];
    if (b2 != -1)
      bits ^= clues->doorswitches[b2][pos/8];
  } else if (params->style == Keys) {
    int j, b;
    for (j=0, b=1; j<params->keys; j++, b<<=1)
      if (coord[2] & b)
        bits ^= clues->doorswitches[j][pos/8];
  } else if (params->style == Levers) {
    int j, b;
    for (j=0, b=1; j<params->levers; j++, b<<=1)
      if (coord[2] & b)
        bits ^= clues->doorswitches[j][pos/8];
  } else if (params->style == Combo) {
    int j, b;
    for (j=0, b=1; j<(params->keys+params->levers+1); j++, b<<=1)
      if (coord[3] & b)
        bits ^= clues->doorswitches[j][pos/8];
  }

  return (bits & (1<<(pos%8)) ? 1 : 0);
}

static void combinealldoors(unsigned char* resvector, const game_params *params, const struct clues *clues, const int* coord, const int* test)
{
  int i;
  int fl = (params->style == ThreeD ? params->size : params->style == Floors || params->style == Combo ? params->floors : 1);
  int len = calcdoorveclen(params->size, fl);
  for (i=0; i<len; i++)
    resvector[i] = clues->doorvector[i];

  if (params->style == Basic && params->difficult) {
    unsigned char* maskvector = makedoorvector(params->size, fl);
    for (i=0; i<len; i++)
      maskvector[i] = 0;
    for (i=0; i<4; i++)
      setdoor(maskvector, params->size, coord[0], coord[1], 0, i, 1);
    if (!test)
      for (i=0; i<len; i++)
        resvector[i] &= maskvector[i];
    sfree(maskvector);
  } else if (params->style == Tandem) {
    int sz = params->size;
    int b1 = (coord[0]==-1 || coord[0]==sz ? -1 : coord[1]*sz + coord[0]);
    int b2 = (coord[2]==-1 || coord[2]==sz ? -1 : coord[3]*sz + coord[2]);
    int bt = (!test || test[0]==-1 || test[0]==sz ? -1 : test[1]*sz + test[0]);
    if (b1 != -1)
      for (i=0; i<len; i++)
        resvector[i] ^= clues->doorswitches[b1][i];
    if (b2 != -1)
      for (i=0; i<len; i++)
        resvector[i] ^= clues->doorswitches[b2][i];
    if (bt != -1)
      for (i=0; i<len; i++)
        resvector[i] ^= clues->doorswitches[bt][i];
  } else if (params->style == Keys) {
    int j, b;
    unsigned char* maskvector = makedoorvector(params->size, fl);
    for (i=0; i<len; i++)
      maskvector[i] = 0;
    for (i=0; i<4; i++)
      setdoor(maskvector, params->size, coord[0], coord[1], 0, i, 1);
    for (j=0, b=1; j<params->keys; j++, b<<=1)
      if (coord[2] & b)
        for (i=0; i<len; i++)
          resvector[i] ^= (maskvector[i] & clues->doorswitches[j][i]);
    sfree(maskvector);
  } else if (params->style == Levers) {
    int j, b;
    for (j=0, b=1; j<params->levers; j++, b<<=1)
      if (coord[2] & b)
        for (i=0; i<len; i++)
          resvector[i] ^= clues->doorswitches[j][i];
  } else if (params->style == Combo) {
    int j, b;
    unsigned char* maskvector = makedoorvector(params->size, fl);
    for (i=0; i<len; i++)
      maskvector[i] = 0;
    for (i=0; i<4; i++)
      setdoor(maskvector, params->size, coord[0], coord[1], coord[2], i, 1);
    for (j=0, b=1; j<params->levers; j++, b<<=1)
      if (coord[3] & b)
        for (i=0; i<len; i++)
          resvector[i] ^= clues->doorswitches[j][i];
    for (; j<params->levers+params->keys; j++, b<<=1)
      if (coord[3] & b)
        for (i=0; i<len; i++)
          resvector[i] ^= (maskvector[i] & clues->doorswitches[j][i]);
    sfree(maskvector);
  }
}

static int canmove(const game_state *state, int x1, int y1, int x2, int y2, int* dir)
{
  int sz = state->par->size;
  if (state->par->style == Tandem && x2 >= 0 && x2 < sz &&
      ((state->coord[0] == x2 && state->coord[1] == y2) ||
       (state->coord[2] == x2 && state->coord[3] == y2)))
    return 0;
  if (x1 == x2 && x1 >= 0 && x1 < sz) {
    if (y1+1 == y2 && y1 >= 0 && y1 < sz-1)
      *dir = 2;
    else if (y1-1 == y2 && y1 >= 1 && y1 < sz)
      *dir = 3;
    else
      return 0;
  } else if (y1 == y2 && y1 >= 0 && y1 < sz) {
    if (x1+1 == x2 && x1 >= 0 && x1 < sz-1)
      *dir = 0;
    else if (x1-1 == x2 && x1 >= 1 && x1 < sz)
      *dir = 1;
    else if (y1 == 0 && x1 == -1 && x2 == 0) {
      if ((state->par->style == ThreeD || state->par->style == Floors || state->par->style == Combo) &&
          state->coord[2] != 0)
        return 0;
      *dir = 0;
      return 1;
    } else if (y1 == sz-1 && x1 == sz-1 && x2 == sz) {
      if ((state->par->style == ThreeD && state->coord[2] != sz-1) ||
          ((state->par->style == Floors || state->par->style == Combo) &&
           state->coord[2] != state->par->floors-1))
        return 0;
      *dir = 0;
      return 1;
    } else if (y1 == 0 && x2 == -1 && x1 == 0) {
      if ((state->par->style == ThreeD || state->par->style == Floors || state->par->style == Combo) &&
          state->coord[2] != 0)
        return 0;
      *dir = 1;
      return 1;
    } else if (y1 == sz-1 && x2 == sz-1 && x1 == sz) {
      if ((state->par->style == ThreeD && state->coord[2] != sz-1) ||
          ((state->par->style == Floors || state->par->style == Combo) &&
           state->coord[2] != state->par->floors-1))
        return 0;
      *dir = 1;
      return 1;
    } else
      return 0;
  } else
    return 0;
  return combinedoor(state->par, state->clues, state->coord, x1, y1, *dir);
}

static char *interpret_move(const game_state *state, game_ui *ui, const game_drawstate *ds,
			    int x, int y, int button)
{
/*
  Move string: (A/B) n/e/s/w(/u/d)(/t), T
  Meaning: Ball A or B, north / east / south / west / up / down / take, and Test
 */
    int sz = state->par->size;
    SmStyle style = state->par->style;
    int dir;
    int tx, ty;
    char retstr[16];

    button &= ~MOD_MASK;

    if (button == LEFT_BUTTON || button == CURSOR_SELECT) {
      if (button == LEFT_BUTTON) {
        tx = FROMCOORDX(x, ds->tilesize);
        ty = FROMCOORDY(y, ds->tilesize);
      } else {
        tx = (style != Tandem || ui->ballnr == 1 ? state->coord[0] : state->coord[2]);
        ty = (style != Tandem || ui->ballnr == 1 ? state->coord[1] : state->coord[3]);
      }
      if ((button == LEFT_BUTTON && tx == -1 && ty == 0) ||
          (button == LEFT_BUTTON && tx == sz && ty == sz-1) ||
          (tx >= 0 && tx < sz && ty >= 0 && ty < sz &&
           (tx != state->coord[0] || ty != state->coord[1]) &&
           (style != Tandem || tx != state->coord[2] || ty != state->coord[3]))) {
        /* Left click on a room without a ball - try to move there */
        if (canmove(state,
                    (style != Tandem || ui->ballnr == 1 ? state->coord[0] : state->coord[2]),
                    (style != Tandem || ui->ballnr == 1 ? state->coord[1] : state->coord[3]),
                    tx, ty, &dir)) {
          if (style == Tandem) {
            sprintf(retstr, "%c%c", (ui->ballnr == 1 ? 'A' : 'B'),
                    (dir==0 ? 'e' : dir==1 ? 'w' : dir==2 ? 's' : 'n'));
          } else
            sprintf(retstr, "%c", (dir==0 ? 'e' : dir==1 ? 'w' : dir==2 ? 's' : 'n'));
          if (ui->tshow)
            ui->tshow = 0, ui->tpos[0] = -1, ui->tpos[1] = -1;
          return dupstr(retstr);
        } else if (style == Tandem &&
                   canmove(state,
                           (ui->ballnr == 1 ? state->coord[2] : state->coord[0]),
                           (ui->ballnr == 1 ? state->coord[3] : state->coord[1]),
                           tx, ty, &dir)) {
          ui->ballnr = (ui->ballnr == 1 ? 2 : 1);
          sprintf(retstr, "%c%c", (ui->ballnr == 1 ? 'A' : 'B'), 
                  (dir==0 ? 'e' : dir==1 ? 'w' : dir==2 ? 's' : 'n'));
          ui->tshow = 0, ui->tpos[0] = -1, ui->tpos[1] = -1;
          return dupstr(retstr);
        }
      }
      if (tx == state->coord[0] && ty == state->coord[1]) {
        /* Left click on the (first) ball - select ball or go up or act */
        if (style == Tandem) {
          if (button == CURSOR_SELECT)
            ui->ballnr = 2;
          else
            ui->ballnr = 1;
        } else if (tx == -1 || tx == sz) {
          /* All folowing cases requre the ball to be inside the board */
          return NULL;
        } else if (style == ThreeD) {
          if (state->clues->roomvector[state->coord[2]*sz*sz + state->coord[1]*sz + state->coord[0]] & 1) {
            sprintf(retstr, "u");
            return dupstr(retstr);
          }
        } else if (style == Floors) {
          int tmp = state->clues->roomvector[state->coord[2]*sz*sz + state->coord[1]*sz + state->coord[0]];
          if (tmp != -1) {
            sprintf(retstr, "F%d", tmp);
            return dupstr(retstr);
          }
        } else if (style == Keys) {
          int tmp = state->clues->roomvector[state->coord[1]*sz + state->coord[0]];
          if (tmp != -1) {
            sprintf(retstr, "K%d", tmp);
            return dupstr(retstr);
          }
        } else if (style == Levers) {
          int tmp = state->clues->roomvector[state->coord[1]*sz + state->coord[0]];
          if (tmp != -1) {
            sprintf(retstr, "L%d", tmp);
            return dupstr(retstr);
          }
        } else if (style == Combo) {
          int tmp = state->clues->roomvector[state->coord[2]*sz*sz + state->coord[1]*sz + state->coord[0]];
          if (tmp < 4) {
            if (tmp & 1) {
              sprintf(retstr, "u");
              return dupstr(retstr);
            }
          } else {
            sprintf(retstr, "C%d", tmp-4);
            return dupstr(retstr);
          }
        }
      } else if (style == Tandem && tx == state->coord[2] && ty == state->coord[3]) {
        /* Left click on the second ball - select ball */
        if (button == CURSOR_SELECT)
          ui->ballnr = 1;
        else
          ui->ballnr = 2;
      } else if ((tx == -1 && ty == 0) || (tx == sz && ty == sz-1) ||
                 (tx >= 0 && tx < sz && ty >= 0 && ty < sz)) {
        /* Otherwise, remove test */
        if (ui->tshow) {
          ui->tshow = 0, ui->tpos[0] = -1, ui->tpos[1] = -1;
          sprintf(retstr, "T");
          return dupstr(retstr);
        } else         
          return MOVE_UI_UPDATE;
      }
      return NULL;
    } else if (button == RIGHT_BUTTON || button == CURSOR_SELECT2) {
      if (button == RIGHT_BUTTON) {
        tx = FROMCOORDX(x, ds->tilesize);
        ty = FROMCOORDY(y, ds->tilesize);
      } else {
        tx = (style != Tandem || ui->ballnr == 1 ? state->coord[0] : state->coord[2]);
        ty = (style != Tandem || ui->ballnr == 1 ? state->coord[1] : state->coord[3]);
      }
      if (style == Basic) {
        if (ui->tshow) {
          ui->tshow = 0, ui->tpos[0] = -1, ui->tpos[1] = -1;
          sprintf(retstr, "T");
          return dupstr(retstr);
        } else {
          ui->tshow = 1, ui->tpos[0] = 1, ui->tpos[1] = 1;
          sprintf(retstr, "T");
          return dupstr(retstr);
        }
      } else if (style == Tandem) {
        /* In Tandem: Toggle test */
        if (tx >= 0 && tx < sz && ty >= 0 && ty < sz &&
            (tx != ui->tpos[0] || ty != ui->tpos[1])) {
          ui->tshow = 1, ui->tpos[0] = tx, ui->tpos[1] = ty;
          sprintf(retstr, "T");
          return dupstr(retstr);
        } else if (ui->tshow) {
          ui->tshow = 0, ui->tpos[0] = -1, ui->tpos[1] = -1;
          sprintf(retstr, "T");
          return dupstr(retstr);
        }
      } else if (tx == -1 || tx == sz) {
        /* All following cases requre the ball to be inside the board */
        return NULL;
      } else if (tx == state->coord[0] && ty == state->coord[1]) {
        if (style == ThreeD) {
          if ((state->clues->roomvector[state->coord[2]*sz*sz + state->coord[1]*sz + state->coord[0]] & ~1) == 2) {
            sprintf(retstr, "d");
            return dupstr(retstr);
          }
        } else if (style == Floors) {
          int tmp = state->clues->roomvector[state->coord[2]*sz*sz + state->coord[1]*sz + state->coord[0]];
          if (tmp != -1) {
            sprintf(retstr, "F%d", tmp);
            return dupstr(retstr);
          }
        } else if (style == Keys) {
          int tmp = state->clues->roomvector[state->coord[1]*sz + state->coord[0]];
          if (tmp != -1) {
            sprintf(retstr, "K%d", tmp);
            return dupstr(retstr);
          }
        } else if (style == Levers) {
          int tmp = state->clues->roomvector[state->coord[1]*sz + state->coord[0]];
          if (tmp != -1) {
            sprintf(retstr, "L%d", tmp);
            return dupstr(retstr);
          }
        } else if (style == Combo) {
          int tmp = state->clues->roomvector[state->coord[2]*sz*sz + state->coord[1]*sz + state->coord[0]];
          if (tmp < 4) {
            if ((tmp & ~1) == 2) {
              sprintf(retstr, "d");
              return dupstr(retstr);
            }
          } else {
            sprintf(retstr, "C%d", tmp-4);
            return dupstr(retstr);
          }
        }
      }
      return NULL;
    }

    if (IS_CURSOR_MOVE(button)) {
      int tx1 = (style != Tandem || ui->ballnr == 1 ? state->coord[0] : state->coord[2]);
      int ty1 = (style != Tandem || ui->ballnr == 1 ? state->coord[1] : state->coord[3]);
      tx = tx1, ty = ty1;
      if (tx <= 0 && ty == 0 && button == CURSOR_LEFT)
        tx = -1;
      else if (tx >= sz-1 && ty == sz-1 && button == CURSOR_RIGHT)
        tx = sz;
      else if (!(button == CURSOR_UP && tx == -1) && !(button == CURSOR_DOWN && tx == sz))
        move_cursor(button, &tx, &ty, sz, sz, 0, NULL);
      if ((tx != tx1 || ty != ty1) &&
          canmove(state, tx1, ty1, tx, ty, &dir)) {
        if (style == Tandem) {
          sprintf(retstr, "%c%c", (ui->ballnr == 1 ? 'A' : 'B'),
                  (dir==0 ? 'e' : dir==1 ? 'w' : dir==2 ? 's' : 'n'));
        } else
          sprintf(retstr, "%c", (dir==0 ? 'e' : dir==1 ? 'w' : dir==2 ? 's' : 'n'));
        if (ui->tshow)
          ui->tshow = 0, ui->tpos[0] = -1, ui->tpos[1] = -1;
        return dupstr(retstr);
      } else
        return MOVE_UI_UPDATE;
    }

    if ((button == 's' || button == 'S') && state->cheated && state->clues->sol) {
      char ch = state->clues->sol[getindex(state->coord, state->par)];
      if (ch == 't') {
        if (style == Floors) {
          int tmp = state->clues->roomvector[state->coord[2]*sz*sz + state->coord[1]*sz + state->coord[0]];
          if (tmp != -1) {
            sprintf(retstr, "F%d", tmp);
            return dupstr(retstr);
          }
        } else if (style == Keys) {
          int tmp = state->clues->roomvector[state->coord[1]*sz + state->coord[0]];
          if (tmp != -1) {
            sprintf(retstr, "K%d", tmp);
            return dupstr(retstr);
          }
        } else if (style == Levers) {
          int tmp = state->clues->roomvector[state->coord[1]*sz + state->coord[0]];
          if (tmp != -1) {
            sprintf(retstr, "L%d", tmp);
            return dupstr(retstr);
          }
        } else if (style == Combo) {
          int tmp = state->clues->roomvector[state->coord[2]*sz*sz + state->coord[1]*sz + state->coord[0]];
          if (tmp >= 4) {
            sprintf(retstr, "C%d", tmp-4);
            return dupstr(retstr);
          }
        }
      } else if (style == Tandem) {
        /* coord 0,1 is always ball A, whereas uppercase is always the lower rightmost ball */
        char bch = (((state->coord[3]*sz + state->coord[2] >= state->coord[1]*sz + state->coord[0]) ^ ((ch & 0x20) == 0)) ? 'A' : 'B');
        sprintf(retstr, "%c%c", bch, (ch | 0x20));
        return dupstr(retstr);
      } else {
        sprintf(retstr, "%c", ch);
        return dupstr(retstr);
      }
    }

    return NULL;
}

static game_state *execute_move(const game_state *from, const char *move)
{
    game_state *ret;
    int boff, tmp;

    if (move[0] == 'S') {
      ret = dup_game(from);
      if (!ret->clues->sol)
        ret->clues->sol = dupstr(move+1);
      ret->cheated = true;
      return ret;
    } else if (move[0] == 'T') {
      ret = dup_game(from);
      return ret;
    } else if (move[0] == 'F') {
      ret = dup_game(from);
      tmp = atoi(move+1);
      ret->coord[2] = tmp;
      return ret;
    } else if (move[0] == 'K' || move[0] == 'L') {
      ret = dup_game(from);
      tmp = atoi(move+1);
      if (move[0] == 'K')
        ret->coord[2] |= (1<<tmp);
      else
        ret->coord[2] ^= (1<<tmp);
      return ret;
    } else if (move[0] == 'C') {
      ret = dup_game(from);
      tmp = atoi(move+1);
      if (tmp < from->par->levers)
        ret->coord[3] ^= (1<<tmp);
      else
        ret->coord[3] |= (1<<tmp);
      return ret;
    } else {
      if (move[0] == 'A' || move[0] == 'B') {
        boff = (move[0] == 'A' ? 0 : 2);
        move++;
      } else
        boff = 0;
      ret = dup_game(from);
      if (move[0]=='n')
        ret->coord[boff+1]--;
      else if (move[0]=='e')
        ret->coord[boff]++;
      else if (move[0]=='s')
        ret->coord[boff+1]++;
      else if (move[0]=='w')
        ret->coord[boff]--;
      else if (move[0]=='u')
        ret->coord[2]++;
      else if (move[0]=='d')
        ret->coord[2]--;
      return ret;
    }
    return (game_state*) from;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
			      const game_ui *ui, int *x, int *y)
{
  *x = TOTSIZEX(params->size, tilesize);
  *y = TOTSIZEY(params->size, tilesize);
}

static void game_set_size(drawing *dr, game_drawstate *ds,
			  const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}

/* simple lightness changing routines, use sqrt for approx gamma compensation */
static void darken_colour(float* dest, float* src, float prop)
{
  int i;
  for (i=0; i<3; i++)
    dest[i] = src[i] * sqrt(1.0-prop); 
}

static void lighten_colour(float* dest, float* src, float prop)
{
  int i;
  for (i=0; i<3; i++)
    dest[i] = sqrt(1.0 - (1.0 - src[i]*src[i]) * (1.0-prop));
}

static void set_colour(float* dest, float r, float g, float b)
{
  dest[0] = sqrt(r), dest[1] = sqrt(g), dest[2] = sqrt(b);
}

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);

    frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

    set_colour(&ret[COL_GRID * 3], 0.0, 0.0, 0.0);
    set_colour(&ret[COL_DOOR * 3], 0.2, 0.2, 0.2);

    set_colour(&ret[COL_0_N * 3], 1.0, 1.0, 0.0);
    set_colour(&ret[COL_0_D * 3], 0.8, 0.8, 0.0);
    set_colour(&ret[COL_0_L * 3], 1.0, 1.0, 0.8);

    set_colour(&ret[COL_1_N * 3], 0.2, 1.0, 0.0);
    darken_colour(&ret[COL_1_D * 3], &ret[COL_1_N * 3], 0.25);
    lighten_colour(&ret[COL_1_L * 3], &ret[COL_1_N * 3], 0.25);

    set_colour(&ret[COL_2_N *3], 1.0, 0.56, 0.0);
    darken_colour(&ret[COL_2_D * 3], &ret[COL_2_N * 3], 0.25);
    lighten_colour(&ret[COL_2_L * 3], &ret[COL_2_N * 3], 0.25);

    set_colour(&ret[COL_3_N *3], 1.0, 0.1, 0.0);
    darken_colour(&ret[COL_3_D * 3], &ret[COL_3_N * 3], 0.25);
    lighten_colour(&ret[COL_3_L * 3], &ret[COL_3_N * 3], 0.25);

    set_colour(&ret[COL_4_N *3], 0.05, 0.05, 1.0);
    darken_colour(&ret[COL_4_D * 3], &ret[COL_4_N * 3], 0.25);
    lighten_colour(&ret[COL_4_L * 3], &ret[COL_4_N * 3], 0.25);

    set_colour(&ret[COL_5_N *3], 1.0, 0.0, 0.6);
    darken_colour(&ret[COL_5_D * 3], &ret[COL_5_N * 3], 0.25);
    lighten_colour(&ret[COL_5_L * 3], &ret[COL_5_N * 3], 0.25);

    set_colour(&ret[COL_6_N *3], 0.0, 0.5, 0.0);
    darken_colour(&ret[COL_6_D * 3], &ret[COL_6_N * 3], 0.25);
    lighten_colour(&ret[COL_6_L * 3], &ret[COL_6_N * 3], 0.25);

    set_colour(&ret[COL_7_N *3], 0.4, 0.0, 1.0);
    darken_colour(&ret[COL_7_D * 3], &ret[COL_7_N * 3], 0.25);
    lighten_colour(&ret[COL_7_L * 3], &ret[COL_7_N * 3], 0.25);

    set_colour(&ret[COL_8_N *3], 0.0, 0.6, 1.0);
    darken_colour(&ret[COL_8_D * 3], &ret[COL_8_N * 3], 0.25);
    lighten_colour(&ret[COL_8_L * 3], &ret[COL_8_N * 3], 0.25);

    set_colour(&ret[COL_9_N *3], 0.5, 0.3, 0.0);
    darken_colour(&ret[COL_9_D * 3], &ret[COL_9_N * 3], 0.25);
    lighten_colour(&ret[COL_9_L * 3], &ret[COL_9_N * 3], 0.25);

    lighten_colour(&ret[COL_SHADE0 * 3], &ret[COL_BACKGROUND * 3], 0.5);
    darken_colour(&ret[COL_SHADE1 * 3], &ret[COL_BACKGROUND * 3], 0.1);
    darken_colour(&ret[COL_SHADE2 * 3], &ret[COL_BACKGROUND * 3], 0.2);
    darken_colour(&ret[COL_SHADE3 * 3], &ret[COL_BACKGROUND * 3], 0.3);
    darken_colour(&ret[COL_SHADE4 * 3], &ret[COL_BACKGROUND * 3], 0.5);

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);
    int sz = state->par->size;
    int ncoord = numcoord(state->par);
    int fl = (state->par->style == ThreeD ? sz : state->par->style == Floors || state->par->style == Combo ? state->par->floors : 1);

    ds->tilesize = 0;
    ds->w = sz;
    ds->h = sz;
    ds->started = false;
    ds->pos = snewn(ncoord, int);
    ds->lastpos = snewn(ncoord, int);
    ds->doors = makedoorvector(sz, fl);
    ds->lastdoors = makedoorvector(sz, fl);
    ds->flash = ds->anim = false;
    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->pos);
    sfree(ds->lastpos);
    sfree(ds->doors);
    sfree(ds->lastdoors);
    sfree(ds);
}

/*
  Draw:
   . grid (utan dörrkarmar, med ev ingång o utgång, med ev bakrundsfärg)
   . solid dörr, h o v
   . öppningsbar dörr h o v (med karm, ev färg, öppet-proportion)
   . boll (olika färg)
   . trappa upp o ner
   . spak (färg, uppe-proportion)
   . nyckel (färg, i rum eller på boll)
   . fördjupning (färg, stor)
   . fördjupning (grå, liten, proportion)
 */

static int get_bgcol(const game_params *params, int* pos)
{
  if (params->style == Floors)
    return COL_0_L + 3*pos[2];
  else
    return -1;
}

static int count_bits(int n)
{
  n = (n & 0x5555) + ((n>>1) & 0x5555);
  n = (n & 0x3333) + ((n>>2) & 0x3333);
  n = (n & 0x0F0F) + ((n>>4) & 0x0F0F);
  n = (n & 0x00FF) + ((n>>8) & 0x00FF);
  return n;
}

static void draw_vert(drawing *dr, int xx, int yy, int tilesize, int num, int mode)
{
  int lasty, curry, i;
  int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
  int wdt2 = wdt1 + 2*((wdt1+5)/6);
  int brd = (wdt2+1)/2;
  int off1r = (tilesize < 16 ? 4 : tilesize/4);
  int off1l = off1r - wdt1%2;
  if (mode) {
    if (mode == 1 || mode == 2) { /* entry or exit*/
      int leadin = (mode == 2 ? num-1 : 0);
      int leadout = (mode == 1 ? num-1 : 0);
      draw_rect(dr, xx-wdt1/2, yy - wdt1/2, wdt1, (off1r + leadin*tilesize - brd + wdt1/2), COL_GRID);
      draw_rect(dr, xx-wdt2/2, yy + leadin*tilesize + off1r - brd, wdt2, brd, COL_GRID);
      draw_rect(dr, xx-wdt2/2, yy + (leadin+1)*tilesize - off1l, wdt2, brd, COL_GRID);
      draw_rect(dr, xx-wdt1/2, yy + (leadin+1)*tilesize - off1l + brd, wdt1, (leadout*tilesize + off1l - brd + (wdt1+1)/2), COL_GRID);
    } else { /* whole */
      draw_rect(dr, xx - wdt1/2, yy - wdt1/2, wdt1, num*tilesize + wdt1, COL_GRID);
    }
  } else {
    lasty = yy - wdt1/2;
    curry = yy + off1r - brd;
    for (i=0; i<num; i++) {
      draw_rect(dr, xx-wdt1/2, lasty, wdt1, (curry - lasty), COL_GRID);
      lasty = curry - off1r - off1l + brd*2 + tilesize;
      curry = curry + tilesize;
    }
    draw_rect(dr, xx - wdt1/2, lasty, wdt1, (curry - off1r + brd + (wdt1+1)/2 - lasty), COL_GRID);
  }
}

static void draw_horiz(drawing *dr, int xx, int yy, int tilesize, int num, int whole)
{
  int lastx, currx, i;
  int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
  int wdt2 = wdt1 + 2*((wdt1+5)/6);
  int brd = (wdt2+1)/2;
  int off1r = (tilesize < 16 ? 4 : tilesize/4);
  int off1l = off1r - wdt1%2;
  if (whole) {
    draw_rect(dr, xx - wdt1/2, yy - wdt1/2, num*tilesize + wdt1, wdt1, COL_GRID);
  } else {
    lastx = xx - wdt1/2;
    currx = xx + off1r - brd;
    for (i=0; i<num; i++) {
      draw_rect(dr, lastx, yy-wdt1/2, (currx - lastx), wdt1, COL_GRID);
      lastx = currx - off1r - off1l + brd*2 + tilesize;
      currx = currx + tilesize;
    }
    draw_rect(dr, lastx, yy-wdt1/2, (currx - off1r + brd + (wdt1+1)/2 - lastx), wdt1, COL_GRID);
  }
}

static void draw_grid(drawing *dr, int sz, int tilesize, int colnr, int entry, int exit)
{
  int i;
  if (colnr != -1)
    draw_rect(dr, COORDX(0, tilesize), COORDY(0, tilesize), sz*tilesize, sz*tilesize, COL_0_L + 3*colnr);    
  draw_horiz(dr, COORDX(0, tilesize), COORDY(0, tilesize), tilesize, sz, 1);
  for (i=1; i<sz; i++)
    draw_horiz(dr, COORDX(0, tilesize), COORDY(i, tilesize), tilesize, sz, 0);
  draw_horiz(dr, COORDX(0, tilesize), COORDY(sz, tilesize), tilesize, sz, 1);
  draw_vert(dr, COORDX(0, tilesize), COORDY(0, tilesize), tilesize, sz, (entry ? 1 : 3));
  for (i=1; i<sz; i++)
    draw_vert(dr, COORDX(i, tilesize), COORDY(0, tilesize), tilesize, sz, 0);
  draw_vert(dr, COORDX(sz, tilesize), COORDY(0, tilesize), tilesize, sz, (exit ? 2 : 3));
}

static void draw_hsoliddoor(drawing *dr, int xx, int yy, int tilesize)
{
  int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
  int wdt2 = wdt1 + 2*((wdt1+5)/6);
  int brd = (wdt2+1)/2;
  int off1r = (tilesize < 16 ? 4 : tilesize/4);
  int off1l = off1r - wdt1%2;
  int x1 = xx - tilesize/2 + off1r;
  int x2 = xx + (tilesize+1)/2 - off1l;
  draw_rect(dr, x1 - brd, yy - wdt1/2, (x2 - x1) + 2*brd, wdt1, COL_GRID);
}

static void draw_hdoor(drawing *dr, int xx, int yy, int tilesize, int colnr, int nobg, float propen)
{
  int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
  int wdt2 = wdt1 + 2*((wdt1+5)/6);
  int brd = (wdt2+1)/2;
  int wdt = wdt1 - 2*((wdt1+2)/6);
  int off1r = (tilesize < 16 ? 4 : tilesize/4);
  int off1l = off1r - wdt1%2;
  int x1 = xx - tilesize/2 + off1r;
  int x2 = xx + (tilesize+1)/2 - off1l;
  int dd;
  int doorcol = (colnr == -1 ? COL_DOOR : COL_0_N + 3*colnr);
  int framecol = (colnr == -1 ? COL_GRID : COL_0_D + 3*colnr);
  draw_rect(dr, x1 - brd, yy-wdt2/2, brd, wdt2, framecol);
  draw_rect(dr, x2, yy-wdt2/2, brd, wdt2, framecol);
  if (propen == 0.0)
    draw_rect(dr, x1, yy - wdt/2, (x2 - x1), wdt, doorcol);
  else if (propen == 1.0) {
    if (!nobg)
      draw_rect(dr, x1, yy - wdt/2, (x2 - x1), wdt, COL_BACKGROUND);
  } else {
    dd = (int)((x2 - x1) * (1.0 - propen) * 0.5 + 0.5);
    draw_rect(dr, x1, yy - wdt/2, dd, wdt, doorcol);
    draw_rect(dr, x1+dd, yy - wdt/2, (x2 - x1) - 2*dd, wdt, COL_BACKGROUND);
    draw_rect(dr, x2-dd, yy - wdt/2, dd, wdt, doorcol);
  }
}

static void draw_vsoliddoor(drawing *dr, int xx, int yy, int tilesize)
{
  int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
  int wdt2 = wdt1 + 2*((wdt1+5)/6);
  int brd = (wdt2+1)/2;
  int off1r = (tilesize < 16 ? 4 : tilesize/4);
  int off1l = off1r - wdt1%2;
  int y1 = yy - tilesize/2 + off1r;
  int y2 = yy + (tilesize+1)/2 - off1l;
  draw_rect(dr, xx - wdt1/2, y1 - brd, wdt1, (y2 - y1) + 2*brd, COL_GRID);
}

static void draw_vdoor(drawing *dr, int xx, int yy, int tilesize, int colnr, int nobg, float propen)
{
  int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
  int wdt2 = wdt1 + 2*((wdt1+5)/6);
  int brd = (wdt2+1)/2;
  int wdt = wdt1 - 2*((wdt1+2)/6);
  int off1r = (tilesize < 16 ? 4 : tilesize/4);
  int off1l = off1r - wdt1%2;
  int y1 = yy - tilesize/2 + off1r;
  int y2 = yy + (tilesize+1)/2 - off1l;
  int dd;
  int doorcol = (colnr == -1 ? COL_DOOR : COL_0_N + 3*colnr);
  int framecol = (colnr == -1 ? COL_GRID : COL_0_D + 3*colnr);
  draw_rect(dr, xx - wdt2/2, y1 - brd, wdt2, brd, framecol);
  draw_rect(dr, xx - wdt2/2, y2, wdt2, brd, framecol);
  if (propen == 0.0)
    draw_rect(dr, xx - wdt/2, y1, wdt, (y2 - y1), doorcol);
  else if (propen == 1.0) {
    if (!nobg)
      draw_rect(dr, xx - wdt/2, y1, wdt, (y2 - y1), COL_BACKGROUND);
  } else {
    dd = (int)((y2 - y1) * (1.0 - propen) * 0.5 + 0.5);
    draw_rect(dr, xx - wdt/2, y1, wdt, dd, doorcol);
    draw_rect(dr, xx - wdt/2, y1+dd, wdt, (y2 - y1) - 2*dd, COL_BACKGROUND);
    draw_rect(dr, xx - wdt/2, y2-dd, wdt, dd, doorcol);
  }
}

static void draw_ball(drawing *dr, int xx, int yy, int tilesize, int ballnr)
{
  int rad = (tilesize < 12 ? 1 : tilesize/4 - 2);
  int loff = (int)(0.714 * rad);
  int lrad = (int)(1.17*0.5*loff);
  int noff = loff - 1;
  int nrad = (int)(1.17*0.5*(noff + (0.714 * rad)));
  int col, dcol, lcol;
  if (ballnr == -1) {
    draw_circle(dr, xx, yy, rad+2, COL_BACKGROUND, COL_BACKGROUND);
  } else {
    col = COL_0_N + 3*ballnr;
    dcol = COL_0_D + 3*ballnr;
    lcol = COL_0_L + 3*ballnr;
    draw_circle(dr, xx+1, yy+1, rad, dcol, dcol);
    draw_circle(dr, xx, yy, rad, col, col);
    draw_circle(dr, xx-loff+lrad, yy-loff+lrad, lrad, lcol, lcol);
    draw_circle(dr, xx-noff+nrad, yy-noff+nrad, nrad, col, col);
  }
}

static void draw_test(drawing *dr, int xx, int yy, int tilesize, float prop, int ballnr)
{
  int rad = tilesize/4 - 3;
  int cx = xx + tilesize/2;
  int cy = yy + tilesize/2;
  int dep;
  int col;

  if (ballnr == -1) {
    draw_circle(dr, cx, cy, rad+2, COL_BACKGROUND, COL_BACKGROUND);
    if (prop) {
      dep = (int)(prop*2)+1;
      col = (dep == 1 ? COL_SHADE1 : dep == 2 ? COL_SHADE2 : COL_SHADE3);
      draw_circle(dr, cx+1, cy+1, rad, COL_SHADE0, COL_SHADE0);
      draw_circle(dr, cx-1, cy-1, rad, COL_SHADE4, COL_SHADE4);
      draw_circle(dr, cx, cy, rad + 2 - dep, col, col);
    }
  } else {
    draw_circle(dr, cx+2, cy, rad+5, COL_BACKGROUND, COL_BACKGROUND);
    if (prop) {
      dep = (int)(prop*2)+1;
      draw_circle(dr, cx+2*dep, cy+dep, rad, COL_SHADE3, COL_SHADE3);
      draw_ball(dr, cx, cy-dep, tilesize, ballnr);
    } else
      draw_ball(dr, cx, cy, tilesize, ballnr);
  }
}

static void draw_pit(drawing *dr, int xx, int yy, int tilesize, int colnr)
{
  int rad = (tilesize - tilesize/8)/3;
  int cx = xx + tilesize/2;
  int cy = yy + tilesize/2;
  int col = COL_0_L + 3*colnr;

  draw_circle(dr, cx, cy, rad+2, COL_BACKGROUND, COL_BACKGROUND);
  draw_circle(dr, cx+1, cy+1, rad, COL_SHADE0, COL_SHADE0);
  draw_circle(dr, cx-1, cy-1, rad, COL_SHADE4, COL_SHADE4);
  draw_circle(dr, cx, cy, rad-1, col, col);
}

static void draw_stairs(drawing *dr, int x, int y, int tilesize, int dirs)
{
  int i;
  int shgt = (tilesize < 16 ? 1 : tilesize / 16);
  int ybase = y + tilesize/2;
  int xbase = x + tilesize/2 + (tilesize/2 - (shgt+1)*3)/2;
  if (dirs & 1) { /* Up */
    draw_rect(dr, xbase, ybase - 3*shgt, 3*(shgt+1), 3*shgt, COL_SHADE4);
    for (i=0; i<3; i++) {
      draw_rect(dr, xbase + i*(shgt+1), ybase - (i+4)*shgt, 1, 4*shgt, COL_SHADE0);
      draw_rect(dr, xbase + i*(shgt+1) + 1, ybase - (i+4)*shgt, shgt, 3*shgt, COL_SHADE2);
    }
  }
  if (dirs & 2) { /* Down */
    draw_rect(dr, xbase, ybase + shgt, 3*(shgt+1), 4*shgt, COL_SHADE4);
    for (i=0; i<3; i++)
      draw_rect(dr, xbase + i*(shgt+1) + 1, ybase + (i+2)*shgt, shgt, (3-i)*shgt, COL_SHADE2);
  }
}

static void draw_key(drawing *dr, int x, int y, int tilesize, int colnr, int xoff)
{
  int i;
  int maxrad = (tilesize < 12 ? 1 : tilesize / 12);
  int minrad = (maxrad + 1) / 2;
  int edge = (minrad + 1) / 2;
  int core = 2*(minrad - edge);
  int len = 2*(maxrad+1);
  int basey = y + tilesize/2;
  int basex = x + tilesize/2 + xoff;
  int col = COL_0_N + 3*colnr;
  for (i=minrad; i<=maxrad; i++)
    draw_circle(dr, basex, basey - (maxrad+1), i, -1, col);
  draw_rect(dr, basex - core/2, basey, core, len, col);
  draw_rect(dr, basex + core/2, basey, edge, len - edge, col);
  draw_rect(dr, basex - core/2 - edge, basey, edge, edge, col);
  draw_rect(dr, basex - core/2 - edge, basey + len/2, edge, edge, col);
}

static void draw_ballkeys(drawing *dr, int x, int y, int tilesize, int keybits, int coloff)
{
  if (keybits) {
    int nk = count_bits(keybits);
    int delta = (tilesize/2 - tilesize/6) / nk;
    int off = -delta * (nk-1) / 2;
    int i, b;
    for (i=0, b=1; b<=keybits; i++, b<<=1)
      if (keybits & b) {
        draw_key(dr, x, y, tilesize, i+coloff, off);
        off += delta;
      }
  }
}

static void draw_lever(drawing *dr, int x, int y, int tilesize, int colnr, float prop)
{
  int lrad = (tilesize < 8 ? 1 : tilesize / 8);
  int ybase = y + tilesize/2;
  int xbase = x + (3*tilesize)/4;
  int ydiff = (int)((tilesize/4) * (1.0 - 2.0*prop));
  int col = COL_0_N + 3*colnr;
  int dcol = COL_0_D + 3*colnr;
  draw_rect(dr, xbase - lrad/2, ybase + (ydiff > 0 ? -lrad/2 : ydiff+lrad/2), lrad/2, (ydiff > 0 ? ydiff : -ydiff), COL_SHADE2);
  draw_rect(dr, xbase, ybase + (ydiff > 0 ? -lrad/2 : ydiff+lrad/2), lrad/2+1, (ydiff > 0 ? ydiff : -ydiff), COL_SHADE4);
  draw_rect(dr, xbase - lrad/4, ybase + (ydiff > 0 ? -3*lrad/4 : ydiff+3*lrad/4), (lrad+3)/2, (ydiff > 0 ? ydiff : -ydiff), COL_SHADE3);
  draw_circle(dr, xbase + 1, ybase + ydiff + 1, lrad, dcol, dcol);
  draw_circle(dr, xbase, ybase + ydiff, lrad, col, col);
}


static void draw_room(drawing *dr, const game_state *state, int tilesize, int* pos, int x, int y)
{
  int open, colnr;
  int sz = state->par->size;
  SmStyle style = state->par->style;
  if (x < 0 || x >= sz) return;
  if (style == ThreeD) {
    open = state->clues->roomvector[pos[2]*sz*sz + y*sz + x];
    draw_stairs(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, open);
  } else if (style == Floors) {
    colnr = state->clues->roomvector[pos[2]*sz*sz + y*sz + x];
    if (colnr != -1)
      draw_pit(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, colnr);
  } else if (style == Keys) {
    colnr = state->clues->roomvector[y*sz + x];
    if (colnr != -1 && !(pos[2]&(1<<colnr)))
      draw_key(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, colnr+1, tilesize/4);
  } else if (style == Levers) {
    colnr = state->clues->roomvector[y*sz + x];
    if (colnr != -1)
      draw_lever(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, colnr+1, (pos[2]&(1<<colnr) ? 1.0 : 0.0));
  } else if (style == Combo) {
    open = state->clues->roomvector[pos[2]*sz*sz + y*sz + x];
    if (open < 4)
      draw_stairs(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, open);
    else if (open < state->par->levers + 4)
      draw_lever(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, open - 4, (pos[3]&(1<<(open-4)) ? 1.0 : 0.0));
    else if (!(pos[3]&(1<<(open-4))))
      draw_key(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, open - state->par->levers + 1, tilesize/4);
  }
}

static void draw_scene(drawing *dr, game_drawstate *ds, const game_state *state, const game_ui *ui, int nobg)
{
  int x, y, i;
  int open, colnr, z;
  int sz = state->par->size;
  SmStyle style = state->par->style;
  int tilesize = ds->tilesize;
  if (!nobg)
    draw_rect(dr, 0, 0, TOTSIZEX(sz, tilesize), TOTSIZEY(sz, tilesize), COL_BACKGROUND);
  draw_grid(dr, sz, tilesize,
            (style == Floors && !nobg ? ds->pos[2] : -1),
            !((style == ThreeD && ds->pos[2]!=0) ||
              (style == Floors && ds->pos[2]!=0) ||
              (style == Combo && ds->pos[2]!=0)),
            !((style == ThreeD && ds->pos[2]!=sz-1) ||
              (style == Floors && ds->pos[2]!=state->par->floors-1) ||
              (style == Combo && ds->pos[2]!=state->par->floors-1))); 

  /* Doors */
  z = (style==ThreeD || style==Floors || style==Combo ? ds->pos[2] : 0);
  for (x=0; x<sz; x++) {
    for (y=0; y<sz-1; y++, i++) {
      open = getdoor(ds->doors, sz, x, y, z, 2);
      if (style==Keys || style==Levers || style==Combo) {
        colnr = state->clues->doorprop[doorbitpos(sz, x, y, z, 2)];
        if (colnr>=0 && style!=Combo)
          colnr += 1;
        else if (colnr>=state->par->levers)
          colnr += 5 - state->par->levers;
      } else
        colnr = -1;
      if (!open && ((style==Basic && !state->par->difficult) ||
                    style==ThreeD || style==Floors ||
                    ((style==Keys  || style==Levers || style==Combo) && colnr==-1)))
        draw_hsoliddoor(dr, COORDX(x, tilesize)+tilesize/2, COORDY(y, tilesize)+tilesize, tilesize);
      else
        draw_hdoor(dr, COORDX(x, tilesize)+tilesize/2, COORDY(y, tilesize)+tilesize,
                   tilesize, colnr, (style == Floors) || nobg, (open ? 1.0 : 0.0));
    }
  }
  for (x=0; x<sz-1; x++) {
    for (y=0; y<sz; y++, i++) {
      open = getdoor(ds->doors, sz, x, y, z, 0);
      if (style==Keys || style==Levers || style==Combo) {
        colnr = state->clues->doorprop[doorbitpos(sz, x, y, z, 0)];
        if (colnr>=0 && style!=Combo)
          colnr += 1;
        else if (colnr>=state->par->levers)
          colnr += 5 - state->par->levers;
      } else
        colnr = -1;
      if (!open && ((style==Basic && !state->par->difficult) ||
                    style==ThreeD || style==Floors ||
                    ((style==Keys  || style==Levers || style==Combo) && colnr==-1)))
        draw_vsoliddoor(dr, COORDX(x, tilesize)+tilesize, COORDY(y, tilesize)+tilesize/2, tilesize);
      else
        draw_vdoor(dr, COORDX(x, tilesize)+tilesize, COORDY(y, tilesize)+tilesize/2,
                   tilesize, colnr, (style == Floors) || nobg, (open ? 1.0 : 0.0));
    }
  }

  /* Room features */
  if (style == ThreeD) {
    for (x=0; x<sz; x++)
      for (y=0; y<sz; y++, i++) {
        open = state->clues->roomvector[ds->pos[2]*sz*sz + y*sz + x];
        draw_stairs(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, open);
      }
  } else if (style == Floors) {
    for (x=0; x<sz; x++)
      for (y=0; y<sz; y++, i++) {
        colnr = state->clues->roomvector[ds->pos[2]*sz*sz + y*sz + x];
        if (colnr != -1)
          draw_pit(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, colnr);
      }
  } else if (style == Keys) {
    for (x=0; x<sz; x++)
      for (y=0; y<sz; y++, i++) {
        colnr = state->clues->roomvector[y*sz + x];
        if (colnr != -1 && !(ds->pos[2]&(1<<colnr)))
          draw_key(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, colnr+1, tilesize/4);
      }
  } else if (style == Levers) {
    for (x=0; x<sz; x++)
      for (y=0; y<sz; y++, i++) {
        colnr = state->clues->roomvector[y*sz + x];
        if (colnr != -1)
          draw_lever(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, colnr+1, (ds->pos[2]&(1<<colnr) ? 1.0 : 0.0));
      }
  } else if (style == Combo) {
    for (x=0; x<sz; x++)
      for (y=0; y<sz; y++, i++) {
        open = state->clues->roomvector[ds->pos[2]*sz*sz + y*sz + x];
        if (open < 4)
          draw_stairs(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, open);
        else if (open < state->par->levers + 4)
          draw_lever(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, open - 4, (ds->pos[3]&(1<<(open-4)) ? 1.0 : 0.0));
        else if (!(ds->pos[3]&(1<<(open-4))))
          draw_key(dr, COORDX(x, tilesize), COORDY(y, tilesize), tilesize, open - state->par->levers + 1, tilesize/4);
      }
  }
}

static void draw_sceneballs(drawing *dr, game_drawstate *ds, const game_state *state, const game_ui *ui)
{
  SmStyle style = state->par->style;
  int tilesize = ds->tilesize;
  if (style == Tandem) {
    if (ds->pos[0] == ds->pos[2] && ds->pos[1] == ds->pos[3]) {
      draw_ball(dr, COORDX(ds->pos[0], tilesize)+tilesize*5/12, COORDY(ds->pos[1], tilesize)+tilesize*3/8, tilesize, 1);
      draw_ball(dr, COORDX(ds->pos[2], tilesize)+tilesize*7/12, COORDY(ds->pos[3], tilesize)+tilesize*5/8, tilesize, 2);
    } else {
      draw_ball(dr, COORDX(ds->pos[0], tilesize)+tilesize/2, COORDY(ds->pos[1], tilesize)+tilesize/2, tilesize, 1);
      draw_ball(dr, COORDX(ds->pos[2], tilesize)+tilesize/2, COORDY(ds->pos[3], tilesize)+tilesize/2, tilesize, 2);
    }
    if (ui->tshow) {
      draw_test(dr, COORDX(ds->testpos[0], tilesize), COORDY(ds->testpos[1], tilesize),
                tilesize, 1.0,
                (ds->pos[0] == ds->testpos[0] && ds->pos[1] == ds->testpos[1] ? 1 :
                 ds->pos[2] == ds->testpos[0] && ds->pos[3] == ds->testpos[1] ? 2 : -1));
    }
  } else if (style == Keys) {
    draw_ball(dr, COORDX(ds->pos[0], tilesize)+tilesize/2, COORDY(ds->pos[1], tilesize)+tilesize/2, tilesize, 0);
    draw_ballkeys(dr, COORDX(ds->pos[0], tilesize), COORDY(ds->pos[1], tilesize), tilesize, ds->pos[2], 1);
  } else if (style == Combo) {
    draw_ball(dr, COORDX(ds->pos[0], tilesize)+tilesize/2, COORDY(ds->pos[1], tilesize)+tilesize/2, tilesize, 0);
    draw_ballkeys(dr, COORDX(ds->pos[0], tilesize), COORDY(ds->pos[1], tilesize), tilesize, ds->pos[3] & (((1<<state->par->keys)-1)<<state->par->levers), 5 - state->par->levers);
  } else {
    draw_ball(dr, COORDX(ds->pos[0], tilesize)+tilesize/2, COORDY(ds->pos[1], tilesize)+tilesize/2, tilesize, 0);
  }
}

static void draw_cleanupanim(drawing *dr, game_drawstate *ds, const game_state *state)
{
  int boff, top, bot, left, right;
  SmStyle style = state->par->style;
  int tilesize = ds->tilesize;
  if (style == Tandem) {
    for (boff=0; boff<4; boff+=2)
      if (ds->pos[boff] != ds->lastpos[boff] || ds->pos[boff+1] != ds->lastpos[boff+1]) {
        left = min(COORDX(ds->pos[boff], tilesize), COORDX(ds->lastpos[boff], tilesize))+tilesize/4;
        right = max(COORDX(ds->pos[boff]+1, tilesize), COORDX(ds->lastpos[boff]+1, tilesize))-tilesize/4;
        top = min(COORDY(ds->pos[boff+1], tilesize), COORDY(ds->lastpos[boff+1], tilesize))+tilesize/4;
        bot = max(COORDY(ds->pos[boff+1]+1, tilesize), COORDY(ds->lastpos[boff+1]+1, tilesize))-tilesize/4;
        draw_rect(dr, left, top, right-left, bot-top, COL_BACKGROUND);
      }
    if (ds->lasttestpos[0] != -1 && ds->lasttestpos[1] != -1 &&
        (ds->lasttestpos[0] != ds->testpos[0] || ds->lasttestpos[1] != ds->testpos[1]) &&
        ((ds->lastpos[0] == ds->lasttestpos[0] && ds->lastpos[1] == ds->lasttestpos[1]) ||
         (ds->lastpos[2] == ds->lasttestpos[0] && ds->lastpos[3] == ds->lasttestpos[1])))
      draw_circle(dr, COORDX(ds->lasttestpos[0], tilesize)+tilesize/2+2, COORDY(ds->lasttestpos[1], tilesize)+tilesize/2, tilesize/4+2, COL_BACKGROUND, COL_BACKGROUND);
  }
}

static void draw_preanim(drawing *dr, game_drawstate *ds, const game_state *state)
{
  SmStyle style = state->par->style;
  int tilesize = ds->tilesize;

  if (style == Tandem) {
    if (ds->lastpos[0] == ds->lastpos[2] && ds->lastpos[1] == ds->lastpos[3] &&
        (ds->lastpos[0] != ds->pos[0] || ds->lastpos[1] != ds->pos[1] ||
         ds->lastpos[2] != ds->pos[2] || ds->lastpos[3] != ds->pos[3]))
      draw_rect(dr, COORDX(ds->lastpos[0], tilesize)+tilesize/8+1, COORDY(ds->lastpos[1], tilesize)+tilesize/8, tilesize-2*(tilesize/8)-2, tilesize-2*(tilesize/8), COL_BACKGROUND);
    if (ds->lasttestpos[0] != -1 && ds->lasttestpos[1] != -1 &&
        (ds->lasttestpos[0] != ds->testpos[0] || ds->lasttestpos[1] != ds->testpos[1]) &&
        ((ds->lastpos[0] == ds->lasttestpos[0] && ds->lastpos[1] == ds->lasttestpos[1]) ||
         (ds->lastpos[2] == ds->lasttestpos[0] && ds->lastpos[3] == ds->lasttestpos[1])))
      draw_circle(dr, COORDX(ds->lasttestpos[0], tilesize)+tilesize/2+2, COORDY(ds->lasttestpos[1], tilesize)+tilesize/2, tilesize/4+2, COL_BACKGROUND, COL_BACKGROUND);
  }
}

static void draw_animation(drawing *dr, game_drawstate *ds, const game_state *state, int bgcol, float animtime)
{
  static float lastanimtime = 0.0;
  int x, y, i;
  int o1, o2, boff, top, bot, left, right, xp, yp, dbl, z, colnr;
  float pr;
  float prop0 = animtime / ANIM_TIME;
  float prop1 = (animtime > ANIM_TIME*0.6 ? 1.0 : (animtime)/(ANIM_TIME*0.6));
  float prop2 = (animtime < ANIM_TIME*0.4 ? 0.0 : (animtime - ANIM_TIME*0.4)/(ANIM_TIME*0.6));
  int sz = state->par->size;
  SmStyle style = state->par->style;
  int tilesize = ds->tilesize;

  if (style!=Combo || ds->pos[2] == ds->lastpos[2]) {
    z = (style==ThreeD || style==Floors || style==Combo ? ds->pos[2] : 0);
    for (x=0; x<sz; x++) {
      for (y=0; y<sz-1; y++, i++) {
        o1 = getdoor(ds->lastdoors, sz, x, y, z, 2);
        o2 = getdoor(ds->doors, sz, x, y, z, 2);
        if (o1 != o2) {
          pr = prop2*o2 + (1.0-prop2)*o1;
          if (style==Keys || style==Levers || style==Combo) {
            colnr = state->clues->doorprop[doorbitpos(sz, x, y, z, 2)];
            if (colnr>=0 && style!=Combo)
              colnr += 1;
            else if (colnr>=state->par->levers)
              colnr += 5 - state->par->levers;
          } else
            colnr = -1;
          draw_hdoor(dr, COORDX(x, tilesize)+tilesize/2, COORDY(y, tilesize)+tilesize,
                     tilesize, colnr, 0, pr);
        }
      }
    }
    for (x=0; x<sz-1; x++) {
      for (y=0; y<sz; y++, i++) {
        o1 = getdoor(ds->lastdoors, sz, x, y, z, 0);
        o2 = getdoor(ds->doors, sz, x, y, z, 0);
        if (o1 != o2) {
          pr = prop2*o2 + (1.0-prop2)*o1;
          if (style==Keys || style==Levers || style==Combo) {
            colnr = state->clues->doorprop[doorbitpos(sz, x, y, z, 0)];
            if (colnr>=0 && style!=Combo)
              colnr += 1;
            else if (colnr>=state->par->levers)
              colnr += 5 - state->par->levers;
          } else
            colnr = -1;
          draw_vdoor(dr, COORDX(x, tilesize)+tilesize, COORDY(y, tilesize)+tilesize/2,
                     tilesize, colnr, 0, pr);
        }
      }
    }
  }

  if (style == ThreeD) {
    if (ds->pos[2] != ds->lastpos[2]) {
      int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
      int wdt2 = wdt1 + 2*((wdt1+5)/6);
      int delta;
      delta = (tilesize/2 - wdt2);
      x = COORDX(ds->lastpos[0], tilesize);
      y = COORDY(ds->lastpos[1], tilesize);
      draw_rect(dr, x + (wdt2+1)/2, y + (wdt2+1)/2, tilesize - wdt2, tilesize - wdt2, (bgcol == -1 ? COL_BACKGROUND : bgcol));
      if (prop0 < 0.5) {
        draw_room(dr, state, ds->tilesize, ds->lastpos, ds->lastpos[0], ds->lastpos[1]);
        xp = x + tilesize/2 + delta*prop0;
        yp = y + tilesize/2 + delta*prop0*(ds->pos[2]>ds->lastpos[2] ? -1 : 1);
        draw_ball(dr, xp, yp, tilesize, 0);
      } else if (lastanimtime < 0.5 * ANIM_TIME) {
        draw_scene(dr, ds, state, 0, 0);
      } else {
        draw_room(dr, state, ds->tilesize, ds->pos, ds->lastpos[0], ds->lastpos[1]);
        xp = x + tilesize/2 + delta*(1.0 - prop0);
        yp = y + tilesize/2 + delta*(1.0 - prop0)*(ds->pos[2]>ds->lastpos[2] ? 1 : -1);
        draw_ball(dr, xp, yp, tilesize, 0);
      }
    }
  } else if (style == Floors) {
    if (ds->pos[2] != ds->lastpos[2]) {
      int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
      int wdt2 = wdt1 + 2*((wdt1+5)/6);
      int delta;
      delta = (tilesize/2 - wdt2);
      x = COORDX(ds->lastpos[0], tilesize);
      y = COORDY(ds->lastpos[1], tilesize);
      if (prop0 < 0.5) {
        draw_rect(dr, x + (wdt2+1)/2, y + (wdt2+1)/2, tilesize - wdt2, tilesize - wdt2, get_bgcol(state->par, ds->lastpos)); /* (bgcol == -1 ? COL_BACKGROUND : bgcol) deosnt work, need to switch col in middle */
        draw_room(dr, state, ds->tilesize, ds->lastpos, ds->lastpos[0], ds->lastpos[1]);
        xp = x + tilesize/2;
        yp = y + tilesize/2 - delta*0.5*(1.0 - (4.0*prop0 - 1.0)*(4.0*prop0 - 1.0));
        draw_ball(dr, xp, yp, tilesize, 0);
      } else if (lastanimtime < 0.5 * ANIM_TIME) {
        draw_scene(dr, ds, state, 0, 0);
      } else {
        draw_rect(dr, x + (wdt2+1)/2, y + (wdt2+1)/2, tilesize - wdt2, tilesize - wdt2, get_bgcol(state->par, ds->pos));
        draw_room(dr, state, ds->tilesize, ds->pos, ds->lastpos[0], ds->lastpos[1]);
        xp = x + tilesize/2;
        yp = y + tilesize/2 + delta*0.25*(1.0 - (4.0*prop0 - 3.0)*(4.0*prop0 - 3.0));
        draw_ball(dr, xp, yp, tilesize, 0);
      }
    }
  } else if (style == Keys) {
    colnr = (ds->lastpos[0] >= 0 && ds->lastpos[0] < sz ?
             state->clues->roomvector[ds->lastpos[1]*sz + ds->lastpos[0]] :
             -1);
    if (colnr != -1) {
      x = COORDX(ds->lastpos[0], tilesize);
      y = COORDY(ds->lastpos[1], tilesize);
      o1 = (ds->lastpos[2]&(1<<colnr) ? 1 : 0);
      o2 = (ds->pos[2]&(1<<colnr) ? 1 : 0);
      if (o1 != o2) {
        int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
        int wdt2 = wdt1 + 2*((wdt1+5)/6);
        int off;
        int* pos = (o2 ? ds->lastpos : ds->pos);
        draw_rect(dr, x + (wdt2+1)/2, y + (wdt2+1)/2, tilesize - wdt2, tilesize - wdt2, (bgcol == -1 ? COL_BACKGROUND : bgcol));
        draw_ball(dr, x+tilesize/2, y+tilesize/2, tilesize, 0);
        draw_ballkeys(dr, x, y, tilesize, pos[2], 1);
        off = (1.0 - o1 + (o1 - o2)*prop0)*tilesize/4;
        draw_key(dr, x, y, tilesize, colnr+1, off);
      }
    }
  } else if (style == Levers) {
    colnr = (ds->lastpos[0] >= 0 && ds->lastpos[0] < sz ?
             state->clues->roomvector[ds->lastpos[1]*sz + ds->lastpos[0]] :
             -1);
    if (colnr != -1) {
      x = COORDX(ds->lastpos[0], tilesize);
      y = COORDY(ds->lastpos[1], tilesize);
      o1 = (ds->lastpos[2]&(1<<colnr) ? 1 : 0);
      o2 = (ds->pos[2]&(1<<colnr) ? 1 : 0);
      if (o1 != o2) {
        int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
        int wdt2 = wdt1 + 2*((wdt1+5)/6);
        pr = prop0*o2 + (1.0-prop0)*o1;
        draw_rect(dr, x + (wdt2+1)/2, y + (wdt2+1)/2, tilesize - wdt2, tilesize - wdt2, (bgcol == -1 ? COL_BACKGROUND : bgcol));
        draw_lever(dr, x, y, tilesize, colnr+1, pr);
        draw_ball(dr, x+tilesize/2, y+tilesize/2, tilesize, 0);
      }
    }
  } else if (style == Combo) {
    if (ds->pos[2] != ds->lastpos[2]) {
      int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
      int wdt2 = wdt1 + 2*((wdt1+5)/6);
      int delta;
      delta = (tilesize/2 - wdt2);
      x = COORDX(ds->lastpos[0], tilesize);
      y = COORDY(ds->lastpos[1], tilesize);
      draw_rect(dr, x + (wdt2+1)/2, y + (wdt2+1)/2, tilesize - wdt2, tilesize - wdt2, (bgcol == -1 ? COL_BACKGROUND : bgcol));
      if (prop0 < 0.5) {
        draw_room(dr, state, ds->tilesize, ds->lastpos, ds->lastpos[0], ds->lastpos[1]);
        xp = x + delta*prop0;
        yp = y + delta*prop0*(ds->pos[2]>ds->lastpos[2] ? -1 : 1);
        draw_ball(dr, xp+tilesize/2, yp+tilesize/2, tilesize, 0);
        draw_ballkeys(dr, xp, yp, tilesize, ds->pos[3] & (((1<<state->par->keys)-1)<<state->par->levers), 5 - state->par->levers);
      } else if (lastanimtime < 0.5 * ANIM_TIME) {
        draw_scene(dr, ds, state, 0, 0);
      } else {
        draw_room(dr, state, ds->tilesize, ds->pos, ds->lastpos[0], ds->lastpos[1]);
        xp = x + delta*(1.0 - prop0);
        yp = y + delta*(1.0 - prop0)*(ds->pos[2]>ds->lastpos[2] ? 1 : -1);
        draw_ball(dr, xp+tilesize/2, yp+tilesize/2, tilesize, 0);
        draw_ballkeys(dr, xp, yp, tilesize, ds->pos[3] & (((1<<state->par->keys)-1)<<state->par->levers), 5 - state->par->levers);
      }
    } else {
      colnr = (ds->lastpos[0] >= 0 && ds->lastpos[0] < sz ?
               state->clues->roomvector[ds->lastpos[2]*sz*sz + ds->lastpos[1]*sz + ds->lastpos[0]] :
               -1);
      if (colnr-4 >= state->par->levers) {
        x = COORDX(ds->lastpos[0], tilesize);
        y = COORDY(ds->lastpos[1], tilesize);
        o1 = (ds->lastpos[3]&(1<<(colnr-4)) ? 1 : 0);
        o2 = (ds->pos[3]&(1<<(colnr-4)) ? 1 : 0);
        if (o1 != o2) {
          int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
          int wdt2 = wdt1 + 2*((wdt1+5)/6);
          int off;
          int* pos = (o2 ? ds->lastpos : ds->pos);
          draw_rect(dr, x + (wdt2+1)/2, y + (wdt2+1)/2, tilesize - wdt2, tilesize - wdt2, (bgcol == -1 ? COL_BACKGROUND : bgcol));
          draw_ball(dr, x+tilesize/2, y+tilesize/2, tilesize, 0);
          draw_ballkeys(dr, x, y, tilesize, pos[3] & (((1<<state->par->keys)-1)<<state->par->levers), 5 - state->par->levers);
          off = (1.0 - o1 + (o1 - o2)*prop0)*tilesize/4;
          draw_key(dr, x, y, tilesize, colnr-state->par->levers+1, off);
        }
      } else if (colnr-4 >= 0) {
        x = COORDX(ds->lastpos[0], tilesize);
        y = COORDY(ds->lastpos[1], tilesize);
        o1 = (ds->lastpos[3]&(1<<(colnr-4)) ? 1 : 0);
        o2 = (ds->pos[3]&(1<<(colnr-4)) ? 1 : 0);
        if (o1 != o2) {
          int wdt1 = (tilesize < 8 ? 1 : tilesize/8);
          int wdt2 = wdt1 + 2*((wdt1+5)/6);
          pr = prop0*o2 + (1.0-prop0)*o1;
          draw_rect(dr, x + (wdt2+1)/2, y + (wdt2+1)/2, tilesize - wdt2, tilesize - wdt2, (bgcol == -1 ? COL_BACKGROUND : bgcol));
          draw_lever(dr, x, y, tilesize, colnr-4, pr);
          draw_ball(dr, x+tilesize/2, y+tilesize/2, tilesize, 0);
          draw_ballkeys(dr, x, y, tilesize, ds->pos[3] & (((1<<state->par->keys)-1)<<state->par->levers), 5 - state->par->levers);
        }
      }
    }
  }

  if (style == Tandem) {
    if (ds->lasttestpos[0] != -1 && ds->lasttestpos[1] != -1 && 
        (ds->lasttestpos[0] != ds->testpos[0] || ds->lasttestpos[1] != ds->testpos[1])) {
      draw_test(dr, COORDX(ds->lasttestpos[0], tilesize), COORDY(ds->lasttestpos[1], tilesize),
                tilesize, 1.0-prop1,
                (ds->pos[0] == ds->lasttestpos[0] && ds->pos[1] == ds->lasttestpos[1] ? 1 :
                 ds->pos[2] == ds->lasttestpos[0] && ds->pos[3] == ds->lasttestpos[1] ? 2 : -1));
    }
    if (ds->testpos[0] != -1 && ds->testpos[1] != -1 && 
        (ds->lasttestpos[0] != ds->testpos[0] || ds->lasttestpos[1] != ds->testpos[1])) {
      draw_test(dr, COORDX(ds->testpos[0], tilesize), COORDY(ds->testpos[1], tilesize),
                tilesize, prop1,
                (ds->pos[0] == ds->testpos[0] && ds->pos[1] == ds->testpos[1] ? 1 :
                 ds->pos[2] == ds->testpos[0] && ds->pos[3] == ds->testpos[1] ? 2 : -1));
    }
    for (boff=0; boff<4; boff+=2)
      if (ds->pos[boff] != ds->lastpos[boff] || ds->pos[boff+1] != ds->lastpos[boff+1]) {
        dbl = ((ds->pos[boff] == ds->pos[2-boff] && ds->pos[boff+1] == ds->pos[2-boff+1]) ||
               (ds->lastpos[boff] == ds->pos[2-boff] && ds->lastpos[boff+1] == ds->pos[2-boff+1]));
        left = COORDX(min(ds->pos[boff], ds->lastpos[boff]), tilesize) + tilesize/4;
        right = COORDX(max(ds->pos[boff], ds->lastpos[boff])+1, tilesize) - tilesize/4;
        top = COORDY(min(ds->pos[boff+1], ds->lastpos[boff+1]), tilesize) + tilesize/4;
        bot = COORDY(max(ds->pos[boff+1], ds->lastpos[boff+1])+1, tilesize) - tilesize/4;
        xp = COORDX(ds->pos[boff], tilesize)*prop0 + COORDX(ds->lastpos[boff], tilesize)*(1.0-prop0) + tilesize/2;
        yp = COORDY(ds->pos[boff+1], tilesize)*prop0 + COORDY(ds->lastpos[boff+1], tilesize)*(1.0-prop0) + tilesize/2;
        draw_rect(dr, left, top, right-left, bot-top, COL_BACKGROUND);
        if (dbl) draw_ball(dr, COORDX(ds->pos[2-boff], tilesize)+tilesize/2, COORDY(ds->pos[2-boff+1], tilesize)+tilesize/2, tilesize, boff ? 1 : 2);
        draw_ball(dr, xp, yp, tilesize, boff ? 2 : 1);
      }
  } else {
    if (ds->pos[0] != ds->lastpos[0] || ds->pos[1] != ds->lastpos[1]) {
      int leftmost = COORDX(0, tilesize);
      int rightmost = COORDX(sz, tilesize);
      left = COORDX(min(ds->pos[0], ds->lastpos[0]), tilesize) + tilesize/4;
      right = COORDX(max(ds->pos[0], ds->lastpos[0])+1, tilesize) - tilesize/4;
      top = COORDY(min(ds->pos[1], ds->lastpos[1]), tilesize) + tilesize/4;
      bot = COORDY(max(ds->pos[1], ds->lastpos[1])+1, tilesize) - tilesize/4;
      xp = COORDX(ds->pos[0], tilesize)*prop0 + COORDX(ds->lastpos[0], tilesize)*(1.0-prop0) + tilesize/2;
      yp = COORDY(ds->pos[1], tilesize)*prop0 + COORDY(ds->lastpos[1], tilesize)*(1.0-prop0) + tilesize/2;
      draw_rect(dr, left, top, right-left, bot-top, (bgcol == -1 ? COL_BACKGROUND : bgcol));
      if (left < leftmost)
        draw_rect(dr, left, top, leftmost - left, bot-top, COL_BACKGROUND);
      else if (right > rightmost)
        draw_rect(dr, rightmost, top, right - rightmost, bot-top, COL_BACKGROUND);
      draw_room(dr, state, ds->tilesize, ds->lastpos, ds->lastpos[0], ds->lastpos[1]);
      draw_room(dr, state, ds->tilesize, ds->lastpos, ds->pos[0], ds->pos[1]);
      draw_ball(dr, xp, yp, tilesize, 0);
      if (style == Keys)
        draw_ballkeys(dr, xp-tilesize/2, yp-tilesize/2, tilesize, ds->pos[2], 1);
      else if (style == Combo)
        draw_ballkeys(dr, xp-tilesize/2, yp-tilesize/2, tilesize, ds->pos[3] & (((1<<state->par->keys)-1)<<state->par->levers), 5 - state->par->levers);
    }
  }
  lastanimtime = animtime;
}

/*
  basic: Only solid or open doors. Ball is animated
  tandem: Movable doors. Ball and doors and test are animated
  3d: Solid or open doors and stairs. Ball is animated, also up/down
  floors: Solid or open doors, coloured background and pits. Ball is animated and background changed
  keys: Solid, open, or colored doors. Keys. Ball and approached colored door is animated, and maybe key movement.
  levers: Solid, open, or colored doors. Levers. Ball, colored doors and levers are animated.
 */
static void game_redraw(drawing *dr, game_drawstate *ds, const game_state *oldstate,
			const game_state *state, int dir, const game_ui *ui,
			float animtime, float flashtime)
{
    int sz = state->par->size;
    int tilesize = ds->tilesize;
    int i;
    unsigned char* tmpdoors;

    if (!ds->started) {
	/*
	 * The initial contents of the window are not guaranteed and
	 * can vary with front ends. To be on the safe side, all
	 * games should start by drawing a big background-colour
	 * rectangle covering the whole window.
	 */
      draw_rect(dr, 0, 0, TOTSIZEX(sz, tilesize), TOTSIZEY(sz, tilesize), COL_BACKGROUND);

      combinealldoors(ds->lastdoors, state->par, state->clues, state->coord, 0);
      set_initial_state(state->par, ds->lastpos);
      set_initial_state(state->par, ds->pos);
      ds->lasttestpos[0] = ds->lasttestpos[1] = -1;
      ds->testpos[0] = ds->testpos[1] = -1;
      
      ds->started = true;
    }

    if (!animtime) {
      if (ds->anim) { /* new animation started before the previous finished - erase remains */
        draw_cleanupanim(dr, ds, state);
      }

      tmpdoors = ds->lastdoors;
      ds->lastdoors = ds->doors;
      combinealldoors(tmpdoors, state->par, state->clues, state->coord, (ui->tshow ? ui->tpos : 0));
      ds->doors = tmpdoors;
      for (i=0; i<numcoord(state->par); i++) {
        ds->lastpos[i] = ds->pos[i];
        ds->pos[i] = state->coord[i];
      }
      ds->lasttestpos[0] = ds->testpos[0];
      ds->lasttestpos[1] = ds->testpos[1];
      if (ui->tshow) {
        ds->testpos[0] = ui->tpos[0];
        ds->testpos[1] = ui->tpos[1];
      } else {
        ds->testpos[0] = -1;
        ds->testpos[0] = -1;
      }

      /* Erase some things now that will otherwise look weird during animation */
      draw_preanim(dr, ds, state);
      ds->anim = 1;
    }

    if (animtime) {
      draw_animation(dr, ds, state, get_bgcol(state->par, ds->lastpos), animtime);
      draw_update(dr, 0, 0, TOTSIZEX(sz, tilesize), TOTSIZEY(sz, tilesize));
    } else if (!oldstate) {
      ds->anim = 0;

      if (flashtime > 0) {
        ds->flash = 1;
        draw_rect(dr, 0, 0, TOTSIZEX(sz, tilesize), TOTSIZEY(sz, tilesize),
                  (flashtime <= FLASH_TIME/3 || flashtime >= FLASH_TIME*2/3 ? COL_SHADE4 : COL_SHADE0));
        draw_scene(dr, ds, state, ui, 1);
      } else if (ds->flash) {
        ds->flash = 0;
        draw_rect(dr, 0, 0, TOTSIZEX(sz, tilesize), TOTSIZEY(sz, tilesize), COL_BACKGROUND);
        draw_scene(dr, ds, state, ui, 0);
      } else {
        draw_scene(dr, ds, state, ui, 0);
      }

      /* Ball */
      draw_sceneballs(dr, ds, state, ui);

      draw_update(dr, 0, 0, TOTSIZEX(sz, tilesize), TOTSIZEY(sz, tilesize));
    }
}

static int check_complete(const game_state *state)
{
  int sz = state->par->size;
  return ((state->coord[0] == sz && state->coord[1] == sz-1) &&
          (state->par->style != Tandem ||
           (state->coord[2] == sz && state->coord[3] == sz-1)));
}

static float game_anim_length(const game_state *oldstate, const game_state *newstate,
			      int dir, game_ui *ui)
{
  return ANIM_TIME;
}

static float game_flash_length(const game_state *oldstate, const game_state *newstate,
			       int dir, game_ui *ui)
{
    if (!oldstate->completed && !oldstate->cheated &&
        !newstate->cheated && !newstate->completed &&
        check_complete(newstate)) {
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
#define thegame supermaze
#endif

const struct game thegame = {
    "Supermaze", NULL, NULL,
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
