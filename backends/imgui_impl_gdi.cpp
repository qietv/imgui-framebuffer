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

    const bool texture_is_white =
        naive_swr_is_white_texture_coordinate(
            vertices[0]->uv, white_texture_coordinate) &&
        naive_swr_is_white_texture_coordinate(
            vertices[1]->uv, white_texture_coordinate) &&
        naive_swr_is_white_texture_coordinate(
            vertices[2]->uv, white_texture_coordinate);

    if (texture_is_white)
    {
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

    render_command->Type = texture_format == ImTextureFormat_Alpha8
        ? NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE
        : NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE;

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

namespace
{
    template<typename Type>
    Type InternalClamp(Type Value, Type Minimum, Type Maximum)
    {
        return ((Value < Minimum)
            ? Minimum
            : ((Value > Maximum) ? Maximum : Value));
    }
}

// Texture data
struct ImGui_ImplGDI_Texture
{
    int Width;
    int Height;
    ImU32* Pixels;

    ImGui_ImplGDI_Texture()
    {
        memset((void*)this, 0, sizeof(*this));
    }
};

static void ImGui_ImplGDI_UpdateTexture(ImTextureData* tex)
{
    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create and upload new texture to graphics system
        //IMGUI_DEBUG_LOG("UpdateTexture #%03d: WantCreate %dx%d\n", tex->UniqueID, tex->Width, tex->Height);
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);

        ImGui_ImplGDI_Texture* texture = IM_NEW(ImGui_ImplGDI_Texture)();
        IM_ASSERT(texture != nullptr && "Failed to allocate memory for texture data!");
        texture->Width = tex->Width;
        texture->Height = tex->Height;
        texture->Pixels = (ImU32*)IM_ALLOC(texture->Width * texture->Height * sizeof(ImU32));
        IM_ASSERT(texture->Pixels != nullptr && "Failed to allocate memory for texture pixels!");
        memcpy(texture->Pixels, tex->GetPixels(), texture->Width * texture->Height * sizeof(ImU32));
        tex->SetTexID((ImTextureID)texture);

        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        // Update selected blocks. We only ever write to textures regions which have never been used before!
        // This backend choose to use tex->Updates[] but you can use tex->UpdateRect to upload a single region.
        ImGui_ImplGDI_Texture* texture = (ImGui_ImplGDI_Texture*)tex->GetTexID();
        IM_ASSERT(texture != nullptr && "Trying to update a texture that was not created or already destroyed!");
        IM_ASSERT(texture->Pixels != nullptr && "Trying to update a texture that was not created or already destroyed!");
        IM_FREE(texture->Pixels);
        texture->Width = tex->Width;
        texture->Height = tex->Height;
        texture->Pixels = (ImU32*)IM_ALLOC(texture->Width * texture->Height * sizeof(ImU32));
        IM_ASSERT(texture->Pixels != nullptr && "Failed to allocate memory for texture pixels!");
        memcpy(texture->Pixels, tex->GetPixels(), texture->Width * texture->Height * sizeof(ImU32));

        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy)
    {
        ImGui_ImplGDI_Texture* texture = (ImGui_ImplGDI_Texture*)tex->GetTexID();
        IM_ASSERT(texture != nullptr && "Trying to destroy a texture that was not created or already destroyed!");
        IM_ASSERT(texture->Pixels != nullptr && "Trying to destroy a texture that was not created or already destroyed!");
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

    // Setup backend capabilities flags
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    return true;
}

