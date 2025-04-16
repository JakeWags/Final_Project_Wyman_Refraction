#version 330 core

out vec4 FragColor;

uniform vec3 lightColor;
uniform vec3 lightDirection;
uniform float lightIntensity;
uniform vec3 specularColor;
uniform float shininess;

uniform float refractiveIndex;

uniform sampler2D background;
uniform sampler2D frontFaceDepth;
uniform sampler2D backFaceDepth;
uniform sampler2D backFaceNormals;

uniform bool normalsPass;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoord;
in float dn;

void main()
{
    // get the normal direction
    vec3 norm = normalize(Normal);

    if (normalsPass) {
        // If in normals pass, output the normal direction
        FragColor = vec4(norm, 1.0);
        return;
    }

    vec3 viewDirection = normalize(-FragPos);

    vec3 dColor = vec3(0.1, 0.1, 0.1);

    vec3 ambient = dColor;

    float cosTheta = max(0, dot(norm, lightDirection));

    vec3 diffuse = dColor * cosTheta;

    // Blinn-Phong specular
    vec3 reflectDirection = reflect(-lightDirection, norm);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);

    float cosPhi = max(0, dot(norm, halfwayDirection));
    float specularStrength = pow(cosPhi, shininess);

    vec3 specular = specularColor * specularStrength;

    // REFRACTION
    float refractiveIndexRatio = 1.0 / refractiveIndex; // air to glass

    // Convert screen-space position to UV coordinates
    vec2 screenUV = gl_FragCoord.xy / vec2(1500, 1500); // 1500/1500 is the window resolution

    // Compute basic refraction using Snell's Law
    vec3 refractedDir = refract(viewDirection, norm, refractiveIndexRatio);

    // Convert refraction direction into texture coordinate offset
    vec2 refractedUV = screenUV + refractedDir.xy * 0.075; // Scale distortion

    // If the UV coordinates are out of bounds, use the texture mirror location (sometimes there are artifacts when you zoom in)
    if (refractedUV.x < 0.0) refractedUV.x = -refractedUV.x;
    if (refractedUV.x > 1.0) refractedUV.x = 2.0 - refractedUV.x;
    if (refractedUV.y < 0.0) refractedUV.y = -refractedUV.y;
    if (refractedUV.y > 1.0) refractedUV.y = 2.0 - refractedUV.y;

    // Clamp the UV coordinates to the texture size
    refractedUV.x = clamp(refractedUV.x, 0.0, 1.0);
    refractedUV.y = clamp(refractedUV.y, 0.0, 1.0);

    // Sample the background texture at the refracted position
    vec3 refractedColor = texture(background, -refractedUV).rgb;


    //////////////////////// two face refraction
    // Compute first hit point P1
    vec3 P1 = FragPos;

    // Compute first refraction direction T1
    vec3 T1 = refract(viewDirection, norm, refractiveIndexRatio);

    // Scale P1 to screen space
    vec2 screenP1UV = gl_FragCoord.xy / vec2(1500, 1500); // 1500/1500 is the window resolution

    // Sample depth textures
    float frontDepth = texture(frontFaceDepth, vec2(screenP1UV.x, screenP1UV.y)).r;
    float backDepth = texture(backFaceDepth, vec2(screenP1UV.x, screenP1UV.y)).r;
    float thickness = backDepth - frontDepth; // geometric thickness

    // debugging
    //FragColor = vec4(vec3((1 - backDepth) * 100), 1.0);
    //FragColor = vec4(vec3(thickness * 100), 1.0);
    //return;

    // Approximate second hit point P2
    vec3 P2 = P1 + thickness * T1;
    // Scale P2 to screen space
    vec2 screenP2 = P2.xy / vec2(1500, 1500); // 1500/1500 is the window resolution

    // Retrieve normal at P2 (N2)
    vec3 N2 = texture(backFaceNormals, vec2(screenP2.xy)).rgb;  // Assuming back-face normal stored

    // Compute second refraction direction T2
    vec3 T2 = refract(T1, normalize(N2), refractiveIndex);

    refractedUV = screenUV + T2.xy * 0.075; // Scale distortion

    // If the UV coordinates are out of bounds, use the texture mirror location (sometimes there are artifacts when you zoom in)
    if (refractedUV.x < 0.0) refractedUV.x = -refractedUV.x;
    if (refractedUV.x > 1.0) refractedUV.x = 2.0 - refractedUV.x;
    if (refractedUV.y < 0.0) refractedUV.y = -refractedUV.y;
    if (refractedUV.y > 1.0) refractedUV.y = 2.0 - refractedUV.y;
    // Clamp the UV coordinates to the texture size
    refractedUV.x = clamp(refractedUV.x, 0.0, 1.0);
    refractedUV.y = clamp(refractedUV.y, 0.0, 1.0);

    // Sample the background texture at the refracted position
    vec3 refractedColor2 = texture(background, -refractedUV).rgb;

    // set the color. Ambient multiplied by 0.5 to make it less intense
    vec3 color = lightColor * ((lightIntensity * (diffuse + specular)) + (lightIntensity * 0.5 * ambient));
    color = color + (refractedColor);

    FragColor = vec4(color, 1.0);
}
