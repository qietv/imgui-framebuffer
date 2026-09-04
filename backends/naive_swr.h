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

#include <stddef.h>
#include <stdint.h>

#ifndef EXTERN_C
#if defined(__cplusplus)
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif
#endif

/**
 * @brief Describes a straight-alpha color with 8-bit components.
 */
typedef struct _NAIVE_SWR_COLOR
{
    uint8_t Red;
    uint8_t Green;
    uint8_t Blue;
    uint8_t Alpha;
} NAIVE_SWR_COLOR, *PNAIVE_SWR_COLOR;

/**
 * @brief Describes a point in framebuffer coordinates.
 */
typedef struct _NAIVE_SWR_POINT
{
    float X;
    float Y;
} NAIVE_SWR_POINT, *PNAIVE_SWR_POINT;
typedef const NAIVE_SWR_POINT *PCNAIVE_SWR_POINT;

/**
 * @brief Describes a two-dimensional size.
 */
typedef struct _NAIVE_SWR_SIZE
{
    float Width;
    float Height;
} NAIVE_SWR_SIZE, *PNAIVE_SWR_SIZE;

/**
 * @brief Describes a normalized texture coordinate.
 */
typedef struct _NAIVE_SWR_TEXTURE_COORDINATE
{
    float U;
    float V;
} NAIVE_SWR_TEXTURE_COORDINATE, *PNAIVE_SWR_TEXTURE_COORDINATE;

/**
 * @brief Describes an integer clipping rectangle in framebuffer pixels.
 *        Left and Top are inclusive. Right and Bottom are exclusive.
 */
typedef struct _NAIVE_SWR_CLIP_RECT
{
    int32_t Left;
    int32_t Top;
    int32_t Right;
    int32_t Bottom;
} NAIVE_SWR_CLIP_RECT, *PNAIVE_SWR_CLIP_RECT;
typedef const NAIVE_SWR_CLIP_RECT *PCNAIVE_SWR_CLIP_RECT;

/**
 * @brief Identifies the storage format of a software-renderer texture.
 */
typedef uint32_t NAIVE_SWR_TEXTURE_FORMAT,
    *PNAIVE_SWR_TEXTURE_FORMAT;

#define NAIVE_SWR_TEXTURE_FORMAT_INVALID \
    ((NAIVE_SWR_TEXTURE_FORMAT)0u)
#define NAIVE_SWR_TEXTURE_FORMAT_ALPHA8 \
    ((NAIVE_SWR_TEXTURE_FORMAT)1u)
#define NAIVE_SWR_TEXTURE_FORMAT_RGBA32 \
    ((NAIVE_SWR_TEXTURE_FORMAT)2u)

/**
 * @brief Describes a non-owning texture view. ByteStride specifies the
 *        number of bytes between adjacent rows. Alpha8 stores one byte per
 *        texel. RGBA32 stores four bytes per texel in R, G, B, A order.
 */
typedef struct _NAIVE_SWR_TEXTURE
{
    const uint8_t* Pixels;
    int32_t Width;
    int32_t Height;
    size_t ByteStride;
    NAIVE_SWR_TEXTURE_FORMAT Format;
} NAIVE_SWR_TEXTURE, *PNAIVE_SWR_TEXTURE;
typedef const NAIVE_SWR_TEXTURE *PCNAIVE_SWR_TEXTURE;

/**
 * @brief Describes a non-owning packed BGRA32 framebuffer. Pixels contain
 *        0xAARRGGBB values. PixelStride specifies the number of uint32_t
 *        pixels between adjacent rows.
 */
typedef struct _NAIVE_SWR_FRAMEBUFFER
{
    uint32_t* Pixels;
    int32_t Width;
    int32_t Height;
    size_t PixelStride;
} NAIVE_SWR_FRAMEBUFFER, *PNAIVE_SWR_FRAMEBUFFER;
typedef const NAIVE_SWR_FRAMEBUFFER *PCNAIVE_SWR_FRAMEBUFFER;

/**
 * @brief Identifies the operation represented by a render command.
 */
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

/**
 * @brief Describes one software-renderer operation. Rectangle texture
 *        positions and sizes use texel coordinates. Triangle texture
 *        coordinates are normalized.
 */
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
typedef const NAIVE_SWR_RENDER_COMMAND *PCNAIVE_SWR_RENDER_COMMAND;

/**
 * @brief Clears the complete framebuffer to the specified color. The
 *        resulting framebuffer alpha channel is always set to 255.
 * @param Framebuffer A non-null pointer to the destination framebuffer.
 * @param Color The clear color.
 */
EXTERN_C void naive_swr_clear_framebuffer(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    NAIVE_SWR_COLOR Color);

/**
 * @brief Renders one command into a framebuffer.
 * @param Framebuffer A non-null pointer to the destination framebuffer.
 * @param ClipRect A non-null pointer to the clipping rectangle.
 * @param Texture The texture used by the command, or null for an
 *        untextured command.
 * @param RenderCommand A non-null pointer to the command to render.
 */
EXTERN_C void naive_swr_render_command(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_TEXTURE Texture,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand);

#endif // !NAIVE_SWR
