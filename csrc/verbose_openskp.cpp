#include "verbose_openskp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <openskp/create.hpp>
#include <openskp/parser.hpp>

namespace {

thread_local std::string g_last_error;

void set_error(const std::string& msg) {
  g_last_error = msg;
}

struct FlattenedMesh {
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> uvs;
  std::vector<int32_t> indices;
  std::vector<float> creasePositions;
  int32_t materialIndex = 0;
  float minX = 0, minY = 0, minZ = 0;
  float maxX = 0, maxY = 0, maxZ = 0;
};

struct FlattenedInstance {
  std::string name;
  int32_t meshIndex = 0;
  int32_t materialIndex = 0;
  int32_t layerIndex = 0;
  int32_t objectId = 0;
  int32_t hidden = 0;
  float worldMat[16] = {};
  float worldMinX = 0, worldMinY = 0, worldMinZ = 0;
  float worldMaxX = 0, worldMaxY = 0, worldMaxZ = 0;
};

struct FlattenedLayer {
  std::string name;
  int32_t hidden = 0;
};

struct FlattenedPage {
  std::string name;
  float eyeX = 0, eyeY = 0, eyeZ = 0;
  float targetX = 0, targetY = 0, targetZ = 0;
  float upX = 0, upY = 1, upZ = 0;
  float fovDeg = 35.0f;
  float orthoHeight = 0.0f;
  int32_t hasCamera = 0;
  int32_t isParallel = 0;
};

struct VskpScene {
  openskp::InstancedScene raw;
  VskpBounds bounds{};
  std::vector<FlattenedMesh> meshes;
  std::vector<FlattenedInstance> instances;
  std::vector<FlattenedLayer> layers;
  std::vector<FlattenedPage> pages;
  std::unordered_map<std::string, int32_t> layerMap;
};

/* Column-major 4x4 matrix multiply: out = A * B */
void mat4_mul(const double a[16], const double b[16], double out[16]) {
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      double sum = 0.0;
      for (int k = 0; k < 4; ++k) {
        sum += a[k * 4 + row] * b[col * 4 + k];
      }
      out[col * 4 + row] = sum;
    }
  }
}

void transform_point(const float m[16], float x, float y, float z, float& ox, float& oy, float& oz) {
  ox = m[0] * x + m[4] * y + m[8] * z + m[12];
  oy = m[1] * x + m[5] * y + m[9] * z + m[13];
  oz = m[2] * x + m[6] * y + m[10] * z + m[14];
}

struct EdgeKey {
  int64_t a, b;
  bool operator==(const EdgeKey& o) const noexcept { return a == o.a && b == o.b; }
};

struct EdgeKeyHash {
  size_t operator()(const EdgeKey& k) const noexcept {
    uint64_t h = static_cast<uint64_t>(k.a) * 0x9E3779B97F4A7C15ULL;
    h ^= static_cast<uint64_t>(k.b) + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    return static_cast<size_t>(h);
  }
};

struct EdgeAdj {
  float ax, ay, az;
  float bx, by, bz;
  float nx0, ny0, nz0;
  float nx1, ny1, nz1;
  int count = 0;
};

/* Quantize position to 0.5mm grid so split-UV vertices share edges cleanly. */
int64_t quantize_pos(float x, float y, float z) {
  int64_t qx = static_cast<int64_t>(std::llround(x * 2000.0f)) & 0x1FFFFF;
  int64_t qy = static_cast<int64_t>(std::llround(y * 2000.0f)) & 0x1FFFFF;
  int64_t qz = static_cast<int64_t>(std::llround(z * 2000.0f)) & 0x1FFFFF;
  return (qx << 42) | (qy << 21) | qz;
}

