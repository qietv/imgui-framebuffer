// dear imgui: Renderer Backend for Windows GDI
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [x] Renderer: Basic Implementation

// The aim of imgui_impl_gdi.h/.cpp is to be usable in your engine without any modification.
// IF YOU FEEL YOU NEED TO MAKE ANY CHANGE TO THIS CODE, please share them and your feedback at https://github.com/ocornut/imgui/

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2026-05-20: Initial version.

#include "imgui.h"

#ifndef IMGUI_DISABLE

#include "imgui_impl_gdi.h"
#include "naive_swr_imgui.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <math.h>

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"         // warning: use of old-style cast                            // yes, they are more terse.
#endif

struct ImGui_ImplGDI_Data
{
    uint32_t* FramebufferPixels;
    size_t FramebufferCapacity;

    ImGui_ImplGDI_Data()
    {
        memset(this, 0, sizeof(*this));
    }
};

static ImGui_ImplGDI_Data* ImGui_ImplGDI_GetBackendData()
{
    return ImGui::GetCurrentContext()
        ? (ImGui_ImplGDI_Data*)ImGui::GetIO().BackendRendererUserData
        : nullptr;
}

static bool ImGui_ImplGDI_EnsureFramebufferCapacity(
    ImGui_ImplGDI_Data* backend_data,
    size_t required_pixel_count)
{
    if (backend_data->FramebufferCapacity >= required_pixel_count)
        return true;

    uint32_t* new_pixels = (uint32_t*)IM_ALLOC(
        required_pixel_count * sizeof(uint32_t));

    if (!new_pixels)
        return false;

    if (backend_data->FramebufferPixels)
        IM_FREE(backend_data->FramebufferPixels);

    backend_data->FramebufferPixels = new_pixels;
    backend_data->FramebufferCapacity = required_pixel_count;

    return true;
}

// Texture data
struct ImGui_ImplGDI_Texture
{
    uint8_t* OwnedPixels;
    NAIVE_SWR_TEXTURE View;

    ImGui_ImplGDI_Texture()
    {
        memset((void*)this, 0, sizeof(*this));
    }
};

static bool ImGui_ImplGDI_RGBA32TextureCanUseAlpha8(
    const uint8_t* pixels,
    size_t pixel_count)
{
    for (size_t index = 0; index < pixel_count; ++index)
    {
        const uint8_t* texel =
            pixels + index * 4;

        /*
         * RGB is irrelevant for a fully transparent texel under
         * nearest-neighbor straight-alpha rendering.
         */
        if (texel[3] != 0 &&
            (texel[0] != 255 ||
                texel[1] != 255 ||
                texel[2] != 255))
        {
            return false;
        }
    }

    return true;
}

