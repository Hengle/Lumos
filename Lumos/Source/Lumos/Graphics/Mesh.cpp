#ifndef LUMOS_PLATFORM_MACOS
#include "Precompiled.h"
#endif
#include "Mesh.h"
#include "RHI/Renderer.h"
#include "RHI/VertexBuffer.h"
#include "RHI/IndexBuffer.h"
#include "Scene/Serialisation/SerialisationImplementation.h"
#include "Core/OS/FileSystem.h"
#include "Maths/MathsUtilities.h"

#include <cereal/archives/json.hpp>
#include <ModelLoaders/meshoptimizer/src/meshoptimizer.h>
#include <glm/gtx/norm.hpp>

namespace Lumos
{
    namespace Graphics
    {
        Mesh::Mesh()
            : m_VertexBuffer(nullptr)
            , m_IndexBuffer(nullptr)
            , m_BoundingBox(nullptr)
            , m_Indices()
            , m_Vertices()
        {
        }

        Mesh::Mesh(const Mesh& mesh)
            : m_VertexBuffer(mesh.m_VertexBuffer)
            , m_IndexBuffer(mesh.m_IndexBuffer)
            , m_BoundingBox(mesh.m_BoundingBox)
            , m_Name(mesh.m_Name)
            , m_Material(mesh.m_Material)
            , m_Indices(mesh.m_Indices)
            , m_Vertices(mesh.m_Vertices)
        {
        }

        Mesh::Mesh(const std::vector<uint32_t>& indices, const std::vector<Vertex>& vertices, bool optimise, float optimiseThreshold)
        {
            // int lod = 2;
            // float threshold = powf(0.7f, float(lod));
            m_Indices  = indices;
            m_Vertices = vertices;

            if(optimise)
            {
                size_t indexCount         = indices.size();
                size_t target_index_count = size_t(indices.size() * optimiseThreshold);

                float target_error = 1e-3f;
                float* resultError = nullptr;

                auto newIndexCount = meshopt_simplify(m_Indices.data(), m_Indices.data(), m_Indices.size(), (const float*)(&m_Vertices[0]), m_Vertices.size(), sizeof(Graphics::Vertex), target_index_count, target_error, resultError);

                auto newVertexCount = meshopt_optimizeVertexFetch( // return vertices (not vertex attribute values)
                    (m_Vertices.data()),
                    (unsigned int*)(m_Indices.data()),
                    newIndexCount, // total new indices (not faces)
                    (m_Vertices.data()),
                    (size_t)m_Vertices.size(), // total vertices (not vertex attribute values)
                    sizeof(Graphics::Vertex)   // vertex stride
                );

                m_Vertices.resize(newVertexCount);
                m_Indices.resize(newIndexCount);

                // LUMOS_LOG_INFO("Mesh Optimizer - Before : {0} indices {1} vertices , After : {2} indices , {3} vertices", indexCount, m_Vertices.size(), newIndexCount, newVertexCount);
            }

            m_BoundingBox = CreateSharedPtr<Maths::BoundingBox>();

            for(auto& vertex : m_Vertices)
            {
                m_BoundingBox->Merge(vertex.Position);
            }

            m_IndexBuffer  = SharedPtr<Graphics::IndexBuffer>(Graphics::IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size()));
            m_VertexBuffer = SharedPtr<VertexBuffer>(VertexBuffer::Create((uint32_t)(sizeof(Graphics::Vertex) * m_Vertices.size()), m_Vertices.data(), BufferUsage::STATIC));

#ifndef LUMOS_PRODUCTION
            m_Stats.VertexCount       = (uint32_t)m_Vertices.size();
            m_Stats.TriangleCount     = m_Stats.VertexCount / 3;
            m_Stats.IndexCount        = (uint32_t)m_Indices.size();
            m_Stats.OptimiseThreshold = optimiseThreshold;
#endif

            const bool storeData = true;
            if(!storeData)
            {
                m_Indices.clear();
                m_Indices.shrink_to_fit();

                m_Vertices.clear();
                m_Vertices.shrink_to_fit();
            }
        }

        Mesh::Mesh(const std::vector<uint32_t>& indices, const std::vector<AnimVertex>& vertices)
        {
            // int lod = 2;
            // float threshold = powf(0.7f, float(lod));
            m_Indices = indices;
            // m_Vertices = vertices;

            m_BoundingBox = CreateSharedPtr<Maths::BoundingBox>();

            for(auto& vertex : vertices)
            {
                m_BoundingBox->Merge(vertex.Position);
            }

            m_IndexBuffer      = SharedPtr<Graphics::IndexBuffer>(Graphics::IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size()));
            m_AnimVertexBuffer = SharedPtr<VertexBuffer>(VertexBuffer::Create((uint32_t)(sizeof(Graphics::AnimVertex) * vertices.size()), vertices.data(), BufferUsage::STATIC));

#ifndef LUMOS_PRODUCTION
            m_Stats.VertexCount       = (uint32_t)vertices.size();
            m_Stats.TriangleCount     = m_Stats.VertexCount / 3;
            m_Stats.IndexCount        = (uint32_t)m_Indices.size();
            m_Stats.OptimiseThreshold = 1.0f;
#endif

