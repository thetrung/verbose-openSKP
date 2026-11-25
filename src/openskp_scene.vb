' verbose-openSKP — Idiomatic VerBose Helpers & Zero-Copy Buffer Accessors

Include "openskp_ffi.vb"

Public Function SkpOpen(path As String) As Pointer
  Return vskp_load_file(path)
End Function

Public Function SkpOpenBuffer(data As Pointer, size As Long) As Pointer
  Return vskp_load_buffer(data, size)
End Function

Public Sub SkpClose(scene As Pointer)
  If Not IsNull(scene) Then
    vskp_free(scene)
  End If
End Sub

Public Function SkpLastError() As String
  Return vskp_last_error()
End Function

Public Function SkpCreateDemo(outPath As String) As Boolean
  Return vskp_create_demo_skp(outPath) <> 0
End Function

Public Function SkpGetBounds(scene As Pointer, ByRef outBounds As SkpBounds) As Boolean
  Return vskp_get_bounds(scene, ByRef outBounds) <> 0
End Function

Public Function SkpMeshCount(scene As Pointer) As Integer
  Return vskp_mesh_count(scene)
End Function

Public Function SkpInstanceCount(scene As Pointer) As Integer
  Return vskp_instance_count(scene)
End Function

Public Function SkpMaterialCount(scene As Pointer) As Integer
  Return vskp_material_count(scene)
End Function

Public Function SkpTextureCount(scene As Pointer) As Integer
  Return vskp_texture_count(scene)
End Function

Public Function SkpLayerCount(scene As Pointer) As Integer
  Return vskp_layer_count(scene)
End Function

Public Function SkpPageCount(scene As Pointer) As Integer
  Return vskp_page_count(scene)
End Function

Public Function SkpGetMesh(scene As Pointer, idx As Integer, ByRef outMesh As SkpMeshData) As Boolean
  Return vskp_get_mesh(scene, idx, ByRef outMesh) <> 0
End Function

Public Function SkpGetInstance(scene As Pointer, idx As Integer, ByRef outInst As SkpInstanceData) As Boolean
  Return vskp_get_instance(scene, idx, ByRef outInst) <> 0
End Function

Public Function SkpGetMaterial(scene As Pointer, idx As Integer, ByRef outMat As SkpMaterialData) As Boolean
  Return vskp_get_material(scene, idx, ByRef outMat) <> 0
End Function

Public Function SkpGetTexture(scene As Pointer, idx As Integer, ByRef outTex As SkpTextureData) As Boolean
  Return vskp_get_texture(scene, idx, ByRef outTex) <> 0
End Function

Public Function SkpGetLayer(scene As Pointer, idx As Integer, ByRef outLayer As SkpLayerData) As Boolean
  Return vskp_get_layer(scene, idx, ByRef outLayer) <> 0
End Function

Public Function SkpGetPage(scene As Pointer, idx As Integer, ByRef outPage As SkpPageCamera) As Boolean
  Return vskp_get_page(scene, idx, ByRef outPage) <> 0
End Function

' Zero-copy typed element accessors over SkpMeshData RAM buffers
Public Function SkpReadSingle(basePtr As Pointer, elemIdx As Integer) As Single
  Return PtrRead(PtrAdd(basePtr, elemIdx * 4), Single)
End Function

Public Function SkpReadInt(basePtr As Pointer, elemIdx As Integer) As Integer
  Return PtrRead(PtrAdd(basePtr, elemIdx * 4), Integer)
End Function
