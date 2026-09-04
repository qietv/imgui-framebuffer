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

#include "naive_swr.h"

static inline NAIVE_SWR_COLOR naive_swr_color_from_imgui(ImU32 value)
{
    NAIVE_SWR_COLOR color;

    color.Red = (uint8_t)((value >> IM_COL32_R_SHIFT) & 0xFFu);
    color.Green = (uint8_t)((value >> IM_COL32_G_SHIFT) & 0xFFu);
    color.Blue = (uint8_t)((value >> IM_COL32_B_SHIFT) & 0xFFu);
    color.Alpha = (uint8_t)((value >> IM_COL32_A_SHIFT) & 0xFFu);

    return color;
}

static inline bool naive_swr_is_white_texture_coordinate(
    const ImVec2& coordinate,
    const ImVec2& white_texture_coordinate)
{
    return coordinate.x == white_texture_coordinate.x &&
        coordinate.y == white_texture_coordinate.y;
}

uint32_t naive_swr_make_render_command(
    const ImDrawVert* vertex_buffer,
    const ImDrawIdx* index_buffer,
    uint32_t remaining_element_count,
    ImTextureFormat texture_format,
    uint32_t texture_width,
    uint32_t texture_height,
    PNAIVE_SWR_RENDER_COMMAND render_command)
{
    IM_ASSERT(remaining_element_count >= 3);

    const ImVec2 white_texture_coordinate =
        ImGui::GetIO().Fonts->TexUvWhitePixel;

    /*
     * First try the canonical ImGui rectangle index pattern:
     *
     * A, B, C, A, C, D
     */
    if (remaining_element_count >= 6 &&
        index_buffer[0] == index_buffer[3] &&
        index_buffer[2] == index_buffer[4])
    {
        const ImDrawVert& a = vertex_buffer[index_buffer[0]];
        const ImDrawVert& b = vertex_buffer[index_buffer[1]];
        const ImDrawVert& c = vertex_buffer[index_buffer[2]];
        const ImDrawVert& d = vertex_buffer[index_buffer[5]];

        const bool position_is_rectangle =
            a.pos.y == b.pos.y &&
            b.pos.x == c.pos.x &&
            c.pos.y == d.pos.y &&
            d.pos.x == a.pos.x;

        const bool color_is_uniform =
            a.col == b.col &&
            a.col == c.col &&
            a.col == d.col;

        const bool texture_is_rectangle =
            a.uv.y == b.uv.y &&
            b.uv.x == c.uv.x &&
            c.uv.y == d.uv.y &&
            d.uv.x == a.uv.x;

        const bool texture_is_white =
            naive_swr_is_white_texture_coordinate(
                a.uv, white_texture_coordinate) &&
            naive_swr_is_white_texture_coordinate(
                b.uv, white_texture_coordinate) &&
            naive_swr_is_white_texture_coordinate(
                c.uv, white_texture_coordinate) &&
            naive_swr_is_white_texture_coordinate(
                d.uv, white_texture_coordinate);

        if (position_is_rectangle &&
            color_is_uniform &&
            (texture_is_white || texture_is_rectangle))
        {
            render_command->Command.Rectangle.Position.X = a.pos.x;
            render_command->Command.Rectangle.Position.Y = a.pos.y;

            render_command->Command.Rectangle.Size.Width =
                c.pos.x - a.pos.x;

            render_command->Command.Rectangle.Size.Height =
                c.pos.y - a.pos.y;

            render_command->Command.Rectangle.Color =
                naive_swr_color_from_imgui(a.col);

            if (render_command->Command.Rectangle.Size.Width == 0.0f ||
                render_command->Command.Rectangle.Size.Height == 0.0f ||
                render_command->Command.Rectangle.Color.Alpha == 0)
            {
                render_command->Type = NAIVE_SWR_RENDER_TYPE_SKIPPED;

                return 6;
            }

            if (texture_is_white)
            {
                render_command->Type = NAIVE_SWR_RENDER_TYPE_SOLID_RECTANGLE;

                return 6;
            }

            const float width = (float)texture_width;
            const float height = (float)texture_height;

            render_command->Command.Rectangle.TexturePosition.X =
                a.uv.x * width;

            render_command->Command.Rectangle.TexturePosition.Y =
                a.uv.y * height;

            render_command->Command.Rectangle.TextureSize.Width =
                (c.uv.x - a.uv.x) * width;

            render_command->Command.Rectangle.TextureSize.Height =
                (c.uv.y - a.uv.y) * height;

            render_command->Type = texture_format == ImTextureFormat_Alpha8
                ? NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE
                : NAIVE_SWR_RENDER_TYPE_RGBA32_RECTANGLE;

            return 6;
        }
    }

    /*
     * Rectangle recognition failed, so process the first triangle.
     */
    const ImDrawVert* vertices[3] =
    {
        &vertex_buffer[index_buffer[0]],
        &vertex_buffer[index_buffer[1]],
        &vertex_buffer[index_buffer[2]]
    };

    for (uint32_t i = 0; i < 3; ++i)
    {
        render_command->Command.Triangle.Positions[i].X =
            vertices[i]->pos.x;

        render_command->Command.Triangle.Positions[i].Y =
            vertices[i]->pos.y;

        render_command->Command.Triangle.Colors[i] =
            naive_swr_color_from_imgui(vertices[i]->col);
    }

    const float area =
        (vertices[1]->pos.x - vertices[0]->pos.x) *
        (vertices[2]->pos.y - vertices[0]->pos.y) -
        (vertices[1]->pos.y - vertices[0]->pos.y) *
        (vertices[2]->pos.x - vertices[0]->pos.x);

    const bool fully_transparent =
        render_command->Command.Triangle.Colors[0].Alpha == 0 &&
        render_command->Command.Triangle.Colors[1].Alpha == 0 &&
        render_command->Command.Triangle.Colors[2].Alpha == 0;

    if (area == 0.0f || fully_transparent)
    {
        render_command->Type = NAIVE_SWR_RENDER_TYPE_SKIPPED;
        return 3;
    }

    const ImU32 color_difference =
        (vertices[0]->col ^ vertices[1]->col) |
        (vertices[0]->col ^ vertices[2]->col);

    const bool texture_is_white =
        naive_swr_is_white_texture_coordinate(
            vertices[0]->uv, white_texture_coordinate) &&
        naive_swr_is_white_texture_coordinate(
            vertices[1]->uv, white_texture_coordinate) &&
        naive_swr_is_white_texture_coordinate(
            vertices[2]->uv, white_texture_coordinate);

    if (texture_is_white)
    {
        if (color_difference == 0)
        {
            render_command->Type =
                render_command->Command.Triangle.Colors[0].Alpha == 255
                ? NAIVE_SWR_RENDER_TYPE_SOLID_OPAQUE_TRIANGLE
                : NAIVE_SWR_RENDER_TYPE_SOLID_TRANSLUCENT_TRIANGLE;

            return 3;
        }

        const ImU32 alpha_mask =
            (ImU32)0xFFu << IM_COL32_A_SHIFT;

        if ((color_difference & ~alpha_mask) == 0)
        {
            render_command->Type =
                NAIVE_SWR_RENDER_TYPE_SOLID_ALPHA_GRADIENT_TRIANGLE;

            return 3;
        }

        render_command->Type = NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE;

        return 3;
    }

    for (uint32_t i = 0; i < 3; ++i)
    {
        render_command->Command.Triangle.TextureCoordinates[i].U =
            vertices[i]->uv.x;

        render_command->Command.Triangle.TextureCoordinates[i].V =
            vertices[i]->uv.y;
    }

    if (texture_format == ImTextureFormat_Alpha8)
    {
        if (color_difference == 0)
        {
            render_command->Type =
                render_command->Command.Triangle.Colors[0].Alpha == 255
                ? NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE
                : NAIVE_SWR_RENDER_TYPE_ALPHA8_TRANSLUCENT_TINT_TRIANGLE;
        }
        else
        {
            render_command->Type = NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE;
        }
    }
    else
    {
        render_command->Type = NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE;
    }

    return 3;
}

#endif // !IMGUI_DISABLE

#ifndef IMGUI_DISABLE
#include "imgui_impl_gdi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <math.h>

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"         // warning: use of old-style cast                            // yes, they are more terse.
#endif

#define IMGUI_IMPL_GDI_ENABLE_SSE2_CONSTANT_BLEND

#if !defined(_DEBUG)
#define IMGUI_IMPL_GDI_ENABLE_SSE2_A8_OPAQUE_TINT_BLEND
#endif

#if (defined(IMGUI_IMPL_GDI_ENABLE_SSE2_CONSTANT_BLEND) || \
    defined(IMGUI_IMPL_GDI_ENABLE_SSE2_A8_OPAQUE_TINT_BLEND)) && \
    (defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || \
    defined(__SSE2__))
#include <emmintrin.h>
#define IMGUI_IMPL_GDI_HAS_SSE2
#endif

// Temporary diagnostic switch. Must be off for formal performance testing.
//#define IMGUI_IMPL_GDI_ENABLE_STATS

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
#include <stdarg.h>
#include <stdio.h>

enum ImGui_ImplGDI_TriangleVertexColorClass
{
    ImGui_ImplGDI_TriangleVertexColor_UniformOpaque,
    ImGui_ImplGDI_TriangleVertexColor_UniformTranslucent,
    ImGui_ImplGDI_TriangleVertexColor_ConstantRGBVaryingAlpha,
    ImGui_ImplGDI_TriangleVertexColor_General,
    ImGui_ImplGDI_TriangleVertexColor_Count
};

