#include <cstdlib>
#include <cstdio>
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

}

Triangulation::Triangulation()
{
  vtxAlloc = 100;
  vtx = (Vertex*)xmalloc(sizeof(Vertex) * vtxAlloc);

  vtxCount = 4;
  vtx[0] = { 0,  0 };
  vtx[1] = { 255,  0 };
  vtx[2] = { 255,  255 };
  vtx[3] = { 0,  255 };

  heAlloc = 100;
  he = (HalfEdge*)xmalloc(sizeof(HalfEdge) * vtxAlloc);

  heCount = 6;
  he[0] = { .vtx = 0, .nxt = 1, .twin = NoIx };
  he[1] = { .vtx = 1, .nxt = 2, .twin = NoIx };
  he[2] = { .vtx = 2, .nxt = 0, .twin = 5 };

  he[3] = { .vtx = 2, .nxt = 4, .twin = NoIx };
  he[4] = { .vtx = 3, .nxt = 5, .twin = NoIx };
  he[5] = { .vtx = 0, .nxt = 3, .twin = 2 };
}

Triangulation::~Triangulation()
{

}
