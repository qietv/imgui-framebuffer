/*
 * PROJECT:    imgui-framebuffer
 * FILE:       naive_swr.h
 * PURPOSE:    Definition for the Naive Software Renderer
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NAIVE_SWR
#define NAIVE_SWR

#include <stdint.h>

typedef struct _NAIVE_SWR_COLOR
{
    uint8_t Red;
    uint8_t Green;
    uint8_t Blue;
    uint8_t Alpha;
} NAIVE_SWR_COLOR, *PNAIVE_SWR_COLOR;

typedef struct _NAIVE_SWR_POINT
{
    float X;
    float Y;
} NAIVE_SWR_POINT, *PNAIVE_SWR_POINT;

typedef struct _NAIVE_SWR_SIZE
{
    float Width;
    float Height;
} NAIVE_SWR_SIZE, *PNAIVE_SWR_SIZE;

typedef struct _NAIVE_SWR_TEXTURE_COORDINATE
{
    float U;
    float V;
} NAIVE_SWR_TEXTURE_COORDINATE, *PNAIVE_SWR_TEXTURE_COORDINATE;

typedef uint32_t NAIVE_SWR_RENDER_TYPE, *PNAIVE_SWR_RENDER_TYPE;

#define NAIVE_SWR_RENDER_TYPE_SKIPPED          ((NAIVE_SWR_RENDER_TYPE)0u)
#define NAIVE_SWR_RENDER_TYPE_SOLID_RECTANGLE  ((NAIVE_SWR_RENDER_TYPE)1u)
#define NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE ((NAIVE_SWR_RENDER_TYPE)2u)
#define NAIVE_SWR_RENDER_TYPE_RGBA32_RECTANGLE ((NAIVE_SWR_RENDER_TYPE)3u)
#define NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE   ((NAIVE_SWR_RENDER_TYPE)4u)
#define NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE  ((NAIVE_SWR_RENDER_TYPE)5u)
#define NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE  ((NAIVE_SWR_RENDER_TYPE)6u)
#define NAIVE_SWR_RENDER_TYPE_SOLID_OPAQUE_TRIANGLE \
    ((NAIVE_SWR_RENDER_TYPE)7u)
#define NAIVE_SWR_RENDER_TYPE_SOLID_TRANSLUCENT_TRIANGLE \
    ((NAIVE_SWR_RENDER_TYPE)8u)
#define NAIVE_SWR_RENDER_TYPE_SOLID_ALPHA_GRADIENT_TRIANGLE \
    ((NAIVE_SWR_RENDER_TYPE)9u)
#define NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE \
       ((NAIVE_SWR_RENDER_TYPE)10u)
#define NAIVE_SWR_RENDER_TYPE_ALPHA8_TRANSLUCENT_TINT_TRIANGLE \
       ((NAIVE_SWR_RENDER_TYPE)11u)

typedef struct _NAIVE_SWR_RENDER_COMMAND
{
    NAIVE_SWR_RENDER_TYPE Type;
    union
    {
        struct
        {
            NAIVE_SWR_POINT Position;
            NAIVE_SWR_SIZE Size;
            NAIVE_SWR_COLOR Color;
            NAIVE_SWR_POINT TexturePosition;
            NAIVE_SWR_SIZE TextureSize;
        } Rectangle;
        struct
        {
            NAIVE_SWR_POINT Positions[3];
            NAIVE_SWR_COLOR Colors[3];
            NAIVE_SWR_TEXTURE_COORDINATE TextureCoordinates[3];
        } Triangle;
    } Command;
} NAIVE_SWR_RENDER_COMMAND, *PNAIVE_SWR_RENDER_COMMAND;

#endif // !NAIVE_SWR