IMGUI_IMPL_API void ImGui_ImplGDI_Shutdown()
{
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
    {
        if (tex->RefCount == 1)
        {
            tex->SetStatus(ImTextureStatus_WantDestroy);
            ImGui_ImplGDI_UpdateTexture(tex);
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
}

IMGUI_IMPL_API void ImGui_ImplGDI_NewFrame()
{

}

static inline int ImGui_ImplGDI_Mul255(int a, int b)
{
    return (a * b + 127) / 255;
}

// Equivalent to (value + 127) / 255 for value in [0, 255 * 255],
// but expressed using operations that are easier to vectorize.
static inline ImU32 ImGui_ImplGDI_Div255Rounded(ImU32 value)
{
    value += 128u;
    return (value + (value >> 8)) >> 8;
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

    x0 = ::InternalClamp(x0, 0, framebuffer_width);
    y0 = ::InternalClamp(y0, 0, framebuffer_height);
    x1 = ::InternalClamp(x1, 0, framebuffer_width);
    y1 = ::InternalClamp(y1, 0, framebuffer_height);

    return x0 < x1 && y0 < y1;
}

static void ImGui_ImplGDI_RenderSolidRectangleCommand(
    HDC hdc,
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

    RECT rectangle_handle;
    rectangle_handle.left = x0;
    rectangle_handle.top = y0;
    rectangle_handle.right = x1;
    rectangle_handle.bottom = y1;

    if (color.Alpha == 255)
    {
        const COLORREF previous_color = ::SetDCBrushColor(
            hdc,
            RGB(color.Red, color.Green, color.Blue));

        ::FillRect(
            hdc,
            &rectangle_handle,
            (HBRUSH)::GetStockObject(DC_BRUSH));

        ::SetDCBrushColor(hdc, previous_color);
        return;
    }

    IM_ASSERT(pixel_buffer != nullptr);

    const ImU32 source_alpha = color.Alpha;
    const ImU32 inverse_alpha = 255u - source_alpha;

    const ImU32 source_red_alpha =
        (ImU32)color.Red * source_alpha;

    const ImU32 source_green_alpha =
        (ImU32)color.Green * source_alpha;

    const ImU32 source_blue_alpha =
        (ImU32)color.Blue * source_alpha;

    const int span_width = x1 - x0;

    ImU32* destination_row =
        pixel_buffer + (size_t)y0 * framebuffer_width + x0;

    for (int y = y0; y < y1; ++y)
    {
        for (int x = 0; x < span_width; ++x)
        {
            const ImU32 destination_color = destination_row[x];

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

            destination_row[x] =
                0xFF000000u |
                (output_red << 16) |
                (output_green << 8) |
                output_blue;
        }

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

    float texture_y =
        rectangle.TexturePosition.Y +
        (((float)y0 + 0.5f) - rectangle.Position.Y) *
        texture_step_y;

    const uint8_t* texture_bytes =
        reinterpret_cast<const uint8_t*>(texture->Pixels);

    for (int y = y0; y < y1; ++y)
    {
        int texture_y_integer = (int)texture_y;

        texture_y_integer = ::InternalClamp(
            texture_y_integer,
            0,
            texture->Height - 1);

        float texture_x = texture_x_start;

        for (int x = x0; x < x1; ++x)
        {
            int texture_x_integer = (int)texture_x;

            texture_x_integer = ::InternalClamp(
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
        RenderType == NAIVE_SWR_RENDER_TYPE_RGBA32_TRIANGLE);

    if (!pixel_buffer ||
        framebuffer_width <= 0 ||
        framebuffer_height <= 0)
    {
        return;
    }

    if (RenderType != NAIVE_SWR_RENDER_TYPE_SOLID_TRIANGLE)
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

    const float inverse_area = 1.0f / area;

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

    const float edge_1_step_x = point_3.Y - point_2.Y;
    const float edge_2_step_x = point_1.Y - point_3.Y;
    const float edge_3_step_x = point_2.Y - point_1.Y;

    const float edge_1_step_y = -(point_3.X - point_2.X);
    const float edge_2_step_y = -(point_1.X - point_3.X);
    const float edge_3_step_y = -(point_2.X - point_1.X);

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

    const NAIVE_SWR_COLOR& color_1 = triangle.Colors[0];
    const NAIVE_SWR_COLOR& color_2 = triangle.Colors[1];
    const NAIVE_SWR_COLOR& color_3 = triangle.Colors[2];

    const uint8_t* texture_bytes =
        texture
        ? reinterpret_cast<const uint8_t*>(texture->Pixels)
        : nullptr;

    for (int y = y0; y < y1; ++y)
    {
        float edge_1 = edge_1_row;
        float edge_2 = edge_2_row;
        float edge_3 = edge_3_row;

        for (int x = x0; x < x1; ++x)
        {
            const float signed_edge_1 = edge_1 * area;
            const float signed_edge_2 = edge_2 * area;
            const float signed_edge_3 = edge_3 * area;

            const bool inside =
                signed_edge_1 >= 0.0f &&
                signed_edge_2 >= 0.0f &&
                signed_edge_3 >= 0.0f &&
                (signed_edge_1 != 0.0f || top_left_1) &&
                (signed_edge_2 != 0.0f || top_left_2) &&
                (signed_edge_3 != 0.0f || top_left_3);

            if (inside)
            {
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
                    ::InternalClamp(vertex_red, 0, 255);

                vertex_green =
                    ::InternalClamp(vertex_green, 0, 255);

                vertex_blue =
                    ::InternalClamp(vertex_blue, 0, 255);

                vertex_alpha =
                    ::InternalClamp(vertex_alpha, 0, 255);

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

                    texture_x = ::InternalClamp(
                        texture_x,
                        0,
                        texture->Width - 1);

                    texture_y = ::InternalClamp(
                        texture_y,
                        0,
                        texture->Height - 1);

                    const size_t texture_index =
                        (size_t)texture_y * texture->Width +
                        texture_x;

                    if (RenderType ==
                        NAIVE_SWR_RENDER_TYPE_ALPHA8_TRIANGLE)
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
}

static void ImGui_ImplGDI_RenderCommand(
    HDC hdc,
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
            hdc,
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

    default:
        IM_ASSERT(false && "Unknown Naive Software Renderer command type.");
        return;
    }
}

IMGUI_IMPL_API void ImGui_ImplGDI_RenderDrawData(ImDrawData* draw_data, void* fb_dev_ctx_handle, ImVec4* clear_color)
{
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    if (fb_width == 0 || fb_height == 0)
        return;

    HDC hdc = (HDC)fb_dev_ctx_handle;
    IM_ASSERT(hdc != nullptr && "Invalid framebuffer device context!");

    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplGDI_UpdateTexture(tex);

    // Setup desired GDI state

    ImU32* pixel_buffer = nullptr;
    {
        BITMAPINFO bitmap_info = {};
        bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_info.bmiHeader.biWidth = fb_width;
        bitmap_info.bmiHeader.biHeight = -fb_height;
        bitmap_info.bmiHeader.biPlanes = 1;
        bitmap_info.bmiHeader.biBitCount = 32;
        bitmap_info.bmiHeader.biCompression = BI_RGB;
        HBITMAP bitmap = ::CreateDIBSection(hdc, &bitmap_info, DIB_RGB_COLORS, (void**)&pixel_buffer, nullptr, 0);
        if (bitmap)
        {
            ::DeleteObject(::SelectObject(hdc, bitmap));
            ::DeleteObject(bitmap);
        }
    }
    IM_ASSERT(pixel_buffer != nullptr && "Failed to create DIB section for rendering!");

    // Clear the framebuffer with the background color (if any)
    if (clear_color)
    {
        COLORREF previous_color = ::SetDCBrushColor(hdc, RGB((BYTE)(clear_color->x * 255.0f), (BYTE)(clear_color->y * 255.0f), (BYTE)(clear_color->z * 255.0f)));
        RECT rect = { 0, 0, fb_width, fb_height };
        ::FillRect(hdc, &rect, (HBRUSH)::GetStockObject(DC_BRUSH));
        ::SetDCBrushColor(hdc, previous_color);
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
                    elem_i += naive_swr_make_render_command(
                        vtx_buffer,
                        idx_buffer + elem_i,
                        pcmd->ElemCount - elem_i,
                        ImTextureFormat_RGBA32,
                        texture->Width,
                        texture->Height,
                        &render_command);

                    ImGui_ImplGDI_RenderCommand(
                        hdc,
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
}

#endif // #ifndef IMGUI_DISABLE
