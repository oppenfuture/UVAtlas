//--------------------------------------------------------------------------------------
// File: UVAtlas.cpp
//
// UVAtlas command-line tool (sample for UVAtlas library)
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>

#include <fstream>
#include <memory>
#include <list>

#include <dxgiformat.h>

#include "UVAtlas.h"
// #include <DirectXTex.h>

#include "Mesh.h"

//Uncomment to add support for OpenEXR (.exr)
//#define USE_OPENEXR

#ifdef USE_OPENEXR
// See <https://github.com/Microsoft/DirectXTex/wiki/Adding-OpenEXR> for details
#include "DirectXTexEXR.h"
#endif

using namespace DirectX;

enum OPTIONS
{
    OPT_RECURSIVE = 1,
    OPT_QUALITY,
    OPT_MAXCHARTS,
    OPT_MAXSTRETCH,
    OPT_GUTTER,
    OPT_WIDTH,
    OPT_HEIGHT,
    OPT_TOPOLOGICAL_ADJ,
    OPT_GEOMETRIC_ADJ,
    OPT_NORMALS,
    OPT_WEIGHT_BY_AREA,
    OPT_WEIGHT_BY_EQUAL,
    OPT_TANGENTS,
    OPT_CTF,
    OPT_COLOR_MESH,
    OPT_UV_MESH,
    OPT_IMT_TEXFILE,
    OPT_IMT_VERTEX,
    OPT_SDKMESH,
    OPT_SDKMESH_V2,
    OPT_CMO,
    OPT_VBO,
    OPT_PLY,
    OPT_OUTPUTFILE,
    OPT_CLOCKWISE,
    OPT_FORCE_32BIT_IB,
    OPT_OVERWRITE,
    OPT_NODDS,
    OPT_FLIP,
    OPT_FLIPU,
    OPT_FLIPV,
    OPT_FLIPZ,
    OPT_NOLOGO,
    OPT_FILELIST,
    OPT_MAX
};

static_assert(OPT_MAX <= 64, "dwOptions is a DWORD64 bitfield");

enum CHANNELS
{
    CHANNEL_NONE = 0,
    CHANNEL_NORMAL,
    CHANNEL_COLOR,
    CHANNEL_TEXCOORD,
};

struct SConversion
{
    char szSrc[MAX_PATH];
};

struct SValue
{
    const char *pName;
    DWORD dwValue;
};

