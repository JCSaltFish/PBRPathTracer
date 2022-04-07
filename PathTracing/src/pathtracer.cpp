#define _USE_MATH_DEFINES
#include <math.h>
#include <omp.h>

#include "pathtracer.h"
#include "OBJ_Loader.h"

bool AABB::Intersect(glm::vec3 ro, glm::vec3 rd)
{
	glm::vec3 tMin = (min - ro) / rd;
	glm::vec3 tMax = (max - ro) / rd;
	glm::vec3 t1 = glm::min(tMin, tMax);
	glm::vec3 t2 = glm::max(tMin, tMax);
	float tNear = glm::max(glm::max(t1.x, t1.y), t1.z);
	float tFar = glm::min(glm::min(t2.x, t2.y), t2.z);
	if (tNear >= tFar)
		return false;
	return true;
}

PathTracer::PathTracer()
{
	m_outImg = 0;
	m_totalImg = 0;
	m_maxDepth = 10;

	m_camDir = glm::vec3(0.0f, 0.0f, 1.0f);
	m_camUp = glm::vec3(0.0f, 1.0f, 0.0f);
	m_camFocal = 0.1f;
	m_camFovy = 90;

	samples = 0;
}

PathTracer::~PathTracer()
{
	if (m_totalImg)
		delete[] m_totalImg;
}

void PathTracer::BuildAABB(glm::vec3 v, AABB& aabb)
{
	float minX = aabb.min.x;
	float minY = aabb.min.y;
	float minZ = aabb.min.z;
	float maxX = aabb.max.x;
	float maxY = aabb.max.y;
	float maxZ = aabb.max.z;

	if (v.x < minX)
		minX = v.x;
	if (v.y < minY)
		minY = v.y;
	if (v.z < minZ)
		minZ = v.z;
	if (v.x > maxX)
		maxX = v.x;
	if (v.y > maxY)
		maxY = v.y;
	if (v.z > maxZ)
		maxZ = v.z;

	aabb.min = glm::vec3(minX, minY, minZ);
	aabb.max = glm::vec3(maxX, maxY, maxZ);
}

int PathTracer::LoadMesh(std::string file, glm::mat4 model, bool ccw)
{
	int id = -1;

	objl::Loader loader;
	if (loader.LoadFile(file))
	{
        Mesh mesh;

		for (int i = 0; i < loader.LoadedIndices.size(); i += 3)
		{
			MeshTriangle t;
			int index = loader.LoadedIndices[i];
			t.v1 = glm::vec3(loader.LoadedVertices[index].Position.X,
				loader.LoadedVertices[index].Position.Y,
				loader.LoadedVertices[index].Position.Z);
			t.v1 = glm::vec3(model * glm::vec4(t.v1, 1.0f));
			BuildAABB(t.v1, mesh.aabb);

			index = loader.LoadedIndices[i + 1];
			t.v2 = glm::vec3(loader.LoadedVertices[index].Position.X,
				loader.LoadedVertices[index].Position.Y,
				loader.LoadedVertices[index].Position.Z);
			t.v2 = glm::vec3(model * glm::vec4(t.v2, 1.0f));
			BuildAABB(t.v2, mesh.aabb);

			index = loader.LoadedIndices[i + 2];
			t.v3 = glm::vec3(loader.LoadedVertices[index].Position.X,
				loader.LoadedVertices[index].Position.Y,
				loader.LoadedVertices[index].Position.Z);
			t.v3 = glm::vec3(model * glm::vec4(t.v3, 1.0f));
			BuildAABB(t.v3, mesh.aabb);
			
			// normal
			glm::vec3 e1 = glm::normalize(t.v2 - t.v1);
			glm::vec3 e2 = glm::normalize(t.v3 - t.v1);
			t.normal = glm::normalize(glm::cross(e1, e2));
			if (ccw)
				t.normal = -t.normal;

            mesh.triangles.push_back(t);
		}
		id = scene_mesh.size();
        scene_mesh.push_back(mesh);
	}

	return id;
}

