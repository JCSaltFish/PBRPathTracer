#include "brdf.h"


// The Cook Torrance BRDF contains both a diffuse and specular part

// Trowbridge-Reitz GGX normal distribution function
// D  statistically approximates the relative surface area of microfacets exactly aligned to the (halfway) vector h
float GGX_distribution(float roughness, glm::vec3 normal, glm::vec3 half) {
    float a2 = roughness * roughness; // a^2 (numerator)
    // Clamp NdotS to prevent numerical instability.
    float n_dot_h = glm::clamp(glm::dot(normal, half), FLT_EPSILON, 1.0f); // make sure no negative in denominator
    // ((n . h)^2 * (a^2 -1) + 1)^2
    float b = ((a2 - 1.0f) * n_dot_h * n_dot_h + 1.0f);
    return a2 / (float(M_PI) * b * b);
}

// The geometry function statistically approximates the relative surface area where its micro surface-details overshadow each other, causing light rays to be occluded.
//The geometry function we will use is a combination of the GGX and Schlick-Beckmann approximation known as Schlick-GGX
float geometry_schlick_GGX(glm::vec3 normal, glm::vec3 view, float k) {
    // k is a remapping of a (roughness)
    float n_dot_v = glm::clamp(glm::dot(normal, view), FLT_EPSILON, 1.0f);
    float nom = n_dot_v;
    float denom = n_dot_v * (1.0f - k) + k;

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
    return ((a + 1.0f) * (a + 1.0f)) / 8.0f;
}

// image based
float remap_k_a_ibl(float a) {
    return (a * a) / 2.0f;
}


// fresnel
//ratio of light that gets reflected over the light that gets refracted, which varies over the angle we're looking at a surface
float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}


//pre-computing F0 for both dielectrics and conductors we can use the same Fresnel-Schlick approximation for both types
glm::vec3 base_color_to_specular(glm::vec3 base_Color, float metalness) {
    // eliminating index of refraction (n) and extinction (k) which are not intuitive 
    // using the /metalness workflow/ but doesn't allow diff specular color cos its being calculated from base color. another option is the specular workflow 
    /*std::cout << " base_Color: " << base_Color.x << " " << base_Color.y << " " << base_Color.z << std::endl;
    std::cout << " metalness: " << metalness << std::endl;*/
    base_Color = glm::vec3(0.5f);
   // return glm::lerp(glm::vec3(MIN_DIELECTRICS_F0), base_Color, metalness);
    return glm::vec3(lerp(MIN_DIELECTRICS_F0, 1.0, metalness)); // THEN WE MULTIPLY COLOR LATER
}

glm::vec3 fresnel_schlick(glm::vec3 half, glm::vec3 view, glm::vec3 f0)
{
    float cos_theta = std::max(glm::dot(half, view), FLT_EPSILON);
    // clamp 1.0f - cos_theta to 0-1 to prevent black spots
    cos_theta = glm::clamp(cos_theta, 0.0f, 1.0f);
    return f0 + (1.0f - f0) * pow(1.0f - cos_theta, 5.0f);
}

float float_fresnel_schlick(glm::vec3 normal, glm::vec3 view, float n)
{
    float ff0 = pow((1 - n) / (1 + n), 2);
    float cos_theta = std::max(glm::dot(normal, view), FLT_EPSILON);
    // clamp 1.0f - cos_theta to 0-1 to prevent black spots
    cos_theta = glm::clamp(cos_theta, 0.0f, 1.0f);
    return ff0 + (1.0f - ff0) * pow(1.0f - cos_theta, 5.0f);
}

// all unit vector inputs
float eval_cook_torrance_specular_brdf(float d, float g, float f, glm::vec3 normal, glm::vec3 view, glm::vec3 light_dir) {
    float numerator = d * g * f;
    // add epsilon to prevent division by 0
    float denominator = 4.0f * std::max(glm::dot(normal, view), FLT_EPSILON) * std::max(glm::dot(normal, light_dir), FLT_EPSILON) + FLT_EPSILON;
    return numerator / denominator;

}

bool on_hemisphere(const Intersect_data& int_data, const glm::vec3 incoming, const glm::vec3 outgoing) {
    // use this function to see if should calculate BRDF (if on the right side of the hemisphere)

    float n_dot_l = std::max(glm::dot(int_data.surf_normal, incoming), FLT_EPSILON);
    float n_dot_v = std::max(glm::dot(int_data.surf_normal, outgoing), FLT_EPSILON);

    bool v_back_facing = (n_dot_v <= 0.0f);
    bool l_back_facing = (n_dot_l <= 0.0f);
    return !(v_back_facing || l_back_facing);
}

// incoming_dir and out_dir sampled from hemisphere
// incoming_dir is initially from light because we want to know how much light is reflected on point, out is view direction  
glm::vec3 eval_direct_BRDF(const Intersect_data& int_data, const glm::vec3 incoming, const glm::vec3 outgoing) {
    // light dir (unit vector pointing toward light source)
    glm::vec3 incoming_dir = glm::normalize(incoming);
    // view dir (flipped from brdf's perspective)
    glm::vec3 flipped_out = -outgoing;
    glm::vec3 out_dir = glm::normalize(-outgoing);

    // get cook-torrance specular term for each light

    // lighting variables
    glm::vec3 half = glm::normalize(flipped_out + incoming); // approximate microsurfaces 

    // fresnel: how much light should be reflected
    // F0 surface reflection at zero incidence or how much the surface reflects if looking directly at the surface
    glm::vec3 f0 = base_color_to_specular(int_data.material.base_color, int_data.material.metalness);

    //glm::vec3 f_term = fresnel_schlick(int_data.surf_normal, -outgoing, f0);
    float ff_term = float_fresnel_schlick(int_data.surf_normal, flipped_out, int_data.material.metalness);

    // D normal distribution term
    float d_term = GGX_distribution(int_data.material.roughness, int_data.surf_normal, half);

    // G geometry attenuation term
    float g_term = geometry_smith(int_data.surf_normal, out_dir, incoming_dir, int_data.material.roughness);

    float diffuse = 1.0f / float(M_PI);

    // cooktorrance specular BRDF
    float reflected = ff_term;
    float refracted = 1.0f - reflected; 
    refracted *= 1.0 - int_data.material.metalness; // metallic dont refract so no diffuse
    glm::vec3 specular = eval_cook_torrance_specular_brdf(d_term, g_term, ff_term, int_data.surf_normal, out_dir, incoming_dir) * int_data.material.base_color;

    return ((refracted * int_data.material.base_color / float(M_PI)) + specular);
}



// indirect lighitng will be from cube map if any

/*
Important implementation detail is that the Fresnel term must be evaluated for the normal of
the sampled microfacet (half vector), rather than the surface (or shading) normal, otherwise the
roughness of the material would be essentially ignored for Fresnel term and object would appear more
reflective than it is.
*/