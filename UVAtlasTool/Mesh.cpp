//--------------------------------------------------------------------------------------
// File: Mesh.cpp
//
// Mesh processing helper class
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=324981
// http://go.microsoft.com/fwlink/?LinkID=512686
//--------------------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable : 4005)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#pragma warning(pop)

#include "Mesh.h"
#include "SDKMesh.h"

#include <fstream>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>
#include <UVAtlas.h>

using namespace DirectX;

// Move constructor
Mesh::Mesh(Mesh&& moveFrom) noexcept : mnFaces(0), mnVerts(0)
{
    *this = std::move(moveFrom);
}

// Move operator
Mesh& Mesh::operator= (Mesh&& moveFrom) noexcept
{
    if (this != &moveFrom)
    {
        mnFaces = moveFrom.mnFaces;
        mnVerts = moveFrom.mnVerts;
        mIndices.swap(moveFrom.mIndices);
        mAttributes.swap(moveFrom.mAttributes);
        mAdjacency.swap(moveFrom.mAdjacency);
        mPositions.swap(moveFrom.mPositions);
        mNormals.swap(moveFrom.mNormals);
        mTangents.swap(moveFrom.mTangents);
        mBiTangents.swap(moveFrom.mBiTangents);
        mTexCoords.swap(moveFrom.mTexCoords);
        mColors.swap(moveFrom.mColors);
        mBlendIndices.swap(moveFrom.mBlendIndices);
        mBlendWeights.swap(moveFrom.mBlendWeights);
    }
    return *this;
}


