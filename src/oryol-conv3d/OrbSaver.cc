//------------------------------------------------------------------------------
//  OrbSaver.cc
//------------------------------------------------------------------------------
#include "OrbSaver.h"
#include "ExportUtil/Log.h"
#include "ExportUtil/VertexCodec.h"
#include <stdio.h>

using namespace OryolTools;
using namespace Oryol;

//------------------------------------------------------------------------------
uint32_t
OrbSaver::addString(const std::string& str) {
    ptrdiff_t index = std::find(strings.begin(), strings.end(), str) - strings.begin();
    if (int(index) < int(strings.size())) {
        // string already exists
        return index;
    }
    else {
        // add new string
        index = strings.size();
        strings.push_back(str);
    }
    return index;
}

//------------------------------------------------------------------------------
VertexFormat::Code
OrbSaver::mapVertexFormat(VertexAttr::Code attr, VertexFormat::Code fmt) const {
    // check if a vertex format mapping has been defined for a vertex attribute,
    // if yes, return mapped format, otherwise return original format
    if (VertexFormats.find(attr) != VertexFormats.end()) {
        return VertexFormats.at(attr);
    }
    else {
        return fmt;
    }
}

//------------------------------------------------------------------------------
static OrbVertexAttr::Enum
toOrbVertexAttr(VertexAttr::Code attr) {
    switch (attr) {
        case VertexAttr::Position:      return OrbVertexAttr::Position;
        case VertexAttr::Normal:        return OrbVertexAttr::Normal;
        case VertexAttr::TexCoord0:     return OrbVertexAttr::TexCoord0;
        case VertexAttr::TexCoord1:     return OrbVertexAttr::TexCoord1;
        case VertexAttr::TexCoord2:     return OrbVertexAttr::TexCoord2;
        case VertexAttr::TexCoord3:     return OrbVertexAttr::TexCoord3;
        case VertexAttr::Tangent:       return OrbVertexAttr::Tangent;
        case VertexAttr::Binormal:      return OrbVertexAttr::Binormal;
        case VertexAttr::Weights:       return OrbVertexAttr::Weights;
        case VertexAttr::Indices:       return OrbVertexAttr::Indices;
        case VertexAttr::Color0:        return OrbVertexAttr::Color0;
        case VertexAttr::Color1:        return OrbVertexAttr::Color1;
        default:                        return OrbVertexAttr::Invalid;
    }
}

//------------------------------------------------------------------------------
static OrbVertexFormat::Enum
toOrbVertexFormat(VertexFormat::Code fmt) {
    switch (fmt) {
        case VertexFormat::Float:       return OrbVertexFormat::Float;
        case VertexFormat::Float2:      return OrbVertexFormat::Float2;
        case VertexFormat::Float3:      return OrbVertexFormat::Float3;
        case VertexFormat::Float4:      return OrbVertexFormat::Float4;
        case VertexFormat::Byte4:       return OrbVertexFormat::Byte4;
        case VertexFormat::Byte4N:      return OrbVertexFormat::Byte4N;
        case VertexFormat::UByte4:      return OrbVertexFormat::UByte4;
        case VertexFormat::UByte4N:     return OrbVertexFormat::UByte4N;
        case VertexFormat::Short2:      return OrbVertexFormat::Short2;
        case VertexFormat::Short2N:     return OrbVertexFormat::Short2N;
        case VertexFormat::Short4:      return OrbVertexFormat::Short4;
        case VertexFormat::Short4N:     return OrbVertexFormat::Short4N;
        default:                        return OrbVertexFormat::Invalid;
    }
}

//------------------------------------------------------------------------------
static OrbAnimKeyFormat::Enum
toOrbAnimKeyFormat(IRep::KeyType::Enum t) {
    switch (t) {
        case IRep::KeyType::Float:      return OrbAnimKeyFormat::Float;
        case IRep::KeyType::Float2:     return OrbAnimKeyFormat::Float2;
        case IRep::KeyType::Float3:     return OrbAnimKeyFormat::Float3;
        case IRep::KeyType::Float4:     return OrbAnimKeyFormat::Float4;
        case IRep::KeyType::Quaternion: return OrbAnimKeyFormat::Quaternion;
        default:                        return OrbAnimKeyFormat::Invalid;
    }
}

//------------------------------------------------------------------------------
int
OrbSaver::vertexStrideInBytes(const IRep& irep) const {
    // compute the size of the destination vertex layout in num bytes
    int size = 0;
    for (const auto& comp : irep.VertexComponents) {
        size += VertexFormat::ByteSize(mapVertexFormat(comp.Attr, comp.Format));
    }
    return size;
}

