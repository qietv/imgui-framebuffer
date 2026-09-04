/*
 * PROJECT:    imgui-framebuffer
 * FILE:       naive_swr.c
 * PURPOSE:    Implementation for the Naive Software Renderer
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "naive_swr.h"

#include <math.h>
#include <string.h>

#define NAIVE_SWR_ENABLE_SSE2_CONSTANT_BLEND

#if defined(NDEBUG)
#define NAIVE_SWR_ENABLE_SSE2_A8_OPAQUE_TINT_BLEND
#endif

#if (defined(NAIVE_SWR_ENABLE_SSE2_CONSTANT_BLEND) || \
    defined(NAIVE_SWR_ENABLE_SSE2_A8_OPAQUE_TINT_BLEND)) && \
    (defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || \
    defined(__SSE2__))
#include <emmintrin.h>
#define NAIVE_SWR_HAS_SSE2
#endif

#if defined(_MSC_VER)
#define NAIVE_SWR_INLINE __inline
#else
#define NAIVE_SWR_INLINE inline
#endif

#define NAIVE_SWR_DIV255_ROUNDED(Value) \
    ((uint32_t)((((uint32_t)(Value) + 128u) * 257u) >> 16))

#define NAIVE_SWR_MUL255(A, B) \
    ((int)NAIVE_SWR_DIV255_ROUNDED((uint32_t)(A) * (uint32_t)(B)))

#define NAIVE_SWR_CLAMP(Value, Minimum, Maximum) \
    (((Value) < (Minimum)) ? (Minimum) : \
    (((Value) > (Maximum)) ? (Maximum) : (Value)))

static NAIVE_SWR_INLINE void naive_swr_fill_span(
    uint32_t* Destination,
    size_t PixelCount,
    uint32_t Color)
{
    while (PixelCount >= 8)
    {
        Destination[0] = Color;
        Destination[1] = Color;
        Destination[2] = Color;
        Destination[3] = Color;
        Destination[4] = Color;
        Destination[5] = Color;
        Destination[6] = Color;
        Destination[7] = Color;
        Destination += 8;
        PixelCount -= 8;
    }

    while (PixelCount != 0)
    {
        *Destination++ = Color;
        --PixelCount;
    }
}

static NAIVE_SWR_INLINE void naive_swr_blend_over(
    uint32_t* Destination,
    int SourceRed,
    int SourceGreen,
    int SourceBlue,
    int SourceAlpha)
{
    const uint32_t Alpha = (uint32_t)SourceAlpha;
    uint32_t DestinationColor;
    uint32_t InverseAlpha;
    uint32_t DestinationBlue;
    uint32_t DestinationGreen;
    uint32_t DestinationRed;
    uint32_t OutputRed;
    uint32_t OutputGreen;
    uint32_t OutputBlue;

    if (Alpha == 0u)
    {
        return;
    }

    if (Alpha == 255u)
    {
        *Destination =
            0xFF000000u |
            ((uint32_t)SourceRed << 16) |
            ((uint32_t)SourceGreen << 8) |
            (uint32_t)SourceBlue;
        return;
    }

    DestinationColor = *Destination;
    InverseAlpha = 255u - Alpha;
    DestinationBlue = DestinationColor & 0xFFu;
    DestinationGreen = (DestinationColor >> 8) & 0xFFu;
    DestinationRed = (DestinationColor >> 16) & 0xFFu;

    OutputRed = NAIVE_SWR_DIV255_ROUNDED(
        (uint32_t)SourceRed * Alpha + DestinationRed * InverseAlpha);
    OutputGreen = NAIVE_SWR_DIV255_ROUNDED(
        (uint32_t)SourceGreen * Alpha + DestinationGreen * InverseAlpha);
    OutputBlue = NAIVE_SWR_DIV255_ROUNDED(
        (uint32_t)SourceBlue * Alpha + DestinationBlue * InverseAlpha);

    *Destination =
        0xFF000000u | (OutputRed << 16) | (OutputGreen << 8) | OutputBlue;
}

/**
 * @brief Stores precomputed values for blending a constant color and
 *        alpha value over a destination span.
 */
typedef struct _NAIVE_SWR_CONSTANT_BLEND_STATE
{
    uint32_t SourceRedAlpha;
    uint32_t SourceGreenAlpha;
    uint32_t SourceBlueAlpha;
    uint32_t InverseAlpha;
} NAIVE_SWR_CONSTANT_BLEND_STATE, *PNAIVE_SWR_CONSTANT_BLEND_STATE;
typedef const NAIVE_SWR_CONSTANT_BLEND_STATE *PCNAIVE_SWR_CONSTANT_BLEND_STATE;

static NAIVE_SWR_INLINE void naive_swr_blend_constant_span(
    uint32_t* Destination,
    size_t PixelCount,
    PCNAIVE_SWR_CONSTANT_BLEND_STATE BlendState)
{
    const uint32_t SourceRedAlpha = BlendState->SourceRedAlpha;
    const uint32_t SourceGreenAlpha = BlendState->SourceGreenAlpha;
    const uint32_t SourceBlueAlpha = BlendState->SourceBlueAlpha;
    const uint32_t InverseAlpha = BlendState->InverseAlpha;

#if defined(NAIVE_SWR_HAS_SSE2) && \
       defined(NAIVE_SWR_ENABLE_SSE2_CONSTANT_BLEND)
    const __m128i Zero = _mm_setzero_si128();
    const __m128i InverseAlpha16 = _mm_set1_epi16((short)InverseAlpha);
    const __m128i Rounding16 = _mm_set1_epi16(128);
    const __m128i SourceTerms16 = _mm_set_epi16(
        0,
        (short)SourceRedAlpha,
        (short)SourceGreenAlpha,
        (short)SourceBlueAlpha,
        0,
        (short)SourceRedAlpha,
        (short)SourceGreenAlpha,
        (short)SourceBlueAlpha);
    const __m128i FramebufferAlphaMask = _mm_set1_epi32((int)0xFF000000u);

    while (PixelCount >= 4)
    {
        const __m128i PackedDestination = _mm_loadu_si128(
            (const __m128i*)(const void*)Destination);
        __m128i Low = _mm_unpacklo_epi8(PackedDestination, Zero);
        __m128i High = _mm_unpackhi_epi8(PackedDestination, Zero);
        __m128i PackedResult;

        Low = _mm_mullo_epi16(Low, InverseAlpha16);
        High = _mm_mullo_epi16(High, InverseAlpha16);
        Low = _mm_add_epi16(Low, SourceTerms16);
        High = _mm_add_epi16(High, SourceTerms16);
        Low = _mm_add_epi16(Low, Rounding16);
        High = _mm_add_epi16(High, Rounding16);
        Low = _mm_add_epi16(Low, _mm_srli_epi16(Low, 8));
        High = _mm_add_epi16(High, _mm_srli_epi16(High, 8));
        Low = _mm_srli_epi16(Low, 8);
        High = _mm_srli_epi16(High, 8);

        PackedResult = _mm_packus_epi16(Low, High);
        PackedResult = _mm_or_si128(PackedResult, FramebufferAlphaMask);
        _mm_storeu_si128((__m128i*)(void*)Destination, PackedResult);

        Destination += 4;
        PixelCount -= 4;
    }
#endif

    while (PixelCount != 0)
    {
        const uint32_t DestinationColor = *Destination;
        const uint32_t DestinationBlue = DestinationColor & 0xFFu;
        const uint32_t DestinationGreen =
            (DestinationColor >> 8) & 0xFFu;
        const uint32_t DestinationRed =
            (DestinationColor >> 16) & 0xFFu;
        const uint32_t OutputRed = NAIVE_SWR_DIV255_ROUNDED(
            SourceRedAlpha + DestinationRed * InverseAlpha);
        const uint32_t OutputGreen = NAIVE_SWR_DIV255_ROUNDED(
            SourceGreenAlpha + DestinationGreen * InverseAlpha);
        const uint32_t OutputBlue = NAIVE_SWR_DIV255_ROUNDED(
            SourceBlueAlpha + DestinationBlue * InverseAlpha);

        *Destination =
            0xFF000000u | (OutputRed << 16) | (OutputGreen << 8) | OutputBlue;

        ++Destination;
        --PixelCount;
    }
}

