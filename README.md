An game engine for Yinsh written with bitboards and Monte Carlo tree search (MCTS)

Key aspects:
- Yinsh boards are represented using 128-bit bitboards which are also used for move generation
- MCTS with UCT is used for move search
- Tree nodes are allocated using an arena allocator, and tree is destroy and created every move search for now, but later it might use a pool allocator

## Compiler limitations
For 128-bit bitboards a `__uint128_t` type was used which is supported in latest gcc and clang compilers, but not in msvc, so for compilation on Windows mingw64 should be used.

Later 128-bit integers can be implemented in a more standardized manner for better compatibility.