enum ImGui_ImplGDI_TriangleStatsClass
{
    ImGui_ImplGDI_TriangleStats_SolidUniformOpaque,
    ImGui_ImplGDI_TriangleStats_SolidUniformTranslucent,
    ImGui_ImplGDI_TriangleStats_SolidConstantRGBVaryingAlpha,
    ImGui_ImplGDI_TriangleStats_SolidGeneralVertexColor,

    ImGui_ImplGDI_TriangleStats_Alpha8UniformOpaque,
    ImGui_ImplGDI_TriangleStats_Alpha8UniformTranslucent,
    ImGui_ImplGDI_TriangleStats_Alpha8ConstantRGBVaryingAlpha,
    ImGui_ImplGDI_TriangleStats_Alpha8GeneralVertexColor,

    ImGui_ImplGDI_TriangleStats_RGBA32Textured,
    ImGui_ImplGDI_TriangleStats_Count
};

struct ImGui_ImplGDI_TrianglePathStats
{
    uint64_t CommandCount;
    uint64_t CandidatePixelCount;
    uint64_t CoveredPixelCount;
};

struct ImGui_ImplGDI_RectanglePathStats
{
    uint64_t CommandCount;
    uint64_t PixelCount;
};

struct ImGui_ImplGDI_Stats
{
    uint64_t FrameCount;

    ImGui_ImplGDI_TrianglePathStats
        Triangles[ImGui_ImplGDI_TriangleStats_Count];

    ImGui_ImplGDI_RectanglePathStats OpaqueRectangles;
    ImGui_ImplGDI_RectanglePathStats TranslucentRectangles;

    ImGui_ImplGDI_RectanglePathStats A8Rectangles;
    ImGui_ImplGDI_RectanglePathStats A8RectanglesExactUnitX;
    ImGui_ImplGDI_RectanglePathStats A8RectanglesContiguousX;
    ImGui_ImplGDI_RectanglePathStats A8RectanglesContiguousXY;
    ImGui_ImplGDI_RectanglePathStats A8RectanglesContiguousXOpaqueTint;
};

static ImGui_ImplGDI_Stats g_ImGui_ImplGDI_Stats = {};

static void ImGui_ImplGDI_StatsDebugPrint(
    const char* format,
    ...)
{
    char buffer[1024];

    va_list arguments;
    va_start(arguments, format);

#if defined(_MSC_VER)
    _vsnprintf_s(
        buffer,
        sizeof(buffer),
        _TRUNCATE,
        format,
        arguments);
#else
    vsnprintf(
        buffer,
        sizeof(buffer),
        format,
        arguments);

    buffer[sizeof(buffer) - 1] = '\0';
#endif

    va_end(arguments);

    ::OutputDebugStringA(buffer);
}

static void ImGui_ImplGDI_PrintRectanglePathStats(
    const char* name,
    const ImGui_ImplGDI_RectanglePathStats& statistics,
    double frame_count)
{
    ImGui_ImplGDI_StatsDebugPrint(
        "%-44s cmd/frame=%8.2f  pixels/frame=%10.0f\n",
        name,
        (double)statistics.CommandCount / frame_count,
        (double)statistics.PixelCount / frame_count);
}

static ImGui_ImplGDI_TriangleVertexColorClass
ImGui_ImplGDI_ClassifyTriangleVertexColor(
    const NAIVE_SWR_COLOR& color_1,
    const NAIVE_SWR_COLOR& color_2,
    const NAIVE_SWR_COLOR& color_3)
{
    const bool rgb_is_uniform =
        color_1.Red == color_2.Red &&
        color_1.Red == color_3.Red &&
        color_1.Green == color_2.Green &&
        color_1.Green == color_3.Green &&
        color_1.Blue == color_2.Blue &&
        color_1.Blue == color_3.Blue;

    const bool alpha_is_uniform =
        color_1.Alpha == color_2.Alpha &&
        color_1.Alpha == color_3.Alpha;

    if (rgb_is_uniform && alpha_is_uniform)
    {
        return color_1.Alpha == 255
            ? ImGui_ImplGDI_TriangleVertexColor_UniformOpaque
            : ImGui_ImplGDI_TriangleVertexColor_UniformTranslucent;
    }

    if (rgb_is_uniform)
    {
        return
            ImGui_ImplGDI_TriangleVertexColor_ConstantRGBVaryingAlpha;
    }

    return ImGui_ImplGDI_TriangleVertexColor_General;
}

static ImGui_ImplGDI_TriangleStatsClass ImGui_ImplGDI_GetTriangleStatsClass(
    bool is_alpha8_textured,
    ImGui_ImplGDI_TriangleVertexColorClass vertex_color_class)
{
    const int first_class = is_alpha8_textured
        ? (int)ImGui_ImplGDI_TriangleStats_Alpha8UniformOpaque
        : (int)ImGui_ImplGDI_TriangleStats_SolidUniformOpaque;

    return (ImGui_ImplGDI_TriangleStatsClass)(
        first_class + (int)vertex_color_class);
}

static void ImGui_ImplGDI_EndStatisticsFrame()
{
    const uint64_t report_frame_count = 120;

    ++g_ImGui_ImplGDI_Stats.FrameCount;

    if (g_ImGui_ImplGDI_Stats.FrameCount < report_frame_count)
        return;

    static const char* triangle_names[] =
    {
        "Solid uniform opaque",
        "Solid uniform translucent",
        "Solid constant RGB / varying alpha",
        "Solid general vertex color",

        "Alpha8 uniform color / alpha 255",
        "Alpha8 uniform color / alpha 1-254",
        "Alpha8 constant RGB / varying alpha",
        "Alpha8 general vertex RGBA",

        "RGBA32 textured"
    };

    const double frame_count =
        (double)g_ImGui_ImplGDI_Stats.FrameCount;

    ImGui_ImplGDI_StatsDebugPrint(
        "\n=== ImGui GDI SWR statistics: %.0f frames ===\n",
        frame_count);

    for (int i = 0;
        i < ImGui_ImplGDI_TriangleStats_Count;
        ++i)
    {
        const ImGui_ImplGDI_TrianglePathStats& statistics =
            g_ImGui_ImplGDI_Stats.Triangles[i];

        const double coverage =
            statistics.CandidatePixelCount != 0
            ? (double)statistics.CoveredPixelCount * 100.0 /
            (double)statistics.CandidatePixelCount
            : 0.0;

        ImGui_ImplGDI_StatsDebugPrint(
            "%-38s cmd/frame=%8.2f"
            "  candidate/frame=%10.0f"
            "  covered/frame=%10.0f"
            "  coverage=%6.2f%%\n",
            triangle_names[i],
            (double)statistics.CommandCount / frame_count,
            (double)statistics.CandidatePixelCount / frame_count,
            (double)statistics.CoveredPixelCount / frame_count,
            coverage);
    }

    ImGui_ImplGDI_StatsDebugPrint(
        "%-38s cmd/frame=%8.2f  pixels/frame=%10.0f\n",
        "Opaque solid rectangles",
        (double)g_ImGui_ImplGDI_Stats
        .OpaqueRectangles.CommandCount / frame_count,
        (double)g_ImGui_ImplGDI_Stats
        .OpaqueRectangles.PixelCount / frame_count);

    ImGui_ImplGDI_StatsDebugPrint(
        "%-38s cmd/frame=%8.2f  pixels/frame=%10.0f\n",
        "Translucent solid rectangles",
        (double)g_ImGui_ImplGDI_Stats
        .TranslucentRectangles.CommandCount / frame_count,
        (double)g_ImGui_ImplGDI_Stats
        .TranslucentRectangles.PixelCount / frame_count);

    ImGui_ImplGDI_PrintRectanglePathStats(
        "A8 rectangles total",
        g_ImGui_ImplGDI_Stats.A8Rectangles,
        frame_count);

    ImGui_ImplGDI_PrintRectanglePathStats(
        "A8 rectangles exact step X = +1",
        g_ImGui_ImplGDI_Stats.A8RectanglesExactUnitX,
        frame_count);

    ImGui_ImplGDI_PrintRectanglePathStats(
        "A8 rectangles sampled contiguous X",
        g_ImGui_ImplGDI_Stats.A8RectanglesContiguousX,
        frame_count);

    ImGui_ImplGDI_PrintRectanglePathStats(
        "A8 rectangles sampled contiguous X/Y",
        g_ImGui_ImplGDI_Stats.A8RectanglesContiguousXY,
        frame_count);

    ImGui_ImplGDI_PrintRectanglePathStats(
        "A8 contiguous X with tint alpha 255",
        g_ImGui_ImplGDI_Stats
        .A8RectanglesContiguousXOpaqueTint,
        frame_count);

    memset(
        &g_ImGui_ImplGDI_Stats,
        0,
        sizeof(g_ImGui_ImplGDI_Stats));
}
#endif

// All current arguments are side-effect-free scalar values.
#define IMGUI_IMPL_GDI_CLAMP(value, minimum, maximum) (((value) < (minimum)) \
    ? (minimum) \
    : (((value) > (maximum)) ? (maximum) : (value)))

struct ImGui_ImplGDI_Data
{
    ImU32* FramebufferPixels;
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

    ImU32* new_pixels = (ImU32*)IM_ALLOC(
        required_pixel_count * sizeof(ImU32));

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
    int Width;
    int Height;
    ImTextureFormat Format;
    uint8_t* Pixels;

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
    if (texture->Pixels)
        IM_FREE(texture->Pixels);

    texture->Width = tex->Width;
    texture->Height = tex->Height;
    texture->Format = internal_format;
    texture->Pixels = new_pixels;

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

