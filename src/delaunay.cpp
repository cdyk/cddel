#include <algorithm> 
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include "delaunay.h"

namespace {

  // -------------------------------------------------------------------------
  //
  // Generic utilities

  void* xmalloc(size_t size)
  {
    void* rv = malloc(size);
    if (rv != nullptr) return rv;

    fprintf(stderr, "Failed to allocate memory.");
    exit(EXIT_FAILURE);
  }

  void* xcalloc(size_t count, size_t size)
  {
    void* rv = calloc(count, size);
    if (rv != nullptr) return rv;

    fprintf(stderr, "Failed to allocate memory.");
    exit(EXIT_FAILURE);
  }

  void* xrealloc(void* ptr, size_t size)
  {
    void* rv = realloc(ptr, size);
    if (rv != nullptr) return rv;

    fprintf(stderr, "Failed to allocate memory.");
    exit(EXIT_FAILURE);
  }

  // -------------------------------------------------------------------------
  //
  // Multi-word integer math

  template<size_t N>
  struct Int {
    static constexpr uint64_t Zeros = 0;
    static constexpr uint64_t Ones = ~uint64_t(0);
    uint64_t word[N];
  };

  template<size_t N>
  Int<N> add(const Int<N>& x, const Int<N>& y)
  {
    Int<N> r{};
    uint8_t c = 0;
    for (size_t i = 0; i < N; i++) {
      c = _addcarry_u64(c, x.word[i], y.word[i], &r.word[i]);
    }
    return r;
  }

  template<size_t N>
  Int<N> sub(const Int<N>& x, const Int<N>& y)
  {
    Int<N> r{};
    uint8_t b = 0;
    for (size_t i = 0; i < N; i++) {
      b = _subborrow_u64(b, x.word[i], y.word[i], &r.word[i]);
    }
    return r;
  }

  template<size_t N>
  Int<2*N> muls(const Int<N>& x, const Int<N>& y)
  {
    Int<2*N> r{};

    for (size_t j = 0; j < N; j++) {
      uint64_t k = 0;
      for (size_t i = 0; i < 2; i++) {
        uint64_t hi;
        uint64_t lo = _umul128(x.word[j], y.word[i], &hi);
        uint8_t c0 = _addcarry_u64(0, lo, r.word[j + i], &lo);
        uint8_t c1 = _addcarry_u64(0, lo, k, &r.word[j + i]);
        k = hi + c0 + c1;
      }
      r.word[j + N] = k;
    }

    if (int64_t(y.word[N-1]) < 0) {
      uint8_t b = 0;
      for (size_t j = 0; j < N; j++) {
        b = _subborrow_u64(b, r.word[N + j], x.word[j], &r.word[N + j]);
      }
    }

    if (int64_t(x.word[N-1]) < 0) {
      uint8_t b = 0;
      for (size_t j = 0; j < N; j++) {
        b = _subborrow_u64(b, r.word[N + j], y.word[j], &r.word[N + j]);
      }
    }

    return r;
  }

  // -------------------------------------------------------------------------
  //
  // Geometric predicates

  int areaSign(const Pos& p1, const Pos& p2, const Pos& p3)
  {
#if 1
    uint64_t x1 = p1.x; // 32 bits
    uint64_t y1 = p1.y;
    uint64_t x2 = p2.x;
    uint64_t y2 = p2.y;
    uint64_t x3 = p3.x;
    uint64_t y3 = p3.y;

    Int<2> x1y2{ x1 * y2 };    // 64 bits extended to 128
    Int<2> x2y3{ x2 * y3 };
    Int<2> x3y1{ x3 * y1 };
    Int<2> a = add(add(x1y2, x2y3), x3y1);  // 66 bits

    Int<2> x1y3{ x1 * y3 };
    Int<2> x2y1{ x2 * y1 };
    Int<2> x3y2{ x3 * y2 };
    Int<2> b = add(add(x1y3, x2y1), x3y2);  // 66 bits

    Int<2> test = sub(a, b);

    if ((test.word[1] | test.word[0])) {
      return int64_t(test.word[1]) < 0 ? -1 : 1;
    }
#else
    uint16_t x1y2 = p1.x * p2.y;
    uint16_t x2y3 = p2.x * p3.y;
    uint16_t x3y1 = p3.x * p1.y;
    uint32_t a = uint32_t(x1y2) + uint32_t(x2y3) + uint32_t(x3y1);

    uint16_t x1y3 = p1.x * p3.y;
    uint16_t x2y1 = p2.x * p1.y;
    uint16_t x3y2 = p3.x * p2.y;
    uint32_t b = uint32_t(x1y3) + uint32_t(x2y1) + uint32_t(x3y2);

    if (a < b) return -1;
    if (b < a) return 1;
#endif
    return 0;
  }