static NAIVE_SWR_INLINE void naive_swr_blend_a8_opaque_tint_span(
    uint32_t* Destination,
    const uint8_t* SourceAlpha,
    size_t PixelCount,
    uint32_t SourceRed,
    uint32_t SourceGreen,
    uint32_t SourceBlue)
{
#if defined(NAIVE_SWR_HAS_SSE2) && \
       defined(NAIVE_SWR_ENABLE_SSE2_A8_OPAQUE_TINT_BLEND)
    if (PixelCount >= 4)
    {
        const __m128i Zero = _mm_setzero_si128();
        const __m128i All25516 = _mm_set1_epi16(255);
        const __m128i Rounding16 = _mm_set1_epi16(128);
        const __m128i SourceColor16 = _mm_set_epi16(
            0,
            (short)SourceRed,
            (short)SourceGreen,
            (short)SourceBlue,
            0,
            (short)SourceRed,
            (short)SourceGreen,
            (short)SourceBlue);
        const __m128i FramebufferAlphaMask = _mm_set1_epi32((int)0xFF000000u);

        while (PixelCount >= 4)
        {
            uint32_t PackedSourceAlpha;
            __m128i AlphaBytes;
            __m128i Alpha16;
            __m128i AlphaPairs16;
            __m128i AlphaLow16;
            __m128i AlphaHigh16;
            __m128i InverseAlphaLow16;
            __m128i InverseAlphaHigh16;
            __m128i PackedDestination;
            __m128i DestinationLow16;
            __m128i DestinationHigh16;
            __m128i SourceLow16;
            __m128i SourceHigh16;
            __m128i PackedResult;

            memcpy(
                &PackedSourceAlpha,
                SourceAlpha,
                sizeof(PackedSourceAlpha));

            AlphaBytes = _mm_cvtsi32_si128((int)PackedSourceAlpha);
            Alpha16 = _mm_unpacklo_epi8(AlphaBytes, Zero);
            AlphaPairs16 = _mm_unpacklo_epi16(Alpha16, Alpha16);
            AlphaLow16 = _mm_unpacklo_epi32(AlphaPairs16, AlphaPairs16);
            AlphaHigh16 = _mm_unpackhi_epi32(AlphaPairs16, AlphaPairs16);

            InverseAlphaLow16 = _mm_sub_epi16(All25516, AlphaLow16);
            InverseAlphaHigh16 = _mm_sub_epi16(All25516, AlphaHigh16);

            PackedDestination = _mm_loadu_si128(
                (const __m128i*)(const void*)Destination);
            DestinationLow16 = _mm_unpacklo_epi8(PackedDestination, Zero);
            DestinationHigh16 = _mm_unpackhi_epi8(PackedDestination, Zero);

            SourceLow16 = _mm_mullo_epi16(SourceColor16, AlphaLow16);
            SourceHigh16 = _mm_mullo_epi16(SourceColor16, AlphaHigh16);
            DestinationLow16 = _mm_mullo_epi16(
                DestinationLow16,
                InverseAlphaLow16);
            DestinationHigh16 = _mm_mullo_epi16(
                DestinationHigh16,
                InverseAlphaHigh16);
            DestinationLow16 = _mm_add_epi16(DestinationLow16, SourceLow16);
            DestinationHigh16 = _mm_add_epi16(DestinationHigh16, SourceHigh16);
            DestinationLow16 = _mm_add_epi16(DestinationLow16, Rounding16);
            DestinationHigh16 = _mm_add_epi16(DestinationHigh16, Rounding16);
            DestinationLow16 = _mm_add_epi16(
                DestinationLow16,
                _mm_srli_epi16(DestinationLow16, 8));
            DestinationHigh16 = _mm_add_epi16(
                DestinationHigh16,
                _mm_srli_epi16(DestinationHigh16, 8));
            DestinationLow16 = _mm_srli_epi16(DestinationLow16, 8);
            DestinationHigh16 = _mm_srli_epi16(DestinationHigh16, 8);

            PackedResult = _mm_packus_epi16(
                DestinationLow16,
                DestinationHigh16);
            PackedResult = _mm_or_si128(PackedResult, FramebufferAlphaMask);
            _mm_storeu_si128((__m128i*)(void*)Destination, PackedResult);

            Destination += 4;
            SourceAlpha += 4;
            PixelCount -= 4;
        }
    }
#endif

    while (PixelCount != 0)
    {
        const uint32_t Alpha = *SourceAlpha;
        const uint32_t InverseAlpha = 255u - Alpha;
        const uint32_t DestinationColor = *Destination;
        const uint32_t DestinationBlue = DestinationColor & 0xFFu;
        const uint32_t DestinationGreen = (DestinationColor >> 8) & 0xFFu;
        const uint32_t DestinationRed = (DestinationColor >> 16) & 0xFFu;
        const uint32_t OutputRed = NAIVE_SWR_DIV255_ROUNDED(
            SourceRed * Alpha + DestinationRed * InverseAlpha);
        const uint32_t OutputGreen = NAIVE_SWR_DIV255_ROUNDED(
            SourceGreen * Alpha + DestinationGreen * InverseAlpha);
        const uint32_t OutputBlue = NAIVE_SWR_DIV255_ROUNDED(
            SourceBlue * Alpha + DestinationBlue * InverseAlpha);

        *Destination =
            0xFF000000u | (OutputRed << 16) | (OutputGreen << 8) | OutputBlue;

        ++Destination;
        ++SourceAlpha;
        --PixelCount;
    }
}

/**
 * @brief Stores one normalized double-precision triangle edge.
 */
typedef struct _NAIVE_SWR_TRIANGLE_EDGE
{
    double RowValue;
    double StepX;
    double StepY;
    int TopLeft;
} NAIVE_SWR_TRIANGLE_EDGE, *PNAIVE_SWR_TRIANGLE_EDGE;
typedef const NAIVE_SWR_TRIANGLE_EDGE *PCNAIVE_SWR_TRIANGLE_EDGE;

/**
 * @brief Stores clipped bounds and normalized coverage state for one
 *        triangle.
 */
typedef struct _NAIVE_SWR_TRIANGLE_STATE
{
    int32_t X0;
    int32_t Y0;
    int32_t X1;
    int32_t Y1;
    double InverseArea;
    NAIVE_SWR_TRIANGLE_EDGE Edges[3];
} NAIVE_SWR_TRIANGLE_STATE, *PNAIVE_SWR_TRIANGLE_STATE;
typedef const NAIVE_SWR_TRIANGLE_STATE *PCNAIVE_SWR_TRIANGLE_STATE;

static NAIVE_SWR_INLINE int naive_swr_clip_pixel_bounds(
    int32_t* X0,
    int32_t* Y0,
    int32_t* X1,
    int32_t* Y1,
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect)
{
    if (*X0 < ClipRect->Left)
    {
        *X0 = ClipRect->Left;
    }

    if (*Y0 < ClipRect->Top)
    {
        *Y0 = ClipRect->Top;
    }

    if (*X1 > ClipRect->Right)
    {
        *X1 = ClipRect->Right;
    }

    if (*Y1 > ClipRect->Bottom)
    {
        *Y1 = ClipRect->Bottom;
    }

    *X0 = NAIVE_SWR_CLAMP(*X0, 0, Framebuffer->Width);
    *Y0 = NAIVE_SWR_CLAMP(*Y0, 0, Framebuffer->Height);
    *X1 = NAIVE_SWR_CLAMP(*X1, 0, Framebuffer->Width);
    *Y1 = NAIVE_SWR_CLAMP(*Y1, 0, Framebuffer->Height);

    return *X0 < *X1 && *Y0 < *Y1;
}

