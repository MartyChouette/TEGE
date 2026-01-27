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
        if (srcImage.buffer_view) {
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

                for (cgltf_size v = 0; v < accessor->count; ++v) {
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
                            }
                            break;
                        }
                        case cgltf_attribute_type_tangent: {
                            cgltf_accessor_read_float(accessor, v, &vertex.tangent.x, 4);
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
            // For now, decompose matrix is complex - skip if only matrix is present
            ENJIN_LOG_WARN(Asset, "Node '%s' uses matrix transform, which is not fully supported",
                srcNode.name ? srcNode.name : "unnamed");
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

    cgltf_free(data);

    ENJIN_LOG_INFO(Asset, "Loaded glTF: %s (%zu meshes, %zu materials, %zu nodes)",
        filepath.c_str(),
        outScene.meshes.size(),
        outScene.materials.size(),
        outScene.nodes.size());

    return true;
}

const std::string& GLTFLoader::GetLastError() {
    return s_LastError;
}

} // namespace Assets
} // namespace Enjin