        if (texture->Pixels)
            IM_FREE(texture->Pixels);

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

// Equivalent to (value + 127) / 255 for value in [0, 255 * 255],
// but expressed using operations that are easier to vectorize.
#define ImGui_ImplGDI_Div255Rounded(value) \
    ((ImU32)((((ImU32)(value) + 128u) * 257u) >> 16))

#define ImGui_ImplGDI_Mul255(a, b) \
    ((int)ImGui_ImplGDI_Div255Rounded((ImU32)(a) * (ImU32)(b)))

static inline void ImGui_ImplGDI_FillSpan(
    ImU32* destination,
    size_t pixel_count,
    ImU32 color)
{
    while (pixel_count >= 8)
    {
        destination[0] = color;
        destination[1] = color;
        destination[2] = color;
        destination[3] = color;
        destination[4] = color;
        destination[5] = color;
        destination[6] = color;
        destination[7] = color;

        destination += 8;
        pixel_count -= 8;
    }

    while (pixel_count != 0)
    {
        *destination++ = color;
        --pixel_count;
    }
}

static inline void ImGui_ImplGDI_BlendOver(
    ImU32* destination,
    int source_red,
    int source_green,
    int source_blue,
    int source_alpha)
{
    const ImU32 alpha = (ImU32)source_alpha;

    if (alpha == 0u)
        return;

    if (alpha == 255u)
    {
        *destination =
            0xFF000000u |
            ((ImU32)source_red << 16) |
            ((ImU32)source_green << 8) |
            ((ImU32)source_blue << 0);

        return;
    }

    const ImU32 destination_color = *destination;
    const ImU32 inverse_alpha = 255u - alpha;

    const ImU32 destination_blue =
        (destination_color >> 0) & 0xFFu;

    const ImU32 destination_green =
        (destination_color >> 8) & 0xFFu;

    const ImU32 destination_red =
        (destination_color >> 16) & 0xFFu;

    const ImU32 output_red =
        ImGui_ImplGDI_Div255Rounded(
            (ImU32)source_red * alpha +
            destination_red * inverse_alpha);

    const ImU32 output_green =
        ImGui_ImplGDI_Div255Rounded(
            (ImU32)source_green * alpha +
            destination_green * inverse_alpha);

    const ImU32 output_blue =
        ImGui_ImplGDI_Div255Rounded(
            (ImU32)source_blue * alpha +
            destination_blue * inverse_alpha);

    *destination =
        0xFF000000u |
        (output_red << 16) |
        (output_green << 8) |
        (output_blue << 0);
}

struct ImGui_ImplGDI_ConstantBlendState
{
    ImU32 SourceRedAlpha;
    ImU32 SourceGreenAlpha;
    ImU32 SourceBlueAlpha;
    ImU32 InverseAlpha;
};

static inline void ImGui_ImplGDI_BlendConstantSpan(
    ImU32* destination,
    size_t pixel_count,
    const ImGui_ImplGDI_ConstantBlendState& blend_state)
{
    const ImU32 source_red_alpha =
        blend_state.SourceRedAlpha;

    const ImU32 source_green_alpha =
        blend_state.SourceGreenAlpha;

    const ImU32 source_blue_alpha =
        blend_state.SourceBlueAlpha;

    const ImU32 inverse_alpha =
        blend_state.InverseAlpha;

#if defined(IMGUI_IMPL_GDI_HAS_SSE2) && \
    defined(IMGUI_IMPL_GDI_ENABLE_SSE2_CONSTANT_BLEND)
    const __m128i zero =
        _mm_setzero_si128();

    const __m128i inverse_alpha_16 =
        _mm_set1_epi16((short)inverse_alpha);

    const __m128i rounding_16 =
        _mm_set1_epi16(128);

    /*
     * Unpacked destination byte order:
     *
     * B0 G0 R0 A0 B1 G1 R1 A1
     */
    const __m128i source_terms_16 =
        _mm_set_epi16(
            0,
            (short)source_red_alpha,
            (short)source_green_alpha,
            (short)source_blue_alpha,
            0,
            (short)source_red_alpha,
            (short)source_green_alpha,
            (short)source_blue_alpha);

    const __m128i framebuffer_alpha_mask =
        _mm_set1_epi32((int)0xFF000000u);

    while (pixel_count >= 4)
    {
        const __m128i packed_destination =
            _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(
                    destination));

        __m128i low =
            _mm_unpacklo_epi8(
                packed_destination,
                zero);

        __m128i high =
            _mm_unpackhi_epi8(
                packed_destination,
                zero);

        low = _mm_mullo_epi16(
            low,
            inverse_alpha_16);

        high = _mm_mullo_epi16(
            high,
            inverse_alpha_16);

        low = _mm_add_epi16(
            low,
            source_terms_16);

        high = _mm_add_epi16(
            high,
            source_terms_16);

        /*
         * Exact rounded division by 255 for values in [0, 65025]:
         *
         * value = value + 128;
         * value = value + (value >> 8);
         * result = value >> 8;
         */
        low = _mm_add_epi16(
            low,
            rounding_16);

        high = _mm_add_epi16(
            high,
            rounding_16);

        low = _mm_add_epi16(
            low,
            _mm_srli_epi16(low, 8));

        high = _mm_add_epi16(
            high,
            _mm_srli_epi16(high, 8));

        low = _mm_srli_epi16(
            low,
            8);

        high = _mm_srli_epi16(
            high,
            8);

        __m128i packed_result =
            _mm_packus_epi16(
                low,
                high);

        /*
         * Keep the framebuffer alpha invariant even if an uncleared
         * destination initially contains an arbitrary alpha byte.
         */
        packed_result = _mm_or_si128(
            packed_result,
            framebuffer_alpha_mask);

        _mm_storeu_si128(
            reinterpret_cast<__m128i*>(
                destination),
            packed_result);

        destination += 4;
        pixel_count -= 4;
    }
#endif

    /*
     * Canonical scalar fallback and SIMD tail.
     */
    for (size_t index = 0;
        index < pixel_count;
        ++index)
    {
        const ImU32 destination_color =
            destination[index];

        const ImU32 destination_blue =
            (destination_color >> 0) & 0xFFu;

        const ImU32 destination_green =
            (destination_color >> 8) & 0xFFu;

        const ImU32 destination_red =
            (destination_color >> 16) & 0xFFu;

        const ImU32 output_red =
            ImGui_ImplGDI_Div255Rounded(
                source_red_alpha +
                destination_red * inverse_alpha);

        const ImU32 output_green =
            ImGui_ImplGDI_Div255Rounded(
                source_green_alpha +
                destination_green * inverse_alpha);

        const ImU32 output_blue =
            ImGui_ImplGDI_Div255Rounded(
                source_blue_alpha +
                destination_blue * inverse_alpha);

        destination[index] =
            0xFF000000u |
            (output_red << 16) |
            (output_green << 8) |
            output_blue;
    }
}