static NAIVE_SWR_INLINE double naive_swr_edge(
    PCNAIVE_SWR_POINT A,
    PCNAIVE_SWR_POINT B,
    double X,
    double Y)
{
    const double AX = (double)A->X;
    const double AY = (double)A->Y;
    const double BX = (double)B->X;
    const double BY = (double)B->Y;
    const double CoefficientX = BY - AY;
    const double CoefficientY = AX - BX;
    const double Constant = AY * BX - AX * BY;

    return CoefficientX * X + CoefficientY * Y + Constant;
}

static NAIVE_SWR_INLINE int naive_swr_is_top_left(
    PCNAIVE_SWR_POINT A,
    PCNAIVE_SWR_POINT B)
{
    const float DeltaX = B->X - A->X;
    const float DeltaY = B->Y - A->Y;

    return DeltaY < 0.0f || (DeltaY == 0.0f && DeltaX > 0.0f);
}

static NAIVE_SWR_INLINE void naive_swr_initialize_triangle_edge(
    PNAIVE_SWR_TRIANGLE_EDGE Edge,
    PCNAIVE_SWR_POINT A,
    PCNAIVE_SWR_POINT B,
    double Normalization,
    int ReverseTopLeft,
    double FirstPixelX,
    double FirstPixelY)
{
    const double RawStepX = (double)B->Y - (double)A->Y;
    const double RawStepY = -((double)B->X - (double)A->X);

    Edge->RowValue = naive_swr_edge(
        A, B, FirstPixelX, FirstPixelY) * Normalization;
    Edge->StepX = RawStepX * Normalization;
    Edge->StepY = RawStepY * Normalization;
    Edge->TopLeft = ReverseTopLeft
        ? naive_swr_is_top_left(B, A)
        : naive_swr_is_top_left(A, B);
}

static int naive_swr_initialize_triangle_state(
    PNAIVE_SWR_TRIANGLE_STATE State,
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    PCNAIVE_SWR_POINT Point1 = &RenderCommand->Command.Triangle.Positions[0];
    PCNAIVE_SWR_POINT Point2 = &RenderCommand->Command.Triangle.Positions[1];
    PCNAIVE_SWR_POINT Point3 = &RenderCommand->Command.Triangle.Positions[2];

    const double Area = naive_swr_edge(
        Point1,
        Point2,
        (double)Point3->X,
        (double)Point3->Y);

    float MinimumX;
    float MinimumY;
    float MaximumX;
    float MaximumY;
    double Normalization;
    double NormalizedArea;
    double FirstPixelX;
    double FirstPixelY;
    int ReverseTopLeft;

    if (Area == 0.0)
    {
        return 0;
    }

    MinimumX = Point1->X;
    MinimumY = Point1->Y;
    MaximumX = Point1->X;
    MaximumY = Point1->Y;

    if (Point2->X < MinimumX)
    {
        MinimumX = Point2->X;
    }

    if (Point3->X < MinimumX)
    {
        MinimumX = Point3->X;
    }

    if (Point2->Y < MinimumY)
    {
        MinimumY = Point2->Y;
    }

    if (Point3->Y < MinimumY)
    {
        MinimumY = Point3->Y;
    }

    if (Point2->X > MaximumX)
    {
        MaximumX = Point2->X;
    }

    if (Point3->X > MaximumX)
    {
        MaximumX = Point3->X;
    }

    if (Point2->Y > MaximumY)
    {
        MaximumY = Point2->Y;
    }

    if (Point3->Y > MaximumY)
    {
        MaximumY = Point3->Y;
    }

    State->X0 = (int32_t)floorf(MinimumX);
    State->Y0 = (int32_t)floorf(MinimumY);
    State->X1 = (int32_t)ceilf(MaximumX);
    State->Y1 = (int32_t)ceilf(MaximumY);

    if (!naive_swr_clip_pixel_bounds(
        &State->X0,
        &State->Y0,
        &State->X1,
        &State->Y1,
        Framebuffer,
        ClipRect))
    {
        return 0;
    }

    Normalization = Area < 0.0 ? -1.0 : 1.0;
    NormalizedArea = Area * Normalization;
    ReverseTopLeft = Area > 0.0;
    FirstPixelX = (double)State->X0 + 0.5;
    FirstPixelY = (double)State->Y0 + 0.5;
    State->InverseArea = 1.0 / NormalizedArea;

    naive_swr_initialize_triangle_edge(
        &State->Edges[0],
        Point2,
        Point3,
        Normalization,
        ReverseTopLeft,
        FirstPixelX,
        FirstPixelY);

    naive_swr_initialize_triangle_edge(
        &State->Edges[1],
        Point3,
        Point1,
        Normalization,
        ReverseTopLeft,
        FirstPixelX,
        FirstPixelY);

    naive_swr_initialize_triangle_edge(
        &State->Edges[2],
        Point1,
        Point2,
        Normalization,
        ReverseTopLeft,
        FirstPixelX,
        FirstPixelY);

    return 1;
}

static NAIVE_SWR_INLINE int naive_swr_constrain_coverage_span(
    double EdgeValue,
    double EdgeStepX,
    int TopLeft,
    int32_t* SpanBegin,
    int32_t* SpanEnd)
{
    double Crossing;
    double CurrentBegin;
    double CurrentLast;
    int32_t Truncated;

    if (EdgeStepX == 0.0)
    {
        return
            EdgeValue >= 0.0 &&
            (EdgeValue != 0.0 || TopLeft);
    }

    Crossing = -EdgeValue / EdgeStepX;
    CurrentBegin = (double)*SpanBegin;
    CurrentLast = (double)(*SpanEnd - 1);

    if (EdgeStepX > 0.0)
    {
        if (TopLeft)
        {
            if (Crossing <= CurrentBegin)
            {
                return *SpanBegin < *SpanEnd;
            }

            if (Crossing > CurrentLast)
            {
                return 0;
            }

            Truncated = (int32_t)Crossing;
            *SpanBegin = Truncated + ((double)Truncated < Crossing ? 1 : 0);
        }
        else
        {
            if (Crossing < CurrentBegin)
            {
                return *SpanBegin < *SpanEnd;
            }

            if (Crossing >= CurrentLast)
            {
                return 0;
            }

            *SpanBegin = (int32_t)Crossing + 1;
        }
    }
    else
    {
        if (TopLeft)
        {
            if (Crossing >= CurrentLast)
            {
                return *SpanBegin < *SpanEnd;
            }

            if (Crossing < CurrentBegin)
            {
                return 0;
            }

            *SpanEnd = (int32_t)Crossing + 1;
        }
        else
        {
            if (Crossing > CurrentLast)
            {
                return *SpanBegin < *SpanEnd;
            }

            if (Crossing <= CurrentBegin)
            {
                return 0;
            }

            Truncated = (int32_t)Crossing;
            *SpanEnd = Truncated + ((double)Truncated < Crossing ? 1 : 0);
        }
    }

    return *SpanBegin < *SpanEnd;
}

