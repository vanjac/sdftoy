#pragma once

const char *vertShaderSrc = R"X(
#version 330 core

uniform vec3 iCamDir, iCamU, iCamV;

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUV;

out vec3 vRayDir;

void main()
{
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vRayDir = aUV.x * iCamU + aUV.y * iCamV + iCamDir;
}
)X";

const char *fragShaderSrc = R"X(
#version 330

in vec3 vRayDir;  // not normalized!!
out vec4 fColor;

uniform vec3 iCamPos;
uniform float iTime;

vec3 render(vec3 rayPos, vec3 rayDir, float time);

void main()
{
    fColor = vec4(render(iCamPos, normalize(vRayDir), iTime), 1);

    // gamma test, should be easily visible
    /*if (vRayDir.x >= 0)
        fColor = vec4(0.05, 0.05, 0.05, 1);
    else
        fColor = vec4(0);*/
}
)X";
