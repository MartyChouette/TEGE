#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Logging/Log.h"
#include <filesystem>

namespace Enjin {
namespace Assets {

std::string GLTFLoader::s_LastError;

bool GLTFLoader::Load(const std::string& filepath, GLTFScene& outScene) {
    s_LastError.clear();

    // Parse the glTF file
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, filepath.c_str(), &data);

    if (result != cgltf_result_success) {
        s_LastError = "Failed to parse glTF file: " + filepath;
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    // Load buffers (required for accessing vertex/index data)
    result = cgltf_load_buffers(&options, data, filepath.c_str());
    if (result != cgltf_result_success) {
        s_LastError = "Failed to load glTF buffers";
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        cgltf_free(data);
        return false;
    }

    // Extract generator metadata (DCC tool identification)
    if (data->asset.generator) {
        outScene.generator = data->asset.generator;
    }

    // Store base path for texture loading
    std::filesystem::path path(filepath);
    outScene.basePath = path.parent_path().string();
    if (!outScene.basePath.empty() && outScene.basePath.back() != '/') {
        outScene.basePath += '/';
    }

    // Load images
    outScene.images.resize(data->images_count);
    for (cgltf_size i = 0; i < data->images_count; ++i) {
        cgltf_image& srcImage = data->images[i];
        GLTFImage& dstImage = outScene.images[i];

        if (srcImage.uri) {
            dstImage.uri = srcImage.uri;
        }
        if (srcImage.mime_type) {
            dstImage.mimeType = srcImage.mime_type;
        }

        // Handle embedded data
        if (srcImage.buffer_view && srcImage.buffer_view->buffer && srcImage.buffer_view->buffer->data) {
            const u8* bufferData = static_cast<const u8*>(srcImage.buffer_view->buffer->data);
            const u8* imageData = bufferData + srcImage.buffer_view->offset;
            dstImage.data.assign(imageData, imageData + srcImage.buffer_view->size);
        }
    }

    // Load materials
    outScene.materials.resize(data->materials_count);
    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        cgltf_material& srcMat = data->materials[i];
        GLTFMaterial& dstMat = outScene.materials[i];

        if (srcMat.name) {
            dstMat.name = srcMat.name;
        }

        dstMat.doubleSided = srcMat.double_sided;
        dstMat.alphaCutoff = srcMat.alpha_cutoff;

        if (srcMat.alpha_mode == cgltf_alpha_mode_blend) {
            dstMat.alphaMode = GLTFMaterial::AlphaMode::Blend;
        } else if (srcMat.alpha_mode == cgltf_alpha_mode_mask) {
            dstMat.alphaMode = GLTFMaterial::AlphaMode::Mask;
        } else {
            dstMat.alphaMode = GLTFMaterial::AlphaMode::Opaque;
        }

        // PBR metallic-roughness
        if (srcMat.has_pbr_metallic_roughness) {
            auto& pbr = srcMat.pbr_metallic_roughness;
            dstMat.baseColorFactor = Math::Vector4(
                pbr.base_color_factor[0],
                pbr.base_color_factor[1],
                pbr.base_color_factor[2],
                pbr.base_color_factor[3]
            );
            dstMat.metallicFactor = pbr.metallic_factor;
            dstMat.roughnessFactor = pbr.roughness_factor;

            if (pbr.base_color_texture.texture) {
                dstMat.baseColorTextureIndex = static_cast<i32>(
                    cgltf_image_index(data, pbr.base_color_texture.texture->image));
            }
            if (pbr.metallic_roughness_texture.texture) {
                dstMat.metallicRoughnessTextureIndex = static_cast<i32>(
                    cgltf_image_index(data, pbr.metallic_roughness_texture.texture->image));
            }
        }

        // Normal map
        if (srcMat.normal_texture.texture) {
            dstMat.normalTextureIndex = static_cast<i32>(
                cgltf_image_index(data, srcMat.normal_texture.texture->image));
        }

        // Occlusion map
        if (srcMat.occlusion_texture.texture) {
            dstMat.occlusionTextureIndex = static_cast<i32>(
                cgltf_image_index(data, srcMat.occlusion_texture.texture->image));
        }

        // Emissive
        dstMat.emissiveFactor = Math::Vector3(
            srcMat.emissive_factor[0],
            srcMat.emissive_factor[1],
            srcMat.emissive_factor[2]
        );
        if (srcMat.emissive_texture.texture) {
            dstMat.emissiveTextureIndex = static_cast<i32>(
                cgltf_image_index(data, srcMat.emissive_texture.texture->image));
        }
    }