  int isDelaunay(const Pos& p1, const Pos& p2, const Pos& p3, const Pos& p4)
  {
#if 1
    uint64_t x1 = p1.x; // 32 bits
    uint64_t y1 = p1.y;
    uint64_t x2 = p2.x;
    uint64_t y2 = p2.y;
    uint64_t x3 = p3.x;
    uint64_t y3 = p3.y;
    uint64_t x4 = p4.x;
    uint64_t y4 = p4.y;

    Int<2> x1y2{ .word = { x1 * y2 } };    // 64 bits extended to 128
    Int<2> x1y3{ .word = { x1 * y3 } };
    Int<2> x1y4{ .word = { x1 * y4 } };
    Int<2> x2y3{ .word = { x2 * y3 } };
    Int<2> x2y1{ .word = { x2 * y1 } };
    Int<2> x3y1{ .word = { x3 * y1 } };
    Int<2> x3y2{ .word = { x3 * y2 } };
    Int<2> x3y4{ .word = { x3 * y4 } };
    Int<2> x4y1{ .word = { x4 * y1 } };
    Int<2> x4y3{ .word = { x4 * y3 } };

    Int<2> sin_123_a = add(add(x3y1, x1y2), x2y3);  // 66 bits
    Int<2> sin_123_b = add(add(x2y1, x3y2), x1y3);
    assert((sin_123_a.word[1] & (~uint64_t(0) << (66 - 64))) == 0);
    assert((sin_123_b.word[1] & (~uint64_t(0) << (66 - 64))) == 0);
    Int<2> sin_123 = sub(sin_123_a, sin_123_b);     // 67 bits

    Int<2> sin_341_a = add(add(x4y1, x1y3), x3y4);
    Int<2> sin_341_b = add(add(x4y3, x1y4), x3y1);
    assert((sin_341_a.word[1] & (~uint64_t(0) << (66 - 64))) == 0);
    assert((sin_341_b.word[1] & (~uint64_t(0) << (66 - 64))) == 0);
    Int<2> sin_341 = sub(sin_341_a, sin_341_b);     // 67 bits

    Int<2> x1x2{ .word = { x1 * x2 } };
    Int<2> x1x3{ .word = { x1 * x3 } };
    Int<2> x1x4{ .word = { x1 * x4 } };
    Int<2> x2x2{ .word = { x2 * x2 } };
    Int<2> x2x3{ .word = { x2 * x3 } };
    Int<2> x3x4{ .word = { x3 * x4 } };
    Int<2> x4x4{ .word = { x4 * x4 } };
    Int<2> y1y2{ .word = { y1 * y2 } };
    Int<2> y1y3{ .word = { y1 * y3 } };
    Int<2> y1y4{ .word = { y1 * y4 } };
    Int<2> y2y2{ .word = { y2 * y2 } };
    Int<2> y2y3{ .word = { y2 * y3 } };
    Int<2> y3y4{ .word = { y3 * y4 } };
    Int<2> y4y4{ .word = { y4 * y4 } };

    Int<2> cos_123_a = add(add(x2x2, x1x3), add(y2y2, y1y3));   // 66 bits
    Int<2> cos_123_b = add(add(y2y3, x1x2), add(x2x3, y1y2));
    assert((cos_123_a.word[1] & (~uint64_t(0) << (66 - 64))) == 0);
    assert((cos_123_b.word[1] & (~uint64_t(0) << (66 - 64))) == 0);
    Int<2> cos_123 = sub(cos_123_a, cos_123_b);                // 67 bits

    Int<2> cos_341_a = add(add(x1x3, x4x4), add(y1y3, y4y4));
    Int<2> cos_341_b = add(add(y1y4, y3y4), add(x1x4, x3x4));
    assert((cos_341_a.word[1] & (~uint64_t(0) << (66 - 64))) == 0);
    assert((cos_341_b.word[1] & (~uint64_t(0) << (66 - 64))) == 0);
    Int<2> cos_341 = sub(cos_341_a, cos_341_b);                // 67 bits

    Int<4> sin_123_cos_341 = muls(sin_123, cos_341);            // 134 bits (3 words suffices)
    Int<4> cos_123_sin_341 = muls(cos_123, sin_341);

    Int<4> test = add(sin_123_cos_341, cos_123_sin_341);

    if ((test.word[3] | test.word[2] | test.word[1] | test.word[0])) {
      return  int64_t(test.word[3]) < 0 ? -1 : 1;
    }
#else
    // Assuming the quadrilateral is split with the diagonal [p1,p3].
    // If pi < angle_123 + angle_341, the diagonal should be swapped.
    //
    // Since angle_123 + angle_341 < 2pi:
    //
    // pi < angle_123 + angle_341 <=>  sin_123 cos_341 + cos_123 sin_341 < 0

    uint16_t x1y2 = p1.x * p2.y;
    uint16_t x1y3 = p1.x * p3.y;
    uint16_t x1y4 = p1.x * p4.y;

    uint16_t x2y3 = p2.x * p3.y;
    uint16_t x2y1 = p2.x * p1.y;

    uint16_t x3y1 = p3.x * p1.y;
    uint16_t x3y2 = p3.x * p2.y;
    uint16_t x3y4 = p3.x * p4.y;

    uint16_t x4y1 = p4.x * p1.y;
    uint16_t x4y3 = p4.x * p3.y;

    // sin_123 = (x3 - x2) (y1 - y2) - (x1 - x2) (y3 - y2)
    //         = (x3y1 + x1y2 + x2y3) - (x2y1 + x3y2 + x1y3)
    int32_t sin_123 = (int32_t(x3y1) + int32_t(x1y2) + int32_t(x2y3))
      - (int32_t(x2y1) + int32_t(x3y2) + int32_t(x1y3));

    // sin_341 = (x1 - x4) (y3 - y4) - (x3 - x4) (y1 - y4)
    //         = (x4y1 + x1y3 + x3y4) - (x4y3 + x1y4 + x3y1)
    int32_t sin_341 = (int32_t(x4y1) + int32_t(x1y3) + int32_t(x3y4))
      - (int32_t(x4y3) + int32_t(x1y4) + int32_t(x3y1));

    uint16_t x1x2 = p1.x * p2.x;
    uint16_t x1x3 = p1.x * p3.x;
    uint16_t x1x4 = p1.x * p4.x;
    uint16_t x2x2 = p2.x * p2.x;
    uint16_t x2x3 = p2.x * p3.x;
    uint16_t x3x4 = p3.x * p4.x;
    uint16_t x4x4 = p4.x * p4.x;

    uint16_t y1y2 = p1.y * p2.y;
    uint16_t y1y3 = p1.y * p3.y;
    uint16_t y1y4 = p1.y * p4.y;
    uint16_t y2y2 = p2.y * p2.y;
    uint16_t y2y3 = p2.y * p3.y;
    uint16_t y3y4 = p3.y * p4.y;
    uint16_t y4y4 = p4.y * p4.y;

    // cos_123 = (x3 - x2) (x1 - x2) + (y3 - y2) (y1 - y2)
    //         = (x2x2 + x1x3 + y2y2 + y1y3) - (y2y3 + x1x2 + x2x3 + y1y2)

    int32_t cos_123 = (x2x2 + x1x3 + y2y2 + y1y3) - (y2y3 + x1x2 + x2x3 + y1y2);


    //
    // cos_341 = (x1 - x4) (x3 - x4) + (y1 - y4) (y3 - y4)]
    //         = (x1x3  + x4x4 + y1y3 + y4^2) - y1y4 - y3y4  - x1 x4 - x3 x4
    int32_t cos_341 = (int32_t(x1x3) + int32_t(x4x4) + int32_t(y1y3) + int32_t(y4y4))
      - (int32_t(y1y4) + int32_t(y3y4) + int32_t(x1x4) + int32_t(x3x4));


    int64_t test = sin_123 * cos_341 + cos_123 * sin_341;
    if (0 < test) return 1;
    if (test < 0) return -1;
#endif
    return 0;
  }

