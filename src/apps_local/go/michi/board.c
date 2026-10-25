// board.c -- Implementation of Go rules and data for Go heuristics
#include "board.h"
/* ===========================================================================
 * Board provides functions to play the ancient game of Go by computer
 *
 * The file board.c contains a minimal set of functions that are necessary to
 * play (efficiently) and score random games which are used by higher level
 * algorithms like MCTS (Monte-Carlo Tree Search). 
 * There are functions for playing and undoing moves and querying the status of 
 * the board. The undo_move() is limited to one undo, the capability to undo 
 * any number of successive moves is delegated to the caller.
 *
 * It is designed so that all the working functions involved to play random 
 * games can fit in the L1 Instruction Cache of recent Intel Processors (32 Kb)
 * and the data working set accordingly fit in the L1 Data Cache (also 32 Kb).
 *
 * The main purpose of the functions included in this file is to play a move
 * and to incrementally update the state of the board during the games. 
 * For efficiency reasons not only the state of each intersection (WHITE, BLACK
 * or EMPTY) is maintained but also the connectivity between stones.
 * The set of stones that are connected are named blocks. The whole set of 
 * liberties of each block is recorded and updated after each move.
 *
 * The representation of the board is 1D (as described in [Mueller 2002). This
 * means that the position of an intersection (or point) is a small integer 
 * that is used as index in various arrays containing attributes of this point.
 * A small border (1 point large) is added around the board to simplify the 
 * code by avoiding a lot of tests. Each point of this border has color OUT.
 * =========================================================================*/

#ifndef NDEBUG
static char  buf[BUFLEN];
#endif
// Displacements towards the neighbors of a point
//                      North East South  West  NE  SE  SW  NW
static int   delta[] = { -N-1,   1,  N+1,   -1, -N,  W,  N, -W};
Mark  *mark1, *mark2;
unsigned int idum    = 1;

//========== Initialization and Terminaison of the Board module ===============
void board_init(void)
{
    mark1 = michi_calloc(1, sizeof(Mark));
    mark2 = michi_calloc(1, sizeof(Mark));
}

void board_finish(void)
{
    free(mark1); mark1 = NULL;
    free(mark2); mark2 = NULL;
}

//====================================== Eyes =================================
char is_eyeish(Position *pos, Point pt)
// test if pt is inside a single-color diamond and return the diamond color or 0
// this could be an eye, but also a false one
{
    Color eyecolor=EMPTY, other=EMPTY;
    int k;
    Point n;
    FORALL_NEIGHBORS(pos, pt, k, n) {
        Color c = point_color(pos,n);
        if(c == OUT) continue;                // ignore OUT of board neighbours
        if(c == EMPTY) return 0;
        if(eyecolor == EMPTY) {
            eyecolor = c;
            other = color_other(c);
        }
        else if (c == other) return 0;
    }
    return eyecolor;
}

char is_eye(Position *pos, Point pt)
// Test if pt is an eye and return its color or 0 if it is not
//                                                  #########   
// Note: this test cannot detect true eyes like     . . X . #   or    X X X
//                                                    X . X #         X   X
//                                                    X X . #           X   X
//                                                        . #           X X X
{
    Color eyecolor=is_eyeish(pos, pt), falsecolor;
    int   at_edge=0, false_count=0, k;
    Point d;
    if (eyecolor == EMPTY) return 0;
    
    // Eye-like shape, but it could be a falsified eye
    falsecolor = color_other(eyecolor);
    FORALL_DIAGONAL_NEIGHBORS(pos, pt, k, d) {
        if(point_color(pos, d) == OUT) at_edge = 1;
        else if(point_color(pos, d) == falsecolor) false_count += 1;
    }
    if (at_edge) false_count += 1;
    if (false_count >= 2) return 0;
    return eyecolor;
}

//=================================== Env4 ====================================
Byte compute_env4(Position *pos, Point pt, int offset)
// Compute value of the environnement of a point (Byte)
// offset=0 for the 4 neighbors, offset=4 for the 4 diagonal neighbors
{
    Byte env4=0, hi, lo, c;
    for (int k=offset ; k<offset+4 ; k++) {
        Point n = pt + delta[k];
        // color coding c -> , 0:EMPTY, 1:OUT, 2:WHITE, 3:BLACK 
        c = point_color(pos, n);
        hi = c >> 1; lo = c & 1;
        env4 |= ((hi<<4)+lo) << (k-offset);
    }
    return env4;
}

