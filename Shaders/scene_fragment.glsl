#version 330 core

const float PI = 3.1415926535897932384626433832795;

uniform vec3 light_color;
uniform vec3 light_position;
uniform vec3 light_direction;

uniform vec3  camLight_position;
uniform vec3  camLight_direction;
uniform vec3  camLight_color;
uniform float camLight_cutoff_inner;
uniform float camLight_cutoff_outer;
uniform float camLight_intensity;

uniform vec3 object_color;   // tinting

const float shading_ambient_strength    = 0.05;
const float shading_diffuse_strength    = 0.5;
const float shading_specular_strength   = 0.9;

uniform float light_cutoff_outer;
uniform float light_cutoff_inner;
uniform float light_near_plane;
uniform float light_far_plane;



uniform vec3 view_position;
uniform sampler2D cam_shadow_map;  // bind on unit 2
uniform sampler2D shadow_map;  // bind on unit 1
uniform sampler2D albedo_tex;  // bind on unit 0

uniform vec2 uv_scale;

in vec3 fragment_position;
in vec4 fragment_position_light_space;
in vec4 fragment_position_camLight_space;
in vec3 fragment_normal;
in vec2 vUV;                   // from vertex shader

out vec4 result;

vec3 ambient_color(vec3 light_color_arg) {
    return shading_ambient_strength * light_color_arg;
}

vec3 diffuse_color(vec3 light_color_arg, vec3 light_position_arg) {
    vec3 L = normalize(light_position_arg - fragment_position);
    return shading_diffuse_strength * light_color_arg * max(dot(normalize(fragment_normal), L), 0.0);
}

vec3 specular_color(vec3 light_color_arg, vec3 light_position_arg) {
    vec3 L = normalize(light_position_arg - fragment_position);
    vec3 V = normalize(view_position - fragment_position);
    vec3 R = reflect(-L, normalize(fragment_normal));
    return shading_specular_strength * light_color_arg * pow(max(dot(R, V), 0.0), 32.0);
}

float shadow_scalar() {
    vec3 ndc = fragment_position_light_space.xyz / fragment_position_light_space.w;
    ndc = ndc * 0.5 + 0.5;
    if (ndc.x < 0.0 || ndc.x > 1.0 ||
        ndc.y < 0.0 || ndc.y > 1.0 ||
        ndc.z < 0.0 || ndc.z > 1.0) {
        return 1.0;
    }
    float closest_depth = texture(shadow_map, ndc.xy).r;
    float current_depth = ndc.z;
    float bias = 0.002;

    return ((current_depth - bias) < closest_depth) ? 1.0 : 0.0;
}

float spotlight_scalar() {
    float theta = dot(normalize(fragment_position - light_position), light_direction);
    if (theta > light_cutoff_inner) {
        return 1.0;
    } else if (theta > light_cutoff_outer) {
        return (1.0 - cos(PI * (theta - light_cutoff_outer) / (light_cutoff_inner - light_cutoff_outer))) * 0.5;
    } else {
        return 0.0;
    }
}
float spotlight_scalar_custom(vec3 Lpos, vec3 Ldir, float innerCos, float outerCos) {
    float theta = dot(normalize(fragment_position - Lpos), normalize(Ldir));
    if (theta > innerCos) {
        return 1.0;
    } else if (theta > outerCos) {
        return (1.0 - cos(PI * (theta - outerCos) / (innerCos - outerCos))) * 0.5;
    } else {
        return 0.0;
    }
}

float shadow_scalar_custom(vec4 lightSpacePos, sampler2D shadowTex) {
    vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
    ndc = ndc * 0.5 + 0.5;
    if (ndc.x < 0.0 || ndc.x > 1.0 ||
        ndc.y < 0.0 || ndc.y > 1.0 ||
        ndc.z < 0.0 || ndc.z > 1.0) {
        return 1.0;
    }
    float closest_depth = texture(shadowTex, ndc.xy).r;
    float current_depth = ndc.z;
    float bias = 0.002;
    return ((current_depth - bias) < closest_depth) ? 1.0 : 0.0;
}

void main()
{
    float lit = shadow_scalar() * spotlight_scalar();

    vec3 baseColor = texture(albedo_tex, vUV * uv_scale).rgb * object_color;

    vec3 ambient  = ambient_color(light_color);
    vec3 diffuse  = lit * diffuse_color(light_color, light_position);
    vec3 specular = lit * specular_color(light_color, light_position);
    float camSpot = spotlight_scalar_custom(
    camLight_position, camLight_direction,
    camLight_cutoff_inner, camLight_cutoff_outer);
    float litCam = camSpot * shadow_scalar_custom(fragment_position_camLight_space, cam_shadow_map);

    diffuse  += litCam * camLight_intensity * diffuse_color(camLight_color, camLight_position);
    specular += litCam * camLight_intensity * specular_color(camLight_color, camLight_position);

    vec3 color = (specular + diffuse + ambient) * baseColor;
    result = vec4(color, 1.0);
}