            const bool storeData = false;
            if(!storeData)
            {
                m_Indices.clear();
                m_Indices.shrink_to_fit();

                m_Vertices.clear();
                m_Vertices.shrink_to_fit();
            }
        }

        Mesh::~Mesh()
        {
        }

        void Mesh::GenerateNormals(Vertex* vertices, uint32_t vertexCount, uint32_t* indices, uint32_t indexCount)
        {
            glm::vec3* normals = new glm::vec3[vertexCount];

            for(uint32_t i = 0; i < vertexCount; ++i)
            {
                normals[i] = glm::vec3();
            }

            if(indices)
            {
                for(uint32_t i = 0; i < indexCount; i += 3)
                {
                    const int a = indices[i];
                    const int b = indices[i + 1];
                    const int c = indices[i + 2];

                    const glm::vec3 _normal = glm::cross((vertices[b].Position - vertices[a].Position), (vertices[c].Position - vertices[a].Position));

                    normals[a] += _normal;
                    normals[b] += _normal;
                    normals[c] += _normal;
                }
            }
            else
            {
                // It's just a list of triangles, so generate face normals
                for(uint32_t i = 0; i < vertexCount - 3; i += 3)
                {
                    glm::vec3& a = vertices[i].Position;
                    glm::vec3& b = vertices[i + 1].Position;
                    glm::vec3& c = vertices[i + 2].Position;

                    const glm::vec3 _normal = glm::cross(b - a, c - a);

                    normals[i]     = _normal;
                    normals[i + 1] = _normal;
                    normals[i + 2] = _normal;
                }
            }

            for(uint32_t i = 0; i < vertexCount; ++i)
            {
                glm::normalize(normals[i]);
                vertices[i].Normal = normals[i];
            }

            delete[] normals;
        }