static bool ImGui_ImplGDI_UploadTexturePixels(
    ImGui_ImplGDI_Texture* texture,
    ImTextureData* tex)
{
    IM_ASSERT(texture != nullptr);
    IM_ASSERT(tex != nullptr);

    if (tex->Width <= 0 || tex->Height <= 0)
        return false;

    if (tex->Format != ImTextureFormat_Alpha8 &&
        tex->Format != ImTextureFormat_RGBA32)
    {
        IM_ASSERT(false && "Unsupported texture format!");
        return false;
    }

    const uint8_t* source_pixels =
        reinterpret_cast<const uint8_t*>(
            tex->GetPixels());

    if (!source_pixels)
        return false;

    const size_t pixel_count =
        (size_t)tex->Width * (size_t)tex->Height;

    ImTextureFormat internal_format =
        tex->Format;

    /*
     * RGBA32 textures whose RGB channels are entirely white are
     * equivalent to Alpha8 masks under the renderer's modulation
     * rules:
     *
     * round(255 * vertex_color / 255) == vertex_color
     */
    if (tex->Format == ImTextureFormat_RGBA32 &&
        ImGui_ImplGDI_RGBA32TextureCanUseAlpha8(
            source_pixels,
            pixel_count))
    {
        internal_format =
            ImTextureFormat_Alpha8;
    }

    const size_t destination_byte_count =
        internal_format == ImTextureFormat_Alpha8
        ? pixel_count
        : pixel_count * 4;

    uint8_t* new_pixels =
        (uint8_t*)IM_ALLOC(destination_byte_count);

    if (!new_pixels)
        return false;

    if (internal_format == ImTextureFormat_Alpha8)
    {
        if (tex->Format == ImTextureFormat_Alpha8)
        {
            memcpy(
                new_pixels,
                source_pixels,
                pixel_count);
        }
        else
        {
            /*
             * Compress white-RGB RGBA32 into tightly packed Alpha8.
             */
            for (size_t index = 0;
                index < pixel_count;
                ++index)
            {
                new_pixels[index] =
                    source_pixels[index * 4 + 3];
            }
        }
    }
    else
    {
        memcpy(
            new_pixels,
            source_pixels,
            pixel_count * 4);
    }

    /*
     * Allocate and populate the new buffer before releasing the old
     * one, so a failed update leaves the previous texture intact.
     */
    if (texture->OwnedPixels)
        IM_FREE(texture->OwnedPixels);

    texture->OwnedPixels = new_pixels;
    texture->View.Pixels = new_pixels;
    texture->View.Width = tex->Width;
    texture->View.Height = tex->Height;
    texture->View.ByteStride = internal_format == ImTextureFormat_Alpha8
        ? (size_t)tex->Width
        : (size_t)tex->Width * 4;
    texture->View.Format = internal_format == ImTextureFormat_Alpha8
        ? NAIVE_SWR_TEXTURE_FORMAT_ALPHA8
        : NAIVE_SWR_TEXTURE_FORMAT_RGBA32;

    return true;
}