void extract_crease_edges(FlattenedMesh& mesh) {
  const size_t triCount = mesh.indices.size() / 3;
  if (triCount == 0) return;

  std::unordered_map<EdgeKey, EdgeAdj, EdgeKeyHash> edges;
  edges.reserve(triCount * 2);

  for (size_t t = 0; t < triCount; ++t) {
    int32_t i0 = mesh.indices[t * 3 + 0];
    int32_t i1 = mesh.indices[t * 3 + 1];
    int32_t i2 = mesh.indices[t * 3 + 2];
    if (i0 < 0 || i1 < 0 || i2 < 0) continue;

    float x0 = mesh.positions[i0 * 3 + 0], y0 = mesh.positions[i0 * 3 + 1], z0 = mesh.positions[i0 * 3 + 2];
    float x1 = mesh.positions[i1 * 3 + 0], y1 = mesh.positions[i1 * 3 + 1], z1 = mesh.positions[i1 * 3 + 2];
    float x2 = mesh.positions[i2 * 3 + 0], y2 = mesh.positions[i2 * 3 + 1], z2 = mesh.positions[i2 * 3 + 2];

    float ux = x1 - x0, uy = y1 - y0, uz = z1 - z0;
    float vx = x2 - x0, vy = y2 - y0, vz = z2 - z0;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 1e-8f) continue;
    nx /= len;
    ny /= len;
    nz /= len;

    const float px[3] = {x0, x1, x2};
    const float py[3] = {y0, y1, y2};
    const float pz[3] = {z0, z1, z2};
    const int64_t qk[3] = {quantize_pos(x0, y0, z0), quantize_pos(x1, y1, z1), quantize_pos(x2, y2, z2)};

    for (int e = 0; e < 3; ++e) {
      int n = (e + 1) % 3;
      int64_t ka = qk[e], kb = qk[n];
      if (ka == kb) continue;
      EdgeKey key = (ka < kb) ? EdgeKey{ka, kb} : EdgeKey{kb, ka};
      auto it = edges.find(key);
      if (it == edges.end()) {
        EdgeAdj adj{};
        adj.ax = px[e]; adj.ay = py[e]; adj.az = pz[e];
        adj.bx = px[n]; adj.by = py[n]; adj.bz = pz[n];
        adj.nx0 = nx; adj.ny0 = ny; adj.nz0 = nz;
        adj.count = 1;
        edges.emplace(key, adj);
      } else {
        if (it->second.count == 1) {
          it->second.nx1 = nx;
          it->second.ny1 = ny;
          it->second.nz1 = nz;
        }
        it->second.count += 1;
      }
    }
  }

  /* Emit boundary edges (count == 1) and hard architectural creases (dihedral > ~28 deg: cos < 0.88). */
  mesh.creasePositions.reserve(edges.size() * 2);
  for (const auto& kv : edges) {
    const EdgeAdj& adj = kv.second;
    bool isCrease = (adj.count == 1) || (adj.count > 2);
    if (!isCrease && adj.count == 2) {
      float dot = adj.nx0 * adj.nx1 + adj.ny0 * adj.ny1 + adj.nz0 * adj.nz1;
      if (dot < 0.88f) isCrease = true;
    }
    if (isCrease) {
      mesh.creasePositions.push_back(adj.ax);
      mesh.creasePositions.push_back(adj.ay);
      mesh.creasePositions.push_back(adj.az);
      mesh.creasePositions.push_back(adj.bx);
      mesh.creasePositions.push_back(adj.by);
      mesh.creasePositions.push_back(adj.bz);
    }
  }
}

int32_t get_or_add_layer(VskpScene& sc, const std::string& name) {
  std::string key = name.empty() ? "Layer0" : name;
  auto it = sc.layerMap.find(key);
  if (it != sc.layerMap.end()) return it->second;
  int32_t idx = static_cast<int32_t>(sc.layers.size());
  FlattenedLayer layer;
  layer.name = key;
  auto hid = sc.raw.layer_hidden.find(key);
  layer.hidden = (hid != sc.raw.layer_hidden.end() && hid->second) ? 1 : 0;
  sc.layers.push_back(std::move(layer));
  sc.layerMap.emplace(key, idx);
  return idx;
}