    // Load meshes
    outScene.meshes.resize(data->meshes_count);
    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        cgltf_mesh& srcMesh = data->meshes[m];
        GLTFMesh& dstMesh = outScene.meshes[m];

        if (srcMesh.name) {
            dstMesh.name = srcMesh.name;
        }

        dstMesh.primitives.resize(srcMesh.primitives_count);
        for (cgltf_size p = 0; p < srcMesh.primitives_count; ++p) {
            cgltf_primitive& srcPrim = srcMesh.primitives[p];
            GLTFPrimitive& dstPrim = dstMesh.primitives[p];

            if (srcPrim.material) {
                dstPrim.materialIndex = static_cast<i32>(cgltf_material_index(data, srcPrim.material));
            }

            // Find vertex count from position attribute
            cgltf_size vertexCount = 0;
            for (cgltf_size a = 0; a < srcPrim.attributes_count; ++a) {
                if (srcPrim.attributes[a].type == cgltf_attribute_type_position) {
                    vertexCount = srcPrim.attributes[a].data->count;
                    break;
                }
            }

            dstPrim.vertices.resize(vertexCount);

            // Load vertex attributes
            for (cgltf_size a = 0; a < srcPrim.attributes_count; ++a) {
                cgltf_attribute& attr = srcPrim.attributes[a];
                cgltf_accessor* accessor = attr.data;

                // Clamp to allocated vertex count to prevent buffer overflow
                cgltf_size count = (accessor->count <= vertexCount) ? accessor->count : vertexCount;
                for (cgltf_size v = 0; v < count; ++v) {
                    GLTFVertex& vertex = dstPrim.vertices[v];

                    switch (attr.type) {
                        case cgltf_attribute_type_position: {
                            cgltf_accessor_read_float(accessor, v, &vertex.position.x, 3);
                            break;
                        }
                        case cgltf_attribute_type_normal: {
                            cgltf_accessor_read_float(accessor, v, &vertex.normal.x, 3);
                            break;
                        }
                        case cgltf_attribute_type_texcoord: {
                            if (attr.index == 0) {
                                cgltf_accessor_read_float(accessor, v, &vertex.texCoord.x, 2);
                            } else if (attr.index == 1) {
                                cgltf_accessor_read_float(accessor, v, &vertex.texCoord1.x, 2);
                            }
                            break;
                        }
                        case cgltf_attribute_type_tangent: {
                            cgltf_accessor_read_float(accessor, v, &vertex.tangent.x, 4);
                            break;
                        }
                        case cgltf_attribute_type_joints: {
                            // JOINTS_0 = influences 1-4, JOINTS_1 = influences 5-8 (dense rigs)
                            cgltf_uint joints[4] = {0, 0, 0, 0};
                            cgltf_accessor_read_uint(accessor, v, joints, 4);
                            if (attr.index == 0) {
                                vertex.boneIndices[0] = static_cast<u32>(joints[0]);
                                vertex.boneIndices[1] = static_cast<u32>(joints[1]);
                                vertex.boneIndices[2] = static_cast<u32>(joints[2]);
                                vertex.boneIndices[3] = static_cast<u32>(joints[3]);
                            } else if (attr.index == 1) {
                                vertex.boneIndices2[0] = static_cast<u32>(joints[0]);
                                vertex.boneIndices2[1] = static_cast<u32>(joints[1]);
                                vertex.boneIndices2[2] = static_cast<u32>(joints[2]);
                                vertex.boneIndices2[3] = static_cast<u32>(joints[3]);
                            }
                            break;
                        }
                        case cgltf_attribute_type_weights: {
                            if (attr.index == 0) {
                                cgltf_accessor_read_float(accessor, v, &vertex.boneWeights.x, 4);
                            } else if (attr.index == 1) {
                                cgltf_accessor_read_float(accessor, v, &vertex.boneWeights2.x, 4);
                            }
                            break;
                        }
                        case cgltf_attribute_type_color: {
                            if (attr.index == 0) {
                                // Read up to 4 components (RGB or RGBA)
                                f32 rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                                cgltf_size numComp = cgltf_num_components(accessor->type);
                                cgltf_accessor_read_float(accessor, v, rgba, numComp > 4 ? 4 : numComp);
                                vertex.color = Math::Vector4(rgba[0], rgba[1], rgba[2], rgba[3]);
                                vertex.hasColor = true;
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
            }

            // Load indices
            if (srcPrim.indices) {
                cgltf_accessor* indexAccessor = srcPrim.indices;
                dstPrim.indices.resize(indexAccessor->count);

                for (cgltf_size i = 0; i < indexAccessor->count; ++i) {
                    dstPrim.indices[i] = static_cast<u32>(cgltf_accessor_read_index(indexAccessor, i));
                }
            }

            // Load morph targets (blend shapes)
            if (srcPrim.targets_count > 0) {
                dstPrim.morphTargets.resize(srcPrim.targets_count);
                for (cgltf_size t = 0; t < srcPrim.targets_count; ++t) {
                    GLTFMorphTarget& dstTarget = dstPrim.morphTargets[t];

                    // Name from mesh-level target_names
                    if (srcMesh.target_names && t < srcMesh.target_names_count) {
                        dstTarget.name = srcMesh.target_names[t];
                    } else {
                        char nameBuf[32];
                        snprintf(nameBuf, sizeof(nameBuf), "target_%zu", t);
                        dstTarget.name = nameBuf;
                    }

                    cgltf_morph_target& srcTarget = srcPrim.targets[t];
                    for (cgltf_size a = 0; a < srcTarget.attributes_count; ++a) {
                        cgltf_attribute& attr = srcTarget.attributes[a];
                        cgltf_accessor* accessor = attr.data;
                        cgltf_size count = (accessor->count <= vertexCount) ? accessor->count : vertexCount;

                        if (attr.type == cgltf_attribute_type_position) {
                            dstTarget.positionDeltas.resize(vertexCount, Math::Vector3(0.0f));
                            for (cgltf_size v = 0; v < count; ++v) {
                                cgltf_accessor_read_float(accessor, v, &dstTarget.positionDeltas[v].x, 3);
                            }
                        } else if (attr.type == cgltf_attribute_type_normal) {
                            dstTarget.normalDeltas.resize(vertexCount, Math::Vector3(0.0f));
                            for (cgltf_size v = 0; v < count; ++v) {
                                cgltf_accessor_read_float(accessor, v, &dstTarget.normalDeltas[v].x, 3);
                            }
                        }
                    }

                    // Ensure normal deltas exist even if not authored
                    if (dstTarget.normalDeltas.empty()) {
                        dstTarget.normalDeltas.resize(vertexCount, Math::Vector3(0.0f));
                    }
                }

                // Default weights
                if (srcMesh.weights && srcMesh.weights_count > 0) {
                    dstMesh.defaultMorphWeights.resize(srcMesh.weights_count);
                    for (cgltf_size w = 0; w < srcMesh.weights_count; ++w) {
                        dstMesh.defaultMorphWeights[w] = srcMesh.weights[w];
                    }
                }
            }
        }
    }

    // Load nodes
    outScene.nodes.resize(data->nodes_count);
    for (cgltf_size n = 0; n < data->nodes_count; ++n) {
        cgltf_node& srcNode = data->nodes[n];
        GLTFNode& dstNode = outScene.nodes[n];

        if (srcNode.name) {
            dstNode.name = srcNode.name;
        }

        if (srcNode.mesh) {
            dstNode.meshIndex = static_cast<i32>(cgltf_mesh_index(data, srcNode.mesh));
        }

        // Transform
        if (srcNode.has_translation) {
            dstNode.translation = Math::Vector3(
                srcNode.translation[0],
                srcNode.translation[1],
                srcNode.translation[2]
            );
        }
        if (srcNode.has_rotation) {
            dstNode.rotation = Math::Quaternion(
                srcNode.rotation[0],
                srcNode.rotation[1],
                srcNode.rotation[2],
                srcNode.rotation[3]
            );
        }
        if (srcNode.has_scale) {
            dstNode.scale = Math::Vector3(
                srcNode.scale[0],
                srcNode.scale[1],
                srcNode.scale[2]
            );
        }
        if (srcNode.has_matrix) {
            // Decompose the 4x4 matrix into translation, rotation, scale
            const f32* m = srcNode.matrix;
            // Translation from last column
            dstNode.translation = Math::Vector3(m[12], m[13], m[14]);
            // Scale from column vector lengths
            Math::Vector3 col0(m[0], m[1], m[2]);
            Math::Vector3 col1(m[4], m[5], m[6]);
            Math::Vector3 col2(m[8], m[9], m[10]);
            f32 sx = col0.Length();
            f32 sy = col1.Length();
            f32 sz = col2.Length();
            // Detect negative determinant (reflection) — flip one axis
            f32 det = m[0] * (m[5] * m[10] - m[6] * m[9])
                    - m[4] * (m[1] * m[10] - m[2] * m[9])
                    + m[8] * (m[1] * m[6] - m[2] * m[5]);
            if (det < 0.0f) sx = -sx;
            dstNode.scale = Math::Vector3(sx, sy, sz);
            // Build rotation matrix by dividing out scale
            if (sx != 0.0f && sy != 0.0f && sz != 0.0f) {
                Math::Matrix4 rotMat = Math::Matrix4::Identity();
                rotMat.m[0] = m[0] / sx; rotMat.m[1] = m[1] / sx; rotMat.m[2] = m[2] / sx;
                rotMat.m[4] = m[4] / sy; rotMat.m[5] = m[5] / sy; rotMat.m[6] = m[6] / sy;
                rotMat.m[8] = m[8] / sz; rotMat.m[9] = m[9] / sz; rotMat.m[10] = m[10] / sz;
                dstNode.rotation = Math::Quaternion::FromMatrix(rotMat);
            }
        }

        // Children
        for (cgltf_size c = 0; c < srcNode.children_count; ++c) {
            dstNode.children.push_back(static_cast<i32>(cgltf_node_index(data, srcNode.children[c])));
        }
    }

    // Get root nodes from default scene
    if (data->scene) {
        for (cgltf_size n = 0; n < data->scene->nodes_count; ++n) {
            outScene.rootNodes.push_back(static_cast<i32>(cgltf_node_index(data, data->scene->nodes[n])));
        }
    } else if (data->scenes_count > 0) {
        // Use first scene
        for (cgltf_size n = 0; n < data->scenes[0].nodes_count; ++n) {
            outScene.rootNodes.push_back(static_cast<i32>(cgltf_node_index(data, data->scenes[0].nodes[n])));
        }
    }

    // Load skins
    outScene.skins.resize(data->skins_count);
    for (cgltf_size s = 0; s < data->skins_count; ++s) {
        cgltf_skin& srcSkin = data->skins[s];
        GLTFSkin& dstSkin = outScene.skins[s];

        if (srcSkin.name) {
            dstSkin.name = srcSkin.name;
        }

        if (srcSkin.skeleton) {
            dstSkin.skeletonRootNode = static_cast<i32>(cgltf_node_index(data, srcSkin.skeleton));
        }

        // Joint node indices
        dstSkin.jointNodeIndices.resize(srcSkin.joints_count);
        for (cgltf_size j = 0; j < srcSkin.joints_count; ++j) {
            dstSkin.jointNodeIndices[j] = static_cast<i32>(cgltf_node_index(data, srcSkin.joints[j]));
        }

        // Inverse bind matrices
        if (srcSkin.inverse_bind_matrices) {
            cgltf_accessor* ibmAccessor = srcSkin.inverse_bind_matrices;
            dstSkin.inverseBindMatrices.resize(ibmAccessor->count);
            for (cgltf_size j = 0; j < ibmAccessor->count; ++j) {
                cgltf_accessor_read_float(ibmAccessor, j,
                    dstSkin.inverseBindMatrices[j].m, 16);
            }
        } else {
            // Default to identity if no inverse bind matrices
            dstSkin.inverseBindMatrices.resize(srcSkin.joints_count, Math::Matrix4::Identity());
        }
    }

    // Set skinIndex on nodes that reference a skin
    for (cgltf_size n = 0; n < data->nodes_count; ++n) {
        if (data->nodes[n].skin) {
            outScene.nodes[n].skinIndex = static_cast<i32>(cgltf_skin_index(data, data->nodes[n].skin));
        }
    }

    // Load animations
    outScene.animations.resize(data->animations_count);
    for (cgltf_size a = 0; a < data->animations_count; ++a) {
        cgltf_animation& srcAnim = data->animations[a];
        GLTFAnimation& dstAnim = outScene.animations[a];

        if (srcAnim.name) {
            dstAnim.name = srcAnim.name;
        } else {
            dstAnim.name = "Animation_" + std::to_string(a);
        }

        dstAnim.duration = 0.0f;
        dstAnim.channels.resize(srcAnim.channels_count);

        for (cgltf_size c = 0; c < srcAnim.channels_count; ++c) {
            cgltf_animation_channel& srcChannel = srcAnim.channels[c];
            GLTFAnimationChannel& dstChannel = dstAnim.channels[c];

            if (srcChannel.target_node) {
                dstChannel.targetNode = static_cast<i32>(cgltf_node_index(data, srcChannel.target_node));
            }

            switch (srcChannel.target_path) {
                case cgltf_animation_path_type_translation:
                    dstChannel.path = GLTFAnimationChannel::Path::Translation;
                    break;
                case cgltf_animation_path_type_rotation:
                    dstChannel.path = GLTFAnimationChannel::Path::Rotation;
                    break;
                case cgltf_animation_path_type_scale:
                    dstChannel.path = GLTFAnimationChannel::Path::Scale;
                    break;
                case cgltf_animation_path_type_weights:
                    dstChannel.path = GLTFAnimationChannel::Path::Weights;
                    break;
                default:
                    continue;
            }

            if (!srcChannel.sampler) continue;
            cgltf_animation_sampler& sampler = *srcChannel.sampler;

            // Read keyframe times from input accessor
            cgltf_accessor* inputAccessor = sampler.input;
            dstChannel.times.resize(inputAccessor->count);
            for (cgltf_size k = 0; k < inputAccessor->count; ++k) {
                cgltf_accessor_read_float(inputAccessor, k, &dstChannel.times[k], 1);
                if (dstChannel.times[k] > dstAnim.duration) {
                    dstAnim.duration = dstChannel.times[k];
                }
            }

            // Read keyframe values from output accessor
            cgltf_accessor* outputAccessor = sampler.output;
            cgltf_size componentCount;
            if (dstChannel.path == GLTFAnimationChannel::Path::Rotation) componentCount = 4;
            else if (dstChannel.path == GLTFAnimationChannel::Path::Weights) {
                // Weights channel: one float per morph target per keyframe
                componentCount = srcChannel.target_node && srcChannel.target_node->mesh
                    ? srcChannel.target_node->mesh->primitives[0].targets_count : 1;
            } else componentCount = 3;
            dstChannel.values.resize(outputAccessor->count * componentCount);
            for (cgltf_size k = 0; k < outputAccessor->count; ++k) {
                cgltf_accessor_read_float(outputAccessor, k,
                    &dstChannel.values[k * componentCount],
                    static_cast<cgltf_size>(componentCount));
            }
        }

        ENJIN_LOG_INFO(Asset, "  Animation '%s': %.2fs, %zu channels",
            dstAnim.name.c_str(), dstAnim.duration, dstAnim.channels.size());
    }

    cgltf_free(data);

    ENJIN_LOG_INFO(Asset, "Loaded glTF: %s (%zu meshes, %zu materials, %zu nodes, %zu skins, %zu animations)",
        filepath.c_str(),
        outScene.meshes.size(),
        outScene.materials.size(),
        outScene.nodes.size(),
        outScene.skins.size(),
        outScene.animations.size());

    return true;
}

const std::string& GLTFLoader::GetLastError() {
    return s_LastError;
}

} // namespace Assets
} // namespace Enjin
