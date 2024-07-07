#pragma once


#ifndef HDR
typedef GLuint KCOLOR;
#else
struct kcolkor_t { GLfloat r, g, b, a; };
typedef struct kcolor_t KCOLOR;
#endif

struct quadpart
{
    float x, y;
    KCOLOR color;
};

struct quad
{
    struct quadpart quads[ QUADCOUNT ];
};


extern struct quad* quads;
extern size_t vtxidx;
extern const size_t vertexsize;

