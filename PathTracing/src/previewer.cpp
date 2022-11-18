#include <algorithm>
#include <unordered_map>

#include "image.h"

#include "previewer.h"

Previewer::Previewer()
{
    mCamPos = glm::vec3(0.0f, 0.0f, -10.0f);
    mCamDir = glm::vec3(0.0f, 0.0f, 1.0f);
    mCamUp = glm::vec3(0.0f, 1.0f, 0.0f);
    mCamFocal = 0.1f;
    mCamFovy = 90;
}

Previewer::~Previewer()
{
    for (int i = 0; i < mLoadedObjects.size(); i++)
        FreeObject(i);
}

void Previewer::Normalize(PreviewerLoader::Vec3& v)
{
    float len2 = v.v[0] * v.v[0] + v.v[1] * v.v[1] + v.v[2] * v.v[2];
    if (len2 > 0.0f)
    {
        float len = sqrtf(len2);
        v.v[0] /= len;
        v.v[1] /= len;
        v.v[2] /= len;
    }
}

void Previewer::CalcNormal(float N[3], float v0[3], float v1[3], float v2[3])
{
    float v10[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
    float v20[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };

    N[0] = v10[1] * v20[2] - v10[2] * v20[1];
    N[1] = v10[2] * v20[0] - v10[0] * v20[2];
    N[2] = v10[0] * v20[1] - v10[1] * v20[0];

    float len2 = N[0] * N[0] + N[1] * N[1] + N[2] * N[2];
    if (len2 > 0.0f)
    {
        float len = sqrtf(len2);
        N[0] /= len;
        N[1] /= len;
        N[2] /= len;
    }
}

void Previewer::CalcTangent
(
    float T[3], float v0[3], float v1[3], float v2[3], float tc0[3], float tc1[3], float tc2[3]
)
{
    float v10[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
    float v20[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };

    float tc10[2] = { tc1[0] - tc0[0], tc1[1] - tc0[1]};
    float tc20[3] = { tc2[0] - tc0[0], tc2[1] - tc0[1] };

    float f = 1.0f / (tc10[0] * tc20[1] - tc20[0] * tc10[1]);

    T[0] = f * (tc20[1] * v10[0] - tc10[1] * v20[0]);
    T[1] = f * (tc20[1] * v10[1] - tc10[1] * v20[1]);
    T[2] = f * (tc20[1] * v10[2] - tc10[1] * v20[2]);
}

const bool Previewer::HasSmoothingGroup(const tinyobj::shape_t& shape)
{
    for (size_t i = 0; i < shape.mesh.smoothing_group_ids.size(); i++)
    {
        if (shape.mesh.smoothing_group_ids[i] > 0)
            return true;
    }
    return false;
}

void Previewer::ComputeSmoothingNormals
(
    const tinyobj::attrib_t& attrib,
    const tinyobj::shape_t& shape,
    std::map<int, PreviewerLoader::Vec3>& smoothVertexNormals
)
{
    smoothVertexNormals.clear();
    std::map<int, PreviewerLoader::Vec3>::iterator iter;

    for (size_t f = 0; f < shape.mesh.indices.size() / 3; f++)
    {
        // Get the three indexes of the face (all faces are triangular)
        tinyobj::index_t idx0 = shape.mesh.indices[3 * f + 0];
        tinyobj::index_t idx1 = shape.mesh.indices[3 * f + 1];
        tinyobj::index_t idx2 = shape.mesh.indices[3 * f + 2];

        // Get the three vertex indexes and coordinates
        int vi[3]{};      // indexes
        float v[3][3]{};  // coordinates
        for (int k = 0; k < 3; k++)
        {
            vi[0] = idx0.vertex_index;
            vi[1] = idx1.vertex_index;
            vi[2] = idx2.vertex_index;

            v[0][k] = attrib.vertices[3 * vi[0] + k];
            v[1][k] = attrib.vertices[3 * vi[1] + k];
            v[2][k] = attrib.vertices[3 * vi[2] + k];
        }

        // Compute the normal of the face
        float normal[3];
        CalcNormal(normal, v[0], v[1], v[2]);

        // Add the normal to the three vertexes
        for (size_t i = 0; i < 3; ++i)
        {
            iter = smoothVertexNormals.find(vi[i]);
            if (iter != smoothVertexNormals.end())
            {
                // add
                iter->second.v[0] += normal[0];
                iter->second.v[1] += normal[1];
                iter->second.v[2] += normal[2];
            }
            else
            {
                smoothVertexNormals[vi[i]].v[0] = normal[0];
                smoothVertexNormals[vi[i]].v[1] = normal[1];
                smoothVertexNormals[vi[i]].v[2] = normal[2];
            }
        }
    }

    // Normalize the normals, that is, make them unit vectors
    for (iter = smoothVertexNormals.begin(); iter != smoothVertexNormals.end(); iter++)
        Normalize(iter->second);

}

void Previewer::ComputeAllSmoothingNormals
(
    tinyobj::attrib_t& attrib,
    std::vector<tinyobj::shape_t>& shapes
)
{
    PreviewerLoader::Vec3 p[3];
    for (size_t s = 0, slen = shapes.size(); s < slen; ++s)
    {
        const tinyobj::shape_t& shape(shapes[s]);
        size_t facecount = shape.mesh.num_face_vertices.size();

        for (size_t f = 0, flen = facecount; f < flen; ++f)
        {
            for (unsigned int v = 0; v < 3; ++v)
            {
                tinyobj::index_t idx = shape.mesh.indices[3 * f + v];
                p[v].v[0] = attrib.vertices[3 * idx.vertex_index];
                p[v].v[1] = attrib.vertices[3 * idx.vertex_index + 1];
                p[v].v[2] = attrib.vertices[3 * idx.vertex_index + 2];
            }

            // cross(p[1] - p[0], p[2] - p[0])
            float nx = (p[1].v[1] - p[0].v[1]) * (p[2].v[2] - p[0].v[2]) -
                (p[1].v[2] - p[0].v[2]) * (p[2].v[1] - p[0].v[1]);
            float ny = (p[1].v[2] - p[0].v[2]) * (p[2].v[0] - p[0].v[0]) -
                (p[1].v[0] - p[0].v[0]) * (p[2].v[2] - p[0].v[2]);
            float nz = (p[1].v[0] - p[0].v[0]) * (p[2].v[1] - p[0].v[1]) -
                (p[1].v[1] - p[0].v[1]) * (p[2].v[0] - p[0].v[0]);

            // Don't normalize here.
            for (unsigned int v = 0; v < 3; ++v)
            {
                tinyobj::index_t idx = shape.mesh.indices[3 * f + v];
                attrib.normals[3 * idx.normal_index] += nx;
                attrib.normals[3 * idx.normal_index + 1] += ny;
                attrib.normals[3 * idx.normal_index + 2] += nz;
            }
        }
    }

    for (size_t i = 0, nlen = attrib.normals.size() / 3; i < nlen; ++i)
    {
        tinyobj::real_t& nx = attrib.normals[3 * i];
        tinyobj::real_t& ny = attrib.normals[3 * i + 1];
        tinyobj::real_t& nz = attrib.normals[3 * i + 2];
        tinyobj::real_t len = sqrtf(nx * nx + ny * ny + nz * nz);
        tinyobj::real_t scale = len == 0 ? 0 : 1 / len;
        nx *= scale;
        ny *= scale;
        nz *= scale;
    }
}

void Previewer::ComputeSmoothingShape
(
    tinyobj::attrib_t& inattrib,
    tinyobj::shape_t& inshape,
    std::vector<std::pair<unsigned int, unsigned int>>& sortedids,
    unsigned int idbegin,
    unsigned int idend,
    std::vector<tinyobj::shape_t>& outshapes,
    tinyobj::attrib_t& outattrib
)
{
    unsigned int sgroupid = sortedids[idbegin].first;
    bool hasmaterials = inshape.mesh.material_ids.size();
    // Make a new shape from the set of faces in the range [idbegin, idend).
    outshapes.emplace_back();
    tinyobj::shape_t& outshape = outshapes.back();
    outshape.name = inshape.name;
    // Skip lines and points.

    std::unordered_map<unsigned int, unsigned int> remap;
    for (unsigned int id = idbegin; id < idend; ++id)
    {
        unsigned int face = sortedids[id].second;

        outshape.mesh.num_face_vertices.push_back(3); // always triangles
        if (hasmaterials)
            outshape.mesh.material_ids.push_back(inshape.mesh.material_ids[face]);
        outshape.mesh.smoothing_group_ids.push_back(sgroupid);
        // Skip tags.

        for (unsigned int v = 0; v < 3; ++v)
        {
            tinyobj::index_t inidx = inshape.mesh.indices[3 * face + v], outidx{};
            auto iter = remap.find(inidx.vertex_index);
            // Smooth group 0 disables smoothing so no shared vertices in that case.
            if (sgroupid && iter != remap.end())
            {
                outidx.vertex_index = (*iter).second;
                outidx.normal_index = outidx.vertex_index;
                outidx.texcoord_index = (inidx.texcoord_index == -1) ? -1 : outidx.vertex_index;
            }
            else
            {
                unsigned int offset = static_cast<unsigned int>(outattrib.vertices.size() / 3);
                outidx.vertex_index = outidx.normal_index = offset;
                outidx.texcoord_index = (inidx.texcoord_index == -1) ? -1 : offset;
                outattrib.vertices.push_back(inattrib.vertices[3 * inidx.vertex_index]);
                outattrib.vertices.push_back(inattrib.vertices[3 * inidx.vertex_index + 1]);
                outattrib.vertices.push_back(inattrib.vertices[3 * inidx.vertex_index + 2]);
                outattrib.normals.push_back(0.0f);
                outattrib.normals.push_back(0.0f);
                outattrib.normals.push_back(0.0f);
                if (inidx.texcoord_index != -1)
                {
                    outattrib.texcoords.push_back(inattrib.texcoords[2 * inidx.texcoord_index]);
                    outattrib.texcoords.push_back(inattrib.texcoords[2 * inidx.texcoord_index + 1]);
                }
                remap[inidx.vertex_index] = offset;
            }
            outshape.mesh.indices.push_back(outidx);
        }
    }
}

void Previewer::ComputeSmoothingShapes
(
    tinyobj::attrib_t& inattrib,
    std::vector<tinyobj::shape_t>& inshapes,
    std::vector<tinyobj::shape_t>& outshapes,
    tinyobj::attrib_t& outattrib
)
{
    for (size_t s = 0, slen = inshapes.size(); s < slen; ++s)
    {
        tinyobj::shape_t& inshape = inshapes[s];

        unsigned int numfaces = static_cast<unsigned int>(inshape.mesh.smoothing_group_ids.size());
        std::vector<std::pair<unsigned int, unsigned int>> sortedids(numfaces);
        for (unsigned int i = 0; i < numfaces; ++i)
            sortedids[i] = std::make_pair(inshape.mesh.smoothing_group_ids[i], i);
        sort(sortedids.begin(), sortedids.end());

        unsigned int activeid = sortedids[0].first;
        unsigned int id = activeid, idbegin = 0, idend = 0;
        // Faces are now bundled by smoothing group id, create shapes from these.
        while (idbegin < numfaces)
        {
            while (activeid == id && ++idend < numfaces)
                id = sortedids[idend].first;
            ComputeSmoothingShape(inattrib, inshape, sortedids, idbegin, idend,
                outshapes, outattrib);
            activeid = id;
            idbegin = idend;
        }
    }
}

const bool Previewer::LoadObject(const std::string& filename, int id)
{
    int nameStartIndex = filename.find_last_of('/') + 1;
    if (nameStartIndex > filename.size() - 1)
        nameStartIndex = 0;
    int nameEndIndex = filename.find_last_of(".");
    if (nameEndIndex == std::string::npos)
        nameEndIndex = filename.size() - 1;
    std::string objName = filename.substr(nameStartIndex, nameEndIndex - nameStartIndex);
    PreviewerLoader::Object obj(objName);
    obj.filename = filename;

    tinyobj::attrib_t inattrib;
    std::vector<tinyobj::shape_t> inshapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;
    if (!tinyobj::LoadObj(&inattrib, &inshapes, &materials, &warn, &err, filename.c_str()))
        return false;

    bool regen_all_normals = inattrib.normals.size() == 0;
    tinyobj::attrib_t outattrib;
    std::vector<tinyobj::shape_t> outshapes;
    if (regen_all_normals)
    {
        ComputeSmoothingShapes(inattrib, inshapes, outshapes, outattrib);
        ComputeAllSmoothingNormals(outattrib, outshapes);
    }

    std::vector<tinyobj::shape_t>& shapes = regen_all_normals ? outshapes : inshapes;
    tinyobj::attrib_t& attrib = regen_all_normals ? outattrib : inattrib;

    if (shapes.size() == 0)
        return false;

    for (size_t s = 0; s < shapes.size(); s++)
    {
        PreviewerLoader::Element element(shapes[s].name);

        std::vector<float> buffer;  // pos(3float), normal(3float), tc(2float)

        // Check for smoothing group and compute smoothing normals
        std::map<int, PreviewerLoader::Vec3> smoothVertexNormals;
        if (!regen_all_normals && (HasSmoothingGroup(shapes[s]) > 0))
            ComputeSmoothingNormals(attrib, shapes[s], smoothVertexNormals);

        for (size_t f = 0; f < shapes[s].mesh.indices.size() / 3; f++)
        {
            tinyobj::index_t idx0 = shapes[s].mesh.indices[3 * f + 0];
            tinyobj::index_t idx1 = shapes[s].mesh.indices[3 * f + 1];
            tinyobj::index_t idx2 = shapes[s].mesh.indices[3 * f + 2];

            float tc[3][2]{};
            if (attrib.texcoords.size() > 0)
            {
                if ((idx0.texcoord_index < 0) ||
                    (idx1.texcoord_index < 0) ||
                    (idx2.texcoord_index < 0))
                {
                    // face does not contain valid uv index.
                    tc[0][0] = 0.0f;
                    tc[0][1] = 0.0f;
                    tc[1][0] = 0.0f;
                    tc[1][1] = 0.0f;
                    tc[2][0] = 0.0f;
                    tc[2][1] = 0.0f;
                }
                else
                {
                    // Flip Y coord.
                    tc[0][0] = attrib.texcoords[2 * idx0.texcoord_index];
                    tc[0][1] = 1.0f - attrib.texcoords[2 * idx0.texcoord_index + 1];
                    tc[1][0] = attrib.texcoords[2 * idx1.texcoord_index];
                    tc[1][1] = 1.0f - attrib.texcoords[2 * idx1.texcoord_index + 1];
                    tc[2][0] = attrib.texcoords[2 * idx2.texcoord_index];
                    tc[2][1] = 1.0f - attrib.texcoords[2 * idx2.texcoord_index + 1];
                }
            }
            else
            {
                tc[0][0] = 0.0f;
                tc[0][1] = 0.0f;
                tc[1][0] = 0.0f;
                tc[1][1] = 0.0f;
                tc[2][0] = 0.0f;
                tc[2][1] = 0.0f;
            }

            float v[3][3]{};
            for (int k = 0; k < 3; k++)
            {
                int f0 = idx0.vertex_index;
                int f1 = idx1.vertex_index;
                int f2 = idx2.vertex_index;

                v[0][k] = attrib.vertices[3 * f0 + k];
                v[1][k] = attrib.vertices[3 * f1 + k];
                v[2][k] = attrib.vertices[3 * f2 + k];
            }

            float n[3][3]{};
            {
                bool invalid_normal_index = false;
                if (attrib.normals.size() > 0)
                {
                    int nf0 = idx0.normal_index;
                    int nf1 = idx1.normal_index;
                    int nf2 = idx2.normal_index;

                    if ((nf0 < 0) || (nf1 < 0) || (nf2 < 0))
                        // normal index is missing from this face.
                        invalid_normal_index = true;
                    else
                    {
                        for (int k = 0; k < 3; k++)
                        {
                            n[0][k] = attrib.normals[3 * nf0 + k];
                            n[1][k] = attrib.normals[3 * nf1 + k];
                            n[2][k] = attrib.normals[3 * nf2 + k];
                        }
                    }
                }
                else
                    invalid_normal_index = true;

                if (invalid_normal_index && !smoothVertexNormals.empty())
                {
                    // Use smoothing normals
                    int f0 = idx0.vertex_index;
                    int f1 = idx1.vertex_index;
                    int f2 = idx2.vertex_index;

                    if (f0 >= 0 && f1 >= 0 && f2 >= 0)
                    {
                        n[0][0] = smoothVertexNormals[f0].v[0];
                        n[0][1] = smoothVertexNormals[f0].v[1];
                        n[0][2] = smoothVertexNormals[f0].v[2];

                        n[1][0] = smoothVertexNormals[f1].v[0];
                        n[1][1] = smoothVertexNormals[f1].v[1];
                        n[1][2] = smoothVertexNormals[f1].v[2];

                        n[2][0] = smoothVertexNormals[f2].v[0];
                        n[2][1] = smoothVertexNormals[f2].v[1];
                        n[2][2] = smoothVertexNormals[f2].v[2];

                        invalid_normal_index = false;
                    }
                }

                if (invalid_normal_index)
                {
                    // compute geometric normal
                    CalcNormal(n[0], v[0], v[1], v[2]);
                    n[1][0] = n[0][0];
                    n[1][1] = n[0][1];
                    n[1][2] = n[0][2];
                    n[2][0] = n[0][0];
                    n[2][1] = n[0][1];
                    n[2][2] = n[0][2];
                }
            }

            float t[3][3]{};
            {
                CalcTangent(t[0], v[0], v[1], v[2], tc[0], tc[1], tc[2]);
                t[1][0] = t[0][0];
                t[1][1] = t[0][1];
                t[1][2] = t[0][2];
                t[2][0] = t[0][0];
                t[2][1] = t[0][1];
                t[2][2] = t[0][2];
            }

            for (int k = 0; k < 3; k++)
            {
                buffer.push_back(v[k][0]);
                buffer.push_back(v[k][1]);
                buffer.push_back(v[k][2]);

                buffer.push_back(n[k][0]);
                buffer.push_back(n[k][1]);
                buffer.push_back(n[k][2]);

                buffer.push_back(t[k][0]);
                buffer.push_back(t[k][1]);
                buffer.push_back(t[k][2]);

                buffer.push_back(tc[k][0]);
                buffer.push_back(tc[k][1]);
            }
        }

        // 3:vtx, 3:normal, 3:tangent, 2:texcoord
        element.numTriangles = buffer.size() / (3 + 3 + 3 + 2) / 3;

        GLuint vao = -1, vbo = -1;
        if (buffer.size() > 0)
        {
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(float), &buffer.at(0), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, 0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, (void*)(sizeof(float) * 3));
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, (void*)(sizeof(float) * 6));
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 11, (void*)(sizeof(float) * 9));
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);
            glEnableVertexAttribArray(3);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            element.vao = vao;
            element.vbo = vbo;
            obj.elements.push_back(element);
        }
    }

    if (id != -1 && id < mLoadedObjects.size())
        mLoadedObjects[id] = obj;
    else
        mLoadedObjects.push_back(obj);

    return true;
}

