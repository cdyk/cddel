// tri.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <cassert>

#include <SDL3/SDL.h>
#include <algorithm>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "delaunay.h"

namespace {

  Triangulation* tri = nullptr;

  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;

  Uint64 lastUpdateTicks = 0;


}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }


  if (!SDL_CreateWindowAndRenderer("Triangles", 1280, 1024, 0, &window, &renderer)) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  tri = new Triangulation();

  std::srand(42);
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
  if (tri) {
    delete tri;
    tri = nullptr;
  }
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }
  else if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
    return SDL_APP_SUCCESS;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  if (tri == nullptr) {
    return SDL_APP_FAILURE;
  }

  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);

  float s = 0.9f * std::min(w, h);
  float xo = 0.5f * (w - s);
  float yo = 0.5f * (h - s) + s;
  float xscale = s / 0xffffffff;
  float yscale = -s / 0xffffffff;

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);

  Uint64 ticks = SDL_GetTicks();
  if (300 < ticks - lastUpdateTicks) {
    lastUpdateTicks = ticks;

    Pos p {
      .x = uint32_t(std::rand()),
      .y = uint32_t(std::rand())
    };

    p.x ^= p.x << 13;
    p.x ^= p.x >> 17;
    p.x ^= p.x << 5;

    p.y ^= p.y << 13;
    p.y ^= p.y >> 17;
    p.y ^= p.y << 5;

    insertVertex(*tri, p);
  }

  SDL_RenderClear(renderer);
  for (size_t i = 0; i < tri->heCount; i++) {
    const HalfEdge& he = tri->he[i];
    if (he.twin != NoIx && tri->he[he.twin].vtx < he.vtx) {
      continue;
    }
    const Vertex& v0 = tri->vtx[he.vtx];
    const Vertex& v1 = tri->vtx[tri->he[he.nxt].vtx];

    if (he.twin == NoIx) {
      SDL_SetRenderDrawColorFloat(renderer, 1.f, 1.f, 0.5f, 1.f);
    }
    else {
      assert(tri->he[he.twin].twin == i);
      SDL_SetRenderDrawColorFloat(renderer, 1.f, 1.f, 1.f, 1.f);
    }
    SDL_RenderLine(renderer,
                   xo + xscale * v0.pos.x,
                   yo + yscale * v0.pos.y,
                   xo + xscale * v1.pos.x,
                   yo + yscale * v1.pos.y);
  }


  SDL_SetRenderDrawColorFloat(renderer, 1.f, 0.f, 0.f, 1.f);
  for (size_t i = 0; i < tri->vtxCount; i++) {
    SDL_FRect rect{
      .x = xo + xscale * tri->vtx[i].pos.x - 2,
      .y = yo + yscale * tri->vtx[i].pos.y - 2,
      .w = 5,
      .h = 5
    };
    SDL_RenderRect(renderer, &rect);

    //SDL_RenderPoint(renderer, x, y);
  }


//  SDL_RenderRect()
  
  SDL_RenderPresent(renderer);
  return SDL_APP_CONTINUE;
}