void put_stone(Position *pos, Point pt)
// Put a stone at point pt. Update env4 and env4d neighbor data
{
    if (pos->to_play == BLACK) {  // BLACK to play
        pos->env4[pt+N+1] |= 0x11;
        pos->env4[pt-1]   |= 0x22;
        pos->env4[pt-N-1] |= 0x44;
        pos->env4[pt+1]   |= 0x88;
        pos->env4d[pt+N]  |= 0x11;
        pos->env4d[pt-W]  |= 0x22;
        pos->env4d[pt-N]  |= 0x44;
        pos->env4d[pt+W]  |= 0x88;
        pos->color[pt] = BLACK;
    }
    else {                // WHITE to play (X=WHITE)
        pos->env4[pt+N+1] |= 0x10;
        pos->env4[pt-1]   |= 0x20;
        pos->env4[pt-N-1] |= 0x40;
        pos->env4[pt+1]   |= 0x80;
        pos->env4d[pt+N]  |= 0x10;
        pos->env4d[pt-W]  |= 0x20;
        pos->env4d[pt-N]  |= 0x40;
        pos->env4d[pt+W]  |= 0x80;
        pos->color[pt] = WHITE;
    }
}

void remove_stone(Position *pos, Point pt)
// Remove a stone at point pt. Update env4 and env4d neighbor data
{
    pos->env4[pt+N+1] &= 0xEE;
    pos->env4[pt-1]   &= 0xDD;
    pos->env4[pt-N-1] &= 0xBB;
    pos->env4[pt+1]   &= 0x77;
    pos->env4d[pt+N]  &= 0xEE;
    pos->env4d[pt-W]  &= 0xDD;
    pos->env4d[pt-N]  &= 0xBB;
    pos->env4d[pt+W]  &= 0x77;
    pos->color[pt] = EMPTY;
}

//===================================== Blocks ================================
int cmpint(const void *i, const void *j)
{
    return *(int *)i - *(int *)j;
}

void compute_block(Position *pos, Point pt, Slist stones, Slist libs, int nlibs)
// Compute block at pt : list of stones and list of liberties
// Return early when nlibs liberties are found
{
    Color color=point_color(pos, pt);
    int   head=2, k, tail=1;
    Point n;

    mark_init(mark1); slist_clear(libs);
    stones[1] = pt; mark(mark1, pt);
    while(head>tail) {
        pt = stones[tail++];
        FORALL_NEIGHBORS(pos, pt, k, n)
            if (!is_marked(mark1, n)) {
                mark(mark1, n);
                if (point_color(pos, n) == color)    stones[head++] = n;
                else if (point_color(pos, n) == EMPTY) {
                    slist_push(libs, n);
                    if (slist_size(libs) >= nlibs) goto finished;
                }
            }
    }
finished:
    stones[0] = head-1;
    mark_release(mark1);
}

void compute_big_eye(Position *pos, Point pt, Slist points)
// Compute big eye at pt (assumed to be EMPTY) -> list of points
{
    int   head=2, k, tail=1;
    Point n;

    mark_init(mark1);
    points[1] = pt; mark(mark1, pt);
    while(head>tail) {
        pt = points[tail++];
        FORALL_NEIGHBORS(pos, pt, k, n)
            if (!is_marked(mark1, n)) {
                mark(mark1, n);
                if (point_color(pos, n) == EMPTY)    points[head++] = n;
            }
    }
    points[0] = head-1;
    mark_release(mark1);
}

__INLINE__ Block new_blkid(Position *pos)
// Generate a new block id (i.e. one that is not already in use)
{
    int b;
    for (b=1 ; b<MAX_BLOCKS ; b++)
        if (block_size(pos, b) == 0)
            return b;

    return 0;
}

__INLINE__ int block_add_lib(Position *pos, Block b, Point l)
// Add liberty l to block b. Normally return 1, but could return 0 if 'l'
// is already found in the liberties of block b (this is not an error)
{
    int k = (l-N)>>5, r = (l-N)&31, res;
    res = (pos->libs[b][k] & (1<<r)) == 0;
    pos->libs[b][k] |= (1<<r);
    pos->nlibs[b] += res;
    return res;
}