static inline void ImGui_ImplGDI_BlendA8OpaqueTintSpan(
    ImU32* destination,
    const uint8_t* source_alpha,
    size_t pixel_count,
    ImU32 source_red,
    ImU32 source_green,
    ImU32 source_blue)
{
#if defined(IMGUI_IMPL_GDI_HAS_SSE2) && \
    defined(IMGUI_IMPL_GDI_ENABLE_SSE2_A8_OPAQUE_TINT_BLEND)

    /*
     * In unoptimized builds, keep all SIMD state construction out
     * of spans that are too short to enter the SIMD loop.
     */
    if (pixel_count >= 4)
    {
        const __m128i zero =
            _mm_setzero_si128();

        const __m128i all_255_16 =
            _mm_set1_epi16(255);

        const __m128i rounding_16 =
            _mm_set1_epi16(128);

        /*
         * Unpacked channel order:
         *
         * B0 G0 R0 A0 B1 G1 R1 A1
         */
        const __m128i source_color_16 =
            _mm_set_epi16(
                0,
                (short)source_red,
                (short)source_green,
                (short)source_blue,
                0,
                (short)source_red,
                (short)source_green,
                (short)source_blue);

        const __m128i framebuffer_alpha_mask =
            _mm_set1_epi32((int)0xFF000000u);

        while (pixel_count >= 4)
        {
            /*
             * Load exactly four Alpha8 texels without assuming
             * alignment or reading past the span.
             */
            ImU32 packed_source_alpha;

            memcpy(
                &packed_source_alpha,
                source_alpha,
                sizeof(packed_source_alpha));

            const __m128i alpha_bytes =
                _mm_cvtsi32_si128(
                    (int)packed_source_alpha);

            /*
             * Convert:
             *
             * a0 a1 a2 a3
             *
             * into:
             *
             * a0 a0 a0 a0 a1 a1 a1 a1
             * a2 a2 a2 a2 a3 a3 a3 a3
             */
            const __m128i alpha_16 =
                _mm_unpacklo_epi8(
                    alpha_bytes,
                    zero);

            const __m128i alpha_pairs_16 =
                _mm_unpacklo_epi16(
                    alpha_16,
                    alpha_16);

            const __m128i alpha_low_16 =
                _mm_unpacklo_epi32(
                    alpha_pairs_16,
                    alpha_pairs_16);

            const __m128i alpha_high_16 =
                _mm_unpackhi_epi32(
                    alpha_pairs_16,
                    alpha_pairs_16);

            const __m128i inverse_alpha_low_16 =
                _mm_sub_epi16(
                    all_255_16,
                    alpha_low_16);

            const __m128i inverse_alpha_high_16 =
                _mm_sub_epi16(
                    all_255_16,
                    alpha_high_16);

            const __m128i packed_destination =
                _mm_loadu_si128(
                    reinterpret_cast<const __m128i*>(
                        destination));

            __m128i destination_low_16 =
                _mm_unpacklo_epi8(
                    packed_destination,
                    zero);

            __m128i destination_high_16 =
                _mm_unpackhi_epi8(
                    packed_destination,
                    zero);

            const __m128i source_low_16 =
                _mm_mullo_epi16(
                    source_color_16,
                    alpha_low_16);

            const __m128i source_high_16 =
                _mm_mullo_epi16(
                    source_color_16,
                    alpha_high_16);

            destination_low_16 =
                _mm_mullo_epi16(
                    destination_low_16,
                    inverse_alpha_low_16);

            destination_high_16 =
                _mm_mullo_epi16(
                    destination_high_16,
                    inverse_alpha_high_16);

            destination_low_16 =
                _mm_add_epi16(
                    destination_low_16,
                    source_low_16);

            destination_high_16 =
                _mm_add_epi16(
                    destination_high_16,
                    source_high_16);

            /*
             * Exact rounded division by 255:
             *
             * value = value + 128;
             * value = value + (value >> 8);
             * result = value >> 8;
             */
            destination_low_16 =
                _mm_add_epi16(
                    destination_low_16,
                    rounding_16);

            destination_high_16 =
                _mm_add_epi16(
                    destination_high_16,
                    rounding_16);

            destination_low_16 =
                _mm_add_epi16(
                    destination_low_16,
                    _mm_srli_epi16(
                        destination_low_16,
                        8));

            destination_high_16 =
                _mm_add_epi16(
                    destination_high_16,
                    _mm_srli_epi16(
                        destination_high_16,
                        8));

            destination_low_16 =
                _mm_srli_epi16(
                    destination_low_16,
                    8);

            destination_high_16 =
                _mm_srli_epi16(
                    destination_high_16,
                    8);

            __m128i packed_result =
                _mm_packus_epi16(
                    destination_low_16,
                    destination_high_16);

            packed_result =
                _mm_or_si128(
                    packed_result,
                    framebuffer_alpha_mask);

            _mm_storeu_si128(
                reinterpret_cast<__m128i*>(
                    destination),
                packed_result);

            destination += 4;
            source_alpha += 4;
            pixel_count -= 4;
        }
    }
#endif

    /*
     * Canonical scalar fallback and SIMD tail.
     */
    for (size_t index = 0;
        index < pixel_count;
        ++index)
    {
        const ImU32 alpha =
            source_alpha[index];

        const ImU32 inverse_alpha =
            255u - alpha;

        const ImU32 destination_color =
            destination[index];

        const ImU32 destination_blue =
            (destination_color >> 0) & 0xFFu;

        const ImU32 destination_green =
            (destination_color >> 8) & 0xFFu;

        const ImU32 destination_red =
            (destination_color >> 16) & 0xFFu;

        const ImU32 output_red =
            ImGui_ImplGDI_Div255Rounded(
                source_red * alpha +
                destination_red * inverse_alpha);

        const ImU32 output_green =
            ImGui_ImplGDI_Div255Rounded(
                source_green * alpha +
                destination_green * inverse_alpha);

        const ImU32 output_blue =
            ImGui_ImplGDI_Div255Rounded(
                source_blue * alpha +
                destination_blue * inverse_alpha);

        destination[index] =
            0xFF000000u |
            (output_red << 16) |
            (output_green << 8) |
            output_blue;
    }
}

static bool ImGui_ImplGDI_ClipPixelBounds(
    int& x0,
    int& y0,
    int& x1,
    int& y1,
    int framebuffer_width,
    int framebuffer_height,
    const ImVec4& clip_rect)
{
    const int clip_x0 = (int)ceilf(clip_rect.x - 0.5f);
    const int clip_y0 = (int)ceilf(clip_rect.y - 0.5f);
    const int clip_x1 = (int)ceilf(clip_rect.z - 0.5f);
    const int clip_y1 = (int)ceilf(clip_rect.w - 0.5f);

    if (x0 < clip_x0)
        x0 = clip_x0;

    if (y0 < clip_y0)
        y0 = clip_y0;

    if (x1 > clip_x1)
        x1 = clip_x1;

    if (y1 > clip_y1)
        y1 = clip_y1;

    x0 = IMGUI_IMPL_GDI_CLAMP(x0, 0, framebuffer_width);
    y0 = IMGUI_IMPL_GDI_CLAMP(y0, 0, framebuffer_height);
    x1 = IMGUI_IMPL_GDI_CLAMP(x1, 0, framebuffer_width);
    y1 = IMGUI_IMPL_GDI_CLAMP(y1, 0, framebuffer_height);

    return x0 < x1 && y0 < y1;
}

static void ImGui_ImplGDI_RenderSolidRectangleCommand(
    ImU32* pixel_buffer,
    int framebuffer_width,
    int framebuffer_height,
    const ImVec4& clip_rect,
    const NAIVE_SWR_RENDER_COMMAND& render_command)
{
    const auto& rectangle = render_command.Command.Rectangle;
    const NAIVE_SWR_COLOR& color = rectangle.Color;

    if (color.Alpha == 0)
        return;

    float left = rectangle.Position.X;
    float top = rectangle.Position.Y;
    float right = left + rectangle.Size.Width;
    float bottom = top + rectangle.Size.Height;

    if (right < left)
    {
        const float temporary = left;
        left = right;
        right = temporary;
    }

    if (bottom < top)
    {
        const float temporary = top;
        top = bottom;
        bottom = temporary;
    }

    int x0 = (int)ceilf(left - 0.5f);
    int y0 = (int)ceilf(top - 0.5f);
    int x1 = (int)ceilf(right - 0.5f);
    int y1 = (int)ceilf(bottom - 0.5f);

    if (!ImGui_ImplGDI_ClipPixelBounds(
        x0,
        y0,
        x1,
        y1,
        framebuffer_width,
        framebuffer_height,
        clip_rect))
    {
        return;
    }

    IM_ASSERT(pixel_buffer != nullptr);

    const int span_width = x1 - x0;

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)

    const int span_height = y1 - y0;
    ImGui_ImplGDI_RectanglePathStats& rectangle_statistics =
        color.Alpha == 255
        ? g_ImGui_ImplGDI_Stats.OpaqueRectangles
        : g_ImGui_ImplGDI_Stats.TranslucentRectangles;

    ++rectangle_statistics.CommandCount;

    rectangle_statistics.PixelCount +=
        (uint64_t)span_width * (uint64_t)span_height;
#endif


    ImU32* destination_row =
        pixel_buffer + (size_t)y0 * framebuffer_width + x0;

    if (color.Alpha == 255)
    {
        const ImU32 fill_color =
            0xFF000000u |
            ((ImU32)color.Red << 16) |
            ((ImU32)color.Green << 8) |
            ((ImU32)color.Blue << 0);

        for (int y = y0; y < y1; ++y)
        {
            ImGui_ImplGDI_FillSpan(
                destination_row,
                (size_t)span_width,
                fill_color);

            destination_row += framebuffer_width;
        }

        return;
    }

    const ImU32 source_alpha = color.Alpha;

    ImGui_ImplGDI_ConstantBlendState blend_state;
    blend_state.SourceRedAlpha = (ImU32)color.Red * source_alpha;
    blend_state.SourceGreenAlpha = (ImU32)color.Green * source_alpha;
    blend_state.SourceBlueAlpha = (ImU32)color.Blue * source_alpha;
    blend_state.InverseAlpha = 255u - source_alpha;

    for (int y = y0; y < y1; ++y)
    {
        ImGui_ImplGDI_BlendConstantSpan(
            destination_row,
            (size_t)span_width,
            blend_state);

        destination_row += framebuffer_width;
    }
}

