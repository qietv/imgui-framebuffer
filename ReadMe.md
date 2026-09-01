# imgui-framebuffer

An experimental, unofficial CPU framebuffer renderer for Dear ImGui that is
functional but not yet optimized.

This project explores building lightweight applications with Dear ImGui in
environments where conventional graphics APIs or UI frameworks may be
unavailable, including Windows PE, Windows RE, Validation OS, older Windows
versions, ReactOS, Linux without Mesa, and bare metal environments. It provides
a CPU framebuffer renderer and serves as a simple renderer backend example. The
current implementation performs acceptably at resolutions up to 1024 × 768, but
is not yet optimized.

This project follows up on https://github.com/ocornut/imgui/pull/9406.

**Work In Progress**

## Project Structure

```text
  imgui     Dear ImGui mainline repository submodule
  backends  Renderer and platform backend implementations
  msbuild   C/C++ example projects built with MSBuild
  cargo     C/C++ example projects built with Cargo
```

## Platform Support

The Windows backend is currently in development. All other backends are planned,
and community contributions are welcome.

| Target | Display method | Example project | Status |
| --- | --- | --- | --- |
| Windows | GDI | MSBuild | In development |
| UEFI | GOP | MSBuild | Planned |
| Linux | `/dev/fb*` | Cargo | Planned |
| Other targets | Linear framebuffer | TBD | Planned; contributions welcome |

## TODO

### Platform Backends

- [x] Complete the Windows GDI backend.
- [ ] Add a UEFI GOP backend.
- [ ] Add a Linux `/dev/fb*` backend.
- [ ] Support other linear-framebuffer targets through community contributions.

### Performance

Follow-up suggestions from Omar Cornut, creator of Dear ImGui:

- [ ] Add a GDI rectangle bitmap-copy fast path:
  - [ ] Detect axis-aligned quads with matching texel and pixel extents.
  - [ ] Render matching quads with `BitBlt()` instead of generic triangles.
  - [ ] Verify that common text glyph quads use the new path.
- [ ] Detect white-pixel UVs and skip unnecessary texture reads.
- [ ] Add a debug mode that colorizes primitives by rendering path:
      opaque, filled, bitmap-copy, or triangle.
- [ ] Profile rounded geometry and opaque versus translucent fills.
- [ ] Study optimized CPU rasterizers and adopt applicable techniques.
- [ ] Audit and remove redundant checks from known-safe hot paths.

Original feedback, quoted verbatim:

```
Thanks for your clarification. It makes sense!

I really made the numbers worse by enabling rounding which creates lots of small
triangles which is a worst case here. I also noticed how changing WindowBG alpha
from 240 to 255 really improved performances as it can use the `::FillRect()`
path.

If one aim is to learn then by all mean, you should pursue this. It's a really
fun project to learn optimization and CPU and using a profiler. Perhaps consider
seeking some optimal references and borrow from them.

I guess the ideal rendering code would be further specialized for common case
(detect is UV coordinates point to "white" pixel = can skip reading input
pixels, vs regular rectangle copy vs slower triangle paths).

I believe the lowest hanging fruits is:

- add support for rectangle bitmap copy. most elements are text quads,
  `ImGui_ImplGDI_RenderDrawRectangle()` doesn't render them because of different
  UV. You should detect axis aligned quad where texel coordinates
  difference == rectangle size and then you can use `BitBlt()`. this will skip
  the generic triangle renderer.

Add a debug mode that colorize every shape according to their render mode
(opaque, filled, triangle) to visualize different path.

PS: you can remove unnecessary error checking (you know exactly what the inputs
are, the pointers are not null etc.).
```

## Documents

- [License](License.md)
- [Release Notes](ReleaseNotes.md)
- [Versioning](Versioning.md)