__INLINE__ void block_remove_lib_extend(Position *pos, Block b, Point l)
// Remove liberty l of an extended block b.
// l is always a liberty. No capture possible (suicide is checked before)
{
    int k = (l-N)>>5, r = (l-N)&31;
    pos->libs[b][k] &= ~(1<<r);
    pos->nlibs[b]--;
}

__INLINE__ int block_count_libs(Position *pos, Block b)
// Return the number of liberties of block b.
{
    int nlibs=0;
    for (int k=0 ; k<LIBS_SIZE ; k++) {
        Libs m32 = pos->libs[b][k];
        nlibs += popcnt_u32(m32);
    }
    return nlibs;
}

void
block_compute_libs(Position *pos, Block b, Slist libs, int max_libs)
// Return the list of Block b libs. 
// Memory for libs should have been allocated by the caller.
{
    int j, k;
    Libs m32;
    slist_clear(libs);
    for (k=0 ; k<LIBS_SIZE ; k++) {
        m32 = pos->libs[b][k];
        while (m32) {
            j = bsf_u32(m32);
            m32 ^= (1<<j);
            slist_push(libs, N+k*8*sizeof(Libs)+j);
            if (slist_size(libs) > max_libs) return;
        }
    }
}

void block_make_list_of_points(Position *pos, Block b, Slist points, Point pt)
{
    //slist_clear(points);
    //FORALL_POINTS(pos, pt)
    //    if (point_block(pos,pt) == b) slist_push(points, pt);
    int   head=2, k, tail=1;
    Point n;

    mark_init(mark1);
    points[1] = pt; mark(mark1, pt);
    while(head>tail) {
        pt = points[tail++];
        FORALL_NEIGHBORS(pos, pt, k, n)
            if (!is_marked(mark1, n)) {
                mark(mark1, n);
                if (point_block(pos, n) == b)    points[head++] = n;
            }
    }
    points[0] = head-1;
    mark_release(mark1);
}

__INLINE__ void block_clear_libs(Position *pos, Block b)
// Delete all liberties from block b
{
    memset(pos->libs[b], 0, LIBS_SIZE*sizeof(Libs));
    pos->nlibs[b] = 0;
}

int block_capture(Position *pos, Block b, Point pt)
// Remove stones of block b from the board (b contains pt), reset data of b
// Return the number of captured stones.
{
    int   captured = block_size(pos, b);
    int   head=2, k, tail=1;
    Point stones[BOARDSIZE], n;

    // remove stones from the board (updating liberties of neighbor blocks) 
    // loop on the stones of block b using the flood fill algorithm
    stones[1] = pt; remove_stone(pos, pt); pos->block[pt] = 0;
    while(head>tail) {
        pt = stones[tail++];
        FORALL_NEIGHBORS(pos, pt, k, n) {
            Block b1 = point_block(pos, n);
            if (b1 == b) {
                stones[head++] = n; 
                remove_stone(pos, n);
                pos->block[n] = 0;
            }
            else
                block_add_lib(pos, b1, pt);
        }
    }

    // reset block data to zero
    block_clear_libs(pos, b);
    pos->bsize[b] = 0;

    return captured;
}

__INLINE__ int block_remove_lib(Position *pos, Block b, Point pt, Point l)
// Remove liberty l of block b. Normally return 1, but could return 0 if 'l'
// is not found in the liberties of block b (this is not an error)
{
    int captured=0, k = (l-N)>>5, r = (l-N)&31, res;
    res = (pos->libs[b][k] & (1<<r));
    if (res) {
        pos->libs[b][k] &= ~(1<<r);
        pos->nlibs[b]--;
        if (pos->nlibs[b]==0)
            captured = block_capture(pos, b, pt);
    }
    return captured;        // number of captured stones
}

__INLINE__ void block_remove_lib_undo(Position *pos, Block b, Point pt, Point l)
// Remove liberty l of block b. Normally return 1, but could return 0 if 'l'
// is not found in the liberties of block b (this is not an error)
// This version is special for undoing move capture (do not capture back ko)
{
    int k = (l-N)>>5, r = (l-N)&31, res;
    res = (pos->libs[b][k] & (1<<r));
    if (res) {
        pos->libs[b][k] &= ~(1<<r);
        pos->nlibs[b]--;
    }
}