void PathTracer::SetMeshMaterial(int id, Material m)
{
	if (id >= 0 && id < scene_mesh.size())
		scene_mesh[id].material = m;
}

void PathTracer::SetOutImage(GLubyte* out)
{
	m_outImg = out;
}

void PathTracer::SetResolution(glm::ivec2 res)
{
	m_imgResolution = res;

	m_totalImg = new float[res.x * res.y * 3];
}

glm::ivec2 PathTracer::GetResolution()
{
	return m_imgResolution;
}

void PathTracer::SetCamera(glm::vec3 pos, glm::vec3 dir, glm::vec3 up)
{
	m_camPos = pos;
	m_camDir = glm::normalize(dir);
	m_camUp = glm::normalize(up);
}

void PathTracer::SetProjection(float f, float fovy)
{
	m_camFocal = f;
	if (m_camFocal <= 0.0f)
		m_camFocal = 0.1f;
	m_camFovy = fovy;
	if (m_camFovy <= 0.0f)
		m_camFovy = 0.1f;
	else if (m_camFovy >= 180.0f)
		m_camFovy = 179.5;
}

bool PathTracer::Intersect(glm::vec3 ro, glm::vec3 rd, MeshTriangle t, float& distOut)
{
	bool res = false;
	if (glm::dot(rd, t.normal) == 0.0f)
		return res;
	distOut = glm::dot((t.v1 - ro), t.normal) / glm::dot(rd, t.normal);
	if (distOut < 0)
		return res;
    // update intersection point
	glm::vec3 p = ro + rd * distOut;
    
	// If the sum of all the angles is approximately equal to 360 degrees
	// then the intersection is in the triangle.
	float r = glm::acos(glm::dot(glm::normalize(t.v1 - p), glm::normalize(t.v2 - p)));
	r += glm::acos(glm::dot(glm::normalize(t.v2 - p), glm::normalize(t.v3 - p)));
	r += glm::acos(glm::dot(glm::normalize(t.v3 - p), glm::normalize(t.v1 - p)));
    if (abs(r - 2.0f * (float)M_PI) < EPS)
        res = true;

	return res;
}

int PathTracer::GetSamples()
{
	return samples;
}

float PathTracer::Rand(glm::vec2 co, float& seed)
{
	seed++;
	return glm::fract(sinf(seed / samples * glm::dot(co, glm::vec2(12.9898f, 78.233f))) * 43758.5453f);
}