template <NAIVE_SWR_RENDER_TYPE RenderType>
static void ImGui_ImplGDI_RenderTexturedRectangleCommand(
    ImU32* pixel_buffer,
    int framebuffer_width,
    int framebuffer_height,
    const ImVec4& clip_rect,
    const ImGui_ImplGDI_Texture* texture,
    const NAIVE_SWR_RENDER_COMMAND& render_command)
{
    IM_ASSERT(
        RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE ||
        RenderType == NAIVE_SWR_RENDER_TYPE_RGBA32_RECTANGLE);

    if (!pixel_buffer ||
        !texture ||
        !texture->Pixels ||
        texture->Width <= 0 ||
        texture->Height <= 0)
    {
        return;
    }

    const auto& rectangle = render_command.Command.Rectangle;
    const NAIVE_SWR_COLOR& color = rectangle.Color;

    if (color.Alpha == 0 ||
        rectangle.Size.Width == 0.0f ||
        rectangle.Size.Height == 0.0f)
    {
        return;
    }

    const float destination_x0 = rectangle.Position.X;
    const float destination_y0 = rectangle.Position.Y;
    const float destination_x1 =
        destination_x0 + rectangle.Size.Width;
    const float destination_y1 =
        destination_y0 + rectangle.Size.Height;

    const float left =
        destination_x0 < destination_x1
        ? destination_x0
        : destination_x1;

    const float right =
        destination_x0 > destination_x1
        ? destination_x0
        : destination_x1;

    const float top =
        destination_y0 < destination_y1
        ? destination_y0
        : destination_y1;

    const float bottom =
        destination_y0 > destination_y1
        ? destination_y0
        : destination_y1;

    int x0 = (int)ceilf(left - 0.5f);
    int y0 = (int)ceilf(top - 0.5f);
    int x1 = (int)ceilf(right - 0.5f);
    int y1 = (int)ceilf(bottom - 0.5f);

    if (!ImGui_ImplGDI_ClipPixelBounds(
        x0,
        y0,
        x1,
        y1,
        framebuffer_width,
        framebuffer_height,
        clip_rect))
    {
        return;
    }

    const float texture_step_x =
        rectangle.TextureSize.Width / rectangle.Size.Width;

    const float texture_step_y =
        rectangle.TextureSize.Height / rectangle.Size.Height;

    const float texture_x_start =
        rectangle.TexturePosition.X +
        (((float)x0 + 0.5f) - rectangle.Position.X) *
        texture_step_x;

    const float texture_y_start =
        rectangle.TexturePosition.Y +
        (((float)y0 + 0.5f) - rectangle.Position.Y) *
        texture_step_y;

    float texture_y = texture_y_start;

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
    if (RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE)
    {
        const int destination_width = x1 - x0;
        const int destination_height = y1 - y0;

        const uint64_t destination_pixel_count =
            (uint64_t)destination_width *
            (uint64_t)destination_height;

        ImGui_ImplGDI_RectanglePathStats& total_statistics =
            g_ImGui_ImplGDI_Stats.A8Rectangles;

        ++total_statistics.CommandCount;
        total_statistics.PixelCount +=
            destination_pixel_count;

        if (texture_step_x == 1.0f)
        {
            ImGui_ImplGDI_RectanglePathStats& statistics =
                g_ImGui_ImplGDI_Stats
                .A8RectanglesExactUnitX;

            ++statistics.CommandCount;
            statistics.PixelCount +=
                destination_pixel_count;
        }

        /*
         * Simulate the current nearest-neighbor X sampling exactly.
         * A sequence is contiguous if its clamped source indexes are:
         *
         * first, first + 1, first + 2, ...
         */
        bool texture_x_is_contiguous = true;
        int first_texture_x = 0;
        float test_texture_x = texture_x_start;

        for (int offset = 0;
            offset < destination_width;
            ++offset)
        {
            int sampled_texture_x =
                (int)test_texture_x;

            sampled_texture_x = IMGUI_IMPL_GDI_CLAMP(
                sampled_texture_x,
                0,
                texture->Width - 1);

            if (offset == 0)
            {
                first_texture_x =
                    sampled_texture_x;
            }
            else if (
                sampled_texture_x !=
                first_texture_x + offset)
            {
                texture_x_is_contiguous = false;
                break;
            }

            test_texture_x += texture_step_x;
        }

        /*
         * Perform the corresponding Y test. Y continuity is not
         * required for a per-row span, but is useful for identifying
         * true 1:1 glyph rectangles.
         */
        bool texture_y_is_contiguous = true;
        int first_texture_y = 0;
        float test_texture_y = texture_y_start;

        for (int offset = 0;
            offset < destination_height;
            ++offset)
        {
            int sampled_texture_y =
                (int)test_texture_y;

            sampled_texture_y = IMGUI_IMPL_GDI_CLAMP(
                sampled_texture_y,
                0,
                texture->Height - 1);

            if (offset == 0)
            {
                first_texture_y =
                    sampled_texture_y;
            }
            else if (
                sampled_texture_y !=
                first_texture_y + offset)
            {
                texture_y_is_contiguous = false;
                break;
            }

            test_texture_y += texture_step_y;
        }

        if (texture_x_is_contiguous)
        {
            ImGui_ImplGDI_RectanglePathStats& statistics =
                g_ImGui_ImplGDI_Stats
                .A8RectanglesContiguousX;

            ++statistics.CommandCount;
            statistics.PixelCount +=
                destination_pixel_count;

            if (color.Alpha == 255)
            {
                ImGui_ImplGDI_RectanglePathStats&
                    opaque_tint_statistics =
                    g_ImGui_ImplGDI_Stats
                    .A8RectanglesContiguousXOpaqueTint;

                ++opaque_tint_statistics.CommandCount;
                opaque_tint_statistics.PixelCount +=
                    destination_pixel_count;
            }

            if (texture_y_is_contiguous)
            {
                ImGui_ImplGDI_RectanglePathStats&
                    xy_statistics =
                    g_ImGui_ImplGDI_Stats
                    .A8RectanglesContiguousXY;

                ++xy_statistics.CommandCount;
                xy_statistics.PixelCount +=
                    destination_pixel_count;
            }
        }
    }
#endif

    const uint8_t* texture_bytes = texture->Pixels;

    if (RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE &&
        color.Alpha == 255 &&
        texture_step_x == 1.0f &&
        texture_step_y == 1.0f)
    {
        const int destination_width =
            x1 - x0;

        const int destination_height =
            y1 - y0;

        const int source_x0 =
            (int)texture_x_start;

        const int source_y0 =
            (int)texture_y_start;

        const float source_x_last =
            texture_x_start +
            (float)(destination_width - 1);

        const float source_y_last =
            texture_y_start +
            (float)(destination_height - 1);

        /*
         * Verify once per rectangle that no per-pixel texture clamp
         * would have taken effect.
         */
        if (texture_x_start >= 0.0f &&
            texture_y_start >= 0.0f &&
            source_x_last < (float)texture->Width &&
            source_y_last < (float)texture->Height &&
            source_x0 >= 0 &&
            source_y0 >= 0 &&
            source_x0 + destination_width <= texture->Width &&
            source_y0 + destination_height <= texture->Height)
        {
            ImU32* destination_row =
                pixel_buffer +
                (size_t)y0 * framebuffer_width +
                x0;

            const uint8_t* source_row =
                texture_bytes +
                (size_t)source_y0 * texture->Width +
                source_x0;

            for (int row = 0;
                row < destination_height;
                ++row)
            {
                ImGui_ImplGDI_BlendA8OpaqueTintSpan(
                    destination_row,
                    source_row,
                    (size_t)destination_width,
                    color.Red,
                    color.Green,
                    color.Blue);

                destination_row +=
                    framebuffer_width;

                source_row +=
                    texture->Width;
            }

            return;
        }
    }

    for (int y = y0; y < y1; ++y)
    {
        int texture_y_integer = (int)texture_y;

        texture_y_integer = IMGUI_IMPL_GDI_CLAMP(
            texture_y_integer,
            0,
            texture->Height - 1);

        float texture_x = texture_x_start;

        for (int x = x0; x < x1; ++x)
        {
            int texture_x_integer = (int)texture_x;

            texture_x_integer = IMGUI_IMPL_GDI_CLAMP(
                texture_x_integer,
                0,
                texture->Width - 1);

            const size_t texture_index =
                (size_t)texture_y_integer * texture->Width +
                texture_x_integer;

            int source_red;
            int source_green;
            int source_blue;
            int source_alpha;

            if (RenderType ==
                NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE)
            {
                const int texture_alpha =
                    texture_bytes[texture_index];

                source_red = color.Red;
                source_green = color.Green;
                source_blue = color.Blue;
                source_alpha = ImGui_ImplGDI_Mul255(
                    texture_alpha,
                    color.Alpha);
            }
            else
            {
                const uint8_t* texel =
                    texture_bytes + texture_index * 4;

                source_red = ImGui_ImplGDI_Mul255(
                    texel[0],
                    color.Red);

                source_green = ImGui_ImplGDI_Mul255(
                    texel[1],
                    color.Green);

                source_blue = ImGui_ImplGDI_Mul255(
                    texel[2],
                    color.Blue);

                source_alpha = ImGui_ImplGDI_Mul255(
                    texel[3],
                    color.Alpha);
            }

            ImU32* destination =
                pixel_buffer + (size_t)y * framebuffer_width + x;

            ImGui_ImplGDI_BlendOver(
                destination,
                source_red,
                source_green,
                source_blue,
                source_alpha);

            texture_x += texture_step_x;
        }

        texture_y += texture_step_y;
    }
}

static inline float ImGui_ImplGDI_Edge(
    const NAIVE_SWR_POINT& a,
    const NAIVE_SWR_POINT& b,
    float x,
    float y)
{
    return (x - a.X) * (b.Y - a.Y) -
        (y - a.Y) * (b.X - a.X);
}

static inline bool ImGui_ImplGDI_IsTopLeft(
    const NAIVE_SWR_POINT& a,
    const NAIVE_SWR_POINT& b)
{
    const float delta_x = b.X - a.X;
    const float delta_y = b.Y - a.Y;

    return delta_y < 0.0f ||
        (delta_y == 0.0f && delta_x > 0.0f);
}

static inline bool ImGui_ImplGDI_ConstrainCoverageSpan(
    float edge_value,
    float edge_step_x,
    bool top_left,
    int& span_begin,
    int& span_end)
{
    if (edge_step_x == 0.0f)
    {
        return edge_value >= 0.0f &&
            (edge_value != 0.0f || top_left);
    }

    const float crossing = -edge_value / edge_step_x;
    const float current_begin = (float)span_begin;
    const float current_last = (float)(span_end - 1);

    if (edge_step_x > 0.0f)
    {
        if (top_left)
        {
            /*
             * First accepted integer is ceil(crossing).
             */
            if (crossing <= current_begin)
                return span_begin < span_end;

            if (crossing > current_last)
                return false;

            /*
             * The range checks above guarantee that crossing is
             * non-negative and safely representable as int.
             */
            const int truncated = (int)crossing;

            span_begin = truncated + ((float)truncated < crossing ? 1 : 0);
        }
        else
        {
            /*
             * First accepted integer is floor(crossing) + 1.
             */
            if (crossing < current_begin)
                return span_begin < span_end;

            if (crossing >= current_last)
                return false;

            span_begin = (int)crossing + 1;
        }
    }
    else
    {
        if (top_left)
        {
            /*
             * Exclusive end is floor(crossing) + 1.
             */
            if (crossing >= current_last)
                return span_begin < span_end;

            if (crossing < current_begin)
                return false;

            span_end = (int)crossing + 1;
        }
        else
        {
            /*
             * Exclusive end is ceil(crossing).
             */
            if (crossing > current_last)
                return span_begin < span_end;

            if (crossing <= current_begin)
                return false;

            const int truncated = (int)crossing;

            span_end = truncated + ((float)truncated < crossing ? 1 : 0);
        }
    }

    return span_begin < span_end;
}