__INLINE__ void block_merge(Position *pos, Block b1, Block b2)
// Merge 2 blocks. If b1==b2 nothing to do (this is not an error)
{
    if (b1==b2) return;

    // Merge stones
    FORALL_POINTS(pos, pt)
        if (point_block(pos, pt) == b2)
            pos->block[pt] = b1;
    int nstones = block_size(pos, b1) + block_size(pos, b2);
    if (nstones<256)
        pos->bsize[b1] = nstones;
    else
        pos->bsize[b1] = 255;

    // Merge libs
    for (int k=0 ; k<LIBS_SIZE ; k++) {
        pos->libs[b1][k] |= pos->libs[b2][k];
        pos->libs[b2][k] = 0;
    }

    // Delete block b2
    pos->nlibs[b2] = 0;
    pos->bsize[b2] = 0;
}

__INLINE__ Block point_create_block(Position *pos, Point pt)
// Create a new block at point pt (assumed EMPTY)
{
    Block b = new_blkid(pos);
    pos->bsize[b] = 1;
    return b;
}

int update_blocks(Position *pos, Point pt)
// Update the blocks if a move is done at point pt. No change if move is illegal
{
    Block b1=0, b2=0, b3=0, b4=0;   // initialization to make compiler happy
    Color other=color_other(pos->to_play);
    Code4 Ecode, Xcode;
    int   captured=0, k;
    Point n;
    pos->undo_capture = 0;

    // Update blocks of opponent
    FORALL_NEIGHBORS(pos, pt, k, n) {
        if (point_color(pos, n) == other) {
            Block b = point_block(pos, n);
            int ncaps = block_remove_lib(pos, b, n, pt);
            if (ncaps)
                pos->undo_capture |= 1<<k;
            captured += ncaps;
        }
    }

    // Check for suicide
    Ecode = select_empty(point_env4(pos, pt));
    if (Ecode == 0) {
        FORALL_NEIGHBORS(pos, pt, k, n) {
            if (point_color(pos,n) == pos->to_play) {
                Block b = point_block(pos, n);
                if (block_nlibs(pos, b) > 1)
                    goto update_friend_blocks;
            }
        }
        // Suicide: restore state of opponent blocks and return
        FORALL_NEIGHBORS(pos, pt, k, n) {
            if (point_color(pos, n) == other) {
                Block b = point_block(pos, n);
                block_add_lib(pos, b, pt);
            }
        }
        return -1;
    }

    // Update stones of friend block(s)
update_friend_blocks:
    if (pos->to_play == BLACK)
        Xcode = select_black(point_env4(pos, pt));
    else 
        Xcode = select_white(point_env4(pos, pt));
    pos->caps[pos->to_play & 1] += captured;
    pos->ko = 0;
    put_stone(pos, pt);
    switch(Xcode) {
        // 0 friend stone in contact
        case 0:
            b1 = pos->block[pt] = point_create_block(pos, pt);
            if (captured==1) {      // check for ko
                switch(Ecode) {
                    case 1:
                        pos->ko = pt + delta[0];
                        break;
                    case 2:
                        pos->ko = pt + delta[1];
                        break;
                    case 4:
                        pos->ko = pt + delta[2];
                        break;
                    case 8:
                        pos->ko = pt + delta[3];
                        break;
                    default:
                        pos->ko = 0;
                        break;
                }
            }
            goto update_libs;
        // 1 friend stone in contact
        case 8:
            b1 = point_block(pos, pt+delta[3]);  // extend block North of pt
            goto extend;
        case 4:
            b1 = point_block(pos, pt+delta[2]);  // extend block East  of pt
            goto extend;
        case 2:
            b1 = point_block(pos, pt+delta[1]);  // extend block South of pt
            goto extend;
        case 1:
            b1 = point_block(pos, pt+delta[0]);  // extend block West  of pt
            goto extend;
        // 2 friend stones in contact
        case 3:
            b2 = point_block(pos, pt+delta[1]);
            b1 = point_block(pos, pt+delta[0]);
            goto merge2;
        case 5:
            b2 = point_block(pos, pt+delta[2]);
            b1 = point_block(pos, pt+delta[0]);
            goto merge2;
        case 6:
            b2 = point_block(pos, pt+delta[2]);
            b1 = point_block(pos, pt+delta[1]);
            goto merge2;
        case 9:
            b2 = point_block(pos, pt+delta[3]);
            b1 = point_block(pos, pt+delta[0]);
            goto merge2;
        case 10:
            b2 = point_block(pos, pt+delta[3]);
            b1 = point_block(pos, pt+delta[1]);
            goto merge2;
        case 12:
            b2 = point_block(pos, pt+delta[3]);
            b1 = point_block(pos, pt+delta[2]);
            goto merge2;
        // 3 friend stones in contact
        case 7:
            b3 = point_block(pos, pt+delta[2]);
            b2 = point_block(pos, pt+delta[1]);
            b1 = point_block(pos, pt+delta[0]);
            goto merge3;
        case 11:
            b3 = point_block(pos, pt+delta[3]);
            b2 = point_block(pos, pt+delta[1]);
            b1 = point_block(pos, pt+delta[0]);
            goto merge3;
        case 13:
            b3 = point_block(pos, pt+delta[3]);
            b2 = point_block(pos, pt+delta[2]);
            b1 = point_block(pos, pt+delta[0]);
            goto merge3;
        case 14:
            b3 = point_block(pos, pt+delta[3]);
            b2 = point_block(pos, pt+delta[2]);
            b1 = point_block(pos, pt+delta[1]);
            goto merge3;
        case 15:
            b4 = point_block(pos, pt+delta[3]);
            b3 = point_block(pos, pt+delta[2]);
            b2 = point_block(pos, pt+delta[1]);
            b1 = point_block(pos, pt+delta[0]);
    }
    block_merge(pos, b1, b4);
merge3:
    block_merge(pos, b1, b3);
merge2:
    block_merge(pos, b1, b2);
    pos->nlibs[b1] = block_count_libs(pos, b1);
extend:
    if (pos->bsize[b1]<255) pos->bsize[b1]++;
    pos->block[pt] = b1;
    block_remove_lib_extend(pos, b1, pt);

    // Update libs of new block
update_libs:
    FORALL_NEIGHBORS(pos, pt, k, n) {
        if (point_color(pos, n) == EMPTY)
            block_add_lib(pos, b1, n);
    }
    return captured;
}

