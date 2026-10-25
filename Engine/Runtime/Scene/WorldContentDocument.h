// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/ContentIdentity.h"
#include "Engine/Runtime/Scene/SceneDocument.h"

struct ContentRecordKey
{
    std::uint64_t key=0;
    std::string guid;
    bool shared=false; // Material placeholder resolves against the world's persistent table.
};
struct ContentDocument
{
    SceneDocument scene;
    std::vector<ContentRecordKey> actors,materials;
};
struct ContentRegionRecord
{
    std::string id,name;
    SceneAssetPath source;
    cy::Vec3f minimum{0.0f},maximum{1.0f};
};
struct RegionContentDocument
{
    std::string world,id;
    ContentDocument content;
};
struct WorldContentDocument
{
    std::string id;
    ContentDocument persistent;
    std::vector<ContentRegionRecord> regions;
};
bool ValidateContentDocument(const ContentDocument&, bool region, std::string& error);
bool EncodeWorldContentDocument(const WorldContentDocument&, std::string&, std::string& error);
bool DecodeWorldContentDocument(const std::string&, WorldContentDocument&, std::string& error);
bool EncodeRegionContentDocument(const RegionContentDocument&, std::string&, std::string& error);
bool DecodeRegionContentDocument(const std::string&, RegionContentDocument&, std::string& error);
bool ReadWorldContentDocument(const std::filesystem::path&, WorldContentDocument&, std::string& error);
bool ReadRegionContentDocument(const std::filesystem::path&, RegionContentDocument&, std::string& error);
bool WriteWorldContentDocument(const std::filesystem::path&, const WorldContentDocument&, std::string& error, bool overwrite=true);
bool WriteRegionContentDocument(const std::filesystem::path&, const RegionContentDocument&, std::string& error, bool overwrite=true);