template <NAIVE_SWR_RENDER_TYPE RenderType>
static void ImGui_ImplGDI_RenderTriangleCommand(
    ImU32* pixel_buffer,
    int framebuffer_width,
    int framebuffer_height,
    const ImVec4& clip_rect,
    const ImGui_ImplGDI_Texture* texture,
    const NAIVE_SWR_RENDER_COMMAND& render_command)
{
    IM_ASSERT(
        RenderType == NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE ||
        RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE ||
        RenderType == NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE ||
        RenderType == NAIVE_SWR_RENDER_TYPE_SOLID_OPAQUE_TRIANGLE ||
        RenderType == NAIVE_SWR_RENDER_TYPE_SOLID_TRANSLUCENT_TRIANGLE ||
        RenderType == NAIVE_SWR_RENDER_TYPE_SOLID_ALPHA_GRADIENT_TRIANGLE ||
        RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE ||
        RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_TRANSLUCENT_TINT_TRIANGLE);

    if (!pixel_buffer ||
        framebuffer_width <= 0 ||
        framebuffer_height <= 0)
    {
        return;
    }

    const bool is_alpha8_textured =
        RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE ||
        RenderType ==
        NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE ||
        RenderType ==
        NAIVE_SWR_RENDER_TYPE_ALPHA8_TRANSLUCENT_TINT_TRIANGLE;

    const bool is_textured =
        is_alpha8_textured ||
        RenderType == NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE;

    if (is_textured)
    {
        if (!texture ||
            !texture->Pixels ||
            texture->Width <= 0 ||
            texture->Height <= 0)
        {
            return;
        }
    }

    const auto& triangle = render_command.Command.Triangle;

    const NAIVE_SWR_POINT& point_1 = triangle.Positions[0];
    const NAIVE_SWR_POINT& point_2 = triangle.Positions[1];
    const NAIVE_SWR_POINT& point_3 = triangle.Positions[2];

    const float area = ImGui_ImplGDI_Edge(
        point_1,
        point_2,
        point_3.X,
        point_3.Y);

    if (area > -0.000001f && area < 0.000001f)
        return;

    const bool reverse_winding = area < 0.0f;
    const float normalized_area =
        reverse_winding ? -area : area;

    float minimum_x = point_1.X;
    float minimum_y = point_1.Y;
    float maximum_x = point_1.X;
    float maximum_y = point_1.Y;

    if (point_2.X < minimum_x) minimum_x = point_2.X;
    if (point_3.X < minimum_x) minimum_x = point_3.X;
    if (point_2.Y < minimum_y) minimum_y = point_2.Y;
    if (point_3.Y < minimum_y) minimum_y = point_3.Y;

    if (point_2.X > maximum_x) maximum_x = point_2.X;
    if (point_3.X > maximum_x) maximum_x = point_3.X;
    if (point_2.Y > maximum_y) maximum_y = point_2.Y;
    if (point_3.Y > maximum_y) maximum_y = point_3.Y;

    int x0 = (int)floorf(minimum_x);
    int y0 = (int)floorf(minimum_y);
    int x1 = (int)ceilf(maximum_x);
    int y1 = (int)ceilf(maximum_y);

    if (!ImGui_ImplGDI_ClipPixelBounds(
        x0,
        y0,
        x1,
        y1,
        framebuffer_width,
        framebuffer_height,
        clip_rect))
    {
        return;
    }

    const bool top_left_1 =
        ImGui_ImplGDI_IsTopLeft(point_2, point_3);

    const bool top_left_2 =
        ImGui_ImplGDI_IsTopLeft(point_3, point_1);

    const bool top_left_3 =
        ImGui_ImplGDI_IsTopLeft(point_1, point_2);

    float edge_1_step_x = point_3.Y - point_2.Y;
    float edge_2_step_x = point_1.Y - point_3.Y;
    float edge_3_step_x = point_2.Y - point_1.Y;

    float edge_1_step_y = -(point_3.X - point_2.X);
    float edge_2_step_y = -(point_1.X - point_3.X);
    float edge_3_step_y = -(point_2.X - point_1.X);

    const float first_pixel_x = (float)x0 + 0.5f;
    const float first_pixel_y = (float)y0 + 0.5f;

    float edge_1_row = ImGui_ImplGDI_Edge(
        point_2,
        point_3,
        first_pixel_x,
        first_pixel_y);

    float edge_2_row = ImGui_ImplGDI_Edge(
        point_3,
        point_1,
        first_pixel_x,
        first_pixel_y);

    float edge_3_row = ImGui_ImplGDI_Edge(
        point_1,
        point_2,
        first_pixel_x,
        first_pixel_y);

    /*
     * The previous implementation multiplied every edge value by
     * area for every candidate pixel. Normalize the winding once
     * instead. The normalized edges and area are both positive for
     * covered pixels, while the resulting barycentric weights remain
     * unchanged.
     */
    if (reverse_winding)
    {
        edge_1_row = -edge_1_row;
        edge_2_row = -edge_2_row;
        edge_3_row = -edge_3_row;

        edge_1_step_x = -edge_1_step_x;
        edge_2_step_x = -edge_2_step_x;
        edge_3_step_x = -edge_3_step_x;

        edge_1_step_y = -edge_1_step_y;
        edge_2_step_y = -edge_2_step_y;
        edge_3_step_y = -edge_3_step_y;
    }

    const NAIVE_SWR_COLOR& color_1 = triangle.Colors[0];
    const NAIVE_SWR_COLOR& color_2 = triangle.Colors[1];
    const NAIVE_SWR_COLOR& color_3 = triangle.Colors[2];

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
    ImGui_ImplGDI_TriangleStatsClass statistics_class;

    if (RenderType == NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE)
    {
        statistics_class = ImGui_ImplGDI_TriangleStats_RGBA32Textured;
    }
    else
    {
        const ImGui_ImplGDI_TriangleVertexColorClass vertex_color_class =
            ImGui_ImplGDI_ClassifyTriangleVertexColor(
                color_1,
                color_2,
                color_3);

        statistics_class = ImGui_ImplGDI_GetTriangleStatsClass(
            is_alpha8_textured,
            vertex_color_class);
    }

    ImGui_ImplGDI_TrianglePathStats& path_statistics =
        g_ImGui_ImplGDI_Stats.Triangles[statistics_class];

    ++path_statistics.CommandCount;

    path_statistics.CandidatePixelCount +=
        (uint64_t)(x1 - x0) * (uint64_t)(y1 - y0);

    uint64_t covered_pixel_count = 0;
#endif

    if (RenderType == NAIVE_SWR_RENDER_TYPE_SOLID_OPAQUE_TRIANGLE)
    {
        IM_ASSERT(color_1.Alpha == 255);

        const ImU32 fill_color =
            0xFF000000u |
            ((ImU32)color_1.Red << 16) |
            ((ImU32)color_1.Green << 8) |
            ((ImU32)color_1.Blue << 0);

        ImU32* destination_row =
            pixel_buffer +
            (size_t)y0 * framebuffer_width +
            x0;

        const int maximum_span_width =
            x1 - x0;

        for (int y = y0; y < y1; ++y)
        {
            int span_begin = 0;
            int span_end = maximum_span_width;

            const bool has_coverage =
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_1_row,
                    edge_1_step_x,
                    top_left_1,
                    span_begin,
                    span_end) &&
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_2_row,
                    edge_2_step_x,
                    top_left_2,
                    span_begin,
                    span_end) &&
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_3_row,
                    edge_3_step_x,
                    top_left_3,
                    span_begin,
                    span_end);

            if (has_coverage)
            {
                const int span_width =
                    span_end - span_begin;

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
                covered_pixel_count +=
                    (uint64_t)span_width;
#endif

                ImGui_ImplGDI_FillSpan(
                    destination_row + span_begin,
                    (size_t)span_width,
                    fill_color);
            }

            destination_row += framebuffer_width;

            edge_1_row += edge_1_step_y;
            edge_2_row += edge_2_step_y;
            edge_3_row += edge_3_step_y;
        }

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
        path_statistics.CoveredPixelCount +=
            covered_pixel_count;
