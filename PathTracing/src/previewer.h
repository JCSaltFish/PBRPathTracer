#ifndef __PREVIEWER_H__
#define __PREVIEWER_H__

#include <string>
#include <vector>
#include <map>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <tiny_obj_loader.h>

#include "pathtracer.h"

namespace PreviewerLoader
{
    struct Vec3
    {
        float v[3];
        Vec3()
        {
            v[0] = 0.0f;
            v[1] = 0.0f;
            v[2] = 0.0f;
        }
    };

    struct Element
    {
        std::string name;
        GLuint vao;
        GLuint vbo;
        int numTriangles;
        GLuint normalTexId;
        std::string normalTexFile;
        Material material;
        bool highlight;

        Element(const std::string& name)
        {
            this->name = name;
            vbo = -1;
            numTriangles = 0;
            normalTexId = -1;
            normalTexFile = "";
            highlight = false;
        }
    };

    struct Object
    {
        std::string name;
        std::string filename;
        std::vector<Element> elements;

        glm::mat4 M;
        glm::mat4 Mpreview;

        bool isSelected;
        bool isScaleLocked;

    private:
        glm::vec3 location;
        glm::vec3 rotation;
        glm::vec3 scale;

    public:
        Object(const std::string& name)
        {
            this->name = name;
            M = glm::mat4(1.0f);
            scale = glm::vec3(1.0f);

            isSelected = false;
            isScaleLocked = true;
        }

    private:
        void UpdateMatrix()
        {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), location);
            glm::mat4 R = glm::rotate(T, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            R = glm::rotate(R, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            R = glm::rotate(R, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat4 S = glm::scale(R, scale);
            M = S;

            glm::vec3 location_preview = glm::vec3(-location.x, location.y, location.z);
            glm::mat4 Tpreview = glm::translate(glm::mat4(1.0f), location_preview);
            glm::vec3 rotation_preview = glm::vec3(rotation.x, -rotation.y, -rotation.z);
            glm::mat4 Rpreview = glm::rotate(Tpreview, rotation_preview.x, glm::vec3(1.0f, 0.0f, 0.0f));
            Rpreview = glm::rotate(Rpreview, rotation_preview.y, glm::vec3(0.0f, 1.0f, 0.0f));
            Rpreview = glm::rotate(Rpreview, rotation_preview.z, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat4 Spreview = glm::scale(Rpreview, scale);
            Mpreview = Spreview;
        }

    public:
        const glm::vec3 GetLocation() const
        {
            return location;
        }
        const glm::vec3 GetRotation() const
        {
            return rotation;
        }
        const glm::vec3 GetScale() const
        {
            return scale;
        }

        void SetLocation(const glm::vec3& v)
        {
            location = v;
            UpdateMatrix();
        }
        void SetRotation(const glm::vec3& v)
        {
            rotation = v;
            UpdateMatrix();
        }
        void SetScale(const glm::vec3& v)
        {
            scale = v;
            UpdateMatrix();
        }
    };
}

class Previewer
{
private:
    std::vector<PreviewerLoader::Object> mLoadedObjects;

    glm::vec3 mCamPos;
    glm::vec3 mCamDir;
    glm::vec3 mCamUp;

    glm::vec3 mCamRot;

    float mCamFocal;
    float mCamFovy;

public:
    Previewer();
    ~Previewer();

private:
    void Normalize(PreviewerLoader::Vec3& v);
    void CalcNormal(float N[3], float v0[3], float v1[3], float v2[3]);
    void CalcTangent
    (
        float T[3], float v0[3], float v1[3], float v2[3], float tc0[3], float tc1[3], float tc2[3]
    );

    const bool HasSmoothingGroup(const tinyobj::shape_t& shape);

    void ComputeSmoothingNormals
    (
        const tinyobj::attrib_t& attrib,
        const tinyobj::shape_t& shape,
        std::map<int, PreviewerLoader::Vec3>& smoothVertexNormals
    );
    void ComputeAllSmoothingNormals
    (
        tinyobj::attrib_t& attrib,
        std::vector<tinyobj::shape_t>& shapes
    );
    void ComputeSmoothingShape
    (
        tinyobj::attrib_t& inattrib,
        tinyobj::shape_t& inshape,
        std::vector<std::pair<unsigned int, unsigned int>>& sortedids,
        unsigned int idbegin,
        unsigned int idend,
        std::vector<tinyobj::shape_t>& outshapes,
        tinyobj::attrib_t& outattrib
    );
    void ComputeSmoothingShapes
    (
        tinyobj::attrib_t& inattrib,
        std::vector<tinyobj::shape_t>& inshapes,
        std::vector<tinyobj::shape_t>& outshapes,
        tinyobj::attrib_t& outattrib
    );

    GLuint LoadTexture(const std::string& file);

    void FreeObject(int objId);

public:
    const bool LoadObject(const std::string& filename, int id = -1);
    std::vector<PreviewerLoader::Object> GetLoadedObjects() const;
    void SetNormalTextureForElement(int objId, int elementId, const std::string& file);
    void SetMaterial(int objId, int elementId, const Material& m);

    void SetLocation(int objId, const glm::vec3& location);
    void SetRotation(int objId, const glm::vec3& rotation);
    void SetScale(int objId, const glm::vec3& scale);

    void SendObjectsToPathTracer(PathTracer* pPathTracer);

    void SetCamera(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& up);
    void SetProjection(float f, float fovy);

    const glm::vec3 CameraPosition() const;
    const glm::vec3 CameraDirection() const;
    const glm::vec3 CameraUp() const;
    const float CameraFovy() const;
    const float CameraFocal() const;
    const glm::vec3 CameraRotation() const;
    void RotateCamera(const glm::vec3& rotation);

    const glm::vec3 PreviewCameraPosition() const;
    const glm::vec3 PreviewCameraDirection() const;
    const glm::vec3 PreviewCameraUp() const;

    void SetPathTracerCamera(PathTracer* pPathTracer);

    const int GetTriangleCount() const;

    void Highlight(int objId, bool highlight);
    void Highlight(int objId, int elementId, bool highlight);

    void SelectObject(int objId, bool select);
    const int GetNumSelectedObjects() const;

    void DeleteSelectedObjects();
    void ReplaceSelectedObjectsWith(const std::string& file);

    void SetName(int objId, const std::string& name);
    void SetName(int objId, int elementId, const std::string& name);
    void SetScaleLocked(int objId, bool locked);

    void ClearScene();
};

#endif