  // -------------------------------------------------------------------------
  //
  // Half-edge data structure management

  VtxIx& vertex(const Triangulation& T, HeIx he)
  {
    assert(he != NoIx);
    return T.he[he].vtx;
  }

  HeIx& next(const Triangulation& T, HeIx he)
  {
    assert(he != NoIx);
    return T.he[he].nxt;
  }

  HeIx& twin(const Triangulation& T, HeIx he)
  {
    assert(he != NoIx);
    return T.he[he].twin;
  }

  uint32_t allocSize(uint32_t minimum, uint32_t allocated)
  {
    uint64_t grow = std::max(std::max(minimum, 1024u), (allocated + 1) / 2);
    uint32_t size = uint32_t(std::min(uint64_t(NoIx), uint64_t(allocated) + grow));
    assert(minimum <= size);
    return size;
  }

  VtxIx allocVtx(Triangulation& triang, uint32_t count = 1)
  {
    uint32_t newCount = triang.vtxCount + count;
    assert(0 < count);
    assert(triang.vtxCount < newCount);
    if (triang.vtxAlloc < newCount) {
      triang.vtxAlloc = allocSize(newCount, triang.vtxAlloc);
      triang.vtx = (Vertex*)xrealloc(triang.vtx, sizeof(Vertex) * triang.vtxAlloc);
    }
    VtxIx firstIx = triang.vtxCount;
    triang.vtxCount += count;
    return firstIx;
  }