std::vector<PreviewerLoader::Object> Previewer::GetLoadedObjects() const
{
    return mLoadedObjects;
}

GLuint Previewer::LoadTexture(const std::string& file)
{
    GLuint tex = -1;
    Image texture(file);
    if (!texture.data())
        return tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width(), texture.height(), 0,
        GL_RGBA, GL_UNSIGNED_BYTE, texture.data());
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

void Previewer::FreeObject(int objId)
{
    if (objId >= mLoadedObjects.size())
        return;

    for (auto& element : mLoadedObjects[objId].elements)
    {
        GLuint& tex = element.normalTexId;
        if (tex != -1)
            glDeleteTextures(1, &tex);

        glDeleteBuffers(1, &element.vbo);
        glDeleteVertexArrays(1, &element.vao);
    }
}

void Previewer::SetNormalTextureForElement(int objId, int elementId, const std::string& file)
{
    if (objId >= mLoadedObjects.size())
        return;
    if (elementId >= mLoadedObjects[objId].elements.size())
        return;

    GLuint& tex = mLoadedObjects[objId].elements[elementId].normalTexId;
    if (tex != -1)
    {
        glDeleteTextures(1, &tex);
        tex = -1;
    }
    if (file.size() == 0)
        return;
    tex = LoadTexture(file);
    mLoadedObjects[objId].elements[elementId].normalTexFile = file;
}

