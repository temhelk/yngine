An game engine for Yinsh written with bitboards and Monte Carlo tree search (MCTS)

Key aspects:
- Yinsh boards are represented using 128-bit bitboards which are also used for move generation
- For move search a parallel lock-free MCTS with UCT is used from this [paper](https://liacs.leidenuniv.nl/~plaata1/papers/paper_ICAART18.pdf)
- Tree nodes are allocated with a pool allocator and tree is reused for next moves
- ~~Tree nodes are allocated using an arena allocator, and tree is destroy and created every move search for now, but later it might use a pool allocator~~

## Compiler limitations
For 128-bit bitboards a `__uint128_t` type was used which is supported in latest gcc and clang compilers, but not in msvc, so for compilation on Windows msys2 with mingw64 or clang should be used.

Later 128-bit integers can be implemented in a more standardized manner for better compatibility.