  HeIx allocHe(Triangulation& triang, uint32_t count = 1)
  {
    uint32_t newCount = triang.heCount + count;
    assert(0 < count);
    assert(triang.heCount < newCount);
    if (triang.heAlloc < newCount) {
      triang.heAlloc = allocSize(newCount, triang.heAlloc);
      triang.he = (HalfEdge*)xrealloc(triang.he, sizeof(HalfEdge) * triang.heAlloc);
    }
    HeIx firstIx = triang.heCount;
    triang.heCount += count;

    for (size_t i = firstIx; i < triang.heCount; i++) {
      triang.he[i].vtx = NoIx;
      triang.he[i].nxt = NoIx;
      triang.he[i].twin = NoIx;
    }

    return firstIx;
  }

  void disconnectHalfEdge(Triangulation& triang, HeIx he)
  {
    HalfEdge& e = triang.he[he];
    if (e.twin != NoIx) {
      triang.he[e.twin].twin = NoIx;
      e.twin = NoIx;
    }
    e.vtx = NoIx;
    e.nxt = NoIx;
  }

  void connectHalfEdge(Triangulation& triang, HeIx curr, HeIx next, HeIx twin, VtxIx vtx)
  {
    assert(curr != NoIx && next != NoIx && vtx != NoIx) ;
    HalfEdge& e = triang.he[curr];
    assert(e.vtx == NoIx);
    assert(e.nxt == NoIx);
    assert(e.twin == NoIx);
    e.vtx = vtx;
    e.nxt = next;
    if (twin != NoIx) {
      e.twin = twin;
      assert(triang.he[twin].twin == NoIx);
      triang.he[twin].twin = curr;
    }
  }

  void disconnectTriangle(Triangulation& triang, HeIx he0)
  {
    HeIx he1 = triang.he[he0].nxt;
    HeIx he2 = triang.he[he1].nxt;
    disconnectHalfEdge(triang, he0);
    disconnectHalfEdge(triang, he1);
    disconnectHalfEdge(triang, he2);
  }

  void connectTriangle(Triangulation& triang,
                       HeIx he0, HeIx tw0, VtxIx v0,
                       HeIx he1, HeIx tw1, VtxIx v1,
                       HeIx he2, HeIx tw2, VtxIx v2)
  {
    const Pos& p0 = triang.vtx[v0].pos;
    const Pos& p1 = triang.vtx[v1].pos;
    const Pos& p2 = triang.vtx[v2].pos;
    int sign = areaSign(p0, p1, p2);
    assert(0 < sign);

    connectHalfEdge(triang, he0, he1, tw0, v0);
    connectHalfEdge(triang, he1, he2, tw1, v1);
    connectHalfEdge(triang, he2, he0, tw2, v2);
  }

  // -------------------------------------------------------------------------
  //
  // Operations on top of the half-edge data structure

  HeIx findContainingTriangle(const Triangulation& triang, bool (&inside)[3], const Pos& pos, HeIx startingPoint)
  {
    HeIx he = startingPoint;

    restart:
      for (size_t i = 0; i < 3; i++) {
        HalfEdge& c = triang.he[he];
        HalfEdge& n = triang.he[c.nxt];
        Vertex& a = triang.vtx[c.vtx];
        Vertex& b = triang.vtx[n.vtx];

        int sign = areaSign(a.pos, b.pos, pos);
        if (sign < 0) {
          assert(c.twin != NoIx); // Going outside of triangulation.
          he = c.twin;
          goto restart;
        }
        inside[i] = 0 < sign;
        he = c.nxt;
      }
      return he;
  }

