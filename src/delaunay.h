#pragma once

#include <cstdint>

static constexpr uint32_t NoIx = 0xFFFFFFFFu;


typedef uint32_t VtxIx;
typedef uint32_t HeIx;

struct Pos
{
  uint32_t x;
  uint32_t y;
};

struct Vertex
{
  Pos pos;
};

struct HalfEdge
{
  VtxIx vtx;
  HeIx nxt;
  HeIx twin;
};

struct Triangulation
{
  Triangulation();
  ~Triangulation();
  Triangulation(const Triangulation&) = delete;
  Triangulation& operator=(const Triangulation&) = delete;


  Vertex* vtx = nullptr;
  HalfEdge* he = nullptr;

  uint32_t vtxCount = 0;
  uint32_t vtxAlloc = 0;

  uint32_t heCount = 0;
  uint32_t heAlloc = 0;
};



VtxIx insertVertex(Triangulation& triang, const Pos& pos);