glm::vec3 PathTracer::Trace(glm::vec3 ro, glm::vec3 rd, glm::vec2 raySeed, float& randSeed, int depth)
{
	float dist = INF;
	int mid = -1;
	int tid = -1;
	for (int i = 0; i < scene_mesh.size(); i++)
	{
		if (!scene_mesh[i].aabb.Intersect(ro, rd))
			continue;

        std::vector<MeshTriangle> triangles = scene_mesh[i].triangles;
		
        for (int t = 0; t < triangles.size(); t++) {
            float d = 0.0f;
            if (Intersect(ro, rd, triangles[t], d))
            {
                if (d < dist)
                {
                    // closest to viewer (to render)
                    dist = d;
                    tid = t;
					mid = i;
                }
            }
        }
	}

	if (tid != -1 && mid != -1)
	{
		Intersect_data intersect_d;
		intersect_d.surf_normal = scene_mesh[mid].triangles[tid].normal;
		intersect_d.point = ro + rd * dist;
		intersect_d.point += intersect_d.surf_normal * EPS;
		intersect_d.material = scene_mesh[mid].material;

		//if (glm::length(intersect_d.material.emissive) != 0.0f)
			//return intersect_d.material.emissive;

        if (depth < m_maxDepth * 2)
		{ 
			depth++;
			// Russian Roulette Path Termination
			float p = fmin(0.95f, fmax(fmax(intersect_d.material.base_color.x, intersect_d.material.base_color.y), intersect_d.material.base_color.z));
			if (depth >= m_maxDepth)
			{
				if (fabs(Rand(raySeed, randSeed)) > p)
					return intersect_d.material.emissive;
			}

            glm::vec3 r = glm::reflect(rd, intersect_d.surf_normal);
            glm::vec3 reflectDir;
            //if (intersect_d.material.reflect_type == SPECULAR) {
                //reflectDir = r;
            //}
            //else if (intersect_d.material.reflect_type == DIFFUSE) {
                // Monte Carlo Integration
                glm::vec3 n = intersect_d.surf_normal;
                glm::vec3 u = glm::abs(n.x) < 1.0f - FLT_EPSILON ? glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), n) : glm::cross(glm::vec3(1.0f), n);
                u = glm::normalize(u);
                glm::vec3 v = glm::normalize(glm::cross(u, n));
                float w = Rand(raySeed, randSeed), theta = Rand(raySeed, randSeed);
                // uniformly sampling on hemisphere
				reflectDir = w * cosf(2.0f * M_PI * theta) * u + w * sinf(2.0f * M_PI * theta) * v + glm::sqrt(1.0f - w * w) * n;
				reflectDir = glm::normalize(reflectDir);
            //}
            //else if (intersect_d.material.reflect_type == GLOSSY)
			//{
                // Monte Carlo Integration
                //glm::vec3 n = intersect_d.surf_normal;
                //glm::vec3 u = glm::abs(n.x) < 1 - FLT_EPSILON ? glm::cross(glm::vec3(1, 0, 0), r) : glm::cross(glm::vec3(1), r);
                //u = glm::normalize(u);
                //glm::vec3 v = glm::cross(u, r);
                //float w = Rand(raySeed, randSeed) * intersect_d.material.roughness, theta = Rand(raySeed, randSeed);
                //// uniformly sampling on hemisphere
                //reflectDir = w * cosf(2 * M_PI * theta) * u + w * sinf(2 * M_PI * theta) * v + glm::sqrt(1 - w * w) * r;
            //}

            // distance from last intersection point 
            //float attenuation = 0.0;
            //if (next_int.point != glm::vec3(-INF)) {
            //    float distance = glm::length(next_int.point - intersect_d.point) + FLT_EPSILON; // will be 0 if primary ray because the prev intersection is itself
            //    attenuation = 1.0f / (distance * distance);
            //}
            // last intersection's material color  (NO NEED COS COLOR IS IN BRDF)
            //glm::vec3 radiance = intersect_d.material.base_color * attenuation;
            float n_dot_l = glm::dot(intersect_d.surf_normal, reflectDir);
 
            // will always return light color if hits the light so this is kd
            // reflectDir points out
            // reflectDir is incoming ray from brdf's perspective
            // return  eval_direct_BRDF(intersect_d, reflectDir, rd);
            float light_intensity = 18.0f;
            //only flip view
            //return light_intensity * Trace(intersect_d.point, reflectDir, seed, depth) * eval_direct_BRDF(intersect_d, reflectDir, rd) * n_dot_l;
			glm::vec3 result = intersect_d.material.emissive + Trace(intersect_d.point, reflectDir, raySeed, randSeed, depth) * intersect_d.material.base_color;
			//result = glm::clamp(result, glm::vec3(0.0f), glm::vec3(1.0f));
			return result;
			//return Trace(intersect_d.point, reflectDir, seed, depth);
		}

		//glm::vec3 radiance = eval_direct_BRDF(intersect_d, reflectDir, rd);

        //glm::vec3 view_dir = glm::normalize(m_camPos - intersect_d.point);

        //// TODO: per surface or per vertex lighting? 
        //// loop through lights in scene 
        //glm::vec3 out_radiance = glm::vec3(0.0);
        //for (int l = 0; l < scene_lights.size(); l++) {
        //    // if ray hits lights 
        //    glm::vec3 light_dir = glm::normalize(scene_lights[l].position - intersect_d.point);
        //    
        //    if (on_hemisphere(intersect_d, light_dir, view_dir)) {

        //        float distance = glm::length(scene_lights[l].position - intersect_d.point);
        //        float attenuation = 1.0f / (distance * distance);
        //        glm::vec3 radiance = scene_lights[l].color * attenuation;
        //        float n_dot_l = std::max(glm::dot(intersect_d.surf_normal, light_dir), FLT_EPSILON);

        //        // cook_torrance specular
        //        // From this ratio of reflection and the energy conservation principle we can directly obtain the refracted portion of light. , refracted = 1-f
        //        glm::vec3 specular = eval_direct_BRDF(intersect_d, light_dir, view_dir); 
        //        glm::vec3 refraction = glm::vec3(1.0f) - specular;

        //        // if we have metallic: 
        //        refraction *= 1.0 - intersect_d.material.metalness;

        //        // BRDF is integration over hemisphere (this includes diffuse)
        //        out_radiance += (refraction * intersect_d.material.base_color / float(M_PI) + specular) * radiance * n_dot_l;
        //        // somehow needs to do: sum += eval_direct_BRDF(intersect_d, light_dir, view_dir) * L(P, Wi) * dot(N, Wi) * dW;
        //    }
        //    
        //}

        //// TODO: remove 5.0f this is to just make it more visible with less lights
        //return out_radiance * 20.0f;
	}

	return glm::vec3(0.0f);
}