static void ImGui_ImplGDI_UpdateTexture(
    ImTextureData* tex)
{
    if (tex->Status == ImTextureStatus_WantCreate)
    {
        IM_ASSERT(
            tex->TexID == ImTextureID_Invalid &&
            tex->BackendUserData == nullptr);

        ImGui_ImplGDI_Texture* texture =
            IM_NEW(ImGui_ImplGDI_Texture)();

        if (!texture)
        {
            IM_ASSERT(
                false &&
                "Failed to allocate texture object!");
            return;
        }

        if (!ImGui_ImplGDI_UploadTexturePixels(
            texture,
            tex))
        {
            IM_DELETE(texture);

            IM_ASSERT(
                false &&
                "Failed to upload texture pixels!");

            return;
        }

        tex->SetTexID((ImTextureID)texture);
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (
        tex->Status == ImTextureStatus_WantUpdates)
    {
        ImGui_ImplGDI_Texture* texture =
            (ImGui_ImplGDI_Texture*)tex->GetTexID();

        IM_ASSERT(
            texture != nullptr &&
            "Trying to update an invalid texture!");

        if (!texture)
            return;

        if (!ImGui_ImplGDI_UploadTexturePixels(
            texture,
            tex))
        {
            IM_ASSERT(
                false &&
                "Failed to update texture pixels!");

            return;
        }

        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (
        tex->Status == ImTextureStatus_WantDestroy)
    {
        ImGui_ImplGDI_Texture* texture =
            (ImGui_ImplGDI_Texture*)tex->GetTexID();

        IM_ASSERT(
            texture != nullptr &&
            "Trying to destroy an invalid texture!");

        if (!texture)
            return;

        if (texture->OwnedPixels)
            IM_FREE(texture->OwnedPixels);

        IM_DELETE(texture);

        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

IMGUI_IMPL_API bool ImGui_ImplGDI_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();

    IM_ASSERT(
        io.BackendRendererUserData == nullptr &&
        "Already initialized a renderer backend!");

    ImGui_ImplGDI_Data* backend_data = IM_NEW(ImGui_ImplGDI_Data)();

    if (!backend_data)
        return false;

    io.BackendRendererUserData = backend_data;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    return true;
}

IMGUI_IMPL_API void ImGui_ImplGDI_Shutdown()
{
    ImGui_ImplGDI_Data* backend_data =
        ImGui_ImplGDI_GetBackendData();

    IM_ASSERT(
        backend_data != nullptr &&
        "No renderer backend to shutdown!");

    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
    {
        if (tex->RefCount == 1)
        {
            tex->SetStatus(ImTextureStatus_WantDestroy);
            ImGui_ImplGDI_UpdateTexture(tex);
        }
    }

    if (backend_data->FramebufferPixels)
        IM_FREE(backend_data->FramebufferPixels);

    IM_DELETE(backend_data);

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(
        ImGuiBackendFlags_RendererHasVtxOffset |
        ImGuiBackendFlags_RendererHasTextures);
}

IMGUI_IMPL_API void ImGui_ImplGDI_NewFrame()
{

}

IMGUI_IMPL_API void ImGui_ImplGDI_RenderDrawData(
    ImDrawData* draw_data,
    void* output_dev_ctx_handle,
    ImVec4* clear_color)
{
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    if (fb_width == 0 || fb_height == 0)
        return;

    HDC output_hdc = (HDC)output_dev_ctx_handle;
    IM_ASSERT(output_hdc != nullptr && "Invalid output device context!");

    ImGui_ImplGDI_Data* backend_data = ImGui_ImplGDI_GetBackendData();

    IM_ASSERT(backend_data != nullptr);

    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplGDI_UpdateTexture(tex);

    // Setup desired GDI state

    const size_t required_pixel_count =
        (size_t)fb_width * (size_t)fb_height;

    if (!ImGui_ImplGDI_EnsureFramebufferCapacity(
        backend_data,
        required_pixel_count))
    {
        IM_ASSERT(false && "Failed to allocate framebuffer!");
        return;
    }

    uint32_t* pixel_buffer = backend_data->FramebufferPixels;

    NAIVE_SWR_FRAMEBUFFER framebuffer;
    framebuffer.Pixels = pixel_buffer;
    framebuffer.Width = fb_width;
    framebuffer.Height = fb_height;
    framebuffer.PixelStride = (size_t)fb_width;

    // Clear the framebuffer with the background color (if any)
    if (clear_color)
    {
        NAIVE_SWR_COLOR swr_clear_color;
        swr_clear_color.Red = (uint8_t)(BYTE)(clear_color->x * 255.0f);
        swr_clear_color.Green = (uint8_t)(BYTE)(clear_color->y * 255.0f);
        swr_clear_color.Blue = (uint8_t)(BYTE)(clear_color->z * 255.0f);
        swr_clear_color.Alpha = 255;
        naive_swr_clear_framebuffer(&framebuffer, swr_clear_color);
    }

    // Will project scissor/clipping rectangles into framebuffer space

    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    for (const ImDrawList* draw_list : draw_data->CmdLists)
    {
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                // User callback, registered via ImDrawList::AddCallback()
                pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space

                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // - Apply scissor/clipping rectangle
                // - Bind texture, Draw

                ImGui_ImplGDI_Texture* texture =
                    (ImGui_ImplGDI_Texture*)pcmd->GetTexID();

                NAIVE_SWR_CLIP_RECT swr_clip_rect;
                swr_clip_rect.Left = (int32_t)ceilf(pcmd->ClipRect.x - 0.5f);
                swr_clip_rect.Top = (int32_t)ceilf(pcmd->ClipRect.y - 0.5f);
                swr_clip_rect.Right = (int32_t)ceilf(pcmd->ClipRect.z - 0.5f);
                swr_clip_rect.Bottom = (int32_t)ceilf(pcmd->ClipRect.w - 0.5f);

                if (pcmd->ElemCount == 0)
                    continue;

                IM_ASSERT(
                    texture != nullptr &&
                    "Invalid GDI texture!");

                if (!texture)
                    continue;

                naive_swr_imgui_render_draw_command(
                    &framebuffer,
                    &swr_clip_rect,
                    &texture->View,
                    draw_list,
                    pcmd);
            }
        }
    }

    BITMAPINFO bitmap_info = {};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = fb_width;
    bitmap_info.bmiHeader.biHeight = -fb_height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    ::SetDIBitsToDevice(
        output_hdc,
        0,
        0,
        (DWORD)fb_width,
        (DWORD)fb_height,
        0,
        0,
        0,
        (UINT)fb_height,
        pixel_buffer,
        &bitmap_info,
        DIB_RGB_COLORS);
}

#endif // #ifndef IMGUI_DISABLE