//===================================== Moves =================================
Position* new_position()
// Return a position with size of board and komi initialized
{
    Position *pos = michi_malloc(sizeof(Position));
    pos->size = N;
    empty_position(pos);
    return pos;
}

char* empty_position(Position *pos)
// Reset pos to an initial empty board position
{
    float komi = pos->komi;
    int k = 0, size = pos->size;
    memset(pos, 0, sizeof(Position));
    pos->size = size; pos->komi = komi;
    for (int row=0 ; row <= (N-size) ; row++)
        for (int col=0 ; col<=N ; col++) pos->color[k++] = OUT;
    for (int row=N-size+1 ; row<=N ; row++) {
        pos->color[k++] = OUT;
        for (int col=1 ; col<=size ; col++) pos->color[k++] = EMPTY;
        for (int col=size+1 ; col<=N ; col++) pos->color[k++] = OUT;
    }
    for (int col=0 ; col<W ; col++) pos->color[k++] = OUT;
    FORALL_POINTS(pos, pt) {
        if (point_color(pos,pt) == OUT) continue;
        pos->env4[pt] = compute_env4(pos, pt, 0);
        pos->env4d[pt] = compute_env4(pos, pt, 4);
    }

    pos->to_play = BLACK;
    //assert(env4_OK(pos));
    return "";              // result OK
}

char* play_move(Position *pos, Point pt)
// Play a move at point pt (color is imposed by alternate play)
// No change of pos if move is illegal
{
    int   captured=0;
    Point ko_old = pos->ko;

    if (pt == pos->ko) return "Error Illegal move: retakes ko";

    captured = update_blocks(pos, pt);
    if (captured == -1) return "Error Illegal move: suicide";

    // Finish update of the position (swap color)
    pos->ko_old = ko_old;
    (pos->n)++;
    //assert(env4_OK(pos));
    pos->last3 = pos->last2;
    pos->last2 = pos->last;
    pos->last  = pt;
    pos->to_play = color_other(pos->to_play);
    michi_assert(pos, blocks_OK(pos, pt));
    return "";          // Move OK
}

char* undo_move(Position *pos)
// Undo the last move
// WARNING: this is a limited version that can only undo one move suitable for
//          undoing move in the random playouts (used minimum memory)
// In order to implement a full undo function, the caller must save the list of
// moves and undo_capture (4 bits) and update last, last2, last3 and 
// undo_capture before calling undo_move()

