// The two symbols michi-c2's core needs from the user interface it came with,
// and nothing else.
//
// That is not a hope, it is a measurement: linking board.c, board_util.c,
// michi.c, patterns.c, params.c and control.c together leaves exactly these two
// undefined. ui.c, sgf.c, debug.c and main.c are 2,300 lines this app does not
// have to carry, and the core does not reach into any of them.

#include "michi.h"

// Every point of the board. Filled once at startup by whoever owns the
// Position; upstream it lives in ui.c beside the GTP loop.
Point allpoints[BOARDSIZE];

// A GoGui live-graphics hook, called from inside the search so a watching
// human can see it think. There is no GoGui here, and a search that stopped to
// draw would be a search that missed its deadline.
void display_live_gfx(Position *pos, TreeNode *tree, int *owner_map) {
    (void)pos;
    (void)tree;
    (void)owner_map;
}
