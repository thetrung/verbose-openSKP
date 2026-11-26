# verbose-openSKP — API Cache

## Project Summary

**verbose-openSKP** (`v0.1.0`) is an idiomatic VerBose-Lang + C ABI wrapper around vendored MIT **OpenSKP** and **miniz** for natively loading SketchUp (`.skp`, legacy `v8–2020` and modern `2021+`) files into flat RAM arrays — preserving component instancing, deduplicated meshes, precomputed architectural crease edges, PBR/base materials, embedded textures, layers/tags, and saved camera pages with zero SketchUp SDK dependency.

- **Umbrella Include**: `Include "openskp.vb"` (re-exports `openskp_types.vb`, `openskp_ffi.vb`, `openskp_scene.vb`)
- **Link Libraries**: `-lverbose_openskp -lstdc++ -lm` (built via `make lib` → `build/libverbose_openskp.a`)

---

## Public Types (`src/openskp_types.vb`)

- `SkpBounds` — Scene AABB (`minX..maxZ`, `centerX..centerZ`, `sizeX..sizeZ`).
- `SkpMeshData` — Unique local-space mesh buffers (`positions`, `normals`, `uvs`, `indices`, `creasePositions`, counts, `materialIndex`, local bounds).
- `SkpInstanceData` — Placed instance metadata (`name`, `meshIndex`, `materialIndex`, `layerIndex`, `objectId`, `hidden`, 4×4 `worldMat(16)`, world AABB).
- `SkpMaterialData` — Material record (`name`, `r/g/b/a`, `metallic`, `roughness`, `textureIndex`, `doubleSided`).
- `SkpTextureData` — Embedded compressed texture blob (`data`, `mimeType`, `filename`, `byteSize`).
- `SkpLayerData` — SketchUp layer/tag (`name`, `hidden`).
- `SkpPageCamera` — Saved SketchUp scene/page camera (`name`, `eye`, `target`, `up`, `fovDeg`, `orthoHeight`, `hasCamera`, `isParallel`).

---

## Idiomatic VerBose Wrappers (`src/openskp_scene.vb`)

| Function / Sub | Description |
| :--- | :--- |
| `SkpOpen(path As String) As Pointer` | Load a `.skp` file from disk and return an opaque scene handle (or null on failure). |
| `SkpOpenBuffer(data As Pointer, size As Long) As Pointer` | Load a `.skp` scene directly from an in-memory byte buffer. |
| `SkpClose(scene As Pointer)` | Free a loaded scene handle and all associated RAM buffers (null-safe). |
| `SkpLastError() As String` | Return the last error message string from OpenSKP loading/writing. |
| `SkpCreateDemo(outPath As String) As Boolean` | Generate a multi-instance architectural demo `.skp` file at `outPath`. |
| `SkpGetBounds(scene As Pointer, ByRef outBounds As SkpBounds) As Boolean` | Populate `outBounds` with the scene's overall world-space AABB. |
| `SkpMeshCount(scene As Pointer) As Integer` | Return number of unique deduplicated meshes in the scene. |
| `SkpInstanceCount(scene As Pointer) As Integer` | Return number of placed mesh instances in the scene. |
| `SkpMaterialCount(scene As Pointer) As Integer` | Return number of materials in the scene. |
| `SkpTextureCount(scene As Pointer) As Integer` | Return number of embedded textures in the scene. |
| `SkpLayerCount(scene As Pointer) As Integer` | Return number of layers/tags in the scene. |
| `SkpPageCount(scene As Pointer) As Integer` | Return number of saved SketchUp camera pages. |
| `SkpGetMesh(scene As Pointer, idx As Integer, ByRef outMesh As SkpMeshData) As Boolean` | Fetch zero-copy mesh descriptor `idx` into `outMesh`. |
| `SkpGetInstance(scene As Pointer, idx As Integer, ByRef outInst As SkpInstanceData) As Boolean` | Fetch placed instance `idx` into `outInst`. |
| `SkpGetMaterial(scene As Pointer, idx As Integer, ByRef outMat As SkpMaterialData) As Boolean` | Fetch material `idx` into `outMat`. |
| `SkpGetTexture(scene As Pointer, idx As Integer, ByRef outTex As SkpTextureData) As Boolean` | Fetch embedded texture `idx` into `outTex`. |
| `SkpGetLayer(scene As Pointer, idx As Integer, ByRef outLayer As SkpLayerData) As Boolean` | Fetch layer/tag `idx` into `outLayer`. |
| `SkpGetPage(scene As Pointer, idx As Integer, ByRef outPage As SkpPageCamera) As Boolean` | Fetch saved camera page `idx` into `outPage`. |
| `SkpReadSingle(basePtr As Pointer, elemIdx As Integer) As Single` | Zero-copy indexed `Single` read from flat float mesh buffers (`positions`/`normals`/`uvs`/`creasePositions`). |
| `SkpReadInt(basePtr As Pointer, elemIdx As Integer) As Integer` | Zero-copy indexed `Integer` read from flat `indices` triangle buffer. |

