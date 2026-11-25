' verbose-openSKP — ABI & End-to-End .skp Round-Trip Test Suite

Include "openskp.vb"

Declare Function unlink(path As Pointer) As Integer

Test "C ABI struct sizes match VerBose SizeOf"
  Assert SizeOf(SkpBounds) = vskp_sizeof_bounds(), "SkpBounds sizeof mismatch"
  Assert SizeOf(SkpMeshData) = vskp_sizeof_mesh_data(), "SkpMeshData sizeof mismatch"
  Assert SizeOf(SkpInstanceData) = vskp_sizeof_instance_data(), "SkpInstanceData sizeof mismatch"
  Assert SizeOf(SkpMaterialData) = vskp_sizeof_material_data(), "SkpMaterialData sizeof mismatch"
  Assert SizeOf(SkpTextureData) = vskp_sizeof_texture_data(), "SkpTextureData sizeof mismatch"
  Assert SizeOf(SkpLayerData) = vskp_sizeof_layer_data(), "SkpLayerData sizeof mismatch"
  Assert SizeOf(SkpPageCamera) = vskp_sizeof_page_camera(), "SkpPageCamera sizeof mismatch"
End Test

Test "Create and parse real instanced .skp demo scene"
  Dim demoPath As String = "/tmp/verbose_openskp_test_cafe.skp"
  Assert SkpCreateDemo(demoPath), "Failed to create demo .skp file"

  Dim sc As Pointer = SkpOpen(demoPath)
  Assert Not IsNull(sc), "Failed to open generated .skp file"

  Dim b As SkpBounds
  Assert SkpGetBounds(sc, ByRef b), "Expected valid scene bounds"
  Assert b.sizeX > 5.0, "Cafe scene width should exceed 5m"
  Assert b.sizeY > 2.5, "Cafe scene height should exceed 2.5m"
  Assert b.sizeZ > 4.0, "Cafe scene depth should exceed 4m"

  Dim meshCnt As Integer = SkpMeshCount(sc)
  Dim instCnt As Integer = SkpInstanceCount(sc)
  Dim matCnt As Integer = SkpMaterialCount(sc)
  Dim lyrCnt As Integer = SkpLayerCount(sc)
  Assert meshCnt >= 5, "Should have deduplicated mesh resources"
  Assert instCnt > meshCnt, "Reused tables/chairs/pillars should produce more instances than unique meshes"
  Assert matCnt >= 6, "Should have cafe palette materials"
  Assert lyrCnt >= 3, "Should have Architecture/Furniture/Props layers"

  Dim totalTris As Integer = 0
  Dim totalCreases As Integer = 0
  For i = 0 To meshCnt - 1 Do
    Dim m As SkpMeshData
    Assert SkpGetMesh(sc, i, ByRef m), "SkpGetMesh should succeed"
    Assert m.vertexCount >= 3, "Mesh must have vertices"
    Assert (m.indexCount >= 3) And ((m.indexCount Mod 3) = 0), "Mesh indices must be triangles"
    Assert Not IsNull(m.positions), "Mesh positions pointer must be non-null"
    Assert Not IsNull(m.normals), "Mesh normals pointer must be non-null"
    Assert Not IsNull(m.indices), "Mesh indices pointer must be non-null"
    totalTris = totalTris + (m.indexCount / 3)
    totalCreases = totalCreases + m.creaseEdgeCount
  End For
  Assert totalTris > 100, "Demo scene should contain >100 unique triangles"
  Assert totalCreases > 50, "Demo scene should extract architectural crease edges"

  ' Verify instance transforms and world AABBs
  For j = 0 To instCnt - 1 Do
    Dim inst As SkpInstanceData
    Assert SkpGetInstance(sc, j, ByRef inst), "SkpGetInstance should succeed"
    Assert (inst.meshIndex >= 0) And (inst.meshIndex < meshCnt), "Instance meshIndex in bounds"
    Assert inst.objectId > 0, "Instance objectId must be positive"
    Assert inst.worldMaxX >= inst.worldMinX, "World AABB X ordered"
    Assert inst.worldMaxY >= inst.worldMinY, "World AABB Y ordered"
    Assert inst.worldMaxZ >= inst.worldMinZ, "World AABB Z ordered"
  End For

  ' Verify materials
  For k = 0 To matCnt - 1 Do
    Dim mat As SkpMaterialData
    Assert SkpGetMaterial(sc, k, ByRef mat), "SkpGetMaterial should succeed"
    Assert (mat.r >= 0.0) And (mat.r <= 1.0), "Material R in 0..1"
    Assert (mat.g >= 0.0) And (mat.g <= 1.0), "Material G in 0..1"
    Assert (mat.b >= 0.0) And (mat.b <= 1.0), "Material B in 0..1"
  End For

  SkpClose(sc)
  unlink(demoPath)
End Test

Test "Invalid file path returns null and sets diagnostic"
  Dim sc As Pointer = SkpOpen("/tmp/does_not_exist_12345.skp")
  Assert IsNull(sc), "Missing .skp should return null"
  Dim err As String = SkpLastError()
  Assert Not IsNull(err), "Error message should be non-null"
End Test
