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

uniform mat4 view;
uniform mat4 p;

in vec3 vertNormal;
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
        FragColor = vec4(vertNormal, 1.0);
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
    vec3 T1 = refract(viewDirection, normalize(vertNormal), refractiveIndexRatio);

    // Scale P1 to screen space
    vec2 screenP1UV = gl_FragCoord.xy / vec2(1500, 1500); // 1500/1500 is the window resolution

    // Sample depth textures
    float frontDepth = texture(frontFaceDepth, vec2(screenP1UV.x, screenP1UV.y)).r;
    float backDepth = texture(backFaceDepth, vec2(screenP1UV.x, screenP1UV.y)).r;
    float thickness = backDepth - frontDepth; // geometric thickness

    // debugging
    //FragColor = vec4(vec3((1 - backDepth) * 100), 1.0);
    FragColor = vec4(vec3(thickness * 1000), 1.0);
    //return;

    // Approximate second hit point P2
    vec3 P2 = P1 + thickness * T1;
   
    vec4 projectedP2 = p * view * vec4(P2, 1.0); // Apply projection matrix
    vec2 screenP2 = projectedP2.xy / projectedP2.w; // Convert to NDC (Normalized Device Coordinates)
    screenP2 = screenP2 * 0.5 + 0.5; // Convert to UV space



    //screenP2 = clamp(screenP2, vec2(0.0), vec2(1.0));

    // Retrieve normal at P2 (N2)
    vec3 N2 = texture(backFaceNormals, screenP2).rgb;  // Assuming back-face normal stored
    if (length(N2) < 0.001) {
        N2 = vec3(0.0, 1.0, 0.0); // Default upward-facing normal to avoid errors
    }

    if (dot(viewDirection, N2) < 0.0) {
        N2 = -N2;
    }

    //N2 = vec3(0.0, 0.0, -1.0); // For debugging)

    // debugging
    FragColor = vec4(N2, 1.0);
    //return;

    // Compute second refraction direction T2
    vec3 T2 = refract(normalize(T1), normalize(N2), refractiveIndex);
    
    if (length(T2) < 0.001) {
        T2 = reflect(viewDirection, normalize(norm));
    }

    FragColor = vec4(T2, 1.0);
    //return;

    //return;

    refractedUV = screenUV + T2.xy * 0.175; // Scale distortion

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
    color = color + (refractedColor2);

    FragColor = vec4(color, 1.0);
}