  void recursiveDelaunaySwap(Triangulation& T, std::vector<HeIx>& todo)
  {
    while (!todo.empty()) {
      HeIx he = todo.back();
      todo.pop_back();
      if (he == NoIx) continue;

      HeIx tw = twin(T, he);
      if (tw == NoIx) continue;


      HeIx l0 = next(T, he);
      HeIx l1 = next(T, l0);
      HeIx l2 = next(T, tw);
      HeIx l3 = next(T, l2);

      
      VtxIx v0 = vertex(T, l0);
      VtxIx v1 = vertex(T, l1);
      VtxIx v2 = vertex(T, l2);
      VtxIx v3 = vertex(T, l3);
      
      int del = isDelaunay(T.vtx[v0].pos, T.vtx[v1].pos, T.vtx[v2].pos, T.vtx[v3].pos);
      if (0 <= del) continue;

      HeIx t0 = twin(T, l0);
      HeIx t1 = twin(T, l1);
      HeIx t2 = twin(T, l2);
      HeIx t3 = twin(T, l3);

      disconnectTriangle(T, he);
      disconnectTriangle(T, tw);

      connectTriangle(T,
                      l0, t0, v0,
                      he, NoIx, v1,
                      l3, t3, v3);

      connectTriangle(T,
                      l2, t2, v2,
                      tw, he, v3,
                      l1, t1, v1);

      todo.push_back(t0);
      todo.push_back(t1);
      todo.push_back(t2);
      todo.push_back(t3);
    }
  }

  void splitEdge(Triangulation& T, HeIx a0, VtxIx mid)
  {
    //             v0                            v0
    //           / | \                         / | \
    //          /  |  \                       /  |  \   
    //         /   |   \                     /   |   \
    //     n0 /    |    \ n3             n0 /    |    \ n3
    //       /c1   |   a2\                 /c1 c0|b0 b2\
    //      /      |      \               /      |      \
    //     /       |       \             /   c2  |  b1   \
    // v1 +      c0|a0      + v3  =>  v1 +------ m -------+ v3
    //     \       |       /             \   d1  |  a2   /
    //      \      |      /               \      |      /
    //       \c2   |   a1/                 \d2 d0|a0 a1/
    //     n1 \    |    / n2             n1 \    |    / n2
    //         \   |   /                     \   |   /
    //          \  |  /                       \  |  /
    //           \ | /                         \ | /
    //             v2                            v2
    HeIx c0 = twin(T, a0);
    bool onBoundary = c0 == NoIx;

    HeIx a1 = next(T, a0);
    HeIx a2 = next(T, a1);

    HeIx n2 = twin(T, a1);
    HeIx n3 = twin(T, a2);

    VtxIx v0 = vertex(T, a0);
    VtxIx v2 = vertex(T, a1);
    VtxIx v3 = vertex(T, a2);

    HeIx b0 = allocHe(T, onBoundary ? 3 : 6);
    HeIx b1 = b0 + 1;
    HeIx b2 = b0 + 2;

    HeIx d0 = onBoundary ? NoIx : (b0 + 3);

    T.he[a0] = { .vtx = mid, .nxt = a1, .twin = d0 };
    T.he[a1] = { .vtx = v2,  .nxt = a2, .twin = n2 };
    T.he[a2] = { .vtx = v3,  .nxt = a0, .twin = b1 };
    if (n2 != NoIx) T.he[n2].twin = a1;

    T.he[b0] = { .vtx = v0,  .nxt = b1, .twin = c0 };
    T.he[b1] = { .vtx = mid, .nxt = b2, .twin = a2 };
    T.he[b2] = { .vtx = v3,  .nxt = b0, .twin = n3 };
    if (n3 != NoIx) T.he[n3].twin = b2;

    if (onBoundary) {
      std::vector<HeIx> todo = { a1, a2, b2 };
      recursiveDelaunaySwap(T, todo);
    }
    else {
      HeIx c1 = next(T, c0);
      HeIx c2 = next(T, c1);
      HeIx d1 = b0 + 4;
      HeIx d2 = b0 + 5;

      HeIx n0 = twin(T, c1);
      HeIx n1 = twin(T, c2);

      VtxIx v1 = vertex(T, c2);

      T.he[c0] = { .vtx = mid, .nxt = c1, .twin = b0 };
      T.he[c1] = { .vtx = v0,  .nxt = c2, .twin = n0 };
      T.he[c2] = { .vtx = v1,  .nxt = c0, .twin = d1 };
      if(n0 != NoIx) T.he[n0].twin = c1;

      T.he[d0] = { .vtx = v2,  .nxt = d1, .twin = a0 };
      T.he[d1] = { .vtx = mid, .nxt = d2, .twin = c2 };
      T.he[d2] = { .vtx = v1,  .nxt = d0, .twin = n1 };
      if (n1 != NoIx) T.he[n1].twin = d2;

      std::vector<HeIx> todo = { a0, a1, a2, b0, b2, c1, c2, d2 };
      recursiveDelaunaySwap(T, todo);
    }
  }

