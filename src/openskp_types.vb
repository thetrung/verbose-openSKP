' verbose-openSKP — ABI-Verified C Interop Types for SketchUp (.skp) Scenes

Public Structure SkpBounds
  Dim minX, minY, minZ As Single
  Dim maxX, maxY, maxZ As Single
  Dim centerX, centerY, centerZ As Single
  Dim sizeX, sizeY, sizeZ As Single
End Structure

Public Structure SkpMeshData
  Dim positions, normals, uvs, indices, creasePositions As Pointer
  Dim vertexCount, indexCount, creaseEdgeCount, materialIndex As Integer
  Dim minX, minY, minZ As Single
  Dim maxX, maxY, maxZ As Single
End Structure

Public Structure SkpInstanceData
  Dim name As Pointer
  Dim meshIndex, materialIndex, layerIndex, objectId, hidden, pad0 As Integer
  Dim worldMat(16) As Single
  Dim worldMinX, worldMinY, worldMinZ As Single
  Dim worldMaxX, worldMaxY, worldMaxZ As Single
End Structure

Public Structure SkpMaterialData
  Dim name As Pointer
  Dim r, g, b, a, metallic, roughness As Single
  Dim textureIndex, doubleSided As Integer
End Structure

Public Structure SkpTextureData
  Dim data, mimeType, filename As Pointer
  Dim byteSize, pad0 As Integer
End Structure

Public Structure SkpLayerData
  Dim name As Pointer
  Dim hidden, pad0 As Integer
End Structure

Public Structure SkpPageCamera
  Dim name As Pointer
  Dim eyeX, eyeY, eyeZ As Single
  Dim targetX, targetY, targetZ As Single
  Dim upX, upY, upZ As Single
  Dim fovDeg, orthoHeight As Single
  Dim hasCamera, isParallel, pad0 As Integer
End Structure
