/*
 * PROJECT:    imgui-framebuffer
 * FILE:       naive_swr_imgui.cpp
 * PURPOSE:    Implementation for the Naive Software Renderer Dear ImGui Adapter
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "naive_swr_imgui.h"

#ifndef IMGUI_DISABLE

static inline NAIVE_SWR_COLOR naive_swr_imgui_color_from_imgui(
    ImU32 Value)
{
    NAIVE_SWR_COLOR Color;

    Color.Red = (uint8_t)((Value >> IM_COL32_R_SHIFT) & 0xFFu);
    Color.Green = (uint8_t)((Value >> IM_COL32_G_SHIFT) & 0xFFu);
    Color.Blue = (uint8_t)((Value >> IM_COL32_B_SHIFT) & 0xFFu);
    Color.Alpha = (uint8_t)((Value >> IM_COL32_A_SHIFT) & 0xFFu);

    return Color;
}

static inline bool naive_swr_imgui_is_white_uv(
    const ImVec2& Coordinate,
    const ImVec2& WhiteUv)
{
    return Coordinate.x == WhiteUv.x && Coordinate.y == WhiteUv.y;
}

uint32_t naive_swr_imgui_make_render_command(
    const ImDrawVert* VertexBuffer,
    const ImDrawIdx* IndexBuffer,
    uint32_t RemainingElementCount,
    PCNAIVE_SWR_TEXTURE Texture,
    PNAIVE_SWR_RENDER_COMMAND RenderCommand)
{
    IM_ASSERT(VertexBuffer != nullptr);
    IM_ASSERT(IndexBuffer != nullptr);
    IM_ASSERT(RemainingElementCount >= 3u);
    IM_ASSERT(Texture != nullptr);
    IM_ASSERT(RenderCommand != nullptr);

    const ImVec2 WhiteUv = ImGui::GetIO().Fonts->TexUvWhitePixel;

    /*
     * First try the canonical ImGui rectangle index pattern:
     *
     * A, B, C, A, C, D
     */
    if (RemainingElementCount >= 6u &&
        IndexBuffer[0] == IndexBuffer[3] &&
        IndexBuffer[2] == IndexBuffer[4])
    {
        const ImDrawVert& A = VertexBuffer[IndexBuffer[0]];
        const ImDrawVert& B = VertexBuffer[IndexBuffer[1]];
        const ImDrawVert& C = VertexBuffer[IndexBuffer[2]];
        const ImDrawVert& D = VertexBuffer[IndexBuffer[5]];

        const bool PositionIsRectangle =
            A.pos.y == B.pos.y &&
            B.pos.x == C.pos.x &&
            C.pos.y == D.pos.y &&
            D.pos.x == A.pos.x;

        const bool ColorIsUniform =
            A.col == B.col &&
            A.col == C.col &&
            A.col == D.col;

        const bool TextureIsRectangle =
            A.uv.y == B.uv.y &&
            B.uv.x == C.uv.x &&
            C.uv.y == D.uv.y &&
            D.uv.x == A.uv.x;

        const bool TextureIsWhite =
            naive_swr_imgui_is_white_uv(A.uv, WhiteUv) &&
            naive_swr_imgui_is_white_uv(B.uv, WhiteUv) &&
            naive_swr_imgui_is_white_uv(C.uv, WhiteUv) &&
            naive_swr_imgui_is_white_uv(D.uv, WhiteUv);

        if (PositionIsRectangle &&
            ColorIsUniform &&
            (TextureIsWhite || TextureIsRectangle))
        {
            RenderCommand->Command.Rectangle.Position.X = A.pos.x;
            RenderCommand->Command.Rectangle.Position.Y = A.pos.y;
            RenderCommand->Command.Rectangle.Size.Width = C.pos.x - A.pos.x;
            RenderCommand->Command.Rectangle.Size.Height = C.pos.y - A.pos.y;
            RenderCommand->Command.Rectangle.Color =
                naive_swr_imgui_color_from_imgui(A.col);

            if (RenderCommand->Command.Rectangle.Size.Width == 0.0f ||
                RenderCommand->Command.Rectangle.Size.Height == 0.0f ||
                RenderCommand->Command.Rectangle.Color.Alpha == 0)
            {
                RenderCommand->Type = NAIVE_SWR_RENDER_TYPE_SKIPPED;
                return 6;
            }

            if (TextureIsWhite)
            {
                RenderCommand->Type =
                    NAIVE_SWR_RENDER_TYPE_SOLID_RECTANGLE;
                return 6;
            }

            const float TextureWidth = (float)Texture->Width;
            const float TextureHeight = (float)Texture->Height;

            RenderCommand->Command.Rectangle.TexturePosition.X =
                A.uv.x * TextureWidth;
            RenderCommand->Command.Rectangle.TexturePosition.Y =
                A.uv.y * TextureHeight;
            RenderCommand->Command.Rectangle.TextureSize.Width =
                (C.uv.x - A.uv.x) * TextureWidth;
            RenderCommand->Command.Rectangle.TextureSize.Height =
                (C.uv.y - A.uv.y) * TextureHeight;

            RenderCommand->Type =
                Texture->Format == NAIVE_SWR_TEXTURE_FORMAT_ALPHA8
                ? NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE
                : NAIVE_SWR_RENDER_TYPE_RGBA32_RECTANGLE;

            return 6;
        }
    }

    /*
     * Rectangle recognition failed, so process the first triangle.
     */
    const ImDrawVert* Vertices[3] =
    {
        &VertexBuffer[IndexBuffer[0]],
        &VertexBuffer[IndexBuffer[1]],
        &VertexBuffer[IndexBuffer[2]]
    };

    for (uint32_t Index = 0; Index < 3u; ++Index)
    {
        RenderCommand->Command.Triangle.Positions[Index].X =
            Vertices[Index]->pos.x;
        RenderCommand->Command.Triangle.Positions[Index].Y =
            Vertices[Index]->pos.y;
        RenderCommand->Command.Triangle.Colors[Index] =
            naive_swr_imgui_color_from_imgui(Vertices[Index]->col);
    }

    const float Area =
        (Vertices[1]->pos.x - Vertices[0]->pos.x) *
        (Vertices[2]->pos.y - Vertices[0]->pos.y) -
        (Vertices[1]->pos.y - Vertices[0]->pos.y) *
        (Vertices[2]->pos.x - Vertices[0]->pos.x);

    const bool FullyTransparent =
        RenderCommand->Command.Triangle.Colors[0].Alpha == 0 &&
        RenderCommand->Command.Triangle.Colors[1].Alpha == 0 &&
        RenderCommand->Command.Triangle.Colors[2].Alpha == 0;

    if (Area == 0.0f || FullyTransparent)
    {
        RenderCommand->Type = NAIVE_SWR_RENDER_TYPE_SKIPPED;
        return 3;
    }

    const ImU32 ColorDifference =
        (Vertices[0]->col ^ Vertices[1]->col) |
        (Vertices[0]->col ^ Vertices[2]->col);

    const bool TextureIsWhite = naive_swr_imgui_is_white_uv(
        Vertices[0]->uv, WhiteUv) &&
        naive_swr_imgui_is_white_uv(Vertices[1]->uv, WhiteUv) &&
        naive_swr_imgui_is_white_uv(Vertices[2]->uv, WhiteUv);

    if (TextureIsWhite)
    {
        if (ColorDifference == 0)
        {
            RenderCommand->Type =
                RenderCommand->Command.Triangle.Colors[0].Alpha == 255
                ? NAIVE_SWR_RENDER_TYPE_SOLID_OPAQUE_TRIANGLE
                : NAIVE_SWR_RENDER_TYPE_SOLID_TRANSLUCENT_TRIANGLE;

            return 3;
        }

        const ImU32 AlphaMask = (ImU32)0xFFu << IM_COL32_A_SHIFT;

        if ((ColorDifference & ~AlphaMask) == 0)
        {
            RenderCommand->Type =
                NAIVE_SWR_RENDER_TYPE_SOLID_ALPHA_GRADIENT_TRIANGLE;

            return 3;
        }

        RenderCommand->Type = NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE;
        return 3;
    }

    for (uint32_t Index = 0; Index < 3u; ++Index)
    {
        RenderCommand->Command.Triangle.TextureCoordinates[Index].U =
            Vertices[Index]->uv.x;
        RenderCommand->Command.Triangle.TextureCoordinates[Index].V =
            Vertices[Index]->uv.y;
    }

    if (Texture->Format == NAIVE_SWR_TEXTURE_FORMAT_ALPHA8)
    {
        if (ColorDifference == 0)
        {
            RenderCommand->Type =
                RenderCommand->Command.Triangle.Colors[0].Alpha == 255
                ? NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE
                : NAIVE_SWR_RENDER_TYPE_ALPHA8_TRANSLUCENT_TINT_TRIANGLE;
        }
        else
        {
            RenderCommand->Type = NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE;
        }
    }
    else
    {
        RenderCommand->Type = NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE;
    }

    return 3;
}

