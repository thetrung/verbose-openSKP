# verbose-openSKP

Idiomatic **VerBose-Lang (`v0.7.0+`)** C ABI wrapper around **[OpenSKP](https://github.com/iamahsanmehmood/openskp)** (MIT) for loading SketchUp (`.skp`) files natively on Linux, macOS, and Windows—with **zero SketchUp SDK or CMake dependency**.

## Features

- **Native `.skp` Loading**: Supports both legacy binary SketchUp (`v8–2020` MFC `CArchive`) and modern (`2021+` VFF/ZIP) formats via bundled MIT OpenSKP + `miniz`.
- **Preserved Component Instancing**: Loads deduplicated unique `MeshData` buffers once in RAM and exposes placed `InstanceData` transforms + precomputed world AABBs.
- **Architectural Crease Edge Extraction**: Automatically extracts boundary and hard dihedral (>28°) crease edges per unique mesh for crisp webtoon/CAD ink lineart.
- **Zero-Copy RAM Access**: Exposes contiguous `Single` (`positions`, `normals`, `uvs`, `creasePositions`) and `Integer` (`indices`) pointers directly to VerBose-Lang, plus `SkpReadSingle`/`SkpReadInt` helpers.
- **Materials, Layers & Saved Cameras**: Exposes PBR/base colors, embedded textures, layer visibility tags, and SketchUp `Page` camera bookmarks (converted to Y-up metres).
- **Built-in Demo Scene Generator**: `SkpCreateDemo(path)` writes a real multi-instance architectural cafe `.skp` file for testing and renderer demos.

## Layout

```text
verbose-openSKP/
├── src/
│   ├── openskp.vb           # Single umbrella Include entry point
│   ├── openskp_types.vb     # ABI-matching SketchUp structs (SkpBounds, SkpMeshData, ...)
│   ├── openskp_ffi.vb       # Raw C ABI Declare bindings (vskp_*)
│   └── openskp_scene.vb     # Idiomatic VerBose wrappers (SkpOpen, SkpGet*, SkpRead*)
├── csrc/
│   ├── verbose_openskp.h    # C ABI header
│   └── verbose_openskp.cpp  # C++ bridge over vendored MIT OpenSKP + miniz
├── tests/
│   └── test_openskp.vb      # ABI sizeof() & .skp round-trip test suite
├── verbose.toml             # Package manifest (for verbose-pkg / vpm)
├── CACHE.md                 # Project summary & full public API reference
└── makefile
```

## Quick Start

Install via `vpm`:

```bash
vpm add verbose-openSKP thetrung/verbose-openSKP
```

```vb
Include "openskp.vb"

Public Function main() As Integer
  Dim sc As Pointer = SkpOpen("model.skp")
  If IsNull(sc) Then
    Printf("Load failed: %s\n", SkpLastError())
    Return 1
  End If
  Printf("Meshes: %d, Instances: %d, Materials: %d\n",
    SkpMeshCount(sc), SkpInstanceCount(sc), SkpMaterialCount(sc))
  SkpClose(sc)
  Return 0
End Function
```

See **[CACHE.md](CACHE.md)** for the full list of accessible structs, wrappers, and C ABI functions.

## Build & Test

```bash
make -j$(nproc) lib    # Compile build/libverbose_openskp.a
make test              # Run VerBose ABI & .skp round-trip test suite
```
