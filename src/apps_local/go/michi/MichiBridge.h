#pragma once

// The whole of michi, as six plain-C functions over plain types.
//
// This boundary exists because michi's headers do NOT compile as C++: they do
// arithmetic on enums and return string literals as `char*`, both of which C
// accepts and C++ refuses. Patching four thousand lines of somebody else's
// engine to satisfy a compiler it was never written for is a sync nobody wants
// to do twice, so the line is drawn here instead: everything on the michi side
// of this header is C, everything on the other side is C++, and the only things
// that cross are integers and byte arrays.
//
// Boards are `size * size` bytes in row-major order, row 0 at the top:
// 0 empty, 1 black, 2 white -- which is `go::kEmpty`, `go::kBlack`, `go::kWhite`
// by construction. Moves are `row * size + col`, or -1 for a pass.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Once per boot. Builds the 3x3 pattern set and michi's board tables, and
// allocates its working position. Safe to call repeatedly.
void michi_bridge_init(void);

// The move michi wants, given the board as it stands.
//
// `nowMs` is a clock the caller lends; when it is null the search runs its full
// simulation count, which is what the host tests want so that a result does not
// depend on how fast the machine running them happens to be.
// `ko` is the point simple ko forbids right now, or -1; `lastMove` is the move
// just played, -1 for none and -2 for a pass. Both are OURS to supply and both
// are load-bearing: without the ko the engine offers the one move the rules
// refuse, and without the last move every local heuristic in the playout aims
// at whichever stone happened to be placed last in scan order.
int michi_bridge_genmove(int size, const uint8_t *board, int toMove, int komiHalves, int ko, int lastMove,
                         int moveNumber, int simulations, uint32_t budgetMs, uint32_t (*nowMs)(void));

// The moves the last search liked, best first, as `row * size + col`. Never a
// pass. Writes at most `max` and returns how many. Lets the caller take the
// next choice when its own rules refuse the first -- michi keeps a superko hash
// this app does not share, so that happens.
int michi_bridge_ranked(int size, int *out, int max);

// What the engine's own position says its ko point, last move and move count
// are, in OUR terms: point indices, -1 for none, -2 for a pass. Exists so that
// "the engine was told" is a fact a test can assert rather than something
// inferred from the move it chose -- michi does not always want the ko even
// when it is offered one, so a test that only watches the move can pass with
// the ko not transferred at all.
void michi_bridge_context(int size, int *ko, int *lastMove, int *moveNumber);

// Sets michi's random generator.
//
// It is a global initialised to 1, and nothing in michi's own GTP loop sets it
// unless a `param_general random_seed` command arrives. So without this call
// every fresh boot replays one game: the engine's answer to a given position is
// fixed, and the first game after a power-on is the same first game every time.
void michi_bridge_seed(uint32_t seed);

// Frees the search tree. The engine stays initialised; the next genmove builds
// a new one. Called when the app closes, so a few hundred kilobytes of PSRAM do
// not sit there for the rest of the boot.
void michi_bridge_forget(void);

// How many simulations the last genmove actually ran before its clock stopped
// it, and how long it took. For the log line that turns an estimate into a fact.
int michi_bridge_last_simulations(void);
uint32_t michi_bridge_last_ms(void);

// Dead-stone detection is deliberately NOT here. michi answers it with
// compute_all_status(), which segmentation faults on a nearly full board --
// reproduced in forty lines of plain C: five empty points is fine, three is a
// fault -- and a counting screen is always a nearly full board. GoEngine's
// owner-map estimator answers it instead. See the note at the foot of
// GoMichi.cpp.

#ifdef __cplusplus
}
#endif
