#pragma once

// Who is dead, and the playout machinery that answers it. Freestanding C++17:
// no renderer, no Arduino, no heap, no exceptions.
//
// This file used to be the opponent as well. michi-c2 is the opponent now
// (GoMichi.h) because it is two or three stones stronger, and this kept the one
// job michi cannot do: its own `compute_all_status` crashes on a nearly-full
// board, which is every board a counting screen has ever shown. See the note in
// GoMichi.cpp.
//
// The method is the strong programs': play the position out to the end a few
// hundred times and see who OWNS each point, not who has a stone on it. A group
// captured during a playout leaves its points empty, so asking which stone is
// there answers nobody; asking who the region belongs to answers the captor.

#include <cstdint>

#include "GoCore.h"
#include "GoFlow.h"

namespace goengine {

// Exposed for the tests: one random playout from `game`, returning the area
// score difference in half points from Black's point of view.
int playoutOnce(const go::Game& game, uint32_t& seed, bool policy);

// Whether passing right now would win for `colour`, counted on the board exactly
// as it stands with every stone alive.
//
// This is the Leela Zero pass rule, and it is what stops the two behaviours that
// make a Go program look broken to a human. It never passes while passing would
// lose, so it does not hand over a won game; and it passes the moment passing
// wins, so it does not fill every neutral point first. Filling them is not
// stupid -- under area scoring a neutral point is worth one -- but a human reads
// it as the machine not knowing the game is over.
bool passingWins(const go::Game& game, uint8_t colour);

// Exposed for ONE test, and it is the test that makes the second board safe.
//
// The search does not use `go::Game`: it plays on a smaller board with no
// history, no dead marks and no tallies, because the game's own board copies
// the whole board to answer one question, and a search asks that question
// times. Two implementations of one rulebook is exactly the shape that drifts,
// so `host-tests/go` plays hundreds of thousands of random positions through
// BOTH and asserts the boards are identical point for point. This is how it
// reaches the second one.
//
// Applies `point` to the fast board built from `game` and writes the resulting
// position back into `game.point`. Returns what the fast board said about
// legality. Nothing but the test calls it.
bool fastPlayForTest(go::Game& game, int point);

// Which stones are probably dead, as a bit a point in the shape of Game::dead.
//
// The counting screen's opening offer, and nothing more than that: the players
// may flip any group it got wrong, and the machine's opinion is a starting
// point rather than a verdict. It plays the position out two hundred times and
// takes the verdict for the GROUP, because a group lives or dies together and
// marking half a dragon dead draws one live stone beside one ghost.
void estimateDead(const go::Game& game, uint32_t& seed, uint8_t out[go::kMaskBytes]);

// The engine's OPINION about which stones are dead: estimateDead seeded from the
// position itself, so the same board always gets the same answer.
//
// That reproducibility is the whole point. The counting screen offers this as a
// starting position and, against the machine, only accepts a marking that still
// matches it -- so an answer that drifted between two askings would refuse the
// very count it had just offered, and bounce the player back to the board for
// agreeing with it.
void opinionOnDead(const go::Game& game, uint8_t out[go::kMaskBytes]);

}  // namespace goengine
