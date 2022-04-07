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

enum rtype { DIFFUSE, SPECULAR, GLOSSY};

struct Material {
    glm::vec3 base_color;
    float metalness;
    float roughness;
    rtype reflect_type;
   glm::vec3 emissive;
   /* float transmissivness;
    float opacity;*/
};

struct Light {
    glm::vec3 color;
    glm::vec3 position;
};

struct Intersect_data {
    glm::vec3 point;
    glm::vec3 surf_normal; //normalized
    Material material;
};

// 4% most common used by UE4, but water is 2% TODO: try changing to 2 . diamond is 1.8% but now we are clamping to 0-1
const float MIN_DIELECTRICS_F0 = 0.04f; // base reflectivity (F0) - index of refraction

bool on_hemisphere(const Intersect_data& int_data, const glm::vec3 incoming, const glm::vec3 outgoing);
glm::vec3 eval_direct_BRDF(const Intersect_data& int_data, const glm::vec3 incoming_dir, const glm::vec3 out_dir);


#endif