static const XMFLOAT3 g_ColorList[8] =
{
    XMFLOAT3(1.0f, 0.5f, 0.5f),
    XMFLOAT3(0.5f, 1.0f, 0.5f),
    XMFLOAT3(1.0f, 1.0f, 0.5f),
    XMFLOAT3(0.5f, 1.0f, 1.0f),
    XMFLOAT3(1.0f, 0.5f, 0.75f),
    XMFLOAT3(0.0f, 0.5f, 0.75f),
    XMFLOAT3(0.5f, 0.5f, 0.75f),
    XMFLOAT3(0.5f, 0.5f, 1.0f),
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

const SValue g_pOptions [] =
{
    { "r",         OPT_RECURSIVE },
    { "q",         OPT_QUALITY },
    { "n",         OPT_MAXCHARTS },
    { "st",        OPT_MAXSTRETCH },
    { "g",         OPT_GUTTER },
    { "w",         OPT_WIDTH },
    { "h",         OPT_HEIGHT },
    { "ta",        OPT_TOPOLOGICAL_ADJ },
    { "ga",        OPT_GEOMETRIC_ADJ },
    { "nn",        OPT_NORMALS },
    { "na",        OPT_WEIGHT_BY_AREA },
    { "ne",        OPT_WEIGHT_BY_EQUAL },
    { "tt",        OPT_TANGENTS },
    { "tb",        OPT_CTF },
    { "c",         OPT_COLOR_MESH },
    { "t",         OPT_UV_MESH },
    { "it",        OPT_IMT_TEXFILE },
    { "iv",        OPT_IMT_VERTEX },
    { "o",         OPT_OUTPUTFILE },
    { "sdkmesh",   OPT_SDKMESH },
    { "sdkmesh2",  OPT_SDKMESH_V2 },
    { "cmo",       OPT_CMO },
    { "vbo",       OPT_VBO },
    { "ply",       OPT_PLY },
    { "cw",        OPT_CLOCKWISE },
    { "ib32",      OPT_FORCE_32BIT_IB },
    { "y",         OPT_OVERWRITE },
    { "nodds",     OPT_NODDS },
    { "flip",      OPT_FLIP },
    { "flipu",     OPT_FLIPU },
    { "flipv",     OPT_FLIPV },
    { "flipz",     OPT_FLIPZ },
    { "nologo",    OPT_NOLOGO },
    { "flist",     OPT_FILELIST },
    { nullptr,      0 }
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

namespace
{

#pragma prefast(disable : 26018, "Only used with static internal arrays")

    DWORD LookupByName(const char *pName, const SValue *pArray)
    {
        while (pArray->pName)
        {
            if (!std::string(pName).compare(pArray->pName))
                return pArray->dwValue;

            pArray++;
        }

        return 0;
    }


    const char* LookupByValue(DWORD pValue, const SValue *pArray)
    {
        while (pArray->pName)
        {
            if (pValue == pArray->dwValue)
                return pArray->pName;

            pArray++;
        }

        return "";
    }

    void PrintLogo()
    {
        wprintf(L"Microsoft (R) UVAtlas Command-line Tool\n");
        wprintf(L"Copyright (C) Microsoft Corp. All rights reserved.\n");
#ifdef _DEBUG
        wprintf(L"*** Debug build ***\n");
#endif
        wprintf(L"\n");
    }


    void PrintUsage()
    {
        PrintLogo();

        wprintf(L"Usage: uvatlas <options> <files>\n");
        wprintf(L"\n");
        wprintf(L"   Input file type must be PLY\n\n");
        wprintf(L"   Output file type:\n");
        wprintf(L"       -ply            Polygon File Fromat (.ply) format\n\n");
        wprintf(L"   -r                  wildcard filename search is recursive\n");
        wprintf(L"   -q <level>          sets quality level to DEFAULT, FAST or QUALITY\n");
        wprintf(L"   -n <number>         maximum number of charts to generate (def: 0)\n");
        wprintf(L"   -st <float>         maximum amount of stretch 0.0 to 1.0 (def: 0.16667)\n");
        wprintf(L"   -g <float>          the gutter width betwen charts in texels (def: 2.0)\n");
        wprintf(L"   -w <number>         texture width (def: 512)\n");
        wprintf(L"   -h <number>         texture height (def: 512)\n");
        wprintf(L"   -ta | -ga           generate topological vs. geometric adjancecy (def: ta)\n");
        wprintf(L"   -nn | -na | -ne     generate normals weighted by angle/area/equal\n");
        wprintf(L"   -tt                 generate tangents\n");
        wprintf(L"   -tb                 generate tangents & bi-tangents\n");
        wprintf(L"   -cw                 faces are clockwise (defaults to counter-clockwise)\n");
        wprintf(L"   -ib32               use 32-bit index buffer (SDKMESH only)\n");
        wprintf(L"   -c                  generate mesh with colors showing charts\n");
        wprintf(L"   -t                  generates a separate mesh with uvs - (*_texture)\n");
        wprintf(L"   -it <filename>      calculate IMT for the mesh using this texture map\n");
        wprintf(
            L"   -iv <channel>       calculate IMT using per-vertex data\n"
            L"                       NORMAL, COLOR, TEXCOORD\n");
        wprintf(L"   -nodds              prevents extension renaming in exported materials\n");
        wprintf(L"   -flip               reverse winding of faces\n");
        wprintf(L"   -flipu              inverts the u texcoords\n");
        wprintf(L"   -flipv              inverts the v texcoords\n");
        wprintf(L"   -flipz              flips the handedness of the positions/normals\n");
        wprintf(L"   -o <filename>       output filename\n");
        wprintf(L"   -y                  overwrite existing output file (if any)\n");
        wprintf(L"   -nologo             suppress copyright message\n");
        wprintf(L"   -flist <filename>   use text file with a list of input files (one per line)\n");

        wprintf(L"\n");
    }


    //--------------------------------------------------------------------------------------
    HRESULT __cdecl UVAtlasCallback(float fPercentDone)
    {
        static ULONGLONG s_lastTick = 0;

        ULONGLONG tick = GetTickCount64();

        if ((tick - s_lastTick) > 1000)
        {
            wprintf(L"%.2f%%   \r", double(fPercentDone) * 100);
            s_lastTick = tick;
        }

        return S_OK;
    }
}

extern HRESULT LoadFromPLY(const char* szFilename, std::unique_ptr<Mesh>& inMesh, std::vector<Mesh::Material>& inMaterial, bool ccw, bool dds);

//--------------------------------------------------------------------------------------
// Entry-point
//--------------------------------------------------------------------------------------
#pragma prefast(disable : 28198, "Command-line tool, frees all memory on exit")

int main(_In_ int argc, _In_z_count_(argc) char* argv[])
{
    // Parameters and defaults
    size_t maxCharts = 0;
    float maxStretch = 0.16667f;
    float gutter = 2.f;
    size_t width = 512;
    size_t height = 512;
    CHANNELS perVertex = CHANNEL_NONE;
    DWORD uvOptions = UVATLAS_DEFAULT;

    char szTexFile[MAX_PATH] = {};
    char szOutputFile[MAX_PATH] = {};

    HRESULT hr = S_OK;

    // Process command line
    DWORD64 dwOptions = 0;
    std::list<SConversion> conversion;

    for (int iArg = 1; iArg < argc; iArg++)
    {
        char *pArg = argv[iArg];

        if (('-' == pArg[0]) || ('/' == pArg[0]))
        {
            pArg++;
            char *pValue;

            for (pValue = pArg; *pValue && (':' != *pValue); pValue++);

            if (*pValue)
                *pValue++ = 0;

            DWORD dwOption = LookupByName(pArg, g_pOptions);

            if (!dwOption || (dwOptions & (DWORD64(1) << dwOption)))
            {
                wprintf(L"ERROR: unknown command-line option '%s'\n\n", pArg);
                PrintUsage();
                return 1;
            }

            dwOptions |= (DWORD64(1) << dwOption);

            // Handle options with additional value parameter
            switch (dwOption)
            {
            case OPT_QUALITY:
            case OPT_MAXCHARTS:
            case OPT_MAXSTRETCH:
            case OPT_GUTTER:
            case OPT_WIDTH:
            case OPT_HEIGHT:
            case OPT_IMT_TEXFILE:
            case OPT_IMT_VERTEX:
            case OPT_OUTPUTFILE:
            case OPT_FILELIST:
                if (!*pValue)
                {
                    if ((iArg + 1 >= argc))
                    {
                        wprintf(L"ERROR: missing value for command-line option '%s'\n\n", pArg);
                        PrintUsage();
                        return 1;
                    }

                    iArg++;
                    pValue = argv[iArg];
                }
                break;
            }

            switch (dwOption)
            {
            case OPT_QUALITY:
                if (!_stricmp(pValue, "DEFAULT"))
                {
                    uvOptions = UVATLAS_DEFAULT;
                }
                else if (!_stricmp(pValue, "FAST"))
                {
                    uvOptions = UVATLAS_GEODESIC_FAST;
                }
                else if (!_stricmp(pValue, "QUALITY"))
                {
                    uvOptions = UVATLAS_GEODESIC_QUALITY;
                }
                else
                {
                    wprintf(L"Invalid value specified with -q (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_MAXCHARTS:
                if (sscanf(pValue, "%zu", &maxCharts) != 1)
                {
                    wprintf(L"Invalid value specified with -n (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_MAXSTRETCH:
                if (sscanf(pValue, "%f", &maxStretch) != 1
                    || maxStretch < 0.f
                    || maxStretch > 1.f)
                {
                    wprintf(L"Invalid value specified with -st (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_GUTTER:
                if (sscanf(pValue, "%f", &gutter) != 1
                    || gutter < 0.f)
                {
                    wprintf(L"Invalid value specified with -g (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_WIDTH:
                if (sscanf(pValue, "%zu", &width) != 1)
                {
                    wprintf(L"Invalid value specified with -w (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_HEIGHT:
                if (sscanf(pValue, "%zu", &height) != 1)
                {
                    wprintf(L"Invalid value specified with -h (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_WEIGHT_BY_AREA:
                if (dwOptions & (DWORD64(1) << OPT_WEIGHT_BY_EQUAL))
                {
                    wprintf(L"Can only use one of nn, na, or ne\n");
                    return 1;
                }
                dwOptions |= (DWORD64(1) << OPT_NORMALS);
                break;

            case OPT_WEIGHT_BY_EQUAL:
                if (dwOptions & (DWORD64(1) << OPT_WEIGHT_BY_AREA))
                {
                    wprintf(L"Can only use one of nn, na, or ne\n");
                    return 1;
                }
                dwOptions |= (DWORD64(1) << OPT_NORMALS);
                break;

            case OPT_IMT_TEXFILE:
                if (dwOptions & (DWORD64(1) << OPT_IMT_VERTEX))
                {
                    wprintf(L"Cannot use both if and iv at the same time\n");
                    return 1;
                }

                strcpy(szTexFile, pValue);
                break;

            case OPT_IMT_VERTEX:
                if (dwOptions & (DWORD64(1) << OPT_IMT_TEXFILE))
                {
                    wprintf(L"Cannot use both if and iv at the same time\n");
                    return 1;
                }

                if (!_stricmp(pValue, "COLOR"))
                {
                    perVertex = CHANNEL_COLOR;
                }
                else if (!_stricmp(pValue, "NORMAL"))
                {
                    perVertex = CHANNEL_NORMAL;
                }
                else if (!_stricmp(pValue, "TEXCOORD"))
                {
                    perVertex = CHANNEL_TEXCOORD;
                }
                else
                {
                    wprintf(L"Invalid value specified with -iv (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_OUTPUTFILE:
                strcpy(szOutputFile, pValue);
                break;

            case OPT_TOPOLOGICAL_ADJ:
                if (dwOptions & (DWORD64(1) << OPT_GEOMETRIC_ADJ))
                {
                    wprintf(L"Cannot use both ta and ga at the same time\n");
                    return 1;
                }
                break;

            case OPT_GEOMETRIC_ADJ:
                if (dwOptions & (DWORD64(1) << OPT_TOPOLOGICAL_ADJ))
                {
                    wprintf(L"Cannot use both ta and ga at the same time\n");
                    return 1;
                }
                break;

            case OPT_SDKMESH:
            case OPT_SDKMESH_V2:
                if (dwOptions & ((DWORD64(1) << OPT_VBO) | (DWORD64(1) << OPT_CMO) | (DWORD64(1) << OPT_PLY)))
                {
                    wprintf(L"Can only use one of sdkmesh, cmo, vbo or ply\n");
                    return 1;
                }
                if (dwOption == OPT_SDKMESH_V2)
                {
                    dwOptions |= (DWORD64(1) << OPT_SDKMESH);
                }
                break;

            case OPT_CMO:
                if (dwOptions & ((DWORD64(1) << OPT_VBO) | (DWORD64(1) << OPT_SDKMESH) | (DWORD64(1) << OPT_PLY)))
                {
                    wprintf(L"Can only use one of sdkmesh, cmo, vbo or ply\n");
                    return 1;
                }
                break;

            case OPT_VBO:
                if (dwOptions & ((DWORD64(1) << OPT_SDKMESH) | (DWORD64(1) << OPT_CMO) | (DWORD64(1) << OPT_PLY)))
                {
                    wprintf(L"Can only use one of sdkmesh, cmo, vbo or ply\n");
                    return 1;
                }
                break;

            case OPT_PLY:
                if (dwOption & ((DWORD64(1) << OPT_SDKMESH) | (DWORD64(1) << OPT_CMO) | (DWORD64(1) << OPT_VBO)))
                {
                    wprintf(L"Can only use one of sdkmesh, cmo, vbo or ply\n");
                    return 1;
                }
                break;

            case OPT_FILELIST:
                {
                    wprintf(L"ERROR: unknown command-line option '%s'\n\n", pArg);
                    PrintUsage();
                    return 1;
                    // std::wifstream inFile(pValue);
                    // if (!inFile)
                    // {
                    //     wprintf(L"Error opening -flist file %ls\n", pValue);
                    //     return 1;
                    // }
                    // wchar_t fname[1024] = {};
                    // for (;;)
                    // {
                    //     inFile >> fname;
                    //     if (!inFile)
                    //         break;

                    //     if (*fname == L'#')
                    //     {
                    //         // Comment
                    //     }
                    //     else if (*fname == L'-')
                    //     {
                    //         wprintf(L"Command-line arguments not supported in -flist file\n");
                    //         return 1;
                    //     }
                    //     else if (wcspbrk(fname, L"?*") != nullptr)
                    //     {
                    //         wprintf(L"Wildcards not supported in -flist file\n");
                    //         return 1;
                    //     }
                    //     else
                    //     {
                    //         SConversion conv;
                    //         wcscpy(conv.szSrc, fname);
                    //         conversion.push_back(conv);
                    //     }

                    //     inFile.ignore(1000, '\n');
                    // }
                    // inFile.close();
                }
                break;
            }
        }
        else if (strpbrk(pArg, "?*") != nullptr)
        {
            wprintf(L"ERROR: unknown command-line option '%s'\n\n", pArg);
            PrintUsage();
            return 1;
            // size_t count = conversion.size();
            // SearchForFiles(pArg, conversion, (dwOptions & (DWORD64(1) << OPT_RECURSIVE)) != 0);
            // if (conversion.size() <= count)
            // {
            //     wprintf(L"No matching files found for %ls\n", pArg);
            //     return 1;
            // }
        }
        else
        {
            SConversion conv;
            strcpy(conv.szSrc, pArg);

            conversion.push_back(conv);
        }
    }

    if (conversion.empty())
    {
        PrintUsage();
        return 0;
    }

    if (*szOutputFile && conversion.size() > 1)
    {
        wprintf(L"Cannot use -o with multiple input files\n");
        return 1;
    }

    if (~dwOptions & (DWORD64(1) << OPT_NOLOGO))
        PrintLogo();

    // Process files
    for (auto pConv = conversion.begin(); pConv != conversion.end(); ++pConv)
    {
        char ext[_MAX_EXT];
        char fname[_MAX_FNAME];
        _splitpath_s(pConv->szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT);

        if (pConv != conversion.begin())
            wprintf(L"\n");

        wprintf(L"reading %s\n", pConv->szSrc);
        fflush(stdout);

        std::unique_ptr<Mesh> inMesh;
        std::vector<Mesh::Material> inMaterial;
        hr = E_NOTIMPL;
        if (_stricmp(ext, ".vbo") == 0)
        {
            wprintf(L"\nERROR: Importing VBO files not supported\n");
            return 1;
        }
        else if (_stricmp(ext, ".sdkmesh") == 0)
        {
            wprintf(L"\nERROR: Importing SDKMESH files not supported\n");
            return 1;
        }
        else if (_stricmp(ext, ".cmo") == 0)
        {
            wprintf(L"\nERROR: Importing Visual Studio CMO files not supported\n");
            return 1;
        }
        else if (_stricmp(ext, ".x") == 0)
        {
            wprintf(L"\nERROR: Legacy Microsoft X files not supported\n");
            return 1;
        }
        else if (_stricmp(ext, ".fbx") == 0)
        {
            wprintf(L"\nERROR: Autodesk FBX files not supported\n");
            return 1;
        }
        else
        {
            hr = LoadFromPLY(pConv->szSrc, inMesh, inMaterial,
                (dwOptions & (1 << OPT_CLOCKWISE)) ? false : true,
                (dwOptions & (1 << OPT_NODDS)) ? false : true);
        }
        if (FAILED(hr))
        {
            wprintf(L" FAILED (%08X)\n", hr);
            return 1;
        }

        size_t nVerts = inMesh->GetVertexCount();
        size_t nFaces = inMesh->GetFaceCount();

        if (!nVerts || !nFaces)
        {
            wprintf(L"\nERROR: Invalid mesh\n");
            return 1;
        }

        assert(inMesh->GetPositionBuffer() != 0);
        assert(inMesh->GetIndexBuffer() != 0);

        wprintf(L"\n%zu vertices, %zu faces", nVerts, nFaces);

        if (dwOptions & (DWORD64(1) << OPT_FLIPU))
        {
            hr = inMesh->InvertUTexCoord();
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed inverting u texcoord (%08X)\n", hr);
                return 1;
            }
        }

        if (dwOptions & (DWORD64(1) << OPT_FLIPV))
        {
            hr = inMesh->InvertVTexCoord();
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed inverting v texcoord (%08X)\n", hr);
                return 1;
            }
        }

        if (dwOptions & (DWORD64(1) << OPT_FLIPZ))
        {
            hr = inMesh->ReverseHandedness();
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed reversing handedness (%08X)\n", hr);
                return 1;
            }
        }

        // Prepare mesh for processing
        {
            // Adjacency
            float epsilon = (dwOptions & (DWORD64(1) << OPT_GEOMETRIC_ADJ)) ? 1e-5f : 0.f;

            hr = inMesh->GenerateAdjacency(epsilon);
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed generating adjacency (%08X)\n", hr);
                return 1;
            }

            // Validation
            std::wstring msgs;
            hr = inMesh->Validate(VALIDATE_BACKFACING | VALIDATE_BOWTIES, &msgs);
            if (!msgs.empty())
            {
                wprintf(L"\nWARNING: \n");
                wprintf(msgs.c_str());
            }

            // Clean
            hr = inMesh->Clean(true);
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed mesh clean (%08X)\n", hr);
                return 1;
            }
            else
            {
                size_t nNewVerts = inMesh->GetVertexCount();
                if (nVerts != nNewVerts)
                {
                    wprintf(L" [%zu vertex dups] ", nNewVerts - nVerts);
                    nVerts = nNewVerts;
                }
            }
        }

        if (!inMesh->GetNormalBuffer())
        {
            dwOptions |= DWORD64(1) << OPT_NORMALS;
        }

        if (!inMesh->GetTangentBuffer() && (dwOptions & (DWORD64(1) << OPT_CMO)))
        {
            dwOptions |= DWORD64(1) << OPT_TANGENTS;
        }

        // Compute vertex normals from faces
        if ((dwOptions & (DWORD64(1) << OPT_NORMALS))
            || ((dwOptions & ((DWORD64(1) << OPT_TANGENTS) | (DWORD64(1) << OPT_CTF))) && !inMesh->GetNormalBuffer()))
        {
            DWORD flags = CNORM_DEFAULT;

            if (dwOptions & (DWORD64(1) << OPT_WEIGHT_BY_EQUAL))
            {
                flags |= CNORM_WEIGHT_EQUAL;
            }
            else if (dwOptions & (DWORD64(1) << OPT_WEIGHT_BY_AREA))
            {
                flags |= CNORM_WEIGHT_BY_AREA;
            }

            if (dwOptions & (DWORD64(1) << OPT_CLOCKWISE))
            {
                flags |= CNORM_WIND_CW;
            }

            hr = inMesh->ComputeNormals(flags);
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed computing normals (flags:%1X, %08X)\n", flags, hr);
                return 1;
            }
        }

        // Compute tangents and bitangents
        if (dwOptions & ((DWORD64(1) << OPT_TANGENTS) | (DWORD64(1) << OPT_CTF)))
        {
            if (!inMesh->GetTexCoordBuffer())
            {
                wprintf(L"\nERROR: Computing tangents/bi-tangents requires texture coordinates\n");
                return 1;
            }

            hr = inMesh->ComputeTangentFrame((dwOptions & (DWORD64(1) << OPT_CTF)) ? true : false);
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed computing tangent frame (%08X)\n", hr);
                return 1;
            }
        }

        // Compute IMT
        std::unique_ptr<float[]> IMTData;
        if (dwOptions & ((DWORD64(1) << OPT_IMT_TEXFILE) | (DWORD64(1) << OPT_IMT_VERTEX)))
        {
            if (dwOptions & (DWORD64(1) << OPT_IMT_TEXFILE))
            {
                hr = E_FAIL;
//                 if (!inMesh->GetTexCoordBuffer())
//                 {
//                     wprintf(L"\nERROR: Computing IMT from texture requires texture coordinates\n");
//                     return 1;
//                 }

//                 wchar_t txext[_MAX_EXT];
//                 _wsplitpath_s(szTexFile, nullptr, nullptr, txext);

//                 ScratchImage iimage;

//                 if (_wcsicmp(txext, L".dds") == 0)
//                 {
//                     hr = LoadFromDDSFile(szTexFile, DDS_FLAGS_NONE, nullptr, iimage);
//                 }
//                 else if (_wcsicmp(ext, L".tga") == 0)
//                 {
//                     hr = LoadFromTGAFile(szTexFile, nullptr, iimage);
//                 }
//                 else if (_wcsicmp(ext, L".hdr") == 0)
//                 {
//                     hr = LoadFromHDRFile(szTexFile, nullptr, iimage);
//                 }
// #ifdef USE_OPENEXR
//                 else if (_wcsicmp(ext, L".exr") == 0)
//                 {
//                     hr = LoadFromEXRFile(szTexFile, nullptr, iimage);
//                 }
// #endif
//                 else
//                 {
//                     // hr = LoadFromWICFile(szTexFile, TEX_FILTER_DEFAULT, nullptr, iimage);
//                     hr = E_FAIL;
//                 }
//                 if (FAILED(hr))
//                 {
//                     wprintf(L"\nWARNING: Failed to load texture for IMT (%08X):\n%ls\n", hr, szTexFile);
//                 }
//                 else
//                 {
//                     const Image* img = iimage.GetImage(0, 0, 0);

//                     ScratchImage floatImage;
//                     if (img->format != DXGI_FORMAT_R32G32B32A32_FLOAT)
//                     {
//                         hr = Convert(*iimage.GetImage(0, 0, 0), DXGI_FORMAT_R32G32B32A32_FLOAT, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, floatImage);
//                         if (FAILED(hr))
//                         {
//                             img = nullptr;
//                             wprintf(L"\nWARNING: Failed converting texture for IMT (%08X):\n%ls\n", hr, szTexFile);
//                         }
//                         else
//                         {
//                             img = floatImage.GetImage(0, 0, 0);
//                         }
//                     }

//                     if (img)
//                     {
//                         wprintf(L"\nComputing IMT from file %ls...\n", szTexFile);
//                         IMTData.reset(new (std::nothrow) float[nFaces * 3]);
//                         if (!IMTData)
//                         {
//                             wprintf(L"\nERROR: out of memory\n");
//                             return 1;
//                         }

//                         hr = UVAtlasComputeIMTFromTexture(inMesh->GetPositionBuffer(), inMesh->GetTexCoordBuffer(), nVerts,
//                             inMesh->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, nFaces,
//                             reinterpret_cast<const float*>(img->pixels), img->width, img->height,
//                             UVATLAS_IMT_DEFAULT, UVAtlasCallback, IMTData.get());
//                         if (FAILED(hr))
//                         {
//                             IMTData.reset();
//                             wprintf(L"WARNING: Failed to compute IMT from texture (%08X):\n%ls\n", hr, szTexFile);
//                         }
//                     }
//                 }
            }
            else
            {
                const wchar_t* szChannel = L"*unknown*";
                const float* pSignal = nullptr;
                size_t signalDim = 0;
                size_t signalStride = 0;
                switch (perVertex)
                {
                case CHANNEL_NORMAL:
                    szChannel = L"normals";
                    if (inMesh->GetNormalBuffer())
                    {
                        pSignal = reinterpret_cast<const float*>(inMesh->GetNormalBuffer());
                        signalDim = 3;
                        signalStride = sizeof(XMFLOAT3);
                    }
                    break;

                case CHANNEL_COLOR:
                    szChannel = L"vertex colors";
                    if (inMesh->GetColorBuffer())
                    {
                        pSignal = reinterpret_cast<const float*>(inMesh->GetColorBuffer());
                        signalDim = 4;
                        signalStride = sizeof(XMFLOAT4);
                    }
                    break;

                case CHANNEL_TEXCOORD:
                    szChannel = L"texture coordinates";
                    if (inMesh->GetTexCoordBuffer())
                    {
                        pSignal = reinterpret_cast<const float*>(inMesh->GetTexCoordBuffer());
                        signalDim = 2;
                        signalStride = sizeof(XMFLOAT2);
                    }
                    break;
                }

                if (!pSignal)
                {
                    wprintf(L"\nWARNING: Mesh does not have channel %ls for IMT\n", szChannel);
                }
                else
                {
                    wprintf(L"\nComputing IMT from %ls...\n", szChannel);

                    IMTData.reset(new (std::nothrow) float[nFaces * 3]);
                    if (!IMTData)
                    {
                        wprintf(L"\nERROR: out of memory\n");
                        return 1;
                    }

                    hr = UVAtlasComputeIMTFromPerVertexSignal(inMesh->GetPositionBuffer(), nVerts,
                        inMesh->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, nFaces,
                        pSignal, signalDim, signalStride, UVAtlasCallback, IMTData.get());

                    if (FAILED(hr))
                    {
                        IMTData.reset();
                        wprintf(L"WARNING: Failed to compute IMT from channel %ls (%08X)\n", szChannel, hr);
                    }
                }
            }
        }
        else
        {
            wprintf(L"\n");
        }

        // Perform UVAtlas isocharting
        wprintf(L"Computing isochart atlas on mesh...\n");

        std::vector<UVAtlasVertex> vb;
        std::vector<uint8_t> ib;
        float outStretch = 0.f;
        size_t outCharts = 0;
        std::vector<uint32_t> facePartitioning;
        std::vector<uint32_t> vertexRemapArray;
        hr = UVAtlasCreate(inMesh->GetPositionBuffer(), nVerts,
            inMesh->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, nFaces,
            maxCharts, maxStretch, width, height, gutter,
            inMesh->GetAdjacencyBuffer(), nullptr,
            IMTData.get(),
            UVAtlasCallback, UVATLAS_DEFAULT_CALLBACK_FREQUENCY,
            uvOptions, vb, ib,
            &facePartitioning,
            &vertexRemapArray,
            &outStretch, &outCharts);
        if (FAILED(hr))
        {
            if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_DATA))
            {
                wprintf(L"\nERROR: Non-manifold mesh\n");
                return 1;
            }
            else
            {
                wprintf(L"\nERROR: Failed creating isocharts (%08X)\n", hr);
                return 1;
            }
        }

        wprintf(L"Output # of charts: %zu, resulting stretching %f, %zu verts\n", outCharts, outStretch, vb.size());

        assert((ib.size() / sizeof(uint32_t)) == (nFaces * 3));
        assert(facePartitioning.size() == nFaces);
        assert(vertexRemapArray.size() == vb.size());

        hr = inMesh->UpdateFaces(nFaces, reinterpret_cast<const uint32_t*>(ib.data()));
        if (FAILED(hr))
        {
            wprintf(L"\nERROR: Failed applying atlas indices (%08X)\n", hr);
            return 1;
        }

        hr = inMesh->VertexRemap(vertexRemapArray.data(), vertexRemapArray.size());
        if (FAILED(hr))
        {
            wprintf(L"\nERROR: Failed applying atlas vertex remap (%08X)\n", hr);
            return 1;
        }

        nVerts = vb.size();

#ifdef _DEBUG
        std::wstring msgs;
        hr = inMesh->Validate(VALIDATE_DEFAULT, &msgs);
        if (!msgs.empty())
        {
            wprintf(L"\nWARNING: \n");
            wprintf(msgs.c_str());
        }
#endif

        // Copy isochart UVs into mesh
        {
            std::unique_ptr<XMFLOAT2[]> texcoord(new (std::nothrow) XMFLOAT2[nVerts]);
            if (!texcoord)
            {
                wprintf(L"\nERROR: out of memory\n");
                return 1;
            }

            auto txptr = texcoord.get();
            size_t j = 0;
            for (auto it = vb.cbegin(); it != vb.cend() && j < nVerts; ++it, ++txptr)
            {
                *txptr = it->uv;
            }

            hr = inMesh->UpdateUVs(nVerts, texcoord.get());
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed to update with isochart UVs\n");
                return 1;
            }
        }

        if (dwOptions & (DWORD64(1) << OPT_COLOR_MESH))
        {
            inMaterial.clear();
            inMaterial.reserve(_countof(g_ColorList));

            for (size_t j = 0; j < _countof(g_ColorList) && (j < outCharts); ++j)
            {
                Mesh::Material mtl = {};

                wchar_t matname[32] = {};
                swprintf(matname, 32, L"Chart%02Iu", j + 1);
                mtl.name = matname;
                mtl.specularPower = 1.f;
                mtl.alpha = 1.f;

                XMVECTOR v = XMLoadFloat3(&g_ColorList[j]);
                XMStoreFloat3(&mtl.diffuseColor, v);

                v = XMVectorScale(v, 0.2f);
                XMStoreFloat3(&mtl.ambientColor, v);

                inMaterial.push_back(mtl);
            }

            std::unique_ptr<uint32_t[]> attr(new (std::nothrow) uint32_t[nFaces]);
            if (!attr)
            {
                wprintf(L"\nERROR: out of memory\n");
                return 1;
            }

            size_t j = 0;
            for (auto it = facePartitioning.cbegin(); it != facePartitioning.cend(); ++it, ++j)
            {
                attr[j] = *it % _countof(g_ColorList);
            }

            hr = inMesh->UpdateAttributes(nFaces, attr.get());
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed applying atlas attributes (%08X)\n", hr);
                return 1;
            }
        }

        if (dwOptions & (DWORD64(1) << OPT_FLIP))
        {
            hr = inMesh->ReverseWinding();
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed reversing winding (%08X)\n", hr);
                return 1;
            }
        }

        // Write results
        wprintf(L"\n\t->\n");

        char outputPath[MAX_PATH] = {};
        char outputExt[_MAX_EXT] = {};

        if (*szOutputFile)
        {
            strcpy(outputPath, szOutputFile);

            _splitpath_s(szOutputFile, nullptr, 0, nullptr, 0, nullptr, 0, outputExt, _MAX_EXT);
        }
        else
        {
            if (dwOptions & (DWORD64(1) << OPT_VBO))
            {
                strcpy(outputExt, ".vbo");
            }
            else if (dwOptions & (DWORD64(1) << OPT_CMO))
            {
                strcpy(outputExt, ".cmo");
            }
            else if (dwOptions & (DWORD64(1) << OPT_PLY))
            {
                strcpy(outputExt, ".ply");
            }
            else
            {
                strcpy(outputExt, ".sdkmesh");
            }

            char outFilename[_MAX_FNAME] = {};
            strcpy(outFilename, fname);

            _makepath_s(outputPath, nullptr, nullptr, outFilename, outputExt);
        }

        if (~dwOptions & (DWORD64(1) << OPT_OVERWRITE))
        {
            struct stat buffer;
            if (stat(outputPath, &buffer) == 0)
            {
                wprintf(L"\nERROR: Output file already exists, use -y to overwrite:\n'%s'\n", outputPath);
                return 1;
            }
        }

        if (!_stricmp(outputExt, ".vbo"))
        {
            if (!inMesh->GetNormalBuffer() || !inMesh->GetTexCoordBuffer())
            {
                wprintf(L"\nERROR: VBO requires position, normal, and texcoord\n");
                return 1;
            }

            if (!inMesh->Is16BitIndexBuffer() || (dwOptions & (DWORD64(1) << OPT_FORCE_32BIT_IB)))
            {
                wprintf(L"\nERROR: VBO only supports 16-bit indices\n");
                return 1;
            }

            wprintf(L"\nERROR: VBO files not supported\n");
            return 1;
        }
        else if (!_stricmp(outputExt, ".sdkmesh"))
        {
            wprintf(L"\nERROR: SDKMESH files not supported\n");
            return 1;
        }
        else if (!_stricmp(outputExt, ".cmo"))
        {
            if (!inMesh->GetNormalBuffer() || !inMesh->GetTexCoordBuffer() || !inMesh->GetTangentBuffer())
            {
                wprintf(L"\nERROR: Visual Studio CMO requires position, normal, tangents, and texcoord\n");
                return 1;
            }

            if (!inMesh->Is16BitIndexBuffer() || (dwOptions & (DWORD64(1) << OPT_FORCE_32BIT_IB)))
            {
                wprintf(L"\nERROR: Visual Studio CMO only supports 16-bit indices\n");
                return 1;
            }

            wprintf(L"\nERROR: CMO files not supported\n");
            return 1;
        }
        else if (!_stricmp(outputExt, ".ply"))
        {
            hr = inMesh->ExportToPLY(outputPath);
        }
        else if (!_stricmp(outputExt, ".x"))
        {
            wprintf(L"\nERROR: Legacy Microsoft X files not supported\n");
            return 1;
        }
        else
        {
            wprintf(L"\nERROR: Unknown output file type '%s'\n", outputExt);
            return 1;
        }

        if (FAILED(hr))
        {
            wprintf(L"\nERROR: Failed write (%08X):-> '%s'\n", hr, outputPath);
            return 1;
        }

        wprintf(L" %zu vertices, %zu faces written:\n'%s'\n", nVerts, nFaces, outputPath);

        // Write out UV mesh visualization
        if (dwOptions & (DWORD64(1) << OPT_UV_MESH))
        {
            hr = inMesh->VisualizeUVs();
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed to create UV visualization mesh\n");
                return 1;
            }

            char uvFilename[_MAX_FNAME] = {};
            strcpy(uvFilename, fname);
            strcat(uvFilename, "_texture");

            _makepath_s(outputPath, nullptr, nullptr, uvFilename, outputExt);

            if (!_stricmp(outputExt, ".vbo"))
            {
                wprintf(L"\nERROR: VBO files not supported\n");
                return 1;
            }
            else if (!_stricmp(outputExt, ".sdkmesh"))
            {
                wprintf(L"\nERROR: SDKMESH files not supported\n");
                return 1;
            }
            else if (!_stricmp(outputExt, ".cmo"))
            {
                wprintf(L"\nERROR: CMO files not supported\n");
                return 1;
            }
            if (FAILED(hr))
            {
                wprintf(L"\nERROR: Failed uv mesh write (%08X):-> '%s'\n", hr, outputPath);
                return 1;
            }
            wprintf(L"uv mesh visualization '%s'\n", outputPath);
        }
    }

    return 0;
}
