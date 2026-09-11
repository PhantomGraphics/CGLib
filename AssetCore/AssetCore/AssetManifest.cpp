#include "AssetManifest.h"

#include "json.hpp"

#include <fstream>
#include <sstream>

namespace Phantom::Asset {

void AssetManifest::upsert(AssetManifestEntry entry)
{
    entries_[entry.id.value()] = std::move(entry);
}

bool AssetManifest::remove(const AssetId& id)
{
    return entries_.erase(id.value()) > 0;
}

const AssetManifestEntry* AssetManifest::find(const AssetId& id) const
{
    const auto it = entries_.find(id.value());
    return it == entries_.end() ? nullptr : &it->second;
}

std::string AssetManifest::toJson() const
{
    nlohmann::json root;
    root["schema"] = "phantom.manifest/1";
    nlohmann::json assets = nlohmann::json::array();
    for (const auto& [key, entry] : entries_) {
        nlohmann::json j;
        j["id"] = entry.id.value();
        j["uri"] = entry.uri.value();
        j["contentHash"] = entry.contentHash.value();
        j["sourceBlend"] = entry.sourceBlend;
        nlohmann::json deps = nlohmann::json::array();
        for (const auto& dep : entry.dependencies) deps.push_back(dep.value());
        j["dependencies"] = std::move(deps);
        assets.push_back(std::move(j));
    }
    root["assets"] = std::move(assets);
    return root.dump();
}

AssetManifest AssetManifest::fromJson(const std::string& json, bool* ok)
{
    AssetManifest manifest;
    if (ok) *ok = false;
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json);
    } catch (const nlohmann::json::parse_error&) {
        return manifest;
    }
    if (!root.is_object() || root.value("schema", "") != "phantom.manifest/1") return manifest;
    if (!root.contains("assets") || !root["assets"].is_array()) return manifest;

    for (const auto& j : root["assets"]) {
        if (!j.is_object() || !j.contains("id")) continue;
        AssetManifestEntry entry;
        entry.id = AssetId(j.value("id", ""));
        if (!entry.id.isValid()) continue;
        if (auto uri = AssetUri::parse(j.value("uri", ""))) entry.uri = *uri;
        entry.contentHash = ContentHash(j.value("contentHash", ""));
        entry.sourceBlend = j.value("sourceBlend", "");
        if (j.contains("dependencies") && j["dependencies"].is_array()) {
            for (const auto& dep : j["dependencies"])
                if (dep.is_string()) entry.dependencies.emplace_back(dep.get<std::string>());
        }
        manifest.upsert(std::move(entry));
    }
    if (ok) *ok = true;
    return manifest;
}

bool AssetManifest::saveToFile(const std::filesystem::path& path) const
{
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << toJson();
    return static_cast<bool>(out);
}

AssetManifest AssetManifest::loadFromFile(const std::filesystem::path& path, bool* ok)
{
    if (ok) *ok = false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return AssetManifest{};
    std::ostringstream buf;
    buf << in.rdbuf();
    return fromJson(buf.str(), ok);
}

} // namespace Phantom::Asset
