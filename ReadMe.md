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

**Work In Progress**

## Project Structure

```text
  imgui             Dear ImGui mainline repository submodule
  backends          Renderer and platform backend implementations
  projects-msbuild  MSBuild-based example projects
  projects-cargo    Cargo-based example projects
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

## Documents

- [License](License.md)
- [Release Notes](ReleaseNotes.md)
- [Versioning](Versioning.md)