static NAIVE_SWR_INLINE int naive_swr_get_triangle_coverage_span(
    PCNAIVE_SWR_TRIANGLE_STATE State,
    int32_t* SpanBegin,
    int32_t* SpanEnd)
{
    *SpanBegin = 0;
    *SpanEnd = State->X1 - State->X0;

    return
        naive_swr_constrain_coverage_span(
            State->Edges[0].RowValue,
            State->Edges[0].StepX,
            State->Edges[0].TopLeft,
            SpanBegin,
            SpanEnd) &&
        naive_swr_constrain_coverage_span(
            State->Edges[1].RowValue,
            State->Edges[1].StepX,
            State->Edges[1].TopLeft,
            SpanBegin,
            SpanEnd) &&
        naive_swr_constrain_coverage_span(
            State->Edges[2].RowValue,
            State->Edges[2].StepX,
            State->Edges[2].TopLeft,
            SpanBegin,
            SpanEnd);
}

static NAIVE_SWR_INLINE void naive_swr_advance_triangle_row(
    PNAIVE_SWR_TRIANGLE_STATE State)
{
    State->Edges[0].RowValue += State->Edges[0].StepY;
    State->Edges[1].RowValue += State->Edges[1].StepY;
    State->Edges[2].RowValue += State->Edges[2].StepY;
}

static int naive_swr_get_rectangle_pixel_bounds(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand,
    int32_t* X0,
    int32_t* Y0,
    int32_t* X1,
    int32_t* Y1)
{
    const NAIVE_SWR_POINT Position = RenderCommand->Command.Rectangle.Position;
    const NAIVE_SWR_SIZE Size = RenderCommand->Command.Rectangle.Size;
    float Left = Position.X;
    float Top = Position.Y;
    float Right = Position.X + Size.Width;
    float Bottom = Position.Y + Size.Height;
    float Temporary;

    if (Right < Left)
    {
        Temporary = Left;
        Left = Right;
        Right = Temporary;
    }

    if (Bottom < Top)
    {
        Temporary = Top;
        Top = Bottom;
        Bottom = Temporary;
    }

    *X0 = (int32_t)ceilf(Left - 0.5f);
    *Y0 = (int32_t)ceilf(Top - 0.5f);
    *X1 = (int32_t)ceilf(Right - 0.5f);
    *Y1 = (int32_t)ceilf(Bottom - 0.5f);

    return naive_swr_clip_pixel_bounds(
        X0, Y0, X1, Y1, Framebuffer, ClipRect);
}

static void naive_swr_render_solid_rectangle(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    const NAIVE_SWR_SIZE Size = RenderCommand->Command.Rectangle.Size;
    const NAIVE_SWR_COLOR Color = RenderCommand->Command.Rectangle.Color;
    NAIVE_SWR_CONSTANT_BLEND_STATE BlendState;
    uint32_t* DestinationRow;
    uint32_t FillColor;
    uint32_t SourceAlpha;
    int32_t X0;
    int32_t Y0;
    int32_t X1;
    int32_t Y1;
    int32_t SpanWidth;
    int32_t Y;

    if (Color.Alpha == 0 ||
        Size.Width == 0.0f ||
        Size.Height == 0.0f)
    {
        return;
    }

    if (!naive_swr_get_rectangle_pixel_bounds(
        Framebuffer, ClipRect, RenderCommand,
        &X0, &Y0, &X1, &Y1))
    {
        return;
    }

    SpanWidth = X1 - X0;
    DestinationRow = Framebuffer->Pixels +
        (size_t)Y0 * Framebuffer->PixelStride + X0;

    if (Color.Alpha == 255)
    {
        FillColor =
            0xFF000000u |
            ((uint32_t)Color.Red << 16) |
            ((uint32_t)Color.Green << 8) |
            (uint32_t)Color.Blue;

        for (Y = Y0; Y < Y1; ++Y)
        {
            naive_swr_fill_span(
                DestinationRow, (size_t)SpanWidth, FillColor);
            DestinationRow += Framebuffer->PixelStride;
        }

        return;
    }

    SourceAlpha = Color.Alpha;
    BlendState.SourceRedAlpha = (uint32_t)Color.Red * SourceAlpha;
    BlendState.SourceGreenAlpha = (uint32_t)Color.Green * SourceAlpha;
    BlendState.SourceBlueAlpha = (uint32_t)Color.Blue * SourceAlpha;
    BlendState.InverseAlpha = 255u - SourceAlpha;

    for (Y = Y0; Y < Y1; ++Y)
    {
        naive_swr_blend_constant_span(
            DestinationRow, (size_t)SpanWidth, &BlendState);
        DestinationRow += Framebuffer->PixelStride;
    }
}