void Previewer::SetMaterial(int objId, int elementId, const Material& m)
{
    if (objId >= mLoadedObjects.size())
        return;
    if (elementId >= mLoadedObjects[objId].elements.size())
        return;
    mLoadedObjects[objId].elements[elementId].material = m;
}

void Previewer::SetLocation(int objId, const glm::vec3& location)
{
    if (objId >= mLoadedObjects.size())
        return;
    mLoadedObjects[objId].SetLocation(location);
}

void Previewer::SetRotation(int objId, const glm::vec3& rotation)
{
    if (objId >= mLoadedObjects.size())
        return;

    float x = glm::mod(rotation.x, 360.0f);
    if (x < 0.0f)
        x += 360.0f;
    float y = glm::mod(rotation.y, 360.0f);
    if (y < 0.0f)
        y += 360.0f;
    float z = glm::mod(rotation.z, 360.0f);
    if (z < 0.0f)
        z += 360.0f;

    mLoadedObjects[objId].SetRotation(glm::vec3(x, y, z));
}

void Previewer::SetScale(int objId, const glm::vec3& scale)
{
    if (objId >= mLoadedObjects.size())
        return;

    float x = scale.x;
    if (x < 0.001f)
        x = 0.001f;
    float y = scale.y;
    if (y < 0.001f)
        y = 0.001f;
    float z = scale.z;
    if (z < 0.001f)
        z = 0.001f;

    if (mLoadedObjects[objId].isScaleLocked)
    {
        glm::vec3 scaleOld = mLoadedObjects[objId].GetScale();
        if (scaleOld.x != x)
        {
            y = scaleOld.y + scaleOld.y / scaleOld.x * (x - scaleOld.x);
            z = scaleOld.z + scaleOld.z / scaleOld.x * (x - scaleOld.x);
        }
        else if (scaleOld.y != y)
        {
            x = scaleOld.x + scaleOld.x / scaleOld.y * (y - scaleOld.y);
            z = scaleOld.z + scaleOld.z / scaleOld.y * (y - scaleOld.y);
        }
        else if (scaleOld.z != z)
        {
            x = scaleOld.x + scaleOld.x / scaleOld.z * (z - scaleOld.z);
            y = scaleOld.y + scaleOld.y / scaleOld.z * (z - scaleOld.z);
        }
    }

    mLoadedObjects[objId].SetScale(glm::vec3(x, y, z));
}

