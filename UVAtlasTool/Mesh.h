//--------------------------------------------------------------------------------------
// File: Mesh.h
//
// Mesh processing helper class
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=324981
// http://go.microsoft.com/fwlink/?LinkID=512686
//--------------------------------------------------------------------------------------

#include <memory>
#include <string>
#include <vector>

#include <stdint.h>

#if defined(_XBOX_ONE) && defined(_TITLE)
#include <d3d11_x.h>
#else
#include <d3d11_1.h>
#endif

#include <DirectXMath.h>

#include <DirectXMesh.h>

class Mesh
{
public:
    Mesh() noexcept : mnFaces(0), mnVerts(0) {}
    Mesh(Mesh&& moveFrom) noexcept;
    Mesh& operator= (Mesh&& moveFrom) noexcept;

    Mesh(Mesh const&) = delete;
    Mesh& operator= (Mesh const&) = delete;

    // Methods
    void Clear();

    HRESULT SetIndexData(_In_ size_t nFaces, _In_reads_(nFaces * 3) const uint16_t* indices, _In_reads_opt_(nFaces) uint32_t* attributes = nullptr);
    HRESULT SetIndexData(_In_ size_t nFaces, _In_reads_(nFaces * 3) const uint32_t* indices, _In_reads_opt_(nFaces) uint32_t* attributes = nullptr);

    HRESULT SetVertexData(_Inout_ DirectX::VBReader& reader, _In_ size_t nVerts);

    HRESULT Validate(_In_ DWORD flags, _In_opt_ std::wstring* msgs) const;

    HRESULT Clean(_In_ bool breakBowties = false);

    HRESULT GenerateAdjacency(_In_ float epsilon);

    HRESULT ComputeNormals(_In_ DWORD flags);

    HRESULT ComputeTangentFrame(_In_ bool bitangents);

    HRESULT UpdateFaces(_In_ size_t nFaces, _In_reads_(nFaces * 3) const uint32_t* indices);

    HRESULT UpdateAttributes(_In_ size_t nFaces, _In_reads_(nFaces * 3) const uint32_t* attributes);

    HRESULT UpdateUVs(_In_ size_t nVerts, _In_reads_(nVerts) const DirectX::XMFLOAT2* uvs);

    HRESULT VertexRemap(_In_reads_(nNewVerts) const uint32_t* remap, _In_ size_t nNewVerts);

    HRESULT ReverseWinding();

    HRESULT InvertUTexCoord();
    HRESULT InvertVTexCoord();

    HRESULT ReverseHandedness();

    HRESULT VisualizeUVs();

    // Accessors
    const uint32_t* GetAttributeBuffer() const { return mAttributes.get(); }
    const uint32_t* GetAdjacencyBuffer() const { return mAdjacency.get(); }
    const DirectX::XMFLOAT3* GetPositionBuffer() const { return mPositions.get(); }
    const DirectX::XMFLOAT3* GetNormalBuffer() const { return mNormals.get(); }
    const DirectX::XMFLOAT2* GetTexCoordBuffer() const { return mTexCoords.get(); }
    const DirectX::XMFLOAT4* GetTangentBuffer() const { return mTangents.get(); }
    const DirectX::XMFLOAT4* GetColorBuffer() const { return mColors.get(); }

    size_t GetFaceCount() const { return mnFaces; }
    size_t GetVertexCount() const { return mnVerts; }

    bool Is16BitIndexBuffer() const;

    const uint32_t* GetIndexBuffer() const { return mIndices.get(); }
    std::unique_ptr<uint16_t[]> GetIndexBuffer16() const;

    HRESULT GetVertexBuffer(_Inout_ DirectX::VBWriter& writer) const;

    // Save mesh to file
    struct Material
    {
        std::wstring        name;
        bool                perVertexColor;
        float               specularPower;
        float               alpha;
        DirectX::XMFLOAT3   ambientColor;
        DirectX::XMFLOAT3   diffuseColor;
        DirectX::XMFLOAT3   specularColor;
        DirectX::XMFLOAT3   emissiveColor;
        std::wstring        texture;
        std::wstring        normalTexture;
        std::wstring        specularTexture;
        std::wstring        emissiveTexture;
        std::wstring        rmaTexture;

        Material() noexcept :
            perVertexColor(false),
            specularPower(1.f),
            alpha(1.f),
            ambientColor{},
            diffuseColor{},
            specularColor{},
            emissiveColor{}
        {
        }

        Material(
            const wchar_t* iname,
            bool pvc,
            float power,
            float ialpha,
            const DirectX::XMFLOAT3& ambient,
            const DirectX::XMFLOAT3 diffuse,
            const DirectX::XMFLOAT3& specular,
            const DirectX::XMFLOAT3& emissive,
            const wchar_t* txtname) :
            name(iname),
            perVertexColor(pvc),
            specularPower(power),
            alpha(ialpha),
            ambientColor(ambient),
            diffuseColor(diffuse),
            specularColor(specular),
            emissiveColor(emissive),
            texture(txtname)
        {
        }
    };

    HRESULT ExportToPLY(const char* szFileName) const;

private:
    size_t                                      mnFaces;
    size_t                                      mnVerts;
    std::unique_ptr<uint32_t[]>                 mIndices;
    std::unique_ptr<uint32_t[]>                 mAttributes;
    std::unique_ptr<uint32_t[]>                 mAdjacency;
    std::unique_ptr<DirectX::XMFLOAT3[]>        mPositions;
    std::unique_ptr<DirectX::XMFLOAT3[]>        mNormals;
    std::unique_ptr<DirectX::XMFLOAT4[]>        mTangents;
    std::unique_ptr<DirectX::XMFLOAT3[]>        mBiTangents;
    std::unique_ptr<DirectX::XMFLOAT2[]>        mTexCoords;
    std::unique_ptr<DirectX::XMFLOAT4[]>        mColors;
    std::unique_ptr<DirectX::XMFLOAT4[]>        mBlendIndices;
    std::unique_ptr<DirectX::XMFLOAT4[]>        mBlendWeights;
};
