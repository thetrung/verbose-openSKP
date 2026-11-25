#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VskpBounds {
  float minX, minY, minZ;
  float maxX, maxY, maxZ;
  float centerX, centerY, centerZ;
  float sizeX, sizeY, sizeZ;
} VskpBounds;

typedef struct VskpMeshData {
  const float* positions;       /* vertexCount * 3 (x, y, z) in local metres, Y-up */
  const float* normals;         /* vertexCount * 3 (nx, ny, nz) */
  const float* uvs;             /* vertexCount * 2 (u, v) */
  const int32_t* indices;       /* indexCount (triangles: indexCount / 3) */
  const float* creasePositions; /* creaseEdgeCount * 6 (ax,ay,az, bx,by,bz) */
  int32_t vertexCount;
  int32_t indexCount;
  int32_t creaseEdgeCount;
  int32_t materialIndex;
  float minX, minY, minZ;
  float maxX, maxY, maxZ;
} VskpMeshData;

typedef struct VskpInstanceData {
  const char* name;
  int32_t meshIndex;
  int32_t materialIndex;
  int32_t layerIndex;
  int32_t objectId;
  int32_t hidden;
  int32_t pad0;
  float worldMat[16]; /* column-major 4x4 transform matrix (metres, Y-up) */
  float worldMinX, worldMinY, worldMinZ;
  float worldMaxX, worldMaxY, worldMaxZ;
} VskpInstanceData;

typedef struct VskpMaterialData {
  const char* name;
  float r, g, b, a;
  float metallic;
  float roughness;
  int32_t textureIndex; /* -1 if untextured */
  int32_t doubleSided;
} VskpMaterialData;

typedef struct VskpTextureData {
  const uint8_t* data;
  const char* mimeType;
  const char* filename;
  int32_t byteSize;
  int32_t pad0;
} VskpTextureData;

typedef struct VskpLayerData {
  const char* name;
  int32_t hidden;
  int32_t pad0;
} VskpLayerData;

typedef struct VskpPageCamera {
  const char* name;
  float eyeX, eyeY, eyeZ;
  float targetX, targetY, targetZ;
  float upX, upY, upZ;
  float fovDeg;
  float orthoHeight;
  int32_t hasCamera;
  int32_t isParallel;
  int32_t pad0;
} VskpPageCamera;

void* vskp_load_file(const char* path);
void* vskp_load_buffer(const uint8_t* data, int64_t size);
void vskp_free(void* scene);
const char* vskp_last_error(void);

int32_t vskp_get_bounds(void* scene, VskpBounds* outBounds);
int32_t vskp_mesh_count(void* scene);
int32_t vskp_instance_count(void* scene);
int32_t vskp_material_count(void* scene);
int32_t vskp_texture_count(void* scene);
int32_t vskp_layer_count(void* scene);
int32_t vskp_page_count(void* scene);

int32_t vskp_get_mesh(void* scene, int32_t idx, VskpMeshData* outMesh);
int32_t vskp_get_instance(void* scene, int32_t idx, VskpInstanceData* outInst);
int32_t vskp_get_material(void* scene, int32_t idx, VskpMaterialData* outMat);
int32_t vskp_get_texture(void* scene, int32_t idx, VskpTextureData* outTex);
int32_t vskp_get_layer(void* scene, int32_t idx, VskpLayerData* outLayer);
int32_t vskp_get_page(void* scene, int32_t idx, VskpPageCamera* outPage);

/* Generates a real architectural webtoon demo .skp file using OpenSKP's writer. */
int32_t vskp_create_demo_skp(const char* outPath);

/* ABI sizeof verification helpers */
int32_t vskp_sizeof_bounds(void);
int32_t vskp_sizeof_mesh_data(void);
int32_t vskp_sizeof_instance_data(void);
int32_t vskp_sizeof_material_data(void);
int32_t vskp_sizeof_texture_data(void);
int32_t vskp_sizeof_layer_data(void);
int32_t vskp_sizeof_page_camera(void);

#ifdef __cplusplus
}
#endif