void Previewer::SendObjectsToPathTracer(PathTracer* pPathTracer)
{
    for (int i = 0; i < mLoadedObjects.size(); i++)
    {
        pPathTracer->LoadObject(mLoadedObjects[i].filename, mLoadedObjects[i].M);
        for (int j = 0; j < mLoadedObjects[i].elements.size(); j++)
        {
            pPathTracer->SetMaterial(i, j, mLoadedObjects[i].elements[j].material);
            if (mLoadedObjects[i].elements[j].normalTexId != -1)
            {
                pPathTracer->SetNormalTextureForElement(i, j,
                    mLoadedObjects[i].elements[j].normalTexFile);
            }
        }
    }
    pPathTracer->BuildBVH();
}

void Previewer::SetCamera(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& up)
{
    mCamPos = pos;
    mCamDir = glm::normalize(dir);
    mCamUp = glm::normalize(up);
}

void Previewer::SetProjection(float f, float fovy)
{
    mCamFocal = f;
    if (mCamFocal <= 0.0f)
        mCamFocal = 0.1f;
    mCamFovy = fovy;
    if (mCamFovy <= 0.0f)
        mCamFovy = 0.1f;
    else if (mCamFovy >= 180.0f)
        mCamFovy = 179.5;
}

const glm::vec3 Previewer::CameraPosition() const
{
    return mCamPos;
}

