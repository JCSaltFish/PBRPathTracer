#pragma once

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


// The Cook Torrance BRDF contains both a diffuse and specular part

// Trowbridge-Reitz GGX normal distribution function
// D  statistically approximates the relative surface area of microfacets exactly aligned to the (halfway) vector h
float GGX_distribution(float roughness, glm::vec3 normal, glm::vec3 half) {
    float a2 = roughness * roughness; // a^2 (numerator)
    // Clamp NdotS to prevent numerical instability.
    float n_dot_h = glm::clamp(glm::dot(normal, half), FLT_EPSILON, 1.0f); // make sure no negative in denominator
    // ((n . h)^2 * (a^2 -1) + 1)^2
    float b = ((a2 - 1.0f) * n_dot_h * n_dot_h + 1.0f);
    return a2 / (M_PI * b * b);
}

// The geometry function statistically approximates the relative surface area where its micro surface-details overshadow each other, causing light rays to be occluded.
//The geometry function we will use is a combination of the GGX and Schlick-Beckmann approximation known as Schlick-GGX
float geometry_schlick_GGX(glm::vec3 normal, glm::vec3 view, float k){ 
    // k is a remapping of a (roughness)
    float n_dot_v = glm::clamp(glm::dot(normal, view), FLT_EPSILON, 1.0f);
    float nom = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;

    return nom / denom;
}
//The geometry function is a multiplier between [0.0, 1.0] with 1.0 (or white) measuring no microfacet shadowing, and 0.0 (or black) complete microfacet shadowing.
float geometry_smith(glm::vec3 normal, glm::vec3 view, glm::vec3 light, float k)
{
    // k = roughness
    float ggx1 = geometry_schlick_GGX(normal, view, k);
    float ggx2 = geometry_schlick_GGX(normal, light, k);

    return ggx1 * ggx2;
}

// TODO: CHECK IF WE NEED TO REMAP OUR VALUES
// direct lighting
float remap_k_a_direct(float a) {
    return ((a + 1.0) * (a + 1.0)) / 8.0;
}

// image based
float remap_k_a_ibl(float a) {
    return (a * a) / 2.0;
}


// fresnel
//ratio of light that gets reflected over the light that gets refracted, which varies over the angle we're looking at a surface

//pre-computing F0 for both dielectrics and conductors we can use the same Fresnel-Schlick approximation for both types
float MIN_DIELECTRICS_F0 = 0.04f; // base reflectivity (F0) - index of refraction
glm::vec3 base_color_to_specular(glm::vec3 base_Color, float metalness) {
    return glm::lerp(glm::vec3(MIN_DIELECTRICS_F0), base_Color, metalness);
}

glm::vec3 fresnel_schlick(glm::vec3 half, glm::vec3 view, glm::vec3 f0)
{
    float cos_theta = std::max(glm::dot(half, view), FLT_EPSILON);
    // clamp 1.0f - cos_theta to 0-1 to prevent black spots
    cos_theta = glm::clamp(cos_theta, 0.0f, 1.0f);
    return f0 + (1.0f - f0) * pow(1.0f - cos_theta, 5.0f);
}

glm::vec3 eval_cook_torrance_specular_brdf(float d, float g, glm::vec3 f, glm::vec3 normal, glm::vec3 view, glm::vec3 light_dir) {
    glm::vec3 numerator = d * g * f;
    // add epsilon to prevent division by 0
    float denominator = 4.0f * std::max(glm::dot(normal, view), FLT_EPSILON) * std::max(glm::dot(normal, light_dir), FLT_EPSILON) + FLT_EPSILON;
    return numerator / denominator;

}

int NUM_LIGHT_SOURCE = 0; // light source count
std::vector<glm::vec3> light; // or store lights in list

// material properties:
// albedo, metallic, roughness, ao(ambience)

// pass in position to calculate for 
void eval_combined_direct_BRDF(glm::vec3 point, glm::vec3 surface_normal, glm::vec3 camera_pos, Material material) {
    // loop over each light source, calculate individual radian and sum its contribution scaled y the BRDF and the light's incident angle 
    // solving the integral over angles for direct light sources
    glm::vec3 out_radiance = glm::vec3(0.0);

    // TODO: MAYBE MOVE THESE OUT BEcAUSE THE INDIRECT ALSO NEEDS THEM 
    glm::vec3 normal = glm::normalize(surface_normal);
    glm::vec3 view_dir = glm::normalize(camera_pos - point);

    for (int i = 0; i < NUM_LIGHT_SOURCE; i++) {

        glm::vec3 light_dir = glm::normalize(light[i] - point);

        // check direction
        // Ignore V and L rays "below" the hemisphere
        // TODO: speed up by precalculating the common dot product terms
        float n_dot_l = glm::dot(normal, light_dir);
        float n_dot_v = glm::dot(normal, view_dir);
        bool v_back_facing = (n_dot_v <= 0.0f);
        bool l_back_facing = (n_dot_l <= 0.0f);
        if (v_back_facing || l_back_facing) {
            continue;
        }

        // precaculate lighting variables
        glm::vec3 half = glm::normalize(view_dir + light_dir);
        float distance = glm::length(light[i] - point);
        float attenuation = 1.0 / (distance * distance);
        glm::vec3 radiance = light[i].color * attenuation; 

        // get cook-torrance specular term for each light

        // fresnel
        // F0 surface reflection at zero incidence or how much the surface reflects if looking directly at the surface
        glm::vec3 f0 = base_color_to_specular(material.base_Color, material.metalness);
        glm::vec3 f_term = fresnel_schlick(half, view_dir, f0);

        // D normal distribution term
        float d_term = GGX_distribution(material.roughness, normal, half);

        // G geometry attenuation term
        float g_term = geometry_smith(normal, view_dir, light_dir, material.roughness);

        // cooktorrance specular BRDF
        glm::vec3 specular = eval_cook_torrance_specular_brdf(d_term, g_term, f_term, normal, view_dir, light_dir);

        // From this ratio of reflection and the energy conservation principle we can directly obtain the refracted portion of light. , refracted = 1-f
        glm::vec3 refraction = glm::vec3(1.0) - specular;

        // if we have metallic: 
        // refraction *= 1.0 - material.metallic;

        float n_dot_l = std::max(glm::dot(normal, light_dir), FLT_EPSILON);
         
        // includes diffuse 
        out_radiance += (refraction * material.albedo / M_PI + specular) * radiance * n_dot_l;

    }

    // TODO: add ambience at the end?
    //glm::vec3 ambient = glm::vec3(0.03) * material.albedo * material.ao;
    //glm::vec3 color = ambient + out_radiance;

    // GAMMA CORRECTION? 
    // color = color / (color + glm::vec3(1.0));
    // color = pow(color, glm::vec3(1.0 / 2.2));
}

// indirect lighitng will be from cube map if any