#define CHECK_VEC3(vec3) Maths::IsInf(vec3.x) || Maths::IsInf(vec3.y) || Maths::IsInf(vec3.z) || Maths::IsNaN(vec3.x) || Maths::IsNaN(vec3.y) || Maths::IsNaN(vec3.z)

        void Mesh::GenerateTangentsAndBitangents(Vertex* vertices, uint32_t vertexCount, uint32_t* indices, uint32_t numIndices)
        {
            for(uint32_t i = 0; i < vertexCount; i++)
            {
                vertices[i].Tangent   = glm::vec3(0.0f);
                vertices[i].Bitangent = glm::vec3(0.0f);
            }

            for(uint32_t i = 0; i < numIndices; i += 3)
            {
                glm::vec3 v0 = vertices[indices[i]].Position;
                glm::vec3 v1 = vertices[indices[i + 1]].Position;
                glm::vec3 v2 = vertices[indices[i + 2]].Position;

                glm::vec2 uv0 = vertices[indices[i]].TexCoords;
                glm::vec2 uv1 = vertices[indices[i + 1]].TexCoords;
                glm::vec2 uv2 = vertices[indices[i + 2]].TexCoords;

                glm::vec3 n0 = vertices[indices[i]].Normal;
                glm::vec3 n1 = vertices[indices[i + 1]].Normal;
                glm::vec3 n2 = vertices[indices[i + 2]].Normal;

                glm::vec3 edge1 = v1 - v0;
                glm::vec3 edge2 = v2 - v0;

                glm::vec2 deltaUV1 = uv1 - uv0;
                glm::vec2 deltaUV2 = uv2 - uv0;

                float den = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
                if(den < Maths::M_EPSILON)
                    den = 1.0f;

                float f = 1.0f / den;

                glm::vec3 tangent   = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
                glm::vec3 bitangent = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);

                // Store tangent and bitangent for each vertex of the triangle
                vertices[indices[i]].Tangent += tangent;
                vertices[indices[i + 1]].Tangent += tangent;
                vertices[indices[i + 2]].Tangent += tangent;

                vertices[indices[i]].Bitangent += bitangent;
                vertices[indices[i + 1]].Bitangent += bitangent;
                vertices[indices[i + 2]].Bitangent += bitangent;
            }

            // Normalize the tangent and bitangent vectors
            for(uint32_t i = 0; i < vertexCount; i++)
            {
                if(glm::length2(vertices[i].Tangent) > Maths::M_EPSILON)
                    vertices[i].Tangent = glm::normalize(vertices[i].Tangent);
                else
                    vertices[i].Tangent = glm::vec3(0.0f);

                if(glm::length2(vertices[i].Tangent) > Maths::M_EPSILON)
                    vertices[i].Bitangent = glm::normalize(vertices[i].Bitangent);
                else
                    vertices[i].Bitangent = glm::vec3(0.0f);

                LUMOS_ASSERT(!CHECK_VEC3(vertices[i].Tangent));
                LUMOS_ASSERT(!CHECK_VEC3(vertices[i].Bitangent));
            }
        }

        glm::vec3 Mesh::GenerateTangent(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec2& ta, const glm::vec2& tb, const glm::vec2& tc)
        {
            const glm::vec2 coord1 = tb - ta;
            const glm::vec2 coord2 = tc - ta;

            const glm::vec3 vertex1 = b - a;
            const glm::vec3 vertex2 = c - a;

            const glm::vec3 axis = glm::vec3(vertex1 * coord2.y - vertex2 * coord1.y);

            const float factor = 1.0f / (coord1.x * coord2.y - coord2.x * coord1.y);

            return axis * factor;
        }

        glm::vec3* Mesh::GenerateNormals(uint32_t numVertices, glm::vec3* vertices, uint32_t* indices, uint32_t numIndices)
        {
            glm::vec3* normals = new glm::vec3[numVertices];

            for(uint32_t i = 0; i < numVertices; ++i)
            {
                normals[i] = glm::vec3();
            }

            if(indices)
            {
                for(uint32_t i = 0; i < numIndices; i += 3)
                {
                    const int a = indices[i];
                    const int b = indices[i + 1];
                    const int c = indices[i + 2];

                    const glm::vec3 _normal = glm::cross((vertices[b] - vertices[a]), (vertices[c] - vertices[a]));

                    normals[a] += _normal;
                    normals[b] += _normal;
                    normals[c] += _normal;
                }
            }
            else
            {
                // It's just a list of triangles, so generate face normals
                for(uint32_t i = 0; i < numVertices - 3; i += 3)
                {
                    glm::vec3& a = vertices[i];
                    glm::vec3& b = vertices[i + 1];
                    glm::vec3& c = vertices[i + 2];

                    const glm::vec3 _normal = glm::cross(b - a, c - a);

                    normals[i]     = _normal;
                    normals[i + 1] = _normal;
                    normals[i + 2] = _normal;
                }
            }

            for(uint32_t i = 0; i < numVertices; ++i)
            {
                glm::normalize(normals[i]);
            }

            return normals;
        }

        glm::vec3* Mesh::GenerateTangents(uint32_t numVertices, glm::vec3* vertices, uint32_t* indices, uint32_t numIndices, glm::vec2* texCoords)
        {
            if(!texCoords)
            {
                return nullptr;
            }

            glm::vec3* tangents = new glm::vec3[numVertices];

            for(uint32_t i = 0; i < numVertices; ++i)
            {
                tangents[i] = glm::vec3();
            }

            if(indices)
            {
                for(uint32_t i = 0; i < numIndices; i += 3)
                {
                    int a = indices[i];
                    int b = indices[i + 1];
                    int c = indices[i + 2];

                    const glm::vec3 tangent = GenerateTangent(vertices[a], vertices[b], vertices[c], texCoords[a], texCoords[b], texCoords[c]);

                    tangents[a] += tangent;
                    tangents[b] += tangent;
                    tangents[c] += tangent;
                }
            }
            else
            {
                for(uint32_t i = 0; i < numVertices; i += 3)
                {
                    const glm::vec3 tangent = GenerateTangent(vertices[i], vertices[i + 1], vertices[i + 2], texCoords[i], texCoords[i + 1],
                                                              texCoords[i + 2]);

                    tangents[i] += tangent;
                    tangents[i + 1] += tangent;
                    tangents[i + 2] += tangent;
                }
            }
            for(uint32_t i = 0; i < numVertices; ++i)
            {
                glm::normalize(tangents[i]);
            }

            return tangents;
        }

        void Mesh::CalculateTriangles()
        {
            for(size_t i = 0; i < m_Indices.size(); i += 3)
            {
                m_Triangles.emplace_back(m_Vertices[m_Indices[i + 0]], m_Vertices[m_Indices[i + 1]], m_Vertices[m_Indices[i + 2]]);
            }
        }

        void Mesh::SetMaterial(const SharedPtr<Material>& material)
        {
            m_Material = material;
        }

        void Mesh::SetAndLoadMaterial(const std::string& filePath)
        {
            std::string data = FileSystem::Get().ReadTextFileVFS(filePath);
            std::istringstream istr;
            istr.str(data);
            cereal::JSONInputArchive input(istr);
            auto material = std::make_unique<Graphics::Material>();
            Lumos::Graphics::load(input, *material.get());
            m_Material = SharedPtr<Material>(material.get());
            material.release();
        }
    }
}
