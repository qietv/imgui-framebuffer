/*
 * PROJECT:    imgui-framebuffer
 * FILE:       naive_swr_imgui.h
 * PURPOSE:    Definition for the Naive Software Renderer Dear ImGui Adapter
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NAIVE_SWR_IMGUI
#define NAIVE_SWR_IMGUI

#include "naive_swr.h"
#include "imgui.h"

#ifndef IMGUI_DISABLE

/**
 * @brief Creates a software-renderer command from the next indexed Dear
 *        ImGui primitive.
 * @param VertexBuffer A non-null pointer to the vertex buffer after applying
 *        the draw-command vertex offset.
 * @param IndexBuffer A non-null pointer to the first index to consume.
 * @param RemainingElementCount Number of available indices. Must be at
 *        least three.
 * @param Texture A non-null pointer to the resolved software texture.
 * @param RenderCommand A non-null pointer that receives the command.
 * @return The number of consumed indices: three for a triangle or six for
 *         a recognized rectangle.
 */
uint32_t naive_swr_imgui_make_render_command(
    const ImDrawVert* VertexBuffer,
    const ImDrawIdx* IndexBuffer,
    uint32_t RemainingElementCount,
    PCNAIVE_SWR_TEXTURE Texture,
    PNAIVE_SWR_RENDER_COMMAND RenderCommand);

/**
 * @brief Renders one non-callback Dear ImGui draw command.
 * @param Framebuffer A non-null pointer to the destination framebuffer.
 * @param ClipRect A non-null pointer to the resolved clipping rectangle.
 * @param Texture A non-null pointer to the resolved software texture.
 * @param DrawList A non-null pointer to the containing draw list.
 * @param DrawCommand A non-null pointer to the draw command. User callbacks
 *        must be handled by the caller.
 */
void naive_swr_imgui_render_draw_command(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_CLIP_RECT ClipRect,
    PCNAIVE_SWR_TEXTURE Texture,
    const ImDrawList* DrawList,
    const ImDrawCmd* DrawCommand);

#endif // !IMGUI_DISABLE

#endif // !NAIVE_SWR_IMGUI