void walk_hierarchy(VskpScene& sc,
                    const openskp::InstancedNode& node,
                    const double parentWorld[16],
                    const std::unordered_map<std::string, std::pair<int32_t, int32_t>>& resRanges,
                    int32_t& nextObjectId) {
  double world[16];
  mat4_mul(parentWorld, node.matrix.data(), world);

  int32_t layerIdx = get_or_add_layer(sc, node.layer);
  int32_t layerHidden = sc.layers[layerIdx].hidden;

  if (node.mesh_resource_id.has_value()) {
    auto it = resRanges.find(*node.mesh_resource_id);
    if (it != resRanges.end()) {
      int32_t startIdx = it->second.first;
      int32_t count = it->second.second;
      int32_t objId = nextObjectId++;
      for (int32_t m = 0; m < count; ++m) {
        int32_t meshIdx = startIdx + m;
        const FlattenedMesh& fm = sc.meshes[meshIdx];
        if (fm.indices.empty()) continue;

        FlattenedInstance inst;
        inst.name = node.name.empty() ? node.definition_name : node.name;
        if (inst.name.empty()) inst.name = "Mesh_" + std::to_string(objId);
        inst.meshIndex = meshIdx;
        inst.materialIndex = fm.materialIndex;
        inst.layerIndex = layerIdx;
        inst.objectId = objId;
        inst.hidden = layerHidden;
        for (int k = 0; k < 16; ++k) {
          inst.worldMat[k] = static_cast<float>(world[k]);
        }

        /* Transform all 8 corners of the local AABB to get exact world AABB. */
        float wminX = std::numeric_limits<float>::infinity();
        float wminY = std::numeric_limits<float>::infinity();
        float wminZ = std::numeric_limits<float>::infinity();
        float wmaxX = -std::numeric_limits<float>::infinity();
        float wmaxY = -std::numeric_limits<float>::infinity();
        float wmaxZ = -std::numeric_limits<float>::infinity();
        const float cx[2] = {fm.minX, fm.maxX};
        const float cy[2] = {fm.minY, fm.maxY};
        const float cz[2] = {fm.minZ, fm.maxZ};
        for (int ix = 0; ix < 2; ++ix) {
          for (int iy = 0; iy < 2; ++iy) {
            for (int iz = 0; iz < 2; ++iz) {
              float tx, ty, tz;
              transform_point(inst.worldMat, cx[ix], cy[iy], cz[iz], tx, ty, tz);
              wminX = std::min(wminX, tx); wmaxX = std::max(wmaxX, tx);
              wminY = std::min(wminY, ty); wmaxY = std::max(wmaxY, ty);
              wminZ = std::min(wminZ, tz); wmaxZ = std::max(wmaxZ, tz);
            }
          }
        }
        inst.worldMinX = wminX; inst.worldMinY = wminY; inst.worldMinZ = wminZ;
        inst.worldMaxX = wmaxX; inst.worldMaxY = wmaxY; inst.worldMaxZ = wmaxZ;
        sc.instances.push_back(std::move(inst));
      }
    }
  }

  for (const auto& child : node.children) {
    walk_hierarchy(sc, child, world, resRanges, nextObjectId);
  }
}

