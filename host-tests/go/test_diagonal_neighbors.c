#include "board.h"

static const int delta[8] = {-20, 1, 20, -1, -19, 21, 19, -21};

int main(void) {
  const Point expected[4] = {81, 121, 119, 79};
  const Point origin = 100;
  Point neighbor = 0;
  int index;
  int visited = 0;
  FORALL_DIAGONAL_NEIGHBORS(NULL, origin, index, neighbor) {
    assert(visited < 4);
    assert(neighbor == expected[visited]);
    ++visited;
  }
  assert(visited == 4 && index == 8);

  visited = 0;
  FORALL_DIAGONAL_NEIGHBORS(NULL, origin, index, neighbor) {
    if (++visited == 2) break;
  }
  assert(visited == 2 && index == 5 && neighbor == 121);

  unsigned int sum = 0;
  FORALL_DIAGONAL_NEIGHBORS(NULL, origin, index, neighbor) {
    if (index == 4 || index == 6) continue;
    sum += neighbor;
  }
  assert(sum == 200 && index == 8);
  puts("Diagonal neighbors: traversal, break and continue passed");
  return 0;
}