static void naive_swr_render_textured_rectangle(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_TEXTURE Texture,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    const NAIVE_SWR_POINT Position = RenderCommand->Command.Rectangle.Position;
    const NAIVE_SWR_SIZE Size = RenderCommand->Command.Rectangle.Size;
    const NAIVE_SWR_COLOR Color = RenderCommand->Command.Rectangle.Color;
    const NAIVE_SWR_POINT TexturePosition =
        RenderCommand->Command.Rectangle.TexturePosition;
    const NAIVE_SWR_SIZE TextureSize =
        RenderCommand->Command.Rectangle.TextureSize;
    float TextureStepX;
    float TextureStepY;
    float TextureXStart;
    float TextureYStart;
    float TextureY;
    uint32_t* DestinationRow;
    int32_t X0;
    int32_t Y0;
    int32_t X1;
    int32_t Y1;
    int32_t DestinationWidth;
    int32_t DestinationHeight;
    int32_t Y;

    if (Texture == NULL ||
        Texture->Pixels == NULL ||
        Texture->Width <= 0 ||
        Texture->Height <= 0 ||
        Color.Alpha == 0 ||
        Size.Width == 0.0f ||
        Size.Height == 0.0f)
    {
        return;
    }

    if (Texture->Format == NAIVE_SWR_TEXTURE_FORMAT_ALPHA8)
    {
        if (Texture->ByteStride < (size_t)Texture->Width)
        {
            return;
        }
    }
    else if (Texture->Format == NAIVE_SWR_TEXTURE_FORMAT_RGBA32)
    {
        if (Texture->ByteStride < (size_t)Texture->Width * 4)
        {
            return;
        }
    }
    else
    {
        return;
    }

    if (!naive_swr_get_rectangle_pixel_bounds(
        Framebuffer, ClipRect, RenderCommand,
        &X0, &Y0, &X1, &Y1))
    {
        return;
    }

    DestinationWidth = X1 - X0;
    DestinationHeight = Y1 - Y0;
    TextureStepX = TextureSize.Width / Size.Width;
    TextureStepY = TextureSize.Height / Size.Height;
    TextureXStart = TexturePosition.X +
        (((float)X0 + 0.5f) - Position.X) * TextureStepX;
    TextureYStart = TexturePosition.Y +
        (((float)Y0 + 0.5f) - Position.Y) * TextureStepY;

    if (Texture->Format == NAIVE_SWR_TEXTURE_FORMAT_ALPHA8 &&
        Color.Alpha == 255 &&
        TextureStepX == 1.0f &&
        TextureStepY == 1.0f)
    {
        const int32_t SourceX0 = (int32_t)TextureXStart;
        const int32_t SourceY0 = (int32_t)TextureYStart;
        const float SourceXLast =
            TextureXStart + (float)(DestinationWidth - 1);
        const float SourceYLast =
            TextureYStart + (float)(DestinationHeight - 1);

        if (TextureXStart >= 0.0f &&
            TextureYStart >= 0.0f &&
            SourceXLast < (float)Texture->Width &&
            SourceYLast < (float)Texture->Height &&
            SourceX0 >= 0 &&
            SourceY0 >= 0 &&
            SourceX0 + DestinationWidth <= Texture->Width &&
            SourceY0 + DestinationHeight <= Texture->Height)
        {
            const uint8_t* SourceRow = Texture->Pixels +
                (size_t)SourceY0 * Texture->ByteStride + SourceX0;

            DestinationRow = Framebuffer->Pixels +
                (size_t)Y0 * Framebuffer->PixelStride + X0;

            for (Y = 0; Y < DestinationHeight; ++Y)
            {
                naive_swr_blend_a8_opaque_tint_span(
                    DestinationRow, SourceRow, (size_t)DestinationWidth,
                    Color.Red, Color.Green, Color.Blue);
                DestinationRow += Framebuffer->PixelStride;
                SourceRow += Texture->ByteStride;
            }

            return;
        }
    }

    DestinationRow = Framebuffer->Pixels +
        (size_t)Y0 * Framebuffer->PixelStride + X0;
    TextureY = TextureYStart;

    if (Texture->Format == NAIVE_SWR_TEXTURE_FORMAT_ALPHA8)
    {
        for (Y = Y0; Y < Y1; ++Y)
        {
            const uint8_t* SourceRow;
            uint32_t* Destination = DestinationRow;
            float TextureX = TextureXStart;
            int32_t TextureYInteger = (int32_t)TextureY;
            int32_t X;

            TextureYInteger = NAIVE_SWR_CLAMP(
                TextureYInteger, 0, Texture->Height - 1);
            SourceRow = Texture->Pixels +
                (size_t)TextureYInteger * Texture->ByteStride;

            for (X = X0; X < X1; ++X)
            {
                int32_t TextureXInteger = (int32_t)TextureX;
                int TextureAlpha;
                int SourceAlpha;

                TextureXInteger = NAIVE_SWR_CLAMP(
                    TextureXInteger, 0, Texture->Width - 1);
                TextureAlpha = SourceRow[TextureXInteger];
                SourceAlpha = NAIVE_SWR_MUL255(
                    TextureAlpha, Color.Alpha);

                naive_swr_blend_over(
                    Destination,
                    Color.Red,
                    Color.Green,
                    Color.Blue,
                    SourceAlpha);

                ++Destination;
                TextureX += TextureStepX;
            }

            DestinationRow += Framebuffer->PixelStride;
            TextureY += TextureStepY;
        }

        return;
    }

    for (Y = Y0; Y < Y1; ++Y)
    {
        const uint8_t* SourceRow;
        uint32_t* Destination = DestinationRow;
        float TextureX = TextureXStart;
        int32_t TextureYInteger = (int32_t)TextureY;
        int32_t X;

        TextureYInteger = NAIVE_SWR_CLAMP(
            TextureYInteger, 0, Texture->Height - 1);
        SourceRow = Texture->Pixels +
            (size_t)TextureYInteger * Texture->ByteStride;

        for (X = X0; X < X1; ++X)
        {
            int32_t TextureXInteger = (int32_t)TextureX;
            const uint8_t* Texel;
            int SourceRed;
            int SourceGreen;
            int SourceBlue;
            int SourceAlpha;

            TextureXInteger = NAIVE_SWR_CLAMP(
                TextureXInteger, 0, Texture->Width - 1);
            Texel = SourceRow + (size_t)TextureXInteger * 4;

            SourceRed = NAIVE_SWR_MUL255(
                Texel[0], Color.Red);
            SourceGreen = NAIVE_SWR_MUL255(
                Texel[1], Color.Green);
            SourceBlue = NAIVE_SWR_MUL255(
                Texel[2], Color.Blue);
            SourceAlpha = NAIVE_SWR_MUL255(
                Texel[3], Color.Alpha);

            naive_swr_blend_over(
                Destination,
                SourceRed,
                SourceGreen,
                SourceBlue,
                SourceAlpha);

            ++Destination;
            TextureX += TextureStepX;
        }

        DestinationRow += Framebuffer->PixelStride;
        TextureY += TextureStepY;
    }
}

static void naive_swr_render_solid_opaque_triangle(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    const NAIVE_SWR_COLOR Color = RenderCommand->Command.Triangle.Colors[0];
    NAIVE_SWR_TRIANGLE_STATE State;
    uint32_t* DestinationRow;
    uint32_t FillColor;
    int32_t Y;

    if (!naive_swr_initialize_triangle_state(
        &State, Framebuffer, ClipRect, RenderCommand))
    {
        return;
    }

    FillColor =
        0xFF000000u |
        ((uint32_t)Color.Red << 16) |
        ((uint32_t)Color.Green << 8) |
        (uint32_t)Color.Blue;
    DestinationRow =
        Framebuffer->Pixels +
        (size_t)State.Y0 * Framebuffer->PixelStride + State.X0;

    for (Y = State.Y0; Y < State.Y1; ++Y)
    {
        int32_t SpanBegin;
        int32_t SpanEnd;

        if (naive_swr_get_triangle_coverage_span(
            &State, &SpanBegin, &SpanEnd))
        {
            const int32_t SpanWidth = SpanEnd - SpanBegin;

            naive_swr_fill_span(
                DestinationRow + SpanBegin,
                (size_t)SpanWidth,
                FillColor);
        }

        DestinationRow += Framebuffer->PixelStride;
        naive_swr_advance_triangle_row(&State);
    }
}

static void naive_swr_render_solid_translucent_triangle(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    const NAIVE_SWR_COLOR Color = RenderCommand->Command.Triangle.Colors[0];
    const uint32_t SourceAlpha = Color.Alpha;
    NAIVE_SWR_CONSTANT_BLEND_STATE BlendState;
    NAIVE_SWR_TRIANGLE_STATE State;
    uint32_t* DestinationRow;
    int32_t Y;

    if (!naive_swr_initialize_triangle_state(
        &State, Framebuffer, ClipRect, RenderCommand))
    {
        return;
    }

    BlendState.SourceRedAlpha = (uint32_t)Color.Red * SourceAlpha;
    BlendState.SourceGreenAlpha = (uint32_t)Color.Green * SourceAlpha;
    BlendState.SourceBlueAlpha = (uint32_t)Color.Blue * SourceAlpha;
    BlendState.InverseAlpha = 255u - SourceAlpha;
    DestinationRow = Framebuffer->Pixels +
        (size_t)State.Y0 * Framebuffer->PixelStride + State.X0;

    for (Y = State.Y0; Y < State.Y1; ++Y)
    {
        int32_t SpanBegin;
        int32_t SpanEnd;

        if (naive_swr_get_triangle_coverage_span(
            &State, &SpanBegin, &SpanEnd))
        {
            const int32_t SpanWidth = SpanEnd - SpanBegin;

            naive_swr_blend_constant_span(
                DestinationRow + SpanBegin,
                (size_t)SpanWidth,
                &BlendState);
        }

        DestinationRow += Framebuffer->PixelStride;
        naive_swr_advance_triangle_row(&State);
    }
}

