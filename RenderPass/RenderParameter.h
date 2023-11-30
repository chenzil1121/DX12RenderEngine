#pragma once
#include "Utility.h"
#include "MathHelper.h"

struct DirectionalLight
{
    XMFLOAT3 Strength;
    XMFLOAT3 Direction;
};

struct PointLight
{
    XMFLOAT4 Position;
    XMFLOAT4 Color;
    float Intensity;
    float Attenuation;
    float radius;
    float Padding;
};

struct PassConstants
{
    XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    XMFLOAT4X4 ViewProjInvert = MathHelper::Identity4x4();
    XMFLOAT4X4 PreViewProj = MathHelper::Identity4x4();
    XMFLOAT4X4 View = MathHelper::Identity4x4();
    XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    XMFLOAT3 CameraPos = { 0.0f,0.0f,0.0f };
    float NearZ;
    float FarZ;
    XMINT2 FrameDimension;
    int PrefilteredEnvMipLevels;
    int LightNum;
    BOOL IsRayTracing;
    //DebugViewType DebugView;
};

struct BackgroudConstants
{
    XMFLOAT4 topColor;
    XMFLOAT4 buttomColor;
};

struct MeshConstants
{
    XMFLOAT4X4 World = MathHelper::Identity4x4();
    XMFLOAT4X4 PreWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 WorldInvertTran = MathHelper::Identity4x4();
    int MeshID;
};

struct PBRParameter
{
    XMFLOAT4 BaseColorFactor;
    float RoughnessFactor;
    float MetallicFactor;
    BOOL HasUV;
};

struct BaseMaterialParameter
{
    XMFLOAT4 DiffuseColor = { 0.89f,0.89f,0.89f,1.0f };
    XMFLOAT4 SpecularColor = { 1.0f,1.0f,1.0f,0.0f };
    float kd = 0.4f;
    float ks = 0.6f;
};

struct RayTracingConstants
{
    XMFLOAT4 cameraPosition;
    PointLight pointLight;
    uint32_t frameCount;
    uint32_t vertexSRVOffsetInHeap;
    uint32_t indexSRVOffsetInHeap;
    uint32_t pbrTexSRVOffsetInHeap;
};

struct InstanceInfo
{
    int VertexBufferID;
    int IndexBufferID;
    int BaseColorTexID;
    int RoughnessMetallicTexID;
    int NormalTexID;
    int pad0;
    int pad1;
    int pad2;
    PBRParameter parameter;
};

struct ShadowMapConstants
{
    XMFLOAT4X4 LightMVP;
    float NearZ;
    float FarZ;
    BOOL isOrthographic;
};

struct GuassianBlurConstants
{
    int BlurRadius;

    float w0;
    float w1;
    float w2;
    float w3;
    float w4;
    float w5;
    float w6;
    float w7;
    float w8;
    float w9;
    float w10;
};