//--------------------------------------------------------------------------------------
void Mesh::Clear()
{
    mnFaces = mnVerts = 0;

    // Release face data
    mIndices.reset();
    mAttributes.reset();
    mAdjacency.reset();

    // Release vertex data
    mPositions.reset();
    mNormals.reset();
    mTangents.reset();
    mBiTangents.reset();
    mTexCoords.reset();
    mColors.reset();
    mBlendIndices.reset();
    mBlendWeights.reset();
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Mesh::SetIndexData(size_t nFaces, const uint16_t* indices, uint32_t* attributes)
{
    if (!nFaces || !indices)
        return E_INVALIDARG;

    if ((uint64_t(nFaces) * 3) >= UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

    // Release face data
    mnFaces = 0;
    mIndices.reset();
    mAttributes.reset();

    std::unique_ptr<uint32_t[]> ib(new (std::nothrow) uint32_t[nFaces * 3]);
    if (!ib)
        return E_OUTOFMEMORY;

    for (size_t j = 0; j < (nFaces * 3); ++j)
    {
        if (indices[j] == uint16_t(-1))
        {
            ib[j] = uint32_t(-1);
        }
        else
        {
            ib[j] = indices[j];
        }
    }

    std::unique_ptr<uint32_t[]> attr;
    if (attributes)
    {
        attr.reset(new (std::nothrow) uint32_t[nFaces]);
        if (!attr)
            return E_OUTOFMEMORY;

        memcpy(attr.get(), attributes, sizeof(uint32_t) * nFaces);
    }

    mIndices.swap(ib);
    mAttributes.swap(attr);
    mnFaces = nFaces;

    return S_OK;
}

_Use_decl_annotations_
HRESULT Mesh::SetIndexData(size_t nFaces, const uint32_t* indices, uint32_t* attributes)
{
    if (!nFaces || !indices)
        return E_INVALIDARG;

    if ((uint64_t(nFaces) * 3) >= UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

    mnFaces = 0;
    mIndices.reset();
    mAttributes.reset();

    std::unique_ptr<uint32_t[]> ib(new (std::nothrow) uint32_t[nFaces * 3]);
    if (!ib)
        return E_OUTOFMEMORY;

    memcpy(ib.get(), indices, sizeof(uint32_t) * nFaces * 3);

    std::unique_ptr<uint32_t[]> attr;
    if (attributes)
    {
        attr.reset(new (std::nothrow) uint32_t[nFaces]);
        if (!attr)
            return E_OUTOFMEMORY;

        memcpy(attr.get(), attributes, sizeof(uint32_t) * nFaces);
    }

    mIndices.swap(ib);
    mAttributes.swap(attr);
    mnFaces = nFaces;

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::SetVertexData(_Inout_ DirectX::VBReader& reader, _In_ size_t nVerts)
{
    if (!nVerts)
        return E_INVALIDARG;

    // Release vertex data
    mnVerts = 0;
    mPositions.reset();
    mNormals.reset();
    mTangents.reset();
    mBiTangents.reset();
    mTexCoords.reset();
    mColors.reset();
    mBlendIndices.reset();
    mBlendWeights.reset();

    // Load positions (required)
    std::unique_ptr<XMFLOAT3[]> pos(new (std::nothrow) XMFLOAT3[nVerts]);
    if (!pos)
        return E_OUTOFMEMORY;

    HRESULT hr = reader.Read(pos.get(), "SV_Position", 0, nVerts);
    if (FAILED(hr))
        return hr;

    // Load normals
    std::unique_ptr<XMFLOAT3[]> norms;
    auto e = reader.GetElement11("NORMAL", 0);
    if (e)
    {
        norms.reset(new (std::nothrow) XMFLOAT3[nVerts]);
        if (!norms)
            return E_OUTOFMEMORY;

        hr = reader.Read(norms.get(), "NORMAL", 0, nVerts);
        if (FAILED(hr))
            return hr;
    }

    // Load tangents
    std::unique_ptr<XMFLOAT4[]> tans1;
    e = reader.GetElement11("TANGENT", 0);
    if (e)
    {
        tans1.reset(new (std::nothrow) XMFLOAT4[nVerts]);
        if (!tans1)
            return E_OUTOFMEMORY;

        hr = reader.Read(tans1.get(), "TANGENT", 0, nVerts);
        if (FAILED(hr))
            return hr;
    }

    // Load bi-tangents
    std::unique_ptr<XMFLOAT3[]> tans2;
    e = reader.GetElement11("BINORMAL", 0);
    if (e)
    {
        tans2.reset(new (std::nothrow) XMFLOAT3[nVerts]);
        if (!tans2)
            return E_OUTOFMEMORY;

        hr = reader.Read(tans2.get(), "BINORMAL", 0, nVerts);
        if (FAILED(hr))
            return hr;
    }

    // Load texture coordinates
    std::unique_ptr<XMFLOAT2[]> texcoord;
    e = reader.GetElement11("TEXCOORD", 0);
    if (e)
    {
        texcoord.reset(new (std::nothrow) XMFLOAT2[nVerts]);
        if (!texcoord)
            return E_OUTOFMEMORY;

        hr = reader.Read(texcoord.get(), "TEXCOORD", 0, nVerts);
        if (FAILED(hr))
            return hr;
    }

    // Load vertex colors
    std::unique_ptr<XMFLOAT4[]> colors;
    e = reader.GetElement11("COLOR", 0);
    if (e)
    {
        colors.reset(new (std::nothrow) XMFLOAT4[nVerts]);
        if (!colors)
            return E_OUTOFMEMORY;

        hr = reader.Read(colors.get(), "COLOR", 0, nVerts);
        if (FAILED(hr))
            return hr;
    }

    // Load skinning bone indices
    std::unique_ptr<XMFLOAT4[]> blendIndices;
    e = reader.GetElement11("BLENDINDICES", 0);
    if (e)
    {
        blendIndices.reset(new (std::nothrow) XMFLOAT4[nVerts]);
        if (!blendIndices)
            return E_OUTOFMEMORY;

        hr = reader.Read(blendIndices.get(), "BLENDINDICES", 0, nVerts);
        if (FAILED(hr))
            return hr;
    }

    // Load skinning bone weights
    std::unique_ptr<XMFLOAT4[]> blendWeights;
    e = reader.GetElement11("BLENDWEIGHT", 0);
    if (e)
    {
        blendWeights.reset(new (std::nothrow) XMFLOAT4[nVerts]);
        if (!blendWeights)
            return E_OUTOFMEMORY;

        hr = reader.Read(blendWeights.get(), "BLENDWEIGHT", 0, nVerts);
        if (FAILED(hr))
            return hr;
    }

    // Return values
    mPositions.swap(pos);
    mNormals.swap(norms);
    mTangents.swap(tans1);
    mBiTangents.swap(tans2);
    mTexCoords.swap(texcoord);
    mColors.swap(colors);
    mBlendIndices.swap(blendIndices);
    mBlendWeights.swap(blendWeights);
    mnVerts = nVerts;

    return S_OK;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Mesh::Validate(DWORD flags, std::wstring* msgs) const
{
    if (!mnFaces || !mIndices || !mnVerts)
        return E_UNEXPECTED;

    return DirectX::Validate(mIndices.get(), mnFaces, mnVerts, mAdjacency.get(), flags, msgs);
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::Clean(_In_ bool breakBowties)
{
    if (!mnFaces || !mIndices || !mnVerts || !mPositions)
        return E_UNEXPECTED;

    std::vector<uint32_t> dups;
    HRESULT hr = DirectX::Clean(mIndices.get(), mnFaces, mnVerts, mAdjacency.get(), mAttributes.get(), dups, breakBowties);
    if (FAILED(hr))
        return hr;

    if (dups.empty())
    {
        // No vertex duplication is needed for mesh clean
        return S_OK;
    }

    size_t nNewVerts = mnVerts + dups.size();

    std::unique_ptr<XMFLOAT3[]> pos(new (std::nothrow) XMFLOAT3[nNewVerts]);
    if (!pos)
        return E_OUTOFMEMORY;

    memcpy(pos.get(), mPositions.get(), sizeof(XMFLOAT3) * mnVerts);

    std::unique_ptr<XMFLOAT3[]> norms;
    if (mNormals)
    {
        norms.reset(new (std::nothrow) XMFLOAT3[nNewVerts]);
        if (!norms)
            return E_OUTOFMEMORY;

        memcpy(norms.get(), mNormals.get(), sizeof(XMFLOAT3) * mnVerts);
    }

    std::unique_ptr<XMFLOAT4[]> tans1;
    if (mTangents)
    {
        tans1.reset(new (std::nothrow) XMFLOAT4[nNewVerts]);
        if (!tans1)
            return E_OUTOFMEMORY;

        memcpy(tans1.get(), mTangents.get(), sizeof(XMFLOAT4) * mnVerts);
    }

    std::unique_ptr<XMFLOAT3[]> tans2;
    if (mBiTangents)
    {
        tans2.reset(new (std::nothrow) XMFLOAT3[nNewVerts]);
        if (!tans2)
            return E_OUTOFMEMORY;

        memcpy(tans2.get(), mBiTangents.get(), sizeof(XMFLOAT3) * mnVerts);
    }

    std::unique_ptr<XMFLOAT2[]> texcoord;
    if (mTexCoords)
    {
        texcoord.reset(new (std::nothrow) XMFLOAT2[nNewVerts]);
        if (!texcoord)
            return E_OUTOFMEMORY;

        memcpy(texcoord.get(), mTexCoords.get(), sizeof(XMFLOAT2) * mnVerts);
    }

    std::unique_ptr<XMFLOAT4[]> colors;
    if (mColors)
    {
        colors.reset(new (std::nothrow) XMFLOAT4[nNewVerts]);
        if (!colors)
            return E_OUTOFMEMORY;

        memcpy(colors.get(), mColors.get(), sizeof(XMFLOAT4) * mnVerts);
    }

    std::unique_ptr<XMFLOAT4[]> blendIndices;
    if (mBlendIndices)
    {
        blendIndices.reset(new (std::nothrow) XMFLOAT4[nNewVerts]);
        if (!blendIndices)
            return E_OUTOFMEMORY;

        memcpy(blendIndices.get(), mBlendIndices.get(), sizeof(XMFLOAT4) * mnVerts);
    }

    std::unique_ptr<XMFLOAT4[]> blendWeights;
    if (mBlendWeights)
    {
        blendWeights.reset(new (std::nothrow) XMFLOAT4[nNewVerts]);
        if (!blendWeights)
            return E_OUTOFMEMORY;

        memcpy(blendWeights.get(), mBlendWeights.get(), sizeof(XMFLOAT4) * mnVerts);
    }

    size_t j = mnVerts;
    for (auto it = dups.begin(); it != dups.end() && (j < nNewVerts); ++it, ++j)
    {
        assert(*it < mnVerts);

        pos[j] = mPositions[*it];

        if (norms)
        {
            norms[j] = mNormals[*it];
        }

        if (tans1)
        {
            tans1[j] = mTangents[*it];
        }

        if (tans2)
        {
            tans2[j] = mBiTangents[*it];
        }

        if (texcoord)
        {
            texcoord.get()[j] = mTexCoords[*it];
        }

        if (colors)
        {
            colors[j] = mColors[*it];
        }

        if (blendIndices)
        {
            blendIndices[j] = mBlendIndices[*it];
        }

        if (blendWeights)
        {
            blendWeights[j] = mBlendWeights[*it];
        }
    }

    mPositions.swap(pos);
    mNormals.swap(norms);
    mTangents.swap(tans1);
    mBiTangents.swap(tans2);
    mTexCoords.swap(texcoord);
    mColors.swap(colors);
    mBlendIndices.swap(blendIndices);
    mBlendWeights.swap(blendWeights);
    mnVerts = nNewVerts;

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::GenerateAdjacency(_In_ float epsilon)
{
    if (!mnFaces || !mIndices || !mnVerts || !mPositions)
        return E_UNEXPECTED;

    if ((uint64_t(mnFaces) * 3) >= UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

    mAdjacency.reset(new (std::nothrow) uint32_t[mnFaces * 3]);
    if (!mAdjacency)
        return E_OUTOFMEMORY;

    return DirectX::GenerateAdjacencyAndPointReps(mIndices.get(), mnFaces, mPositions.get(), mnVerts, epsilon, nullptr, mAdjacency.get());
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::ComputeNormals(_In_ DWORD flags)
{
    if (!mnFaces || !mIndices || !mnVerts || !mPositions)
        return E_UNEXPECTED;

    mNormals.reset(new (std::nothrow) XMFLOAT3[mnVerts]);
    if (!mNormals)
        return E_OUTOFMEMORY;

    return DirectX::ComputeNormals(mIndices.get(), mnFaces, mPositions.get(), mnVerts, flags, mNormals.get());
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::ComputeTangentFrame(_In_ bool bitangents)
{
    if (!mnFaces || !mIndices || !mnVerts || !mPositions || !mNormals || !mTexCoords)
        return E_UNEXPECTED;

    mTangents.reset();
    mBiTangents.reset();

    std::unique_ptr<XMFLOAT4[]> tan1(new (std::nothrow) XMFLOAT4[mnVerts]);
    if (!tan1)
        return E_OUTOFMEMORY;

    std::unique_ptr<XMFLOAT3[]> tan2;
    if (bitangents)
    {
        tan2.reset(new (std::nothrow) XMFLOAT3[mnVerts]);
        if (!tan2)
            return E_OUTOFMEMORY;

        HRESULT hr = DirectX::ComputeTangentFrame(mIndices.get(), mnFaces, mPositions.get(), mNormals.get(), mTexCoords.get(), mnVerts,
            tan1.get(), tan2.get());
        if (FAILED(hr))
            return hr;
    }
    else
    {
        mBiTangents.reset();

        HRESULT hr = DirectX::ComputeTangentFrame(mIndices.get(), mnFaces, mPositions.get(), mNormals.get(), mTexCoords.get(), mnVerts,
            tan1.get());
        if (FAILED(hr))
            return hr;
    }

    mTangents.swap(tan1);
    mBiTangents.swap(tan2);

    return S_OK;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Mesh::UpdateFaces(size_t nFaces, const uint32_t* indices)
{
    if (!nFaces || !indices)
        return E_INVALIDARG;

    if (!mnFaces || !mIndices)
        return E_UNEXPECTED;

    if (mnFaces != nFaces)
        return E_FAIL;

    if ((uint64_t(nFaces) * 3) >= UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

    memcpy(mIndices.get(), indices, sizeof(uint32_t) * 3 * nFaces);

    return S_OK;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Mesh::UpdateAttributes(size_t nFaces, const uint32_t* attributes)
{
    if (!nFaces || !attributes)
        return E_INVALIDARG;

    if (!mnFaces || !mIndices || !mnVerts || !mPositions)
        return E_UNEXPECTED;

    if (mnFaces != nFaces)
        return E_FAIL;

    if (!mAttributes)
    {
        std::unique_ptr<uint32_t[]> attr(new (std::nothrow) uint32_t[nFaces]);
        if (!attr)
            return E_OUTOFMEMORY;

        memcpy(attr.get(), attributes, sizeof(uint32_t) * nFaces);

        mAttributes.swap(attr);
    }
    else
    {
        memcpy(mAttributes.get(), attributes, sizeof(uint32_t) * nFaces);
    }

    std::unique_ptr<uint32_t> remap(new (std::nothrow) uint32_t[mnFaces]);
    if (!remap)
        return E_OUTOFMEMORY;

    HRESULT hr = AttributeSort(mnFaces, mAttributes.get(), remap.get());
    if (FAILED(hr))
        return hr;

    if (mAdjacency)
    {
        hr = ReorderIBAndAdjacency(mIndices.get(), mnFaces, mAdjacency.get(), remap.get());
    }
    else
    {
        hr = ReorderIB(mIndices.get(), mnFaces, remap.get());
    }
    if (FAILED(hr))
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Mesh::UpdateUVs(size_t nVerts, const XMFLOAT2* uvs)
{
    if (!nVerts || !uvs)
        return E_INVALIDARG;

    if (!mnVerts || !mPositions)
        return E_UNEXPECTED;

    if (nVerts != mnVerts)
        return E_FAIL;

    if (!mTexCoords)
    {
        std::unique_ptr<XMFLOAT2[]> texcoord;
        texcoord.reset(new (std::nothrow) XMFLOAT2[mnVerts]);
        if (!texcoord)
            return E_OUTOFMEMORY;

        memcpy(texcoord.get(), uvs, sizeof(XMFLOAT2) * mnVerts);

        mTexCoords.swap(texcoord);
    }
    else
    {
        memcpy(mTexCoords.get(), uvs, sizeof(XMFLOAT2) * mnVerts);
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Mesh::VertexRemap(const uint32_t* remap, size_t nNewVerts)
{
    if (!remap || !nNewVerts)
        return E_INVALIDARG;

    if (!mnVerts || !mPositions)
        return E_UNEXPECTED;

    if (nNewVerts < mnVerts)
        return E_FAIL;

    std::unique_ptr<XMFLOAT3[]> pos(new (std::nothrow) XMFLOAT3[nNewVerts]);
    if (!pos)
        return E_OUTOFMEMORY;

    HRESULT hr = UVAtlasApplyRemap(mPositions.get(), sizeof(XMFLOAT3), mnVerts, nNewVerts, remap, pos.get());
    if (FAILED(hr))
        return hr;

    std::unique_ptr<XMFLOAT3[]> norms;
    if (mNormals)
    {
        norms.reset(new (std::nothrow) XMFLOAT3[nNewVerts]);
        if (!norms)
            return E_OUTOFMEMORY;

        hr = UVAtlasApplyRemap(mNormals.get(), sizeof(XMFLOAT3), mnVerts, nNewVerts, remap, norms.get());
        if (FAILED(hr))
            return hr;
    }

    std::unique_ptr<XMFLOAT4[]> tans1;
    if (mTangents)
    {
        tans1.reset(new (std::nothrow) XMFLOAT4[nNewVerts]);
        if (!tans1)
            return E_OUTOFMEMORY;

        hr = UVAtlasApplyRemap(mTangents.get(), sizeof(XMFLOAT4), mnVerts, nNewVerts, remap, tans1.get());
        if (FAILED(hr))
            return hr;
    }

    std::unique_ptr<XMFLOAT3[]> tans2;
    if (mBiTangents)
    {
        tans2.reset(new (std::nothrow) XMFLOAT3[nNewVerts]);
        if (!tans2)
            return E_OUTOFMEMORY;

        hr = UVAtlasApplyRemap(mBiTangents.get(), sizeof(XMFLOAT3), mnVerts, nNewVerts, remap, tans2.get());
        if (FAILED(hr))
            return hr;
    }

    std::unique_ptr<XMFLOAT2[]> texcoord;
    if (mTexCoords)
    {
        texcoord.reset(new (std::nothrow) XMFLOAT2[nNewVerts]);
        if (!texcoord)
            return E_OUTOFMEMORY;

        hr = UVAtlasApplyRemap(mTexCoords.get(), sizeof(XMFLOAT2), mnVerts, nNewVerts, remap, texcoord.get());
        if (FAILED(hr))
            return hr;
    }

    std::unique_ptr<XMFLOAT4[]> colors;
    if (mColors)
    {
        colors.reset(new (std::nothrow) XMFLOAT4[nNewVerts]);
        if (!colors)
            return E_OUTOFMEMORY;

        hr = UVAtlasApplyRemap(mColors.get(), sizeof(XMFLOAT4), mnVerts, nNewVerts, remap, colors.get());
        if (FAILED(hr))
            return hr;
    }

    std::unique_ptr<XMFLOAT4[]> blendIndices;
    if (mBlendIndices)
    {
        blendIndices.reset(new (std::nothrow) XMFLOAT4[nNewVerts]);
        if (!blendIndices)
            return E_OUTOFMEMORY;

        hr = UVAtlasApplyRemap(mBlendIndices.get(), sizeof(XMFLOAT4), mnVerts, nNewVerts, remap, blendIndices.get());
        if (FAILED(hr))
            return hr;
    }

    std::unique_ptr<XMFLOAT4[]> blendWeights;
    if (mBlendWeights)
    {
        blendWeights.reset(new (std::nothrow) XMFLOAT4[nNewVerts]);
        if (!blendWeights)
            return E_OUTOFMEMORY;

        hr = UVAtlasApplyRemap(mBlendWeights.get(), sizeof(XMFLOAT4), mnVerts, nNewVerts, remap, blendWeights.get());
        if (FAILED(hr))
            return hr;
    }

    mPositions.swap(pos);
    mNormals.swap(norms);
    mTangents.swap(tans1);
    mBiTangents.swap(tans2);
    mTexCoords.swap(texcoord);
    mColors.swap(colors);
    mBlendIndices.swap(blendIndices);
    mBlendWeights.swap(blendWeights);
    mnVerts = nNewVerts;

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::ReverseWinding()
{
    if (!mIndices || !mnFaces)
        return E_UNEXPECTED;

    auto iptr = mIndices.get();
    for (size_t j = 0; j < mnFaces; ++j)
    {
        std::swap(*iptr, *(iptr + 2));
        iptr += 3;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::InvertUTexCoord()
{
    if (!mTexCoords)
        return E_UNEXPECTED;

    auto tptr = mTexCoords.get();
    for (size_t j = 0; j < mnVerts; ++j, ++tptr)
    {
        tptr->x = 1.f - tptr->x;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::InvertVTexCoord()
{
    if (!mTexCoords)
        return E_UNEXPECTED;

    auto tptr = mTexCoords.get();
    for (size_t j = 0; j < mnVerts; ++j, ++tptr)
    {
        tptr->y = 1.f - tptr->y;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::ReverseHandedness()
{
    if (!mPositions)
        return E_UNEXPECTED;

    auto ptr = mPositions.get();
    for (size_t j = 0; j < mnVerts; ++j, ++ptr)
    {
        ptr->z = -ptr->z;
    }

    if (mNormals)
    {
        auto nptr = mNormals.get();
        for (size_t j = 0; j < mnVerts; ++j, ++nptr)
        {
            nptr->z = -nptr->z;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::VisualizeUVs()
{
    if (!mnVerts || !mPositions || !mTexCoords)
        return E_UNEXPECTED;

    const XMFLOAT2* sptr = mTexCoords.get();
    XMFLOAT3* dptr = mPositions.get();
    for (size_t j = 0; j < mnVerts; ++j)
    {
        dptr->x = sptr->x;
        dptr->y = sptr->y;
        dptr->z = 0;
        ++sptr;
        ++dptr;
    }

    if (mNormals)
    {
        XMFLOAT3* nptr = mNormals.get();
        for (size_t j = 0; j < mnVerts; ++j)
        {
            XMStoreFloat3(nptr, g_XMIdentityR2);
            ++nptr;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
bool Mesh::Is16BitIndexBuffer() const
{
    if (!mIndices || !mnFaces)
        return false;

    if ((uint64_t(mnFaces) * 3) >= UINT32_MAX)
        return false;

    const uint32_t* iptr = mIndices.get();
    for (size_t j = 0; j < (mnFaces * 3); ++j)
    {
        uint32_t index = *(iptr++);
        if (index != uint32_t(-1)
            && (index >= UINT16_MAX))
        {
            return false;
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
std::unique_ptr<uint16_t[]> Mesh::GetIndexBuffer16() const
{
    std::unique_ptr<uint16_t[]> ib;

    if (!mIndices || !mnFaces)
        return ib;

    if ((uint64_t(mnFaces) * 3) >= UINT32_MAX)
        return ib;

    size_t count = mnFaces * 3;

    ib.reset(new (std::nothrow) uint16_t[count]);
    if (!ib)
        return ib;

    const uint32_t* iptr = mIndices.get();
    for (size_t j = 0; j < count; ++j)
    {
        uint32_t index = *(iptr++);
        if (index == uint32_t(-1))
        {
            ib[j] = uint16_t(-1);
        }
        else if (index >= UINT16_MAX)
        {
            ib.reset();
            return ib;
        }
        else
        {
            ib[j] = static_cast<uint16_t>(index);
        }
    }

    return ib;
}


//--------------------------------------------------------------------------------------
HRESULT Mesh::GetVertexBuffer(_Inout_ DirectX::VBWriter& writer) const
{
    if (!mnVerts || !mPositions)
        return E_UNEXPECTED;

    HRESULT hr = writer.Write(mPositions.get(), "SV_Position", 0, mnVerts);
    if (FAILED(hr))
        return hr;

    if (mNormals)
    {
        auto e = writer.GetElement11("NORMAL", 0);
        if (e)
        {
            hr = writer.Write(mNormals.get(), "NORMAL", 0, mnVerts);
            if (FAILED(hr))
                return hr;
        }
    }

    if (mTangents)
    {
        auto e = writer.GetElement11("TANGENT", 0);
        if (e)
        {
            hr = writer.Write(mTangents.get(), "TANGENT", 0, mnVerts);
            if (FAILED(hr))
                return hr;
        }
    }

    if (mBiTangents)
    {
        auto e = writer.GetElement11("BINORMAL", 0);
        if (e)
        {
            hr = writer.Write(mBiTangents.get(), "BINORMAL", 0, mnVerts);
            if (FAILED(hr))
                return hr;
        }
    }

    if (mTexCoords)
    {
        auto e = writer.GetElement11("TEXCOORD", 0);
        if (e)
        {
            hr = writer.Write(mTexCoords.get(), "TEXCOORD", 0, mnVerts);
            if (FAILED(hr))
                return hr;
        }
    }

    if (mColors)
    {
        auto e = writer.GetElement11("COLOR", 0);
        if (e)
        {
            hr = writer.Write(mColors.get(), "COLOR", 0, mnVerts);
            if (FAILED(hr))
                return hr;
        }
    }

    if (mBlendIndices)
    {
        auto e = writer.GetElement11("BLENDINDICES", 0);
        if (e)
        {
            hr = writer.Write(mBlendIndices.get(), "BLENDINDICES", 0, mnVerts);
            if (FAILED(hr))
                return hr;
        }
    }

    if (mBlendWeights)
    {
        auto e = writer.GetElement11("BLENDWEIGHT", 0);
        if (e)
        {
            hr = writer.Write(mBlendWeights.get(), "BLENDWEIGHT", 0, mnVerts);
            if (FAILED(hr))
                return hr;
        }
    }

    return S_OK;
}


//======================================================================================
// PLY
//======================================================================================

_Use_decl_annotations_
HRESULT Mesh::ExportToPLY(const char* szFileName) const
{
    if (!szFileName)
        return E_INVALIDARG;

    if (!mnFaces || !mIndices || !mnVerts || !mPositions)
        return E_UNEXPECTED;

    std::wofstream out(szFileName);
    if (!out.good()) {
        return E_FAIL;
    }

    out << "ply\n";
    out << "format ascii 1.0\n";
    out << "element vertex " << mnVerts << "\n";
    out << "property float x\nproperty float y\nproperty float z\n";
    if (mNormals) {
        out << "property float nx\nproperty float ny\nproperty float nz\n";
    }
    if (mTexCoords) {
        out << "property float u\nproperty float v\n";
    }
    out << "element face " << mnFaces << "\n";
    out << "property list uchar int vertex_indices\n";
    out << "end_header\n";

    for (size_t i = 0; i < mnVerts; ++i) {
        out << mPositions[i].x << " " << mPositions[i].y << " " << mPositions[i].z << " ";
        if (mNormals) {
            out << mNormals[i].x << " " << mNormals[i].y << " " << mNormals[i].z << " ";
        }
        if (mTexCoords) {
            out << mTexCoords[i].x << " " << mTexCoords[i].y;
        }
        out << "\n";
    }

    for (size_t i = 0; i < mnFaces; ++i) {
        out << 3 << " " << mIndices[3 * i] << " " << mIndices[3 * i + 1] << " " << mIndices[3 * i + 2] << "\n";
    }

    return out.good() ? S_OK : E_FAIL;
}
