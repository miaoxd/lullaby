/*
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "lullaby/systems/nine_patch/nine_patch_system.h"

#include "lullaby/modules/dispatcher/dispatcher.h"
#include "lullaby/modules/render/mesh_data.h"
#include "lullaby/systems/layout/layout_box_system.h"
#include "lullaby/systems/render/render_system.h"
#include "lullaby/systems/transform/transform_system.h"
#include "lullaby/util/logging.h"
#include "lullaby/generated/nine_patch_def_generated.h"
#include "mathfu/glsl_mappings.h"

namespace lull {

static const HashValue kNinePatchDefHash = ConstHash("NinePatchDef");

NinePatchSystem::NinePatchSystem(Registry* registry)
    : System(registry), nine_patches_(16) {
  RegisterDef(this, kNinePatchDefHash);
  RegisterDependency<RenderSystem>(this);

  Dispatcher* dispatcher = registry_->Get<Dispatcher>();
  dispatcher->Connect(this, [this](const DesiredSizeChangedEvent& event) {
    OnDesiredSizeChanged(event);
  });
}

NinePatchSystem::~NinePatchSystem() {
  Dispatcher* dispatcher = registry_->Get<Dispatcher>();
  dispatcher->DisconnectAll(this);
}

void NinePatchSystem::Create(Entity entity, DefType type, const Def* def) {
  if (type != kNinePatchDefHash) {
    LOG(DFATAL) << "NinePatchSystem::Create() received invalid DefType";
    return;
  }

  const auto* data = ConvertDef<NinePatchDef>(def);

  NinePatch& nine_patch = nine_patches_[entity];

  NinePatchFromDef(data, &nine_patch);
}

void NinePatchSystem::PostCreateInit(Entity entity, DefType type,
                                     const Def* /*def*/) {
  if (type != kNinePatchDefHash) {
    LOG(DFATAL) << "NinePatchSystem::PostCreateInit() received invalid DefType";
    return;
  }

  auto iter = nine_patches_.find(entity);
  if (iter == nine_patches_.end()) {
    LOG(WARNING) << "Entity is not registered with the NinePatchSystem: "
                 << entity;
    return;
  }
  UpdateNinePatchMesh(entity, kNullEntity, &iter->second);
}

void NinePatchSystem::Destroy(Entity entity) { nine_patches_.erase(entity); }

void NinePatchSystem::SetSize(Entity entity, const mathfu::vec2& size) {
  auto iter = nine_patches_.find(entity);
  if (iter == nine_patches_.end()) {
    LOG(WARNING) << "Entity is not registered with the NinePatchSystem: "
                 << entity;
    return;
  }
  iter->second.size = size;
  UpdateNinePatchMesh(entity, kNullEntity, &iter->second);
}

Optional<mathfu::vec2> NinePatchSystem::GetSize(Entity entity) const {
  auto iter = nine_patches_.find(entity);
  if (iter == nine_patches_.end()) {
    return Optional<mathfu::vec2>();
  }
  return iter->second.size;
}

void NinePatchSystem::SetOriginalSize(Entity entity, const mathfu::vec2& size) {
  auto iter = nine_patches_.find(entity);
  if (iter == nine_patches_.end()) {
    LOG(WARNING) << "Entity is not registered with the NinePatchSystem: "
                 << entity;
    return;
  }
  iter->second.original_size = size;
  UpdateNinePatchMesh(entity, kNullEntity, &iter->second);
}

Optional<mathfu::vec2> NinePatchSystem::GetOriginalSize(Entity entity) const {
  auto iter = nine_patches_.find(entity);
  if (iter == nine_patches_.end()) {
    return Optional<mathfu::vec2>();
  }
  return iter->second.original_size;
}

void NinePatchSystem::UpdateNinePatchMesh(Entity entity, Entity source,
                                          const NinePatch* nine_patch) {
  auto* render_system = registry_->Get<RenderSystem>();

  auto nine_patch_mesh_fn = [&nine_patch](lull::MeshData* mesh) {
    GenerateNinePatchMesh(*nine_patch, mesh);
  };

  render_system->UpdateDynamicMesh(
      entity, lull::MeshData::PrimitiveType::kTriangles,
      lull::VertexPTT::kFormat, nine_patch->GetVertexCount(),
      nine_patch->GetIndexCount(), nine_patch_mesh_fn);

  auto *layout_box_system = registry_->Get<LayoutBoxSystem>();
  if (layout_box_system) {
    Aabb aabb(mathfu::vec3(-nine_patch->size / 2.f, 0.f),
              mathfu::vec3(nine_patch->size / 2.f, 0.f));
    if (source != kNullEntity) {
      layout_box_system->SetActualBox(entity, source, aabb);
    } else {
      layout_box_system->SetOriginalBox(entity, aabb);
    }
  }

  auto* transform_system = registry_->Get<TransformSystem>();

  const mathfu::vec2 half_size = 0.5f * nine_patch->size;
  Aabb aabb(mathfu::vec3(-half_size, 0.f), mathfu::vec3(half_size, 0.f));

  transform_system->SetAabb(entity, aabb);
}

void NinePatchSystem::OnDesiredSizeChanged(
    const DesiredSizeChangedEvent& event) {
  auto iter = nine_patches_.find(event.target);
  if (iter != nine_patches_.end()) {
    auto* layout_box_system = registry_->Get<LayoutBoxSystem>();
    // Create a copy of params and override with desired_size if set so that
    // original params are unchanged.
    NinePatch nine_patch = iter->second;
    Optional<float> x = layout_box_system->GetDesiredSizeX(event.target);
    Optional<float> y = layout_box_system->GetDesiredSizeY(event.target);
    if (x) {
      nine_patch.size.x = *x;
    }
    if (y) {
      nine_patch.size.y = *y;
    }
    UpdateNinePatchMesh(event.target, event.source, &nine_patch);
  }
}

}  // namespace lull
