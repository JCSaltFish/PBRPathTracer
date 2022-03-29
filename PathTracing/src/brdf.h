#pragma once

#ifndef __BRDF_H__
#define __BRDF_H__

#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include <algorithm> 
#include <vector>

// glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/half_float.hpp>
#include <glm/gtx/compatibility.hpp>

//https://learnopengl.com/PBR/Theory 
//https://github.com/boksajak/brdf/blob/master/brdf.h
// physically based
// BRDF (bidirectional reflective distribution function) is a function that takes as input 
// the incoming(light) direction w, the outgoing(view) direction w, the surface normal n, and a surface parameter a that represents the microsurface's roughness.

struct Material {
    glm::vec3 base_color;
    float metalness;
    float roughness;

   /* glm::vec3 emissive;
    float transmissivness;
    float opacity;*/
};

struct Light {
    glm::vec3 color;
    glm::vec3 position;
};

const float MIN_DIELECTRICS_F0 = 0.04f; // base reflectivity (F0) - index of refraction


glm::vec3 eval_combined_direct_BRDF(const glm::vec3 point, const glm::vec3 surface_normal, const glm::vec3 camera_pos, const Material material, const std::vector<Light>& light_list);


#endif