// TODO optimize undo block extend
//      Currently, the block is scanned and a new block is rebuild
{
    Block b, bl;
    Color c=point_color(pos, pos->last), other=color_other(c);
    int k;
    Point l=pos->last, libs[BOARDSIZE], n, points[BOARDSIZE];

    // Create again blocks captured by the last move
    if (pos->undo_capture) {
        pos->color[l] = c;
        int ncaptured = 0;
        for (int k=0; k<4 ; k++) {
            if (pos->undo_capture & (1<<k)) {
                n = l + delta[k];
                compute_big_eye(pos, n, points);
                Block bn = new_blkid(pos);
                pos->bsize[bn] = slist_size(points);
                ncaptured += slist_size(points);
                FORALL_IN_SLIST(points, s) {
                    put_stone(pos, s);
                    for (int j=0 ; j<4 ; j++) {
                        Point m=s+delta[j];
                        if (point_color(pos, m) == c) {
                            b = point_block(pos, m);
                            block_remove_lib_undo(pos, b, 0, s);
                        }
                    }
                    pos->block[s] = bn;
                }
                block_add_lib(pos, bn, l);
            }
        }
        pos->caps[c & 1] -= ncaptured;
        pos->color[l] = EMPTY;
    }

    // Update other attributes of the position (order must be preserved)
    pos->last = pos->last2;
    pos->last2 = pos->last3;
    pos->ko = pos->ko_old;
    pos->n--;
    pos->to_play = color_other(pos->to_play);
    if (l==PASS_MOVE) goto finished;

    // Update information at the point of the last move
    bl = point_block(pos, l);
    remove_stone(pos, l);
    pos->block[l] = 0;
    
    // Update the position (correct only for block create, extend and merge)
    FORALL_NEIGHBORS(pos, l, k, n) {
        if (point_color(pos, n) == other) {
            // opponent neighbor
            b = point_block(pos, n);
            block_add_lib(pos, b, l);
        }
        else if (point_color(pos, n) == c) {
            // friend neighbor
            b = point_block(pos, n);
            if (b == bl) {
                compute_block(pos, n, points, libs, BOARDSIZE);
                Block bn = new_blkid(pos);
                pos->bsize[bn] = slist_size(points);
                if (slist_size(points) < 256)
                    pos->bsize[bn] = slist_size(points);
                else
                    pos->bsize[bn] = 255;
                FORALL_IN_SLIST(points, s)
                    pos->block[s] = bn;
                FORALL_IN_SLIST(libs, l)
                    block_add_lib(pos, bn, l);
            }
        }
    }
    // Delete the block bl that is no more useful 
    block_clear_libs(pos, bl);
    pos->bsize[bl] = 0;

finished:
    // check that the updated Position is OK
    michi_assert(pos, all_blocks_OK(pos));
    michi_assert(pos, env4_OK(pos));
    return "";
}

char* pass_move(Position *pos)
// Play a Pass move in the current position 
{
    pos->ko_old = pos->ko;
    pos->undo_capture = 0;
    (pos->n)++;
    pos->last3 = pos->last2;
    pos->last2 = pos->last;
    pos->last  = pos->ko = 0;
    pos->to_play = color_other(pos->to_play);
    return "";          // PASS moVE is always OK
}

//================================ Go heuristics ==============================
void make_list_last_moves_neighbors(Position *pos, Slist points)
// generate a randomly shuffled list of points including and surrounding 
// the last two moves (but with the last move having priority)
{
    Point l, last2_neighbors[12];
    mark_init(mark1);
    if (pos->last == PASS_MOVE)
        slist_clear(points);
    else {
        l = pos->last;
        points[0] = 9;
        points[1] = l; mark(mark1, l);
        points[2] = l + delta[0]; mark(mark1, l + delta[0]);
        points[3] = l + delta[1]; mark(mark1, l + delta[1]);
        points[4] = l + delta[2]; mark(mark1, l + delta[2]);
        points[5] = l + delta[3]; mark(mark1, l + delta[3]);
        points[6] = l + delta[4]; mark(mark1, l + delta[4]);
        points[7] = l + delta[5]; mark(mark1, l + delta[5]);
        points[8] = l + delta[6]; mark(mark1, l + delta[6]);
        points[9] = l + delta[7]; mark(mark1, l + delta[7]);
        slist_shuffle(points);
    }
    if (pos->last2 != PASS_MOVE) {
        l = pos->last2;
        last2_neighbors[0] = 9;
        last2_neighbors[1] = l;
        last2_neighbors[2] = l + delta[0];
        last2_neighbors[3] = l + delta[1];
        last2_neighbors[4] = l + delta[2];
        last2_neighbors[5] = l + delta[3];
        last2_neighbors[6] = l + delta[4];
        last2_neighbors[7] = l + delta[5];
        last2_neighbors[8] = l + delta[6];
        last2_neighbors[9] = l + delta[7];
        slist_shuffle(last2_neighbors);
        FORALL_IN_SLIST(last2_neighbors, n)
            if (!is_marked(mark1, n))
                slist_push(points, n);
    }
    mark_release(mark1);
}