---

## Raw C ABI Bindings (`src/openskp_ffi.vb` / `csrc/verbose_openskp.h`)

| Function / Sub | Description |
| :--- | :--- |
| `vskp_load_file(path As Pointer) As Pointer` | C ABI: load `.skp` file from path. |
| `vskp_load_buffer(data As Pointer, size As Long) As Pointer` | C ABI: load `.skp` from raw byte buffer. |
| `vskp_free(scene As Pointer)` | C ABI: release scene memory. |
| `vskp_last_error() As Pointer` | C ABI: get last error C string. |
| `vskp_get_bounds(scene As Pointer, ByRef outBounds As SkpBounds) As Integer` | C ABI: write scene bounds (`1` on success, `0` on failure). |
| `vskp_mesh_count(scene As Pointer) As Integer` | C ABI: query unique mesh count. |
| `vskp_instance_count(scene As Pointer) As Integer` | C ABI: query instance count. |
| `vskp_material_count(scene As Pointer) As Integer` | C ABI: query material count. |
| `vskp_texture_count(scene As Pointer) As Integer` | C ABI: query texture count. |
| `vskp_layer_count(scene As Pointer) As Integer` | C ABI: query layer count. |
| `vskp_page_count(scene As Pointer) As Integer` | C ABI: query camera page count. |
| `vskp_get_mesh(scene As Pointer, idx As Integer, ByRef outMesh As SkpMeshData) As Integer` | C ABI: fetch mesh struct at `idx`. |
| `vskp_get_instance(scene As Pointer, idx As Integer, ByRef outInst As SkpInstanceData) As Integer` | C ABI: fetch instance struct at `idx`. |
| `vskp_get_material(scene As Pointer, idx As Integer, ByRef outMat As SkpMaterialData) As Integer` | C ABI: fetch material struct at `idx`. |
| `vskp_get_texture(scene As Pointer, idx As Integer, ByRef outTex As SkpTextureData) As Integer` | C ABI: fetch texture struct at `idx`. |
| `vskp_get_layer(scene As Pointer, idx As Integer, ByRef outLayer As SkpLayerData) As Integer` | C ABI: fetch layer struct at `idx`. |
| `vskp_get_page(scene As Pointer, idx As Integer, ByRef outPage As SkpPageCamera) As Integer` | C ABI: fetch camera page struct at `idx`. |
| `vskp_create_demo_skp(outPath As Pointer) As Integer` | C ABI: generate demo architectural `.skp` file (`1` on success). |
| `vskp_sizeof_bounds() As Integer` | C ABI: return `sizeof(VskpBounds)` for ABI verification. |
| `vskp_sizeof_mesh_data() As Integer` | C ABI: return `sizeof(VskpMeshData)` for ABI verification. |
| `vskp_sizeof_instance_data() As Integer` | C ABI: return `sizeof(VskpInstanceData)` for ABI verification. |
| `vskp_sizeof_material_data() As Integer` | C ABI: return `sizeof(VskpMaterialData)` for ABI verification. |
| `vskp_sizeof_texture_data() As Integer` | C ABI: return `sizeof(VskpTextureData)` for ABI verification. |
| `vskp_sizeof_layer_data() As Integer` | C ABI: return `sizeof(VskpLayerData)` for ABI verification. |
| `vskp_sizeof_page_camera() As Integer` | C ABI: return `sizeof(VskpPageCamera)` for ABI verification. |
