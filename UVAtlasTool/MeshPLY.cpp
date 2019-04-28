//--------------------------------------------------------------------------------------
// File: MeshOBJ.cpp
//
// Helper code for loading Mesh data from Wavefront OBJ
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=324981
// http://go.microsoft.com/fwlink/?LinkID=512686
//--------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP

#include <unordered_map>
#include <fstream>
#include <codecvt>
#include <boost/algorithm/string.hpp>

#include <directxcollision.h>

#include "Mesh.h"

using namespace DirectX;

namespace
{
    std::wstring ProcessTextureFileName(const wchar_t* inName, bool dds)
    {
        if (!inName || !*inName)
            return std::wstring();

        wchar_t txext[_MAX_EXT] = {};
        wchar_t txfname[_MAX_FNAME] = {};
        _wsplitpath_s(inName, nullptr, 0, nullptr, 0, txfname, _MAX_FNAME, txext, _MAX_EXT);

        if (dds)
        {
            wcscpy_s(txext, L".dds");
        }

        wchar_t texture[_MAX_PATH] = {};
        _wmakepath_s(texture, nullptr, nullptr, txfname, txext);
        return std::wstring(texture);
    }
}

template<class index_t>
class PlyReader {
public:
    typedef index_t index_t;

    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 textureCoordinate;
    };

    PlyReader() noexcept : hasNormals(false), hasTexcoords(false) {}

    HRESULT Load(_In_z_ const wchar_t* szFileName, bool ccw = true) {
        Clear();

        static const size_t MAX_POLY = 64;

        using namespace DirectX;

        std::ifstream InFile(szFileName, std::ios_base::binary);
        if (!InFile)
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

        wchar_t fname[_MAX_FNAME] = {};
        _wsplitpath_s(szFileName, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, nullptr, 0);

        name = fname;

        Material defmat;

        wcscpy_s(defmat.strName, L"default");
        materials.emplace_back(defmat);

        std::string line;
        std::getline(InFile, line);
        if (line != "ply") {
            return E_FAIL;
        }

        std::getline(InFile, line);
        bool is_ascii = true;
        bool is_little_endian = true;
        {
            std::vector<std::string> str_vec;
            boost::algorithm::split(str_vec, line, boost::algorithm::is_any_of(" "));
            if (str_vec.size() >= 2) {
                if (str_vec[1] == "binary_little_endian") {
                    is_ascii = false;
                    is_little_endian = true;
                } else if (str_vec[1] == "binary_big_endian") {
                    is_ascii = false;
                    is_little_endian = false;
                } else {
                    is_ascii = true;
                }
            } else {
                return E_FAIL;
            }
        }
        // windows is little endian
        if (!is_ascii && !is_little_endian) {
            return E_FAIL;
        }

        std::vector<std::string> str_vec;
        while (InFile.good()) {
            std::getline(InFile, line);
            boost::algorithm::split(str_vec, line, boost::algorithm::is_any_of(" "));
            std::for_each(str_vec.begin(), str_vec.end(), [](std::string &str) {
                str = boost::trim_copy(str);
            });

            if (str_vec.empty()) {
                continue;
            }

            if (str_vec[0] == "end_header") {
                break;
            } else if (str_vec[0] == "comment") {
                if (str_vec.size() > 2 && str_vec[1] == "TextureFile") {
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                    auto texture_file = converter.from_bytes(str_vec[2]);
                    wcscpy_s(defmat.strTexture, texture_file.c_str());
                }
            } else if (str_vec[0] == "element") {
                if (str_vec.size() > 2) {
                    if (str_vec[1] == "vertex") {
                        try {
                            vertices.resize((size_t)std::stoi(str_vec[2]));
                        } catch (const std::exception &) {
                            return E_FAIL;
                        }
                    } else if (str_vec[1] == "face") {
                        try {
                            indices.resize((size_t)std::stoi(str_vec[2]) * 3);
                        } catch (const std::exception &) {
                            return E_FAIL;
                        }
                    }
                }
            } else if (str_vec[0] == "property") {
                if (str_vec.back() == "nz") {
                    hasNormals = true;
                } else if (str_vec.back() == "v") {
                    hasTexcoords = true;
                }
            }
        }

        if (vertices.empty()) {
            return E_FAIL;
        }

        std::vector<char> read_buffer;

        size_t vertex_data_size = 3 + (hasNormals ? 3 : 0) + (hasTexcoords ? 2 : 0);
        if (is_ascii) {
            for (auto &vertex : vertices) {
                std::getline(InFile, line);
                boost::algorithm::split(str_vec, line, boost::algorithm::is_any_of(" "));
                if (str_vec.size() < vertex_data_size) {
                    return E_FAIL;
                }

                size_t i = 0;
                try {
                    vertex.position.x = std::stof(str_vec[i++]);
                    vertex.position.y = std::stof(str_vec[i++]);
                    vertex.position.z = std::stof(str_vec[i++]);
                    if (hasNormals) {
                        vertex.normal.x = std::stof(str_vec[i++]);
                        vertex.normal.y = std::stof(str_vec[i++]);
                        vertex.normal.z = std::stof(str_vec[i++]);
                    }
                    if (hasTexcoords) {
                        vertex.textureCoordinate.x = std::stof(str_vec[i++]);
                        vertex.textureCoordinate.y = std::stof(str_vec[i++]);
                    }
                } catch (const std::exception &) {
                    return E_FAIL;
                }
            }
        } else {
            read_buffer.resize(sizeof(float) * vertex_data_size * vertices.size());
            InFile.read(read_buffer.data(), read_buffer.size());
            for (size_t i_vertex = 0; i_vertex < vertices.size(); ++i_vertex) {
                float *data = (float*)&read_buffer[sizeof(float) * vertex_data_size * i_vertex];
                auto &vertex = vertices[i_vertex];
                size_t i = 0;
                vertex.position.x = data[i++];
                vertex.position.y = data[i++];
                vertex.position.z = data[i++];
                if (hasNormals) {
                    vertex.normal.x = data[i++];
                    vertex.normal.y = data[i++];
                    vertex.normal.z = data[i++];
                }
                if (hasTexcoords) {
                    vertex.textureCoordinate.x = data[i++];
                    vertex.textureCoordinate.y = data[i++];
                }
            }
        }

        read_buffer.resize(sizeof(int) * 3);
        for (size_t i_face = 0; i_face < indices.size() / 3; ++i_face) {
            if (is_ascii) {
                std::getline(InFile, line);
                boost::algorithm::split(str_vec, line, boost::algorithm::is_any_of(" "));
                if (str_vec.empty()) {
                    return E_FAIL;
                }

                try {
                    auto num_vertex = (size_t)std::stoi(str_vec[0]);
                    if (num_vertex != 3 || str_vec.size() < 4) {
                        return E_FAIL;
                    }

                    for (size_t i = 0; i < 3; ++i) {
                        indices[i_face * 3 + (ccw ? i : 2 - i)] = (index_t)std::stoi(str_vec[i + 1]);
                    }
                } catch (const std::exception &) {
                    return E_FAIL;
                }
            } else {
                uint8_t num_vertex = 0;
                InFile.read((char*)&num_vertex, sizeof(uint8_t));

                if (num_vertex != 3) {
                    return E_FAIL;
                }

                for (size_t i = 0; i < 3; ++i) {
                    int index = 0;
                    InFile.read((char*)&index, sizeof(int));
                    indices[i_face * 3 + (ccw ? i : 2 - i)] = (index_t)index;
                }
            }
        }

        // Cleanup
        InFile.close();

        std::vector<XMFLOAT3> positions(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            positions[i] = vertices[i].position;
        }
        BoundingBox::CreateFromPoints(bounds, positions.size(), positions.data(), sizeof(XMFLOAT3));

        return S_OK;
    }

    void Clear() {
        vertices.clear();
        indices.clear();
        attributes.clear();
        materials.clear();
        name.clear();
        hasNormals = false;
        hasTexcoords = false;

        bounds.Center.x = bounds.Center.y = bounds.Center.z = 0.f;
        bounds.Extents.x = bounds.Extents.y = bounds.Extents.z = 0.f;
    }

    struct Material {
        DirectX::XMFLOAT3 vAmbient;
        DirectX::XMFLOAT3 vDiffuse;
        DirectX::XMFLOAT3 vSpecular;
        DirectX::XMFLOAT3 vEmissive;
        uint32_t nShininess;
        float fAlpha;

        bool bSpecular;
        bool bEmissive;

        wchar_t strName[MAX_PATH];
        wchar_t strTexture[MAX_PATH];
        wchar_t strNormalTexture[MAX_PATH];
        wchar_t strSpecularTexture[MAX_PATH];
        wchar_t strEmissiveTexture[MAX_PATH];
        wchar_t strRMATexture[MAX_PATH];

        Material() noexcept :
            vAmbient(0.2f, 0.2f, 0.2f),
            vDiffuse(0.8f, 0.8f, 0.8f),
            vSpecular(1.0f, 1.0f, 1.0f),
            vEmissive(0.f, 0.f, 0.f),
            nShininess(0),
            fAlpha(1.f),
            bSpecular(false),
            bEmissive(false),
            strName{},
            strTexture{},
            strNormalTexture{},
            strSpecularTexture{},
            strEmissiveTexture{},
            strRMATexture{}
        {
        }
    };

    std::vector<Vertex>     vertices;
    std::vector<index_t>    indices;
    std::vector<uint32_t>   attributes;
    std::vector<Material>   materials;

    std::wstring            name;
    bool                    hasNormals;
    bool                    hasTexcoords;

    DirectX::BoundingBox    bounds;
};