#endif

        return;
    }

    if (RenderType == NAIVE_SWR_RENDER_TYPE_SOLID_TRANSLUCENT_TRIANGLE)
    {
        IM_ASSERT(
            color_1.Alpha > 0 &&
            color_1.Alpha < 255);

        const ImU32 source_alpha = color_1.Alpha;

        ImGui_ImplGDI_ConstantBlendState blend_state;

        blend_state.SourceRedAlpha =
            (ImU32)color_1.Red * source_alpha;

        blend_state.SourceGreenAlpha =
            (ImU32)color_1.Green * source_alpha;

        blend_state.SourceBlueAlpha =
            (ImU32)color_1.Blue * source_alpha;

        blend_state.InverseAlpha =
            255u - source_alpha;

        ImU32* destination_row =
            pixel_buffer +
            (size_t)y0 * framebuffer_width +
            x0;

        const int maximum_span_width = x1 - x0;

        for (int y = y0; y < y1; ++y)
        {
            int span_begin = 0;
            int span_end = maximum_span_width;

            const bool has_coverage =
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_1_row,
                    edge_1_step_x,
                    top_left_1,
                    span_begin,
                    span_end) &&
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_2_row,
                    edge_2_step_x,
                    top_left_2,
                    span_begin,
                    span_end) &&
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_3_row,
                    edge_3_step_x,
                    top_left_3,
                    span_begin,
                    span_end);

            if (has_coverage)
            {
                const int span_width = span_end - span_begin;

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
                covered_pixel_count +=
                    (uint64_t)span_width;
#endif

                ImGui_ImplGDI_BlendConstantSpan(
                    destination_row + span_begin,
                    (size_t)span_width,
                    blend_state);
            }

            destination_row += framebuffer_width;

            edge_1_row += edge_1_step_y;
            edge_2_row += edge_2_step_y;
            edge_3_row += edge_3_step_y;
        }

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
        path_statistics.CoveredPixelCount +=
            covered_pixel_count;
#endif

        return;
    }

    const float inverse_area = 1.0f / normalized_area;

    if (RenderType == NAIVE_SWR_RENDER_TYPE_SOLID_ALPHA_GRADIENT_TRIANGLE)
    {
        const int source_red = color_1.Red;
        const int source_green = color_1.Green;
        const int source_blue = color_1.Blue;

        const float alpha_1 =
            (float)color_1.Alpha;

        const float alpha_2 =
            (float)color_2.Alpha;

        const float alpha_3 =
            (float)color_3.Alpha;

        /*
         * Alpha is affine across the triangle. Build its value and
         * derivatives once instead of calculating three barycentric
         * weights and three weighted alpha terms for every covered
         * pixel.
         */
        float alpha_row =
            (alpha_1 * edge_1_row +
                alpha_2 * edge_2_row +
                alpha_3 * edge_3_row) *
            inverse_area;

        const float alpha_step_x =
            (alpha_1 * edge_1_step_x +
                alpha_2 * edge_2_step_x +
                alpha_3 * edge_3_step_x) *
            inverse_area;

        const float alpha_step_y =
            (alpha_1 * edge_1_step_y +
                alpha_2 * edge_2_step_y +
                alpha_3 * edge_3_step_y) *
            inverse_area;

        ImU32* destination_row =
            pixel_buffer +
            (size_t)y0 * framebuffer_width +
            x0;

        const int maximum_span_width = x1 - x0;

        for (int y = y0; y < y1; ++y)
        {
            int span_begin = 0;
            int span_end = maximum_span_width;

            const bool has_coverage =
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_1_row,
                    edge_1_step_x,
                    top_left_1,
                    span_begin,
                    span_end) &&
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_2_row,
                    edge_2_step_x,
                    top_left_2,
                    span_begin,
                    span_end) &&
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_3_row,
                    edge_3_step_x,
                    top_left_3,
                    span_begin,
                    span_end);

            if (has_coverage)
            {
                const float interpolated_alpha_at_span_begin =
                    alpha_row +
                    alpha_step_x * (float)span_begin;

                float interpolated_alpha = interpolated_alpha_at_span_begin;

                const int span_width = span_end - span_begin;

                ImU32* destination = destination_row + span_begin;

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
                covered_pixel_count +=
                    (uint64_t)span_width;
#endif

                for (int offset = 0; offset < span_width; ++offset)
                {
                    int source_alpha = (int)(interpolated_alpha + 0.5f);

                    source_alpha = IMGUI_IMPL_GDI_CLAMP(
                        source_alpha,
                        0,
                        255);

                    ImGui_ImplGDI_BlendOver(
                        destination,
                        source_red,
                        source_green,
                        source_blue,
                        source_alpha);

                    ++destination;

                    interpolated_alpha += alpha_step_x;
                }
            }

            destination_row += framebuffer_width;

            edge_1_row += edge_1_step_y;
            edge_2_row += edge_2_step_y;
            edge_3_row += edge_3_step_y;

            alpha_row += alpha_step_y;
        }

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
        path_statistics.CoveredPixelCount += covered_pixel_count;
#endif

        return;
    }

    const uint8_t* texture_bytes = texture ? texture->Pixels : nullptr;

    if (RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE ||
        RenderType == NAIVE_SWR_RENDER_TYPE_ALPHA8_TRANSLUCENT_TINT_TRIANGLE)
    {
        const int source_red = color_1.Red;
        const int source_green = color_1.Green;
        const int source_blue = color_1.Blue;
        const int tint_alpha = color_1.Alpha;

        if (RenderType ==
            NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE)
        {
            IM_ASSERT(tint_alpha == 255);
        }
        else
        {
            IM_ASSERT(tint_alpha > 0 && tint_alpha < 255);
        }

        const NAIVE_SWR_TEXTURE_COORDINATE& texture_coordinate_1 =
            triangle.TextureCoordinates[0];

        const NAIVE_SWR_TEXTURE_COORDINATE& texture_coordinate_2 =
            triangle.TextureCoordinates[1];

        const NAIVE_SWR_TEXTURE_COORDINATE& texture_coordinate_3 =
            triangle.TextureCoordinates[2];

        const float texture_width = (float)texture->Width;
        const float texture_height = (float)texture->Height;

        /*
         * Build the affine planes directly in texel space, moving the
         * normalized-UV-to-texel multiplication out of the pixel loop.
         */

        float texture_x_row =
            (texture_coordinate_1.U *
                (edge_1_row * inverse_area) +
                texture_coordinate_2.U *
                (edge_2_row * inverse_area) +
                texture_coordinate_3.U *
                (edge_3_row * inverse_area)) *
            texture_width;
        float texture_y_row =
            (texture_coordinate_1.V *
                (edge_1_row * inverse_area) +
                texture_coordinate_2.V *
                (edge_2_row * inverse_area) +
                texture_coordinate_3.V *
                (edge_3_row * inverse_area)) *
            texture_height;
        const float texture_x_step_x =
            (texture_coordinate_1.U *
                (edge_1_step_x * inverse_area) +
                texture_coordinate_2.U *
                (edge_2_step_x * inverse_area) +
                texture_coordinate_3.U *
                (edge_3_step_x * inverse_area)) *
            texture_width;
        const float texture_y_step_x =
            (texture_coordinate_1.V *
                (edge_1_step_x * inverse_area) +
                texture_coordinate_2.V *
                (edge_2_step_x * inverse_area) +
                texture_coordinate_3.V *
                (edge_3_step_x * inverse_area)) *
            texture_height;
        const float texture_x_step_y =
            (texture_coordinate_1.U *
                (edge_1_step_y * inverse_area) +
                texture_coordinate_2.U *
                (edge_2_step_y * inverse_area) +
                texture_coordinate_3.U *
                (edge_3_step_y * inverse_area)) *
            texture_width;
        const float texture_y_step_y =
            (texture_coordinate_1.V *
                (edge_1_step_y * inverse_area) +
                texture_coordinate_2.V *
                (edge_2_step_y * inverse_area) +
                texture_coordinate_3.V *
                (edge_3_step_y * inverse_area)) *
            texture_height;

        ImU32* destination_row =
            pixel_buffer +
            (size_t)y0 * framebuffer_width +
            x0;

        const int maximum_span_width =
            x1 - x0;

        for (int y = y0; y < y1; ++y)
        {
            int span_begin = 0;
            int span_end = maximum_span_width;

            const bool has_coverage =
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_1_row,
                    edge_1_step_x,
                    top_left_1,
                    span_begin,
                    span_end) &&
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_2_row,
                    edge_2_step_x,
                    top_left_2,
                    span_begin,
                    span_end) &&
                ImGui_ImplGDI_ConstrainCoverageSpan(
                    edge_3_row,
                    edge_3_step_x,
                    top_left_3,
                    span_begin,
                    span_end);

            if (has_coverage)
            {
                const float span_offset = (float)span_begin;

                float sampled_texture_x =
                    texture_x_row +
                    texture_x_step_x * span_offset;

                float sampled_texture_y =
                    texture_y_row +
                    texture_y_step_x * span_offset;

                const int span_width =
                    span_end - span_begin;

                ImU32* destination =
                    destination_row + span_begin;

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
                covered_pixel_count +=
                    (uint64_t)span_width;
#endif

                for (int offset = 0;
                    offset < span_width;
                    ++offset)
                {
                    int texture_x = (int)sampled_texture_x;

                    int texture_y = (int)sampled_texture_y;

                    texture_x = IMGUI_IMPL_GDI_CLAMP(
                        texture_x,
                        0,
                        texture->Width - 1);

                    texture_y = IMGUI_IMPL_GDI_CLAMP(
                        texture_y,
                        0,
                        texture->Height - 1);

                    const size_t texture_index =
                        (size_t)texture_y * texture->Width +
                        texture_x;

                    const int texture_alpha =
                        texture_bytes[texture_index];

                    const int source_alpha =
                        RenderType ==
                        NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE
                        ? texture_alpha
                        : ImGui_ImplGDI_Mul255(
                            texture_alpha,
                            tint_alpha);

                    ImGui_ImplGDI_BlendOver(
                        destination,
                        source_red,
                        source_green,
                        source_blue,
                        source_alpha);

                    ++destination;

                    sampled_texture_x += texture_x_step_x;
                    sampled_texture_y += texture_y_step_x;
                }
            }

            destination_row += framebuffer_width;

            edge_1_row += edge_1_step_y;
            edge_2_row += edge_2_step_y;
            edge_3_row += edge_3_step_y;

            texture_x_row += texture_x_step_y;
            texture_y_row += texture_y_step_y;
        }

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
        path_statistics.CoveredPixelCount +=
            covered_pixel_count;