  void splitTriangle(Triangulation& T, HeIx he0, VtxIx mid)
  {
    HeIx he1 = T.he[he0].nxt;
    HeIx he2 = T.he[he1].nxt;

    VtxIx v0 = T.he[he0].vtx;
    VtxIx v1 = T.he[he1].vtx;
    VtxIx v2 = T.he[he2].vtx;

    HeIx tw0 = T.he[he0].twin;
    HeIx tw1 = T.he[he1].twin;
    HeIx tw2 = T.he[he2].twin;

    HeIx he3 = allocHe(T, 6);

    disconnectTriangle(T, he0);
    connectTriangle(T,
                    he0, tw0, v0,
                    he1, NoIx, v1,
                    he2, NoIx, mid);
    connectTriangle(T,
                    he3 + 0, tw1, v1,
                    he3 + 1, NoIx, v2,
                    he3 + 2, he1, mid);
    connectTriangle(T,
                    he3 + 3, tw2, v2,
                    he3 + 4, he2, v0,
                    he3 + 5, he3 + 1, mid);

    std::vector<HeIx> todo = { tw0, tw1, tw2 };
    recursiveDelaunaySwap(T, todo);
  }

}

Triangulation::Triangulation()
{
  VtxIx v = allocVtx(*this, 4);
  vtx[v + 0] = { .pos = { 0,  0 } };
  vtx[v + 1] = { .pos = { ~0u,  0 } };
  vtx[v + 2] = { .pos = { ~0u, ~0u } };
  vtx[v + 3] = { .pos = { 0,  ~0u }  };

  HeIx h = allocHe(*this, 6);

  connectTriangle(*this,
                  h + 0, NoIx, 0,
                  h + 1, NoIx, 1,
                  h + 2, NoIx, 2);

  connectTriangle(*this,
                  h + 3, NoIx, 2,
                  h + 4, NoIx, 3,
                  h + 5, h + 2, 0);

  assert(0 < areaSign(vtx[he[h + 0].vtx].pos,
                      vtx[he[h + 1].vtx].pos,
                      vtx[he[h + 2].vtx].pos));
  assert(0 < areaSign(vtx[he[h + 3].vtx].pos,
                      vtx[he[h + 4].vtx].pos,
                      vtx[he[h + 5].vtx].pos));
}

Triangulation::~Triangulation()
{

}


VtxIx insertVertex(Triangulation& T, const Pos& pos)
{
  bool inside[3] = {};

  HeIx he = findContainingTriangle(T, inside, pos, 0);
  if (he == NoIx) return NoIx;

  size_t insideCase = (inside[0] ? 1 : 0) + (inside[1] ? 2 : 0) + (inside[2] ? 4 : 0);
  switch (insideCase) {

  // Point on three edges boundary => degenerate triangle.
  case 0b000:
    assert(false && "Hit degenerate triangle");
    return NoIx;

  // Point on two edges => lies on a corner
  case 0b001: he = next(T, he); [[fallthrough]];
  case 0b100: he = next(T, he); [[fallthrough]];
  case 0b010: {
    VtxIx v = vertex(T, he);
    assert(T.vtx[v].pos.x == pos.x && T.vtx[v].pos.y == pos.y);
    return v;
  }

  // Point on one edge => lies in the interior of an edge
  case 0b011: he = next(T, he); [[fallthrough]];
  case 0b101: he = next(T, he); [[fallthrough]];
  case 0b110: {
    const Pos& a = T.vtx[vertex(T, he)].pos;
    const Pos& b = T.vtx[vertex(T, next(T, he))].pos;
    assert(areaSign(a, b, pos) == 0);

    VtxIx v = allocVtx(T);
    T.vtx[v].pos = pos;
    splitEdge(T, he, v);
    return v;
  }

  // Point in the interior of the triangle
  case 0b111: {
    VtxIx v = allocVtx(T);
    T.vtx[v].pos = pos;
    splitTriangle(T, he, v);
    return v;
  }

  default:
    assert(false);
    return NoIx;
  }
}