static void naive_swr_render_solid_alpha_gradient_triangle(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    const NAIVE_SWR_COLOR Color1 = RenderCommand->Command.Triangle.Colors[0];
    const NAIVE_SWR_COLOR Color2 = RenderCommand->Command.Triangle.Colors[1];
    const NAIVE_SWR_COLOR Color3 = RenderCommand->Command.Triangle.Colors[2];
    const int SourceRed = Color1.Red;
    const int SourceGreen = Color1.Green;
    const int SourceBlue = Color1.Blue;
    const double Alpha1 = (double)Color1.Alpha;
    const double Alpha2 = (double)Color2.Alpha;
    const double Alpha3 = (double)Color3.Alpha;
    NAIVE_SWR_TRIANGLE_STATE State;
    uint32_t* DestinationRow;
    double AlphaRow;
    double AlphaStepX;
    double AlphaStepY;
    int32_t Y;

    if (!naive_swr_initialize_triangle_state(
        &State, Framebuffer, ClipRect, RenderCommand))
    {
        return;
    }

    if (Color1.Alpha == Color2.Alpha)
    {
        const double ScaledAlphaDelta = (Alpha3 - Alpha1) * State.InverseArea;

        AlphaRow = Alpha1 + State.Edges[2].RowValue * ScaledAlphaDelta;
        AlphaStepX = State.Edges[2].StepX * ScaledAlphaDelta;
        AlphaStepY = State.Edges[2].StepY * ScaledAlphaDelta;
    }
    else if (Color1.Alpha == Color3.Alpha)
    {
        const double ScaledAlphaDelta = (Alpha2 - Alpha1) * State.InverseArea;

        AlphaRow = Alpha1 + State.Edges[1].RowValue * ScaledAlphaDelta;
        AlphaStepX = State.Edges[1].StepX * ScaledAlphaDelta;
        AlphaStepY = State.Edges[1].StepY * ScaledAlphaDelta;
    }
    else if (Color2.Alpha == Color3.Alpha)
    {
        const double ScaledAlphaDelta = (Alpha1 - Alpha2) * State.InverseArea;

        AlphaRow = Alpha2 + State.Edges[0].RowValue * ScaledAlphaDelta;
        AlphaStepX = State.Edges[0].StepX * ScaledAlphaDelta;
        AlphaStepY = State.Edges[0].StepY * ScaledAlphaDelta;
    }
    else
    {
        AlphaRow =
            (Alpha1 * State.Edges[0].RowValue +
                Alpha2 * State.Edges[1].RowValue +
                Alpha3 * State.Edges[2].RowValue) *
            State.InverseArea;

        AlphaStepX =
            (Alpha1 * State.Edges[0].StepX +
                Alpha2 * State.Edges[1].StepX +
                Alpha3 * State.Edges[2].StepX) *
            State.InverseArea;

        AlphaStepY =
            (Alpha1 * State.Edges[0].StepY +
                Alpha2 * State.Edges[1].StepY +
                Alpha3 * State.Edges[2].StepY) *
            State.InverseArea;
    }

    DestinationRow = Framebuffer->Pixels +
        (size_t)State.Y0 * Framebuffer->PixelStride + State.X0;

    for (Y = State.Y0; Y < State.Y1; ++Y)
    {
        int32_t SpanBegin;
        int32_t SpanEnd;

        if (naive_swr_get_triangle_coverage_span(&State, &SpanBegin, &SpanEnd))
        {
            const int32_t SpanWidth = SpanEnd - SpanBegin;
            uint32_t* Destination = DestinationRow + SpanBegin;
            double InterpolatedAlpha =
                AlphaRow + AlphaStepX * (double)SpanBegin;
            int32_t Offset;

            for (Offset = 0; Offset < SpanWidth; ++Offset)
            {
                int SourceAlpha = (int)(InterpolatedAlpha + 0.5);

                SourceAlpha = NAIVE_SWR_CLAMP(SourceAlpha, 0, 255);
                naive_swr_blend_over(
                    Destination,
                    SourceRed,
                    SourceGreen,
                    SourceBlue,
                    SourceAlpha);

                ++Destination;
                InterpolatedAlpha += AlphaStepX;
            }
        }

        DestinationRow += Framebuffer->PixelStride;
        naive_swr_advance_triangle_row(&State);
        AlphaRow += AlphaStepY;
    }
}

/**
 * @brief Stores an affine texture-coordinate plane in texel coordinates.
 */
typedef struct _NAIVE_SWR_TEXTURE_PLANE
{
    double XRow;
    double YRow;
    double XStepX;
    double YStepX;
    double XStepY;
    double YStepY;
} NAIVE_SWR_TEXTURE_PLANE, *PNAIVE_SWR_TEXTURE_PLANE;

static NAIVE_SWR_INLINE int naive_swr_is_valid_alpha8_texture(
    PCNAIVE_SWR_TEXTURE Texture)
{
    return Texture != NULL &&
        Texture->Pixels != NULL &&
        Texture->Width > 0 &&
        Texture->Height > 0 &&
        Texture->ByteStride >= (size_t)Texture->Width &&
        Texture->Format == NAIVE_SWR_TEXTURE_FORMAT_ALPHA8;
}

static NAIVE_SWR_INLINE int naive_swr_is_valid_rgba32_texture(
    PCNAIVE_SWR_TEXTURE Texture)
{
    return Texture != NULL &&
        Texture->Pixels != NULL &&
        Texture->Width > 0 &&
        Texture->Height > 0 &&
        Texture->ByteStride >= (size_t)Texture->Width * 4 &&
        Texture->Format == NAIVE_SWR_TEXTURE_FORMAT_RGBA32;
}

static void naive_swr_initialize_texture_plane(
    PNAIVE_SWR_TEXTURE_PLANE Plane,
    PCNAIVE_SWR_TRIANGLE_STATE State,
    PCNAIVE_SWR_TEXTURE Texture,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    const NAIVE_SWR_TEXTURE_COORDINATE TextureCoordinate1 =
        RenderCommand->Command.Triangle.TextureCoordinates[0];
    const NAIVE_SWR_TEXTURE_COORDINATE TextureCoordinate2 =
        RenderCommand->Command.Triangle.TextureCoordinates[1];
    const NAIVE_SWR_TEXTURE_COORDINATE TextureCoordinate3 =
        RenderCommand->Command.Triangle.TextureCoordinates[2];
    const double TextureWidth = (double)Texture->Width;
    const double TextureHeight = (double)Texture->Height;
    const double Weight1Row = State->Edges[0].RowValue * State->InverseArea;
    const double Weight2Row = State->Edges[1].RowValue * State->InverseArea;
    const double Weight3Row = State->Edges[2].RowValue * State->InverseArea;
    const double Weight1StepX = State->Edges[0].StepX * State->InverseArea;
    const double Weight2StepX = State->Edges[1].StepX * State->InverseArea;
    const double Weight3StepX = State->Edges[2].StepX * State->InverseArea;
    const double Weight1StepY = State->Edges[0].StepY * State->InverseArea;
    const double Weight2StepY = State->Edges[1].StepY * State->InverseArea;
    const double Weight3StepY = State->Edges[2].StepY * State->InverseArea;

    Plane->XRow = ((double)TextureCoordinate1.U * Weight1Row +
        (double)TextureCoordinate2.U * Weight2Row +
        (double)TextureCoordinate3.U * Weight3Row) * TextureWidth;
    Plane->YRow = ((double)TextureCoordinate1.V * Weight1Row +
        (double)TextureCoordinate2.V * Weight2Row +
        (double)TextureCoordinate3.V * Weight3Row) * TextureHeight;
    Plane->XStepX = ((double)TextureCoordinate1.U * Weight1StepX +
        (double)TextureCoordinate2.U * Weight2StepX +
        (double)TextureCoordinate3.U * Weight3StepX) * TextureWidth;
    Plane->YStepX = ((double)TextureCoordinate1.V * Weight1StepX +
        (double)TextureCoordinate2.V * Weight2StepX +
        (double)TextureCoordinate3.V * Weight3StepX) * TextureHeight;
    Plane->XStepY = ((double)TextureCoordinate1.U * Weight1StepY +
        (double)TextureCoordinate2.U * Weight2StepY +
        (double)TextureCoordinate3.U * Weight3StepY) * TextureWidth;
    Plane->YStepY = ((double)TextureCoordinate1.V * Weight1StepY +
        (double)TextureCoordinate2.V * Weight2StepY +
        (double)TextureCoordinate3.V * Weight3StepY) * TextureHeight;
}