const glm::vec3 Previewer::CameraDirection() const
{
    return mCamDir;
}

const glm::vec3 Previewer::CameraUp() const
{
    return mCamUp;
}

const glm::vec3 Previewer::PreviewCameraPosition() const
{
    return glm::vec3(-mCamPos.x, mCamPos.y, mCamPos.z);
}

const glm::vec3 Previewer::PreviewCameraDirection() const
{
    return glm::vec3(-mCamDir.x, mCamDir.y, mCamDir.z);
}

const glm::vec3 Previewer::PreviewCameraUp() const
{
    return glm::vec3(-mCamUp.x, mCamUp.y, mCamUp.z);
}

const glm::vec3 Previewer::CameraRotation() const
{
    return mCamRot;
}

void Previewer::RotateCamera(const glm::vec3& rotation)
{
    float x = glm::mod(rotation.x, 360.0f);
    if (x < 0.0f)
        x += 360.0f;
    float y = glm::mod(rotation.y, 360.0f);
    if (y < 0.0f)
        y += 360.0f;
    float z = glm::mod(rotation.z, 360.0f);
    if (z < 0.0f)
        z += 360.0f;

    mCamRot = glm::vec3(x, y, z);

    glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), mCamRot.x, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), mCamRot.y, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), mCamRot.z, glm::vec3(0.0f, 0.0f, 1.0f));
    mCamDir = glm::normalize(glm::vec3(Rz * Ry * Rx * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)));
    mCamUp = glm::normalize(glm::vec3(Rz * Ry * Rx * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)));
}