void* finalize_scene(openskp::InstancedScene&& rawScene) {
  auto sc = std::make_unique<VskpScene>();
  sc->raw = std::move(rawScene);

  /* Register all known layers first */
  get_or_add_layer(*sc, "Layer0");
  for (const auto& kv : sc->raw.layer_hidden) {
    get_or_add_layer(*sc, kv.first);
  }

  /* Flatten unique mesh resources into per-primitive RAM meshes + crease edges */
  std::unordered_map<std::string, std::pair<int32_t, int32_t>> resRanges;
  for (const auto& res : sc->raw.mesh_resources) {
    int32_t startIdx = static_cast<int32_t>(sc->meshes.size());
    int32_t cnt = 0;
    for (const auto& prim : res.primitives) {
      if (prim.positions.empty() || prim.indices.empty()) continue;
      FlattenedMesh fm;
      fm.positions = prim.positions;
      fm.normals = prim.normals;
      fm.uvs = prim.uvs;
      fm.indices.resize(prim.indices.size());
      for (size_t i = 0; i < prim.indices.size(); ++i) {
        fm.indices[i] = static_cast<int32_t>(prim.indices[i]);
      }
      /* Ensure normals match vertex count */
      if (fm.normals.size() != fm.positions.size()) {
        fm.normals.assign(fm.positions.size(), 0.0f);
        for (size_t i = 0; i + 2 < fm.normals.size(); i += 3) {
          fm.normals[i + 1] = 1.0f;
        }
      }
      if (fm.uvs.size() != (fm.positions.size() / 3) * 2) {
        fm.uvs.assign((fm.positions.size() / 3) * 2, 0.0f);
      }
      fm.materialIndex = static_cast<int32_t>(prim.material_index);

      float minX = std::numeric_limits<float>::infinity();
      float minY = std::numeric_limits<float>::infinity();
      float minZ = std::numeric_limits<float>::infinity();
      float maxX = -std::numeric_limits<float>::infinity();
      float maxY = -std::numeric_limits<float>::infinity();
      float maxZ = -std::numeric_limits<float>::infinity();
      for (size_t v = 0; v + 2 < fm.positions.size(); v += 3) {
        float x = fm.positions[v + 0];
        float y = fm.positions[v + 1];
        float z = fm.positions[v + 2];
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
        minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
      }
      fm.minX = minX; fm.minY = minY; fm.minZ = minZ;
      fm.maxX = maxX; fm.maxY = maxY; fm.maxZ = maxZ;

      extract_crease_edges(fm);
      sc->meshes.push_back(std::move(fm));
      cnt += 1;
    }
    resRanges.emplace(res.id, std::make_pair(startIdx, cnt));
  }

  /* Walk placed scene hierarchy */
  double identity[16] = {
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1,
  };
  int32_t nextObjId = 1;
  walk_hierarchy(*sc, sc->raw.scene_hierarchy, identity, resRanges, nextObjId);

  /* Compute overall bounds */
  if (sc->raw.bounds.has_value()) {
    const auto& b = *sc->raw.bounds;
    sc->bounds.minX = static_cast<float>(b.min[0]);
    sc->bounds.minY = static_cast<float>(b.min[1]);
    sc->bounds.minZ = static_cast<float>(b.min[2]);
    sc->bounds.maxX = static_cast<float>(b.max[0]);
    sc->bounds.maxY = static_cast<float>(b.max[1]);
    sc->bounds.maxZ = static_cast<float>(b.max[2]);
    sc->bounds.centerX = static_cast<float>(b.center[0]);
    sc->bounds.centerY = static_cast<float>(b.center[1]);
    sc->bounds.centerZ = static_cast<float>(b.center[2]);
    sc->bounds.sizeX = static_cast<float>(b.size[0]);
    sc->bounds.sizeY = static_cast<float>(b.size[1]);
    sc->bounds.sizeZ = static_cast<float>(b.size[2]);
  } else if (!sc->instances.empty()) {
    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();
    for (const auto& inst : sc->instances) {
      minX = std::min(minX, inst.worldMinX); maxX = std::max(maxX, inst.worldMaxX);
      minY = std::min(minY, inst.worldMinY); maxY = std::max(maxY, inst.worldMaxY);
      minZ = std::min(minZ, inst.worldMinZ); maxZ = std::max(maxZ, inst.worldMaxZ);
    }
    sc->bounds.minX = minX; sc->bounds.minY = minY; sc->bounds.minZ = minZ;
    sc->bounds.maxX = maxX; sc->bounds.maxY = maxY; sc->bounds.maxZ = maxZ;
    sc->bounds.centerX = 0.5f * (minX + maxX);
    sc->bounds.centerY = 0.5f * (minY + maxY);
    sc->bounds.centerZ = 0.5f * (minZ + maxZ);
    sc->bounds.sizeX = maxX - minX;
    sc->bounds.sizeY = maxY - minY;
    sc->bounds.sizeZ = maxZ - minZ;
  }

  /* Convert SketchUp Pages (Z-up inches -> glTF Y-up metres: (x, z, -y) * 0.0254) */
  constexpr double kInchToMetre = 0.0254;
  for (const auto& p : sc->raw.pages) {
    FlattenedPage fp;
    fp.name = p.name;
    fp.fovDeg = static_cast<float>(p.fov);
    fp.orthoHeight = static_cast<float>(p.ortho_height * kInchToMetre);
    fp.isParallel = p.parallel ? 1 : 0;
    if (p.eye.has_value() && p.target.has_value()) {
      fp.hasCamera = 1;
      fp.eyeX = static_cast<float>((*p.eye)[0] * kInchToMetre);
      fp.eyeY = static_cast<float>((*p.eye)[2] * kInchToMetre);
      fp.eyeZ = static_cast<float>(-(*p.eye)[1] * kInchToMetre);
      fp.targetX = static_cast<float>((*p.target)[0] * kInchToMetre);
      fp.targetY = static_cast<float>((*p.target)[2] * kInchToMetre);
      fp.targetZ = static_cast<float>(-(*p.target)[1] * kInchToMetre);
      if (p.up.has_value()) {
        fp.upX = static_cast<float>((*p.up)[0]);
        fp.upY = static_cast<float>((*p.up)[2]);
        fp.upZ = static_cast<float>(-(*p.up)[1]);
      }
    }
    sc->pages.push_back(std::move(fp));
  }

  return sc.release();
}

/* Helper to emit an axis-aligned box into any SkpBuilder or ComponentDefinitionBuilder (inches, Z-up). */
template <typename BuilderT>
void add_box(BuilderT& b,
             double x0, double y0, double z0,
             double x1, double y1, double z1,
             int mat, int layer) {
  openskp::FaceOptions opt;
  opt.material = mat;
  opt.layer = layer;
  /* Bottom (z0) */
  b.add_face({{x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}}, opt);
  /* Top (z1) */
  b.add_face({{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}, opt);
  /* Front (-Y) */
  b.add_face({{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}}, opt);
  /* Back (+Y) */
  b.add_face({{x1, y1, z0}, {x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}}, opt);
  /* Left (-X) */
  b.add_face({{x0, y1, z0}, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}}, opt);
  /* Right (+X) */
  b.add_face({{x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}}, opt);
}

}  // namespace