static NAIVE_SWR_INLINE int naive_swr_sample_alpha8(
    PCNAIVE_SWR_TEXTURE Texture,
    double TextureX,
    double TextureY)
{
    int32_t TextureXInteger = (int32_t)TextureX;
    int32_t TextureYInteger = (int32_t)TextureY;

    TextureXInteger = NAIVE_SWR_CLAMP(
        TextureXInteger, 0, Texture->Width - 1);
    TextureYInteger = NAIVE_SWR_CLAMP(
        TextureYInteger, 0, Texture->Height - 1);

    return Texture->Pixels[
        (size_t)TextureYInteger * Texture->ByteStride +
            (size_t)TextureXInteger];
}

static void naive_swr_render_alpha8_opaque_tint_triangle(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_TEXTURE Texture,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    const NAIVE_SWR_COLOR Color = RenderCommand->Command.Triangle.Colors[0];
    NAIVE_SWR_TRIANGLE_STATE State;
    NAIVE_SWR_TEXTURE_PLANE Plane;
    uint32_t* DestinationRow;
    int32_t Y;

    if (!naive_swr_is_valid_alpha8_texture(Texture))
    {
        return;
    }

    if (!naive_swr_initialize_triangle_state(
        &State, Framebuffer, ClipRect, RenderCommand))
    {
        return;
    }

    naive_swr_initialize_texture_plane(
        &Plane, &State, Texture, RenderCommand);
    DestinationRow = Framebuffer->Pixels +
        (size_t)State.Y0 * Framebuffer->PixelStride + State.X0;

    for (Y = State.Y0; Y < State.Y1; ++Y)
    {
        int32_t SpanBegin;
        int32_t SpanEnd;

        if (naive_swr_get_triangle_coverage_span(&State, &SpanBegin, &SpanEnd))
        {
            const int32_t SpanWidth = SpanEnd - SpanBegin;
            const double SpanOffset = (double)SpanBegin;
            uint32_t* Destination = DestinationRow + SpanBegin;
            double TextureX = Plane.XRow + Plane.XStepX * SpanOffset;
            double TextureY = Plane.YRow + Plane.YStepX * SpanOffset;
            int32_t Offset;

            for (Offset = 0; Offset < SpanWidth; ++Offset)
            {
                const int SourceAlpha = naive_swr_sample_alpha8(
                    Texture, TextureX, TextureY);

                naive_swr_blend_over(
                    Destination,
                    Color.Red,
                    Color.Green,
                    Color.Blue,
                    SourceAlpha);

                ++Destination;
                TextureX += Plane.XStepX;
                TextureY += Plane.YStepX;
            }
        }

        DestinationRow += Framebuffer->PixelStride;
        naive_swr_advance_triangle_row(&State);
        Plane.XRow += Plane.XStepY;
        Plane.YRow += Plane.YStepY;
    }
}

static void naive_swr_render_alpha8_translucent_tint_triangle(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_TEXTURE Texture,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    const NAIVE_SWR_COLOR Color = RenderCommand->Command.Triangle.Colors[0];
    const int TintAlpha = Color.Alpha;
    NAIVE_SWR_TRIANGLE_STATE State;
    NAIVE_SWR_TEXTURE_PLANE Plane;
    uint32_t* DestinationRow;
    int32_t Y;

    if (!naive_swr_is_valid_alpha8_texture(Texture))
    {
        return;
    }

    if (!naive_swr_initialize_triangle_state(
        &State, Framebuffer, ClipRect, RenderCommand))
    {
        return;
    }

    naive_swr_initialize_texture_plane(
        &Plane, &State, Texture, RenderCommand);
    DestinationRow = Framebuffer->Pixels +
        (size_t)State.Y0 * Framebuffer->PixelStride + State.X0;

    for (Y = State.Y0; Y < State.Y1; ++Y)
    {
        int32_t SpanBegin;
        int32_t SpanEnd;

        if (naive_swr_get_triangle_coverage_span(&State, &SpanBegin, &SpanEnd))
        {
            const int32_t SpanWidth = SpanEnd - SpanBegin;
            const double SpanOffset = (double)SpanBegin;
            uint32_t* Destination = DestinationRow + SpanBegin;
            double TextureX = Plane.XRow + Plane.XStepX * SpanOffset;
            double TextureY = Plane.YRow + Plane.YStepX * SpanOffset;
            int32_t Offset;

            for (Offset = 0; Offset < SpanWidth; ++Offset)
            {
                const int TextureAlpha = naive_swr_sample_alpha8(
                    Texture, TextureX, TextureY);
                const int SourceAlpha =
                    NAIVE_SWR_MUL255(TextureAlpha, TintAlpha);

                naive_swr_blend_over(
                    Destination,
                    Color.Red,
                    Color.Green,
                    Color.Blue,
                    SourceAlpha);

                ++Destination;
                TextureX += Plane.XStepX;
                TextureY += Plane.YStepX;
            }
        }

        DestinationRow += Framebuffer->PixelStride;
        naive_swr_advance_triangle_row(&State);
        Plane.XRow += Plane.XStepY;
        Plane.YRow += Plane.YStepY;
    }
}