void PathTracer::RenderFrame()
{
	samples++;
	//samples = 20;

	// Position world space image plane
	glm::vec3 imgCenter = m_camPos + m_camDir * m_camFocal;
	float imgHeight = 2.0f * m_camFocal * tan((m_camFovy / 2.0f) * M_PI / 180.0f);
	float aspect = (float)m_imgResolution.x / (float)m_imgResolution.y;
	float imgWidth = imgHeight * aspect;
	float deltaX = imgWidth / (float)m_imgResolution.x;
	float deltaY = imgHeight / (float)m_imgResolution.y;
	glm::vec3 camRight = glm::normalize(glm::cross(m_camUp, m_camDir));

	// Starting at top left
	glm::vec3 topLeft = imgCenter - camRight * (imgWidth * 0.5f);
	topLeft += m_camUp * (imgHeight * 0.5f);

	int numThreads = omp_get_max_threads();
	if (numThreads > 2)
		numThreads -= 3;
	else if (numThreads > 1)
		numThreads -= 2;
	else if (numThreads > 0)
		numThreads--;
	// Loop through each pixel
	#pragma omp parallel for num_threads(numThreads)
    for (int i = 0; i < m_imgResolution.y; i++)
    {
        glm::vec3 pixel = topLeft - m_camUp * ((float)i * deltaY);
        for (int j = 0; j < m_imgResolution.x; j++)
        {
            glm::vec3 rayDir = glm::normalize(pixel - m_camPos);
			float seed = 0.0f;
            glm::vec3 color = Trace(m_camPos, rayDir, glm::vec2(i, j), seed);

			// Draw
			int imgPixel = ((m_imgResolution.y - 1 - i) * m_imgResolution.x + j) * 3;

			m_totalImg[imgPixel] += color.r;
			m_totalImg[imgPixel + 1] += color.g;
			m_totalImg[imgPixel + 2] += color.b;

			glm::vec3 res = glm::vec3(m_totalImg[imgPixel] / samples,
				m_totalImg[imgPixel + 1] / samples,
				m_totalImg[imgPixel + 2] / samples);
			res = glm::clamp(res, glm::vec3(0.0f), glm::vec3(1.0f));

			m_outImg[imgPixel] = res.r * 255;
			m_outImg[imgPixel + 1] = res.g * 255;
			m_outImg[imgPixel + 2] = res.b * 255;

			pixel += camRight * deltaX;
		}
	}
}