#endif

        return;
    }

    for (int y = y0; y < y1; ++y)
    {
        float edge_1 = edge_1_row;
        float edge_2 = edge_2_row;
        float edge_3 = edge_3_row;

        for (int x = x0; x < x1; ++x)
        {
            const bool inside =
                edge_1 >= 0.0f &&
                edge_2 >= 0.0f &&
                edge_3 >= 0.0f &&
                (edge_1 != 0.0f || top_left_1) &&
                (edge_2 != 0.0f || top_left_2) &&
                (edge_3 != 0.0f || top_left_3);

            if (inside)
            {
#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
                ++covered_pixel_count;
#endif

                const float weight_1 = edge_1 * inverse_area;
                const float weight_2 = edge_2 * inverse_area;
                const float weight_3 = edge_3 * inverse_area;

                int vertex_red = (int)(
                    color_1.Red * weight_1 +
                    color_2.Red * weight_2 +
                    color_3.Red * weight_3 +
                    0.5f);

                int vertex_green = (int)(
                    color_1.Green * weight_1 +
                    color_2.Green * weight_2 +
                    color_3.Green * weight_3 +
                    0.5f);

                int vertex_blue = (int)(
                    color_1.Blue * weight_1 +
                    color_2.Blue * weight_2 +
                    color_3.Blue * weight_3 +
                    0.5f);

                int vertex_alpha = (int)(
                    color_1.Alpha * weight_1 +
                    color_2.Alpha * weight_2 +
                    color_3.Alpha * weight_3 +
                    0.5f);

                vertex_red =
                    IMGUI_IMPL_GDI_CLAMP(vertex_red, 0, 255);

                vertex_green =
                    IMGUI_IMPL_GDI_CLAMP(vertex_green, 0, 255);

                vertex_blue =
                    IMGUI_IMPL_GDI_CLAMP(vertex_blue, 0, 255);

                vertex_alpha =
                    IMGUI_IMPL_GDI_CLAMP(vertex_alpha, 0, 255);

                int source_red = vertex_red;
                int source_green = vertex_green;
                int source_blue = vertex_blue;
                int source_alpha = vertex_alpha;

                if (RenderType !=
                    NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE)
                {
                    const float texture_u =
                        triangle.TextureCoordinates[0].U * weight_1 +
                        triangle.TextureCoordinates[1].U * weight_2 +
                        triangle.TextureCoordinates[2].U * weight_3;

                    const float texture_v =
                        triangle.TextureCoordinates[0].V * weight_1 +
                        triangle.TextureCoordinates[1].V * weight_2 +
                        triangle.TextureCoordinates[2].V * weight_3;

                    int texture_x =
                        (int)(texture_u * texture->Width);

                    int texture_y =
                        (int)(texture_v * texture->Height);

                    texture_x = IMGUI_IMPL_GDI_CLAMP(
                        texture_x,
                        0,
                        texture->Width - 1);

                    texture_y = IMGUI_IMPL_GDI_CLAMP(
                        texture_y,
                        0,
                        texture->Height - 1);

                    const size_t texture_index =
                        (size_t)texture_y * texture->Width +
                        texture_x;

                    if (is_alpha8_textured)
                    {
                        const int texture_alpha =
                            texture_bytes[texture_index];

                        source_alpha = ImGui_ImplGDI_Mul255(
                            texture_alpha,
                            vertex_alpha);
                    }
                    else
                    {
                        const uint8_t* texel =
                            texture_bytes + texture_index * 4;

                        source_red = ImGui_ImplGDI_Mul255(
                            texel[0],
                            vertex_red);

                        source_green = ImGui_ImplGDI_Mul255(
                            texel[1],
                            vertex_green);

                        source_blue = ImGui_ImplGDI_Mul255(
                            texel[2],
                            vertex_blue);

                        source_alpha = ImGui_ImplGDI_Mul255(
                            texel[3],
                            vertex_alpha);
                    }
                }

                ImU32* destination =
                    pixel_buffer + (size_t)y * framebuffer_width + x;

                ImGui_ImplGDI_BlendOver(
                    destination,
                    source_red,
                    source_green,
                    source_blue,
                    source_alpha);
            }

            edge_1 += edge_1_step_x;
            edge_2 += edge_2_step_x;
            edge_3 += edge_3_step_x;
        }

        edge_1_row += edge_1_step_y;
        edge_2_row += edge_2_step_y;
        edge_3_row += edge_3_step_y;
    }

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
    path_statistics.CoveredPixelCount += covered_pixel_count;
#endif
}

static void ImGui_ImplGDI_RenderCommand(
    ImU32* pixel_buffer,
    int framebuffer_width,
    int framebuffer_height,
    const ImVec4& clip_rect,
    const ImGui_ImplGDI_Texture* texture,
    const NAIVE_SWR_RENDER_COMMAND& render_command)
{
    switch (render_command.Type)
    {
    case NAIVE_SWR_RENDER_TYPE_SKIPPED:
        return;

    case NAIVE_SWR_RENDER_TYPE_SOLID_RECTANGLE:
        ImGui_ImplGDI_RenderSolidRectangleCommand(
            pixel_buffer,
            framebuffer_width,
            framebuffer_height,
            clip_rect,
            render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE:
        ImGui_ImplGDI_RenderTexturedRectangleCommand<
            NAIVE_SWR_RENDER_TYPE_ALPHA8_RECTANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                texture,
                render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_RGBA32_RECTANGLE:
        ImGui_ImplGDI_RenderTexturedRectangleCommand<
            NAIVE_SWR_RENDER_TYPE_RGBA32_RECTANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                texture,
                render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE:
        ImGui_ImplGDI_RenderTriangleCommand<
            NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                nullptr,
                render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE:
        ImGui_ImplGDI_RenderTriangleCommand<
            NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                texture,
                render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE:
        ImGui_ImplGDI_RenderTriangleCommand<
            NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                texture,
                render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_SOLID_OPAQUE_TRIANGLE:
        ImGui_ImplGDI_RenderTriangleCommand<
            NAIVE_SWR_RENDER_TYPE_SOLID_OPAQUE_TRIANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                nullptr,
                render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_SOLID_TRANSLUCENT_TRIANGLE:
        ImGui_ImplGDI_RenderTriangleCommand<
            NAIVE_SWR_RENDER_TYPE_SOLID_TRANSLUCENT_TRIANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                nullptr,
                render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_SOLID_ALPHA_GRADIENT_TRIANGLE:
        ImGui_ImplGDI_RenderTriangleCommand<
            NAIVE_SWR_RENDER_TYPE_SOLID_ALPHA_GRADIENT_TRIANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                nullptr,
                render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE:
        ImGui_ImplGDI_RenderTriangleCommand<
            NAIVE_SWR_RENDER_TYPE_ALPHA8_OPAQUE_TINT_TRIANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                texture,
                render_command);
        return;

    case NAIVE_SWR_RENDER_TYPE_ALPHA8_TRANSLUCENT_TINT_TRIANGLE:
        ImGui_ImplGDI_RenderTriangleCommand<
            NAIVE_SWR_RENDER_TYPE_ALPHA8_TRANSLUCENT_TINT_TRIANGLE>(
                pixel_buffer,
                framebuffer_width,
                framebuffer_height,
                clip_rect,
                texture,
                render_command);
        return;

    default:
        IM_ASSERT(false && "Unknown Naive Software Renderer command type.");
        return;
    }
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

    ImU32* pixel_buffer = backend_data->FramebufferPixels;

    // Clear the framebuffer with the background color (if any)
    if (clear_color)
    {
        const ImU32 red =
            (ImU32)(BYTE)(clear_color->x * 255.0f);

        const ImU32 green =
            (ImU32)(BYTE)(clear_color->y * 255.0f);

        const ImU32 blue =
            (ImU32)(BYTE)(clear_color->z * 255.0f);

        const ImU32 clear_pixel =
            0xFF000000u |
            (red << 16) |
            (green << 8) |
            blue;

        ImGui_ImplGDI_FillSpan(
            pixel_buffer,
            (size_t)fb_width * (size_t)fb_height,
            clear_pixel);
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

                const ImDrawVert* vtx_buffer = draw_list->VtxBuffer.Data + pcmd->VtxOffset;
                const ImDrawIdx* idx_buffer = draw_list->IdxBuffer.Data + pcmd->IdxOffset;

                ImGui_ImplGDI_Texture* texture = (ImGui_ImplGDI_Texture*)pcmd->GetTexID();

                for (unsigned int elem_i = 0; elem_i < pcmd->ElemCount;)
                {
                    NAIVE_SWR_RENDER_COMMAND render_command;

                    IM_ASSERT(
                        texture != nullptr &&
                        "Invalid GDI texture!");

                    if (!texture)
                        continue;

                    elem_i += naive_swr_make_render_command(
                        vtx_buffer,
                        idx_buffer + elem_i,
                        pcmd->ElemCount - elem_i,
                        texture->Format,
                        texture->Width,
                        texture->Height,
                        &render_command);

                    ImGui_ImplGDI_RenderCommand(
                        pixel_buffer,
                        fb_width,
                        fb_height,
                        pcmd->ClipRect,
                        texture,
                        render_command);
                }
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

#if defined(IMGUI_IMPL_GDI_ENABLE_STATS)
    ImGui_ImplGDI_EndStatisticsFrame();
#endif
}

#endif // #ifndef IMGUI_DISABLE