static void naive_swr_render_general_triangle(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_TEXTURE Texture,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand,
    NAIVE_SWR_RENDER_TYPE RenderType)
{
    const NAIVE_SWR_COLOR Color1 = RenderCommand->Command.Triangle.Colors[0];
    const NAIVE_SWR_COLOR Color2 = RenderCommand->Command.Triangle.Colors[1];
    const NAIVE_SWR_COLOR Color3 = RenderCommand->Command.Triangle.Colors[2];
    NAIVE_SWR_TRIANGLE_STATE State;
    uint32_t* DestinationRow;
    int32_t Y;

    if (RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE)
    {
        if (!naive_swr_is_valid_alpha8_texture(Texture))
        {
            return;
        }
    }
    else if (RenderType == NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE)
    {
        if (!naive_swr_is_valid_rgba32_texture(Texture))
        {
            return;
        }
    }
    else if (RenderType != NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE)
    {
        return;
    }

    if (!naive_swr_initialize_triangle_state(
        &State, Framebuffer, ClipRect, RenderCommand))
    {
        return;
    }

    DestinationRow =
        Framebuffer->Pixels +
        (size_t)State.Y0 * Framebuffer->PixelStride + State.X0;

    for (Y = State.Y0; Y < State.Y1; ++Y)
    {
        double Edge1 = State.Edges[0].RowValue;
        double Edge2 = State.Edges[1].RowValue;
        double Edge3 = State.Edges[2].RowValue;
        uint32_t* Destination = DestinationRow;
        int32_t X;

        for (X = State.X0; X < State.X1; ++X)
        {
            if (Edge1 >= 0.0 &&
                Edge2 >= 0.0 &&
                Edge3 >= 0.0 &&
                (Edge1 != 0.0 || State.Edges[0].TopLeft) &&
                (Edge2 != 0.0 || State.Edges[1].TopLeft) &&
                (Edge3 != 0.0 || State.Edges[2].TopLeft))
            {
                const double Weight1 = Edge1 * State.InverseArea;
                const double Weight2 = Edge2 * State.InverseArea;
                const double Weight3 = Edge3 * State.InverseArea;
                int VertexRed;
                int VertexGreen;
                int VertexBlue;
                int VertexAlpha;
                int SourceRed;
                int SourceGreen;
                int SourceBlue;
                int SourceAlpha;

                VertexRed = (int)(
                    (double)Color1.Red * Weight1 +
                    (double)Color2.Red * Weight2 +
                    (double)Color3.Red * Weight3 +
                    0.5);
                VertexGreen = (int)(
                    (double)Color1.Green * Weight1 +
                    (double)Color2.Green * Weight2 +
                    (double)Color3.Green * Weight3 +
                    0.5);
                VertexBlue = (int)(
                    (double)Color1.Blue * Weight1 +
                    (double)Color2.Blue * Weight2 +
                    (double)Color3.Blue * Weight3 +
                    0.5);
                VertexAlpha = (int)(
                    (double)Color1.Alpha * Weight1 +
                    (double)Color2.Alpha * Weight2 +
                    (double)Color3.Alpha * Weight3 +
                    0.5);

                VertexRed = NAIVE_SWR_CLAMP(VertexRed, 0, 255);
                VertexGreen = NAIVE_SWR_CLAMP(VertexGreen, 0, 255);
                VertexBlue = NAIVE_SWR_CLAMP(VertexBlue, 0, 255);
                VertexAlpha = NAIVE_SWR_CLAMP(VertexAlpha, 0, 255);
                SourceRed = VertexRed;
                SourceGreen = VertexGreen;
                SourceBlue = VertexBlue;
                SourceAlpha = VertexAlpha;

                if (RenderType != NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE)
                {
                    double TextureU;
                    double TextureV;
                    int32_t TextureX;
                    int32_t TextureY;
                    size_t TextureOffset;

                    TextureU =
                        (double)RenderCommand->Command.Triangle
                        .TextureCoordinates[0].U * Weight1 +
                        (double)RenderCommand->Command.Triangle
                        .TextureCoordinates[1].U * Weight2 +
                        (double)RenderCommand->Command.Triangle
                        .TextureCoordinates[2].U * Weight3;
                    TextureV =
                        (double)RenderCommand->Command.Triangle
                        .TextureCoordinates[0].V * Weight1 +
                        (double)RenderCommand->Command.Triangle
                        .TextureCoordinates[1].V * Weight2 +
                        (double)RenderCommand->Command.Triangle
                        .TextureCoordinates[2].V * Weight3;

                    TextureX = (int32_t)(TextureU * Texture->Width);
                    TextureY = (int32_t)(TextureV * Texture->Height);
                    TextureX = NAIVE_SWR_CLAMP(TextureX, 0, Texture->Width - 1);
                    TextureY = NAIVE_SWR_CLAMP(TextureY, 0, Texture->Height - 1);
                    TextureOffset = (size_t)TextureY * Texture->ByteStride;

                    if (RenderType ==
                        NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE)
                    {
                        const int TextureAlpha =
                            Texture->Pixels[TextureOffset + TextureX];

                        SourceAlpha = NAIVE_SWR_MUL255(
                            TextureAlpha, VertexAlpha);
                    }
                    else
                    {
                        const uint8_t* Texel =
                            Texture->Pixels +
                            TextureOffset +
                            (size_t)TextureX * 4;

                        SourceRed = NAIVE_SWR_MUL255(Texel[0], VertexRed);
                        SourceGreen = NAIVE_SWR_MUL255(Texel[1], VertexGreen);
                        SourceBlue = NAIVE_SWR_MUL255(Texel[2], VertexBlue);
                        SourceAlpha = NAIVE_SWR_MUL255(Texel[3], VertexAlpha);
                    }
                }

                naive_swr_blend_over(
                    Destination,
                    SourceRed,
                    SourceGreen,
                    SourceBlue,
                    SourceAlpha);
            }

            ++Destination;
            Edge1 += State.Edges[0].StepX;
            Edge2 += State.Edges[1].StepX;
            Edge3 += State.Edges[2].StepX;
        }

        DestinationRow += Framebuffer->PixelStride;
        naive_swr_advance_triangle_row(&State);
    }
}

static NAIVE_SWR_INLINE int naive_swr_is_valid_framebuffer(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer)
{
    return Framebuffer != NULL &&
        Framebuffer->Pixels != NULL &&
        Framebuffer->Width > 0 &&
        Framebuffer->Height > 0 &&
        Framebuffer->PixelStride >= (size_t)Framebuffer->Width;
}

static NAIVE_SWR_INLINE int naive_swr_is_valid_clip_rect(
    PCNAIVE_SWR_CLIP_RECT ClipRect)
{
    return ClipRect != NULL &&
        ClipRect->Left < ClipRect->Right &&
        ClipRect->Top < ClipRect->Bottom;
}

EXTERN_C void naive_swr_clear_framebuffer(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    NAIVE_SWR_COLOR Color)
{
    uint32_t ClearPixel;
    int32_t Row;

    if (!naive_swr_is_valid_framebuffer(Framebuffer))
    {
        return;
    }

    ClearPixel =
        0xFF000000u |
        ((uint32_t)Color.Red << 16) |
        ((uint32_t)Color.Green << 8) |
        (uint32_t)Color.Blue;

    if (Framebuffer->PixelStride == (size_t)Framebuffer->Width)
    {
        naive_swr_fill_span(
            Framebuffer->Pixels,
            (size_t)Framebuffer->Width * (size_t)Framebuffer->Height,
            ClearPixel);
        return;
    }

    for (Row = 0; Row < Framebuffer->Height; ++Row)
    {
        uint32_t* Destination =
            Framebuffer->Pixels +
            (size_t)Row * Framebuffer->PixelStride;
        naive_swr_fill_span(
            Destination,
            (size_t)Framebuffer->Width,
            ClearPixel);
    }
}

EXTERN_C void naive_swr_render_command(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_TEXTURE Texture,
    PCNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    if (!naive_swr_is_valid_framebuffer(Framebuffer) ||
        !naive_swr_is_valid_clip_rect(ClipRect) ||
        RenderCommand == NULL)
    {
        return;
    }

    switch (RenderCommand->Type)
    {
    case NAIVE_SWR_RENDER_TYPE_SKIPPED:
        return;
    case NAIVE_SWR_RENDER_TYPE_SOLID_RECTANGLE:
        naive_swr_render_solid_rectangle(
            Framebuffer, ClipRect, RenderCommand);
        return;
    case NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE:
        if (!naive_swr_is_valid_alpha8_texture(Texture))
        {
            return;
        }
        naive_swr_render_textured_rectangle(
            Framebuffer, ClipRect, Texture, RenderCommand);
        return;
    case NAIVE_SWR_RENDER_TYPE_RGBA32_RECTANGLE:
        if (!naive_swr_is_valid_rgba32_texture(Texture))
        {
            return;
        }
        naive_swr_render_textured_rectangle(
            Framebuffer, ClipRect, Texture, RenderCommand);
        return;
    case NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE:
        naive_swr_render_general_triangle(
            Framebuffer,
            ClipRect,
            NULL,
            RenderCommand,
            NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE);
        return;
    case NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE:
        naive_swr_render_general_triangle(
            Framebuffer,
            ClipRect,
            Texture,
            RenderCommand,
            NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE);
        return;
    case NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE:
        naive_swr_render_general_triangle(
            Framebuffer,
            ClipRect,
            Texture,
            RenderCommand,
            NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE);
        return;
    case NAIVE_SWR_RENDER_TYPE_SOLID_OPAQUE_TRIANGLE:
        naive_swr_render_solid_opaque_triangle(
            Framebuffer, ClipRect, RenderCommand);
        return;
    case NAIVE_SWR_RENDER_TYPE_SOLID_TRANSLUCENT_TRIANGLE:
        naive_swr_render_solid_translucent_triangle(
            Framebuffer, ClipRect, RenderCommand);
        return;
    case NAIVE_SWR_RENDER_TYPE_SOLID_ALPHA_GRADIENT_TRIANGLE:
        naive_swr_render_solid_alpha_gradient_triangle(
            Framebuffer, ClipRect, RenderCommand);
        return;
    case NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE:
        naive_swr_render_alpha8_opaque_tint_triangle(
            Framebuffer, ClipRect, Texture, RenderCommand);
        return;
    case NAIVE_SWR_RENDER_TYPE_ALPHA8_TRANSLUCENT_TINT_TRIANGLE:
        naive_swr_render_alpha8_translucent_tint_triangle(
            Framebuffer, ClipRect, Texture, RenderCommand);
        return;
    default:
        return;
    }
}