const float Previewer::CameraFovy() const
{
    return mCamFovy;
}

const float Previewer::CameraFocal() const
{
    return mCamFocal;
}

void Previewer::SetPathTracerCamera(PathTracer* pPathTracer)
{
    pPathTracer->SetCamera(mCamPos, mCamDir, mCamUp);
    pPathTracer->SetProjection(mCamFocal, mCamFovy);
}

const int Previewer::GetTriangleCount() const
{
    int count = 0;
    for (auto& obj : mLoadedObjects)
    {
        for (auto& element : obj.elements)
            count += element.numTriangles;
    }
    return count;
}

void Previewer::Highlight(int objId, bool highlight)
{
    if (objId >= mLoadedObjects.size())
        return;

    for (auto& element : mLoadedObjects[objId].elements)
        element.highlight = highlight;
}

void Previewer::Highlight(int objId, int elementId, bool highlight)
{
    if (objId >= mLoadedObjects.size())
        return;
    if (elementId >= mLoadedObjects[objId].elements.size())
        return;

    mLoadedObjects[objId].elements[elementId].highlight = highlight;
}

void Previewer::SelectObject(int objId, bool select)
{
    if (objId >= mLoadedObjects.size())
        return;

    mLoadedObjects[objId].isSelected = select;
}

const int Previewer::GetNumSelectedObjects() const
{
    int res = 0;
    for (auto& obj : mLoadedObjects)
    {
        if (obj.isSelected)
            res++;
    }
    return res;
}

