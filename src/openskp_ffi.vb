' verbose-openSKP — Native C ABI FFI Declarations

Include "openskp_types.vb"

Declare Function vskp_load_file(path As Pointer) As Pointer
Declare Function vskp_load_buffer(data As Pointer, size As Long) As Pointer
Declare Sub vskp_free(scene As Pointer)
Declare Function vskp_last_error() As Pointer

Declare Function vskp_get_bounds(scene As Pointer, ByRef outBounds As SkpBounds) As Integer
Declare Function vskp_mesh_count(scene As Pointer) As Integer
Declare Function vskp_instance_count(scene As Pointer) As Integer
Declare Function vskp_material_count(scene As Pointer) As Integer
Declare Function vskp_texture_count(scene As Pointer) As Integer
Declare Function vskp_layer_count(scene As Pointer) As Integer
Declare Function vskp_page_count(scene As Pointer) As Integer

Declare Function vskp_get_mesh(scene As Pointer, idx As Integer, ByRef outMesh As SkpMeshData) As Integer
Declare Function vskp_get_instance(scene As Pointer, idx As Integer, ByRef outInst As SkpInstanceData) As Integer
Declare Function vskp_get_material(scene As Pointer, idx As Integer, ByRef outMat As SkpMaterialData) As Integer
Declare Function vskp_get_texture(scene As Pointer, idx As Integer, ByRef outTex As SkpTextureData) As Integer
Declare Function vskp_get_layer(scene As Pointer, idx As Integer, ByRef outLayer As SkpLayerData) As Integer
Declare Function vskp_get_page(scene As Pointer, idx As Integer, ByRef outPage As SkpPageCamera) As Integer

Declare Function vskp_create_demo_skp(outPath As Pointer) As Integer

Declare Function vskp_sizeof_bounds() As Integer
Declare Function vskp_sizeof_mesh_data() As Integer
Declare Function vskp_sizeof_instance_data() As Integer
Declare Function vskp_sizeof_material_data() As Integer
Declare Function vskp_sizeof_texture_data() As Integer
Declare Function vskp_sizeof_layer_data() As Integer
Declare Function vskp_sizeof_page_camera() As Integer
