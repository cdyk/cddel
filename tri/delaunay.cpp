#include <algorithm> 
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include "delaunay.h"

namespace {

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

  int areaSign(const Pos& p1, const Pos& p2, const Pos& p3)
  {
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
    return 0;
  }

  int isDelaunay(const Pos& p1, const Pos& p2, const Pos& p3, const Pos& p4)
  {
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
    int32_t cos_341 = (int32_t(x1x3)  + int32_t(x4x4) + int32_t(y1y3) + int32_t(y4y4))
                    - (int32_t(y1y4) + int32_t(y3y4) + int32_t(x1x4) + int32_t(x3x4));


    int64_t test = sin_123 * cos_341 + cos_123 * sin_341;

    if (0 < test) return 1;
    if (test < 0) return -1;

    return 0;
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
    connectHalfEdge(triang, he0, he1, tw0, v0);
    connectHalfEdge(triang, he1, he2, tw1, v1);
    connectHalfEdge(triang, he2, he0, tw2, v2);
  }

  HeIx findContainingTriangle(const Triangulation& triang, int (&signs)[3], const Pos& pos, HeIx startingPoint)
  {
    HeIx he = startingPoint;

    restart:
      for (size_t i = 0; i < 3; i++) {
        HalfEdge& c = triang.he[he];
        HalfEdge& n = triang.he[c.nxt];
        Vertex& a = triang.vtx[c.vtx];
        Vertex& b = triang.vtx[n.vtx];

        signs[i] = areaSign(a.pos, b.pos, pos);
        if (signs[i] < 0) {
          assert(c.twin != NoIx); // Going outside of triangulation.
          he = c.twin;
          goto restart;
        }
        he = c.nxt;
      }
      return he;
  }

  void splitTriangle(Triangulation& triang, HeIx he0, VtxIx mid)
  {
    HeIx he1 = triang.he[he0].nxt;
    HeIx he2 = triang.he[he1].nxt;

    VtxIx v0 = triang.he[he0].vtx;
    VtxIx v1 = triang.he[he1].vtx;
    VtxIx v2 = triang.he[he2].vtx;

    HeIx tw0 = triang.he[he0].twin;
    HeIx tw1 = triang.he[he1].twin;
    HeIx tw2 = triang.he[he2].twin;

    HeIx he3 = allocHe(triang, 6);

    disconnectTriangle(triang, he0);
    connectTriangle(triang,
                    he0, tw0, v0,
                    he1, NoIx, v1,
                    he2, NoIx, mid);
    connectTriangle(triang,
                    he3 + 0, tw1, v1,
                    he3 + 1, NoIx, v2,
                    he3 + 2, he1, mid);
    connectTriangle(triang,
                    he3 + 3, tw2, v2,
                    he3 + 4, he2, v0,
                    he3 + 5, he3 + 1, mid);
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

}

Triangulation::Triangulation()
{
  VtxIx v = allocVtx(*this, 4);
  vtx[v + 0] = { .pos = { 0,  0 } };
  vtx[v + 1] = { .pos = { 255,  0 } };
  vtx[v + 2] = { .pos = { 255,  255 } };
  vtx[v + 3] = { .pos = { 0,  255 }  };

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
  int signs[3] = {};

  HeIx he = findContainingTriangle(T, signs, pos, 0);
  int signSum = signs[0] + signs[1] + signs[2];
  if (signSum == 0) {
    // Degnerate triangle
    assert(false);
  }
  else if (signSum == 1) {

    for (int i = 0; i < 3; i++) {
      VtxIx v = vertex(T, he);
      if (T.vtx[v].pos.x == pos.x && T.vtx[v].pos.y == pos.y) {
        fprintf(stderr, "Duplicate point\n");
        return v;
      }
      he = next(T, he);
    }
    assert(false);
  }
  else if (signSum == 2) {
    fprintf(stderr, "On edge\n");
    return NoIx;
  }

  VtxIx v = allocVtx(T);
  T.vtx[v].pos = pos;

  std::vector<HeIx> todo;
  {
    HeIx e = he;
    for (int i = 0; i < 3; i++) {
      todo.push_back(twin(T, e));
      e = next(T, e);
    }
  }
  splitTriangle(T, he, v);

  recursiveDelaunaySwap(T, todo);

  return v;
}