HRESULT LoadFromPLY(
    const wchar_t* szFilename,
    std::unique_ptr<Mesh>& inMesh,
    std::vector<Mesh::Material>& inMaterial,
    bool ccw,
    bool dds)
{
    PlyReader<uint32_t> plyReader;
    HRESULT hr = plyReader.Load(szFilename, ccw);
    if (FAILED(hr))
        return hr;

    inMesh.reset(new (std::nothrow) Mesh);
    if (!inMesh)
        return E_OUTOFMEMORY;

    if (plyReader.indices.empty() || plyReader.vertices.empty())
        return E_FAIL;

    hr = inMesh->SetIndexData(plyReader.indices.size() / 3, plyReader.indices.data(),
        plyReader.attributes.empty() ? nullptr : plyReader.attributes.data());
    if (FAILED(hr))
        return hr;

    static const D3D11_INPUT_ELEMENT_DESC s_vboLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    static const D3D11_INPUT_ELEMENT_DESC s_vboLayoutAlt[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    const D3D11_INPUT_ELEMENT_DESC* layout = s_vboLayout;
    size_t nDecl = _countof(s_vboLayout);

    if (!plyReader.hasNormals && !plyReader.hasTexcoords)
    {
        nDecl = 1;
    }
    else if (plyReader.hasNormals && !plyReader.hasTexcoords)
    {
        nDecl = 2;
    }
    else if (!plyReader.hasNormals && plyReader.hasTexcoords)
    {
        layout = s_vboLayoutAlt;
        nDecl = _countof(s_vboLayoutAlt);
    }

    VBReader vbr;
    hr = vbr.Initialize(layout, nDecl);
    if (FAILED(hr))
        return hr;

    hr = vbr.AddStream(plyReader.vertices.data(), plyReader.vertices.size(), 0, sizeof(PlyReader<uint32_t>::Vertex));
    if (FAILED(hr))
        return hr;

    hr = inMesh->SetVertexData(vbr, plyReader.vertices.size());
    if (FAILED(hr))
        return hr;

    if (!plyReader.materials.empty())
    {
        inMaterial.clear();
        inMaterial.reserve(plyReader.materials.size());

        for (auto it = plyReader.materials.cbegin(); it != plyReader.materials.cend(); ++it)
        {
            Mesh::Material mtl = {};

            mtl.name = it->strName;
            mtl.specularPower = (it->bSpecular) ? float(it->nShininess) : 1.f;
            mtl.alpha = it->fAlpha;
            mtl.ambientColor = it->vAmbient;
            mtl.diffuseColor = it->vDiffuse;
            mtl.specularColor = (it->bSpecular) ? it->vSpecular : XMFLOAT3(0.f, 0.f, 0.f);
            mtl.emissiveColor = (it->bEmissive) ? it->vEmissive : XMFLOAT3(0.f, 0.f, 0.f);

            mtl.texture = ProcessTextureFileName(it->strTexture, dds);
            mtl.normalTexture = ProcessTextureFileName(it->strNormalTexture, dds);
            mtl.specularTexture = ProcessTextureFileName(it->strSpecularTexture, dds);
            if (it->bEmissive)
            {
                mtl.emissiveTexture = ProcessTextureFileName(it->strEmissiveTexture, dds);
            }
            mtl.rmaTexture = ProcessTextureFileName(it->strRMATexture, dds);

            inMaterial.push_back(mtl);
        }
    }

    return S_OK;
}