void naive_swr_imgui_render_draw_command(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_TEXTURE Texture,
    const ImDrawList* DrawList,
    const ImDrawCmd* DrawCommand)
{
    IM_ASSERT(Framebuffer != nullptr);
    IM_ASSERT(ClipRect != nullptr);
    IM_ASSERT(Texture != nullptr);
    IM_ASSERT(DrawList != nullptr);
    IM_ASSERT(DrawCommand != nullptr);

    if (Framebuffer == nullptr ||
        ClipRect == nullptr ||
        Texture == nullptr ||
        DrawList == nullptr ||
        DrawCommand == nullptr)
    {
        return;
    }

    IM_ASSERT(DrawCommand->UserCallback == nullptr);

    if (DrawCommand->UserCallback != nullptr ||
        DrawCommand->ElemCount == 0)
    {
        return;
    }

    const uint32_t ElementCount = (uint32_t)DrawCommand->ElemCount;

    IM_ASSERT(ElementCount >= 3u && (ElementCount % 3u) == 0u);

    if (ElementCount < 3u || (ElementCount % 3u) != 0u)
    {
        return;
    }

    const ImDrawVert* VertexBuffer =
        DrawList->VtxBuffer.Data + DrawCommand->VtxOffset;
    const ImDrawIdx* IndexBuffer =
        DrawList->IdxBuffer.Data + DrawCommand->IdxOffset;

    uint32_t ElementOffset = 0;

    while (ElementOffset < ElementCount)
    {
        const uint32_t RemainingCount = ElementCount - ElementOffset;
        NAIVE_SWR_RENDER_COMMAND RenderCommand;

        const uint32_t ConsumedCount = naive_swr_imgui_make_render_command(
                VertexBuffer,
                IndexBuffer + ElementOffset,
                RemainingCount,
                Texture,
                &RenderCommand);

        IM_ASSERT(ConsumedCount == 3u || ConsumedCount == 6u);
        IM_ASSERT(ConsumedCount <= RemainingCount);

        naive_swr_render_command(
            Framebuffer,
            ClipRect,
            Texture,
            &RenderCommand);

        ElementOffset += ConsumedCount;
    }
}

#endif // !IMGUI_DISABLE