void Previewer::DeleteSelectedObjects()
{
    auto iter = mLoadedObjects.begin();
    while (iter != mLoadedObjects.end())
    {
        if (iter->isSelected)
        {
            FreeObject(iter - mLoadedObjects.begin());
            iter = mLoadedObjects.erase(iter);
        }
        else
            iter++;
    }
}

void Previewer::ReplaceSelectedObjectsWith(const std::string& file)
{
    for (int i = 0; i < mLoadedObjects.size(); i++)
    {
        if (mLoadedObjects[i].isSelected)
        {
            glm::vec3 location = mLoadedObjects[i].GetLocation();
            glm::vec3 rotaion = mLoadedObjects[i].GetRotation();
            glm::vec3 scale = mLoadedObjects[i].GetScale();
            FreeObject(i);
            LoadObject(file, i);
            mLoadedObjects[i].SetLocation(location);
            mLoadedObjects[i].SetRotation(rotaion);
            mLoadedObjects[i].SetScale(scale);
        }
    }
}

void Previewer::SetName(int objId, const std::string& name)
{
    if (objId >= mLoadedObjects.size())
        return;

    mLoadedObjects[objId].name = name;
}

void Previewer::SetName(int objId, int elementId, const std::string& name)
{
    if (objId >= mLoadedObjects.size())
        return;
    if (elementId >= mLoadedObjects[objId].elements.size())
        return;

    mLoadedObjects[objId].elements[elementId].name = name;
}

void Previewer::SetScaleLocked(int objId, bool locked)
{
    if (objId >= mLoadedObjects.size())
        return;

    mLoadedObjects[objId].isScaleLocked = locked;
}

void Previewer::ClearScene()
{
    while (!mLoadedObjects.empty())
    {
        FreeObject(0);
        mLoadedObjects.erase(mLoadedObjects.begin());
    }
}
