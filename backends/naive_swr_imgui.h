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
 * @brief Creates software-renderer texture storage from Dear ImGui texture
 *        data.
 * @param TextureData A non-null pointer to the source texture data.
 * @param OwnedPixels A non-null pointer that receives newly allocated pixel
 *        storage. Release the storage with IM_FREE.
 * @param Texture A non-null pointer that receives a non-owning texture view
 *        over the allocated storage.
 * @return true on success. On failure, the output parameters are unchanged.
 */
bool naive_swr_imgui_create_texture(
    ImTextureData* TextureData,
    uint8_t** OwnedPixels,
    PNAIVE_SWR_TEXTURE Texture);

/**
 * @brief Renders one non-callback Dear ImGui draw command.
 * @param Framebuffer A non-null pointer to the destination framebuffer.
 * @param Texture A non-null pointer to the resolved software texture.
 * @param DrawList A non-null pointer to the containing draw list.
 * @param DrawCommand A non-null pointer to the draw command. User callbacks
 *        must be handled by the caller.
 * @note Vertex positions and clipping coordinates are interpreted directly
 *       as framebuffer pixel coordinates. Texture sampling is nearest-neighbor.
 *       ImFontAtlasFlags_NoBakedLines can improve anti-aliased line quality
 *       at a performance cost.
 */
void naive_swr_imgui_render_draw_command(
    PCNAIVE_SWR_FRAMEBUFFER Framebuffer,
    PCNAIVE_SWR_TEXTURE Texture,
    const ImDrawList* DrawList,
    const ImDrawCmd* DrawCommand);

/**
 * @brief Ensures that a framebuffer allocation can hold the requested
 *        number of pixels.
 * @param FramebufferPixels A non-null pointer to storage allocated with
 *        IM_ALLOC.
 * @param FramebufferCapacity A non-null pointer to the current pixel
 *        capacity.
 * @param RequiredPixelCount The required number of pixels.
 * @return true if sufficient capacity is available. On failure, the
 *         existing allocation and capacity are unchanged.
 */
bool naive_swr_imgui_ensure_framebuffer_capacity(
    uint32_t** FramebufferPixels,
    size_t* FramebufferCapacity,
    size_t RequiredPixelCount);

#endif // !IMGUI_DISABLE

#endif // !NAIVE_SWR_IMGUI