extern "C" {

void* vskp_load_file(const char* path) {
  if (!path || path[0] == '\0') {
    set_error("Empty path passed to vskp_load_file");
    return nullptr;
  }
  try {
    g_last_error.clear();
    auto file = openskp::SkpFile::open(path);
    auto scene = file.build_instanced_scene();
    return finalize_scene(std::move(scene));
  } catch (const std::exception& e) {
    set_error(e.what());
    return nullptr;
  }
}

void* vskp_load_buffer(const uint8_t* data, int64_t size) {
  if (!data || size <= 0) {
    set_error("Empty buffer passed to vskp_load_buffer");
    return nullptr;
  }
  try {
    g_last_error.clear();
    openskp::ByteBuffer buf(data, data + size);
    auto scene = openskp::build_instanced_scene(std::move(buf));
    return finalize_scene(std::move(scene));
  } catch (const std::exception& e) {
    set_error(e.what());
    return nullptr;
  }
}

void vskp_free(void* scene) {
  delete static_cast<VskpScene*>(scene);
}

const char* vskp_last_error(void) {
  return g_last_error.c_str();
}

int32_t vskp_get_bounds(void* scene, VskpBounds* outBounds) {
  if (!scene || !outBounds) return 0;
  *outBounds = static_cast<VskpScene*>(scene)->bounds;
  return 1;
}

int32_t vskp_mesh_count(void* scene) {
  if (!scene) return 0;
  return static_cast<int32_t>(static_cast<VskpScene*>(scene)->meshes.size());
}

int32_t vskp_instance_count(void* scene) {
  if (!scene) return 0;
  return static_cast<int32_t>(static_cast<VskpScene*>(scene)->instances.size());
}

int32_t vskp_material_count(void* scene) {
  if (!scene) return 0;
  return static_cast<int32_t>(static_cast<VskpScene*>(scene)->raw.gltf_materials.size());
}

int32_t vskp_texture_count(void* scene) {
  if (!scene) return 0;
  return static_cast<int32_t>(static_cast<VskpScene*>(scene)->raw.textures.size());
}

int32_t vskp_layer_count(void* scene) {
  if (!scene) return 0;
  return static_cast<int32_t>(static_cast<VskpScene*>(scene)->layers.size());
}

int32_t vskp_page_count(void* scene) {
  if (!scene) return 0;
  return static_cast<int32_t>(static_cast<VskpScene*>(scene)->pages.size());
}

int32_t vskp_get_mesh(void* scene, int32_t idx, VskpMeshData* outMesh) {
  if (!scene || !outMesh) return 0;
  auto* sc = static_cast<VskpScene*>(scene);
  if (idx < 0 || static_cast<size_t>(idx) >= sc->meshes.size()) return 0;
  const FlattenedMesh& m = sc->meshes[idx];
  outMesh->positions = m.positions.empty() ? nullptr : m.positions.data();
  outMesh->normals = m.normals.empty() ? nullptr : m.normals.data();
  outMesh->uvs = m.uvs.empty() ? nullptr : m.uvs.data();
  outMesh->indices = m.indices.empty() ? nullptr : m.indices.data();
  outMesh->creasePositions = m.creasePositions.empty() ? nullptr : m.creasePositions.data();
  outMesh->vertexCount = static_cast<int32_t>(m.positions.size() / 3);
  outMesh->indexCount = static_cast<int32_t>(m.indices.size());
  outMesh->creaseEdgeCount = static_cast<int32_t>(m.creasePositions.size() / 6);
  outMesh->materialIndex = m.materialIndex;
  outMesh->minX = m.minX; outMesh->minY = m.minY; outMesh->minZ = m.minZ;
  outMesh->maxX = m.maxX; outMesh->maxY = m.maxY; outMesh->maxZ = m.maxZ;
  return 1;
}

int32_t vskp_get_instance(void* scene, int32_t idx, VskpInstanceData* outInst) {
  if (!scene || !outInst) return 0;
  auto* sc = static_cast<VskpScene*>(scene);
  if (idx < 0 || static_cast<size_t>(idx) >= sc->instances.size()) return 0;
  const FlattenedInstance& inst = sc->instances[idx];
  outInst->name = inst.name.c_str();
  outInst->meshIndex = inst.meshIndex;
  outInst->materialIndex = inst.materialIndex;
  outInst->layerIndex = inst.layerIndex;
  outInst->objectId = inst.objectId;
  outInst->hidden = inst.hidden;
  outInst->pad0 = 0;
  std::memcpy(outInst->worldMat, inst.worldMat, sizeof(outInst->worldMat));
  outInst->worldMinX = inst.worldMinX; outInst->worldMinY = inst.worldMinY; outInst->worldMinZ = inst.worldMinZ;
  outInst->worldMaxX = inst.worldMaxX; outInst->worldMaxY = inst.worldMaxY; outInst->worldMaxZ = inst.worldMaxZ;
  return 1;
}

int32_t vskp_get_material(void* scene, int32_t idx, VskpMaterialData* outMat) {
  if (!scene || !outMat) return 0;
  auto* sc = static_cast<VskpScene*>(scene);
  if (idx < 0 || static_cast<size_t>(idx) >= sc->raw.gltf_materials.size()) return 0;
  const auto& m = sc->raw.gltf_materials[idx];
  outMat->name = m.name.c_str();
  outMat->r = static_cast<float>(m.pbr_metallic_roughness.base_color_factor[0]);
  outMat->g = static_cast<float>(m.pbr_metallic_roughness.base_color_factor[1]);
  outMat->b = static_cast<float>(m.pbr_metallic_roughness.base_color_factor[2]);
  outMat->a = static_cast<float>(m.pbr_metallic_roughness.base_color_factor[3]);
  outMat->metallic = static_cast<float>(m.pbr_metallic_roughness.metallic_factor);
  outMat->roughness = static_cast<float>(m.pbr_metallic_roughness.roughness_factor);
  outMat->textureIndex = m.pbr_metallic_roughness.base_color_texture.has_value()
                             ? static_cast<int32_t>(*m.pbr_metallic_roughness.base_color_texture)
                             : -1;
  outMat->doubleSided = m.double_sided ? 1 : 0;
  return 1;
}

int32_t vskp_get_texture(void* scene, int32_t idx, VskpTextureData* outTex) {
  if (!scene || !outTex) return 0;
  auto* sc = static_cast<VskpScene*>(scene);
  if (idx < 0 || static_cast<size_t>(idx) >= sc->raw.textures.size()) return 0;
  const auto& t = sc->raw.textures[idx];
  outTex->data = t.data.empty() ? nullptr : t.data.data();
  outTex->mimeType = t.mime_type.c_str();
  outTex->filename = t.filename.c_str();
  outTex->byteSize = static_cast<int32_t>(t.data.size());
  outTex->pad0 = 0;
  return 1;
}

int32_t vskp_get_layer(void* scene, int32_t idx, VskpLayerData* outLayer) {
  if (!scene || !outLayer) return 0;
  auto* sc = static_cast<VskpScene*>(scene);
  if (idx < 0 || static_cast<size_t>(idx) >= sc->layers.size()) return 0;
  const auto& l = sc->layers[idx];
  outLayer->name = l.name.c_str();
  outLayer->hidden = l.hidden;
  outLayer->pad0 = 0;
  return 1;
}

int32_t vskp_get_page(void* scene, int32_t idx, VskpPageCamera* outPage) {
  if (!scene || !outPage) return 0;
  auto* sc = static_cast<VskpScene*>(scene);
  if (idx < 0 || static_cast<size_t>(idx) >= sc->pages.size()) return 0;
  const auto& p = sc->pages[idx];
  outPage->name = p.name.c_str();
  outPage->eyeX = p.eyeX; outPage->eyeY = p.eyeY; outPage->eyeZ = p.eyeZ;
  outPage->targetX = p.targetX; outPage->targetY = p.targetY; outPage->targetZ = p.targetZ;
  outPage->upX = p.upX; outPage->upY = p.upY; outPage->upZ = p.upZ;
  outPage->fovDeg = p.fovDeg;
  outPage->orthoHeight = p.orthoHeight;
  outPage->hasCamera = p.hasCamera;
  outPage->isParallel = p.isParallel;
  outPage->pad0 = 0;
  return 1;
}

int32_t vskp_create_demo_skp(const char* outPath) {
  if (!outPath || outPath[0] == '\0') {
    set_error("Empty path passed to vskp_create_demo_skp");
    return 0;
  }
  try {
    g_last_error.clear();
    auto builder = openskp::create();

    /* 1. Register webtoon-friendly architectural palette materials (all before layers/defs) */
    int matWall   = builder->add_material("WallCream",   openskp::Color3{244, 238, 226});
    int matFloor  = builder->add_material("FloorOak",    openskp::Color3{204, 168, 126});
    int matWood   = builder->add_material("DeskWalnut",  openskp::Color3{152, 102, 68});
    int matChair  = builder->add_material("ChairTeal",   openskp::Color3{86, 154, 168});
    int matTrim   = builder->add_material("TrimSlate",   openskp::Color3{76, 86, 106});
    int matAccent = builder->add_material("CafeCoral",   openskp::Color3{228, 118, 96});
    int matPlant  = builder->add_material("PlantSage",   openskp::Color3{102, 168, 106});
    int matBook   = builder->add_material("BookSky",     openskp::Color3{124, 174, 224});

    /* 2. Register SketchUp layers (tags) */
    int lyrArch  = builder->add_layer("Architecture");
    int lyrFurn  = builder->add_layer("Furniture");
    int lyrProps = builder->add_layer("Props");

    /* 3. Reusable ComponentDefinitions (SketchUp inches, Z-up) */
    /* 3a. Cafe Table component (36 x 30 x 30 in) */
    auto& defTable = builder->add_component_definition("CafeTable");
    add_box(defTable, -18, -15, 28,  18,  15, 30, matWood, lyrFurn); /* tabletop */
    add_box(defTable, -16, -13,  0, -13, -10, 28, matTrim, lyrFurn); /* leg 1 */
    add_box(defTable,  13, -13,  0,  16, -10, 28, matTrim, lyrFurn); /* leg 2 */
    add_box(defTable, -16,  10,  0, -13,  13, 28, matTrim, lyrFurn); /* leg 3 */
    add_box(defTable,  13,  10,  0,  16,  13, 28, matTrim, lyrFurn); /* leg 4 */
    defTable.close();

    /* 3b. Cafe Chair component (18 x 18 x 34 in) */
    auto& defChair = builder->add_component_definition("CafeChair");
    add_box(defChair, -9, -9, 17,  9,  9, 19, matChair, lyrFurn); /* seat */
    add_box(defChair, -9,  6, 19,  9,  9, 34, matChair, lyrFurn); /* backrest */
    add_box(defChair, -8, -8,  0, -6, -6, 17, matTrim,  lyrFurn); /* legs */
    add_box(defChair,  6, -8,  0,  8, -6, 17, matTrim,  lyrFurn);
    add_box(defChair, -8,  6,  0, -6,  8, 17, matTrim,  lyrFurn);
    add_box(defChair,  6,  6,  0,  8,  8, 17, matTrim,  lyrFurn);
    defChair.close();

    /* 3c. Potted Plant prop */
    auto& defPlant = builder->add_component_definition("PottedPlant");
    add_box(defPlant, -6, -6,  0,  6,  6, 14, matAccent, lyrProps);
    add_box(defPlant, -9, -9, 14,  9,  9, 30, matPlant,  lyrProps);
    add_box(defPlant, -5, -5, 30,  5,  5, 40, matPlant,  lyrProps);
    defPlant.close();

    /* 3d. Architectural Pillar */
    auto& defPillar = builder->add_component_definition("Pillar");
    add_box(defPillar, -7, -7,   0,  7,  7,   6, matTrim, lyrArch);
    add_box(defPillar, -5, -5,   6,  5,  5, 114, matWall, lyrArch);
    add_box(defPillar, -7, -7, 114,  7,  7, 120, matTrim, lyrArch);
    defPillar.close();

    /* 4. Root Architecture: floor, back wall with two window cutouts, side wall, counter, stairs, bookshelf */
    add_box(*builder, -150, -110, -6, 150, 110, 0, matFloor, lyrArch);

    /* Back wall (+Y = 104..110) with two tall webtoon cafe window openings (holes!) */
    openskp::FaceOptions wallOpt;
    wallOpt.material = matWall;
    wallOpt.layer = lyrArch;
    wallOpt.holes = {
        {{-110, 104, 34}, {-30, 104, 34}, {-30, 104, 96}, {-110, 104, 96}},
        {{  30, 104, 34}, {110, 104, 34}, {110, 104, 96}, {  30, 104, 96}},
    };
    builder->add_face({{-150, 104, 0}, {150, 104, 0}, {150, 104, 120}, {-150, 104, 120}}, wallOpt);
    /* Window frames / mullions */
    add_box(*builder, -112, 100, 32, -28, 106, 36, matTrim, lyrArch);
    add_box(*builder, -112, 100, 94, -28, 106, 98, matTrim, lyrArch);
    add_box(*builder,  -72, 101, 36, -68, 105, 94, matTrim, lyrArch);
    add_box(*builder,   28, 100, 32, 112, 106, 36, matTrim, lyrArch);
    add_box(*builder,   28, 100, 94, 112, 106, 98, matTrim, lyrArch);
    add_box(*builder,   68, 101, 36,  72, 105, 94, matTrim, lyrArch);

    /* Left side wall (-X = -150..-144) */
    add_box(*builder, -150, -110, 0, -144, 104, 120, matWall, lyrArch);

    /* Cafe service counter on left */
    add_box(*builder, -138, 22, 0, -64, 54, 38, matWood, lyrFurn);
    add_box(*builder, -140, 20, 38, -62, 56, 41, matAccent, lyrFurn);
    /* Espresso machine & props on counter */
    add_box(*builder, -124, 30, 41, -104, 46, 58, matTrim, lyrProps);
    add_box(*builder,  -92, 32, 41,  -78, 44, 52, matBook, lyrProps);

    /* Staircase on right back leading to loft landing */
    for (int s = 0; s < 6; ++s) {
      double sx0 = 84.0 + s * 10.0;
      double sz1 = (s + 1) * 10.0;
      add_box(*builder, sx0, -96, 0, sx0 + 10.0, -44, sz1, matWood, lyrArch);
    }
    /* Upper landing + railing */
    add_box(*builder, 120, -98, 56, 150, 24, 62, matWood, lyrArch);
    add_box(*builder, 120, -44, 62, 124, 24, 94, matTrim, lyrArch);

    /* Bookshelf along left wall */
    add_box(*builder, -143, -84, 0, -128, -20, 84, matWood, lyrFurn);
    add_box(*builder, -141, -78, 22, -130, -56, 34, matBook, lyrProps);
    add_box(*builder, -141, -50, 46, -130, -28, 60, matAccent, lyrProps);

    /* Place instanced components (reused definitions!) */
    openskp::InstanceOptions io;
    io.layer = lyrArch;
    io.name = "Pillar_Center";
    io.translation = {0, 96, 0};
    builder->add_instance(defPillar, io);

    io.name = "Pillar_FrontLeft";
    io.translation = {-136, -96, 0};
    builder->add_instance(defPillar, io);

    /* 3 Cafe seating sets (Table + 2 Chairs each) */
    const double tablePos[3][2] = {{-52, -46}, {22, -42}, {26, 36}};
    for (int i = 0; i < 3; ++i) {
      openskp::InstanceOptions ti;
      ti.layer = lyrFurn;
      ti.name = "Table_" + std::to_string(i + 1);
      ti.translation = {tablePos[i][0], tablePos[i][1], 0};
      builder->add_instance(defTable, ti);

      openskp::InstanceOptions c1;
      c1.layer = lyrFurn;
      c1.name = "Chair_" + std::to_string(i * 2 + 1);
      c1.translation = {tablePos[i][0], tablePos[i][1] + 24.0, 0};
      builder->add_instance(defChair, c1);

      openskp::InstanceOptions c2;
      c2.layer = lyrFurn;
      c2.name = "Chair_" + std::to_string(i * 2 + 2);
      c2.translation = {tablePos[i][0], tablePos[i][1] - 24.0, 0};
      c2.rotation = openskp::Rotation{{0, 0, 1}, 3.14159265358979323846};
      builder->add_instance(defChair, c2);
    }

    /* Potted plants */
    openskp::InstanceOptions p1;
    p1.layer = lyrProps;
    p1.name = "Plant_WindowLeft";
    p1.translation = {-126, 86, 0};
    builder->add_instance(defPlant, p1);

    openskp::InstanceOptions p2;
    p2.layer = lyrProps;
    p2.name = "Plant_WindowRight";
    p2.translation = {126, 86, 0};
    builder->add_instance(defPlant, p2);

    builder->save(outPath);
    return 1;
  } catch (const std::exception& e) {
    set_error(e.what());
    return 0;
  }
}

int32_t vskp_sizeof_bounds(void) { return static_cast<int32_t>(sizeof(VskpBounds)); }
int32_t vskp_sizeof_mesh_data(void) { return static_cast<int32_t>(sizeof(VskpMeshData)); }
int32_t vskp_sizeof_instance_data(void) { return static_cast<int32_t>(sizeof(VskpInstanceData)); }
int32_t vskp_sizeof_material_data(void) { return static_cast<int32_t>(sizeof(VskpMaterialData)); }
int32_t vskp_sizeof_texture_data(void) { return static_cast<int32_t>(sizeof(VskpTextureData)); }
int32_t vskp_sizeof_layer_data(void) { return static_cast<int32_t>(sizeof(VskpLayerData)); }
int32_t vskp_sizeof_page_camera(void) { return static_cast<int32_t>(sizeof(VskpPageCamera)); }

}  // extern "C"