//------------------------------------------------------------------------------
static uint32_t
roundup4(uint32_t val) {
    return (val + 3) & ~3;
}

//------------------------------------------------------------------------------
void
OrbSaver::Save(const std::string& path, const IRep& irep) {
    FILE* fp = fopen(path.c_str(), "wb");
    Log::FailIf(!fp, "Failed to open file '%s'\n", path.c_str());

    uint32_t offset = sizeof(OrbHeader);

    // setup the header with offset and numers of items
    OrbHeader hdr;
    hdr.Magic = 'ORB1';
    hdr.VertexComponentOffset = offset;
    hdr.NumVertexComponents = irep.VertexComponents.size();
    offset += sizeof(OrbVertexComponent) * hdr.NumVertexComponents;
    hdr.ValuePropOffset = offset;
    hdr.NumValueProps = irep.NumValueProps();
    offset += sizeof(OrbValueProperty) * hdr.NumValueProps;
    hdr.TexturePropOffset = offset;
    hdr.NumTextureProps = irep.NumTextureProps();
    offset += sizeof(OrbTextureProperty) * hdr.NumTextureProps;
    hdr.MaterialOffset = offset;
    hdr.NumMaterials = irep.Materials.size();
    offset += sizeof(OrbMaterial) * hdr.NumMaterials;
    hdr.MeshOffset = offset;
    hdr.NumMeshes = irep.NumMeshes();
    offset += sizeof(OrbMesh) * hdr.NumMeshes;
    hdr.BoneOffset = offset;
    hdr.NumBones = irep.Bones.size();
    offset += sizeof(OrbBone) * hdr.NumBones;
    hdr.NodeOffset = offset;
    hdr.NumNodes = irep.Nodes.size();
    offset += sizeof(OrbNode) * hdr.NumNodes;
    hdr.AnimKeyComponentOffset = offset;
    hdr.NumAnimKeyComponents = irep.KeyComponents.size();
    offset += sizeof(OrbAnimKeyFormat) * hdr.NumAnimKeyComponents;
    hdr.AnimCurveOffset = offset;
    hdr.NumAnimCurves = irep.NumAnimCurves();
    offset += sizeof(OrbAnimCurve) * hdr.NumAnimCurves;
    hdr.AnimClipOffset = offset;
    hdr.NumAnimClips = irep.AnimClips.size();
    offset += sizeof(OrbAnimClip) * hdr.NumAnimClips;
    const int vertexStrideInFloats = irep.VertexStrideBytes() / sizeof(float);
    Log::FailIf((irep.VertexData.size() % vertexStrideInFloats) != 0, "Vertex data size mismatch!\n");
    const int numVertices = irep.VertexData.size() / vertexStrideInFloats;
    Log::FailIf((irep.NumMeshVertices() != numVertices), "Number of vertices mismatch!\n");
    hdr.VertexDataOffset = offset;
    hdr.VertexDataSize = numVertices * vertexStrideInBytes(irep);
    offset += hdr.VertexDataSize;
    hdr.IndexDataOffset = offset;
    hdr.IndexDataSize = roundup4(irep.IndexData.size() * sizeof(uint16_t));
    offset += hdr.IndexDataSize;
    hdr.AnimKeyDataOffset = offset;
    hdr.AnimKeyDataSize = irep.KeyData.size() * sizeof(float);
    offset += hdr.AnimKeyDataSize;
    hdr.StringPoolDataOffset = offset;
    hdr.StringPoolDataSize = 0;     // this will be filled in at the end!
    fwrite(&hdr, 1, sizeof(hdr), fp);

    // write vertex components
    Log::FailIf(ftell(fp) != hdr.VertexComponentOffset, "File offset error (VertexComponentOffset)\n");
    for (const auto& src : irep.VertexComponents) {
        OrbVertexComponent dst;
        dst.Attr = toOrbVertexAttr(src.Attr);
        dst.Format = toOrbVertexFormat(mapVertexFormat(src.Attr, src.Format));
        fwrite(&dst, 1, sizeof(dst), fp);
    }

    // write value properties
    {
        Log::FailIf(ftell(fp) != hdr.ValuePropOffset, "File offset error (ValuePropOffset)\n");
        for (const auto& mat : irep.Materials) {
            for (const auto& src : mat.Values) {
                OrbValueProperty dst;
                dst.Name = addString(src.Name);
                dst.Dim = IRep::PropType::NumFloats(src.Type);
                for (int i = 0; i < 4; i++) {
                    dst.Value[i] = src.Value[i];
                }
                fwrite(&dst, 1, sizeof(dst), fp);
            }
        }
    }

    // write texture properties
    Log::FailIf(ftell(fp) != hdr.TexturePropOffset, "File offset error (TexturePropOffset)\n");
    for (const auto& mat : irep.Materials) {
        for (const auto& src : mat.Textures) {
            OrbTextureProperty dst;
            dst.Name = addString(src.Name);
            dst.Location = addString(src.Location);
            fwrite(&dst, 1, sizeof(dst), fp);
        }
    }

    // write materials
    {
        Log::FailIf(ftell(fp) != hdr.MaterialOffset, "File offset error (MaterialOffset)\n");
        uint32_t valPropIndex = 0;
        uint32_t texPropIndex = 0;
        for (const auto& src : irep.Materials) {
            OrbMaterial dst;
            dst.Name = addString(src.Name);
            dst.Shader = addString(src.Shader);
            dst.FirstValueProp = valPropIndex;
            dst.NumValueProps = src.Values.size();
            dst.FirstTextureProp = texPropIndex;
            dst.NumTextureProps = src.Textures.size();
            valPropIndex += dst.NumValueProps;
            texPropIndex += dst.NumTextureProps;
            fwrite(&dst, 1, sizeof(dst), fp);
        }
    }

    // write meshes
    Log::FailIf(ftell(fp) != hdr.MeshOffset, "File offset error (MeshOffset)\n");
    for (const auto& node : irep.Nodes) {
        for (const auto& src : node.Meshes) {
            OrbMesh dst;
            dst.Material = src.Material;
            dst.FirstVertex = src.FirstVertex;
            dst.NumVertices = src.NumVertices;
            dst.FirstIndex = src.FirstIndex;
            dst.NumIndices = src.NumIndices;
            for (int i = 0; i < 3; i++) {
                dst.Size[i] = src.Size[i];
            }
            fwrite(&dst, 1, sizeof(dst), fp);
        }
    }

    // write bones
    Log::FailIf(ftell(fp) != hdr.BoneOffset, "File offset error (BoneOffset)\n");
    for (const auto& src : irep.Bones) {
        OrbBone dst;
        dst.Name = addString(src.Name);
        dst.Parent = src.Parent;
        for (int i = 0; i < 3; i++) {
            dst.Translate[i] = src.Translate[i];
            dst.Scale[i] = src.Scale[i];
        }
        for (int i = 0; i < 4; i++) {
            dst.Rotate[i] = src.Rotate[i];
        }
        fwrite(&dst, 1, sizeof(dst), fp);
    }

    // write nodes
    {
        Log::FailIf(ftell(fp) != hdr.NodeOffset, "File offset error (NodeOffset)\n");
        uint32_t meshIndex = 0;
        for (const auto& src : irep.Nodes) {
            OrbNode dst;
            dst.Name = addString(src.Name);
            dst.Parent = src.Parent;
            dst.FirstMesh = meshIndex;
            dst.NumMeshes = src.Meshes.size();
            for (int i = 0; i < 3; i++) {
                dst.Translate[i] = src.Translate[i];
                dst.Scale[i] = src.Scale[i];
            }
            for (int i = 0; i < 4; i++) {
                dst.Rotate[i] = src.Rotate[i];
            }
            fwrite(&dst, 1, sizeof(dst), fp);
            meshIndex += dst.NumMeshes;
        }
    }

    // write anim key formats
    Log::FailIf(ftell(fp) != hdr.AnimKeyComponentOffset, "File offset error (AnimKeyComponentOffset)\n");
    for (const auto& src : irep.KeyComponents) {
        OrbAnimKeyComponent dst;
        dst.KeyFormat = toOrbAnimKeyFormat(src.Type);
        fwrite(&dst, 1, sizeof(dst), fp);
    }

    // write anim curves
    Log::FailIf(ftell(fp) != hdr.AnimCurveOffset, "File offset error (AnimCurveOffset)\n");
    for (const auto& clip : irep.AnimClips) {
        for (const auto& src : clip.Curves) {
            OrbAnimCurve dst;
            dst.KeyOffset = src.KeyOffset;
            for (int i = 0; i < 4; i++) {
                dst.StaticKey[i] = src.StaticKey[i];
            }
            fwrite(&dst, 1, sizeof(dst), fp);
        }
    } 

    // write anim clips
    {
        uint32_t curveIndex = 0;
        Log::FailIf(ftell(fp) != hdr.AnimClipOffset, "File offset error (AnimClipOffset)\n");
        for (const auto& src : irep.AnimClips) {
            OrbAnimClip dst;
            dst.Name = addString(src.Name);
            dst.KeyDuration = src.KeyDuration;
            dst.FirstCurve = curveIndex;
            dst.NumCurves = src.Curves.size();
            curveIndex += dst.NumCurves;
            fwrite(&dst, 1, sizeof(dst), fp);
        }
    }

    // write the vertex data
    {
        Log::FailIf(ftell(fp) != hdr.VertexDataOffset, "File offset error (VertexDataOffset)\n");
        const float* srcPtr = &(irep.VertexData[0]);
        uint8_t encodeSpace[1024];
        Log::FailIf(this->vertexStrideInBytes(irep) >= int(sizeof(encodeSpace)), "Dst vertex stride too big\n");
        int allEncodedBytes = 0;
        for (int i = 0; i < numVertices; i++) {
            uint8_t* dstPtr = encodeSpace;
            for (const auto& srcComp : irep.VertexComponents) {
                VertexFormat::Code dstFmt = this->mapVertexFormat(srcComp.Attr, srcComp.Format);
                const int numSrcItems = VertexFormat::NumItems(srcComp.Format);
                switch (dstFmt) {
                    case VertexFormat::Float:
                        dstPtr = VertexCodec::Encode<VertexFormat::Float>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::Float2:
                        dstPtr = VertexCodec::Encode<VertexFormat::Float2>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::Float3:
                        dstPtr = VertexCodec::Encode<VertexFormat::Float3>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::Float4:
                        dstPtr = VertexCodec::Encode<VertexFormat::Float4>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::Byte4:
                        dstPtr = VertexCodec::Encode<VertexFormat::Byte4>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::Byte4N:
                        dstPtr = VertexCodec::Encode<VertexFormat::Byte4N>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::UByte4:
                        dstPtr = VertexCodec::Encode<VertexFormat::UByte4>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::UByte4N:
                        dstPtr = VertexCodec::Encode<VertexFormat::UByte4N>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::Short2:
                        // FIXME: currently hardcoded for 3.15 fixed-point UV coords!
                        dstPtr = VertexCodec::Encode<VertexFormat::Short2>(dstPtr, 8192.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::Short2N:
                        dstPtr = VertexCodec::Encode<VertexFormat::Short2N>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::Short4:
                        dstPtr = VertexCodec::Encode<VertexFormat::Short4>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    case VertexFormat::Short4N:
                        dstPtr = VertexCodec::Encode<VertexFormat::Short4N>(dstPtr, 1.0f, srcPtr, numSrcItems);
                        break;
                    default: break;
                }
                srcPtr += numSrcItems;
            }
            const int numEncodedBytes = dstPtr - encodeSpace;
            allEncodedBytes += numEncodedBytes;
            fwrite(encodeSpace, 1, numEncodedBytes, fp);
        }
        Log::FailIf(srcPtr != &(*irep.VertexData.end()), "Encoding source length error\n");
        Log::FailIf(allEncodedBytes != int(hdr.VertexDataSize), "Encoded destination length error!\n");
    }

    // write vertex indices
    {
        Log::FailIf(ftell(fp) != hdr.IndexDataOffset, "File offset error (IndexDataOffset)\n");
        const uint16_t* ptr = &irep.IndexData[0];
        int numBytes = irep.IndexData.size() * sizeof(uint16_t);
        fwrite(ptr, 1, numBytes, fp);
        if ((numBytes & 3) != 0) {
            uint16_t padding = 0;
            fwrite(&padding, 1, sizeof(padding), fp);
        }
    }

    // FIXME: write animation keys
    Log::FailIf(hdr.AnimKeyDataSize > 0, "FIXME: write anim keys!\n");

    // write string pool
    {
        Log::FailIf(ftell(fp) != hdr.StringPoolDataOffset, "File offset error (StringPoolDataOffset)\n");
        uint32_t stringPoolDataSize = 0;
        for (const auto& str : this->strings) {
            uint32_t strDataLength = str.length()+1;
            stringPoolDataSize += strDataLength;
            fwrite(str.c_str(), 1, str.length()+1, fp);
        }
        // patch the written string pool size back into the hdr
        Log::FailIf(stringPoolDataSize != (ftell(fp)-hdr.StringPoolDataOffset), "String pool size mismatch!\n");
        fseek(fp, offsetof(OrbHeader, StringPoolDataSize), SEEK_SET);
        fwrite(&stringPoolDataSize, 1, sizeof(stringPoolDataSize), fp);
        fseek(fp, 0, SEEK_END);
    }
    fclose(fp);
}