void make_list_neighbor_blocks_in_atari(Position *pos, Block b, Slist blocks, Point pt) 
// Return a list of blocks in atari in contact with block b
{
    Color c;
    int   k;
    Point n, stones[BOARDSIZE];

    slist_clear(blocks);
    block_make_list_of_points(pos, b, stones, pt);
    c = color_other(point_color(pos, stones[1]));

    FORALL_IN_SLIST(stones, pt) {
        FORALL_NEIGHBORS(pos, pt, k, n) {
            if (point_color(pos,n) == c) {
                Block b1 = point_block(pos, n);
                if (block_nlibs(pos,b1) == 1) slist_insert(blocks, b1);
            }
        }
    }
}

void compute_cfg_distances(Position *pos, Point pt, char cfg_map[BOARDSIZE])
// Return a board map listing common fate graph distances from a given point.
// This corresponds to the concept of locality while contracting groups to 
// single points.
{
    int   head=1, k, tail=0;
    Point n;
    // FORK CHANGE: the fringe is allocated, not a frame. 30*BOARDSIZE Points is
    // 25,376 bytes at N=13, in ONE frame, on the deepest path the search takes
    // -- it was the whole of the go_search task's measured 28,752 bytes on its
    // own. A queue that long is upstream sizing for 19x19 with room to spare;
    // it is never a reason to give a task twenty-five kilobytes of stack.
    Point *fringe = michi_cfg_fringe();

    memset(cfg_map, -1, BOARDSIZE);
    cfg_map[pt] = 0;

    // flood-fill like mechanics
    fringe[0]=pt;
    while(head > tail) {
        pt = fringe[tail++];
        FORALL_NEIGHBORS(pos, pt, k, n) {
            Color c = point_color(pos, n);
            if (c == OUT) continue;
            if (0 <= cfg_map[n] && cfg_map[n] <= cfg_map[pt]) continue;
            int cfg_before = cfg_map[n];
            if (c != EMPTY && c==point_color(pos,pt)) 
                cfg_map[n] = cfg_map[pt];
            else
                cfg_map[n] = cfg_map[pt]+1;
            if (cfg_before < 0 || cfg_before > cfg_map[n]) {
                fringe[head++] = n;
                //assert(head < 30*BOARDSIZE);
            }
        }
    }
}

int line_height(Point pt, int size)
// Return the line number above nearest board edge (0 based)
{
    // FORK CHANGE: the (N-size) term. empty_position() lays a board of `size`
    // out at ARRAY rows N-size+1..N, not 1..size, so on a build where N is the
    // maximum rather than the board being played this arithmetic was reading
    // every row wrong. On N=13 with a 9x9 game it returned 0 -- the first-line
    // penalty -- for tengen, and no penalty at all for the real first line,
    // which inverts the opening priors on the default board.
    div_t d = div(pt,N+1);
    int row = d.quot - (N - size), col=d.rem;
    if (row > size/2) row = size+1-row;
    if (col > size/2) col = size+1-col;
    if (row < col) return row-1;
    else           return col-1;
}

int empty_area(Position *pos, Point pt, int dist)
// Check whether there are any stones in Manhattan distance up to dist
{
    int   k;
    Point n;
    FORALL_NEIGHBORS(pos, pt, k, n) {
        if (point_is_stone(pos,n)) 
            return 0;
        else if (point_color(pos,n) == EMPTY 
                && dist>1 && !empty_area(pos, n, dist-1))
            return 0;
    }
    return 1;
}
