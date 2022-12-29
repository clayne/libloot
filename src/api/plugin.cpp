/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

    This file is part of LOOT.

    LOOT is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LOOT is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LOOT.  If not, see
    <https://www.gnu.org/licenses/>.
    */

#include "api/plugin.h"

#include <boost/algorithm/string.hpp>
#include <filesystem>

#include "api/game/game.h"
#include "api/helpers/crc.h"
#include "api/helpers/logging.h"
#include "api/helpers/text.h"
#include "loot/exception/file_access_error.h"

namespace loot {
Plugin::Plugin(const GameType gameType,
               const GameCache& gameCache,
               std::filesystem::path pluginPath,
               const bool headerOnly) :
    name_(pluginPath.filename().u8string()),
    esPlugin(
        std::unique_ptr<::Plugin, decltype(&esp_plugin_free)>(nullptr,
                                                              esp_plugin_free)),
    isEmpty_(true),
    loadsArchive_(false),
    numOverrideRecords_(0) {
  auto logger = getLogger();

  try {
    // In case the plugin is ghosted.
    if (!std::filesystem::exists(pluginPath)) {
      pluginPath += GHOST_FILE_EXTENSION;
    }

    Load(pluginPath, gameType, headerOnly);

    auto ret = esp_plugin_is_empty(esPlugin.get(), &isEmpty_);
    if (ret != ESP_OK) {
      throw FileAccessError(
          "Error checking if \"" + name_ +
          "\" is empty. esplugin error code: " + std::to_string(ret));
    }

    if (!headerOnly) {
      crc_ = GetCrc32(pluginPath);

      ret = esp_plugin_count_override_records(esPlugin.get(),
                                              &numOverrideRecords_);
      if (ret != ESP_OK) {
        throw FileAccessError(
            "Error counting override records in \"" + name_ +
            "\". esplugin error code: " + std::to_string(ret));
      }
    }

    tags_ = ExtractBashTags(GetDescription());
    loadsArchive_ = LoadsArchive(gameType, gameCache, pluginPath);
  } catch (const std::exception& e) {
    if (logger) {
      logger->error(
          "Cannot read plugin file \"{}\". Details: {}", name_, e.what());
    }
    throw FileAccessError("Cannot read \"" + name_ +
                          "\". Details: " + e.what());
  }
}

std::string Plugin::GetName() const { return name_; }

std::optional<float> Plugin::GetHeaderVersion() const {
  float version = 0.0f;

  const auto ret = esp_plugin_header_version(esPlugin.get(), &version);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }

  if (std::isnan(version)) {
    return std::nullopt;
  }

  return version;
}

std::optional<std::string> Plugin::GetVersion() const {
  return ExtractVersion(GetDescription());
}

std::vector<std::string> Plugin::GetMasters() const {
  char** masters = nullptr;
  size_t numMasters = 0;
  const auto ret = esp_plugin_masters(esPlugin.get(), &masters, &numMasters);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::vector<std::string> mastersVec(masters, masters + numMasters);
  esp_string_array_free(masters, numMasters);

  return mastersVec;
}

std::vector<Tag> Plugin::GetBashTags() const { return tags_; }

std::optional<uint32_t> Plugin::GetCRC() const { return crc_; }

bool Plugin::IsMaster() const {
  bool isMaster = false;
  const auto ret = esp_plugin_is_master(esPlugin.get(), &isMaster);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }

  return isMaster;
}

bool Plugin::IsLightPlugin() const {
  bool isLightPlugin = false;
  const auto ret = esp_plugin_is_light_plugin(esPlugin.get(), &isLightPlugin);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }

  return isLightPlugin;
}

bool Plugin::IsValidAsLightPlugin() const {
  bool isValid = false;
  const auto ret =
      esp_plugin_is_valid_as_light_plugin(esPlugin.get(), &isValid);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }

  return isValid;
}

bool Plugin::IsEmpty() const { return isEmpty_; }

bool Plugin::LoadsArchive() const { return loadsArchive_; }

bool Plugin::DoFormIDsOverlap(const PluginInterface& plugin) const {
  try {
    auto& otherPlugin = dynamic_cast<const Plugin&>(plugin);

    bool doPluginsOverlap = false;
    const auto ret = esp_plugin_do_records_overlap(
        esPlugin.get(), otherPlugin.esPlugin.get(), &doPluginsOverlap);
    if (ret != ESP_OK) {
      throw FileAccessError(name_ +
                            " : esplugin error code: " + std::to_string(ret));
    }

    return doPluginsOverlap;
  } catch (std::bad_cast&) {
    auto logger = getLogger();
    if (logger) {
      logger->error(
          "Tried to check if FormIDs overlapped with a non-Plugin "
          "implementation of PluginInterface.");
    }
  }

  return false;
}

size_t Plugin::GetOverlapSize(
    const std::vector<const PluginInterface*>& plugins) const {
  if (plugins.empty()) {
    return 0;
  }

  std::vector<::Plugin*> esPlugins;
  for (const auto& plugin : plugins) {
    const auto otherPlugin = dynamic_cast<const Plugin* const>(plugin);

    if (otherPlugin == nullptr) {
      const auto logger = getLogger();
      if (logger) {
        logger->error(
            "Tried to check how many FormIDs overlapped with a non-Plugin "
            "implementation of PluginSortingInterface.");
      }
      throw std::invalid_argument(
          "Tried to check how many FormIDs overlapped with a non-Plugin "
          "implementation of PluginSortingInterface.");
    }

    esPlugins.push_back(otherPlugin->esPlugin.get());
  }

  size_t overlapSize = 0;
  const auto ret = esp_plugin_records_overlap_size(
      esPlugin.get(), esPlugins.data(), esPlugins.size(), &overlapSize);
  if (ret != ESP_OK) {
    throw FileAccessError("Error getting overlap size for \"" + name_ +
                          "\". esplugin error code: " + std::to_string(ret));
  }

  return overlapSize;
}

size_t Plugin::NumOverrideFormIDs() const { return numOverrideRecords_; }

uint32_t Plugin::GetRecordAndGroupCount() const {
  uint32_t recordAndGroupCount = 0;
  const auto ret =
      esp_plugin_record_and_group_count(esPlugin.get(), &recordAndGroupCount);
  if (ret != ESP_OK) {
    throw FileAccessError("Error getting record and group count for \"" +
                          name_ +
                          "\". esplugin error code: " + std::to_string(ret));
  }

  return recordAndGroupCount;
}

bool Plugin::IsValid(const GameType gameType,
                     const std::filesystem::path& pluginPath) {
  // Check that the file has a valid extension.
  if (hasPluginFileExtension(pluginPath.filename().u8string(), gameType)) {
    bool isValid = false;
    auto returnCode = esp_plugin_is_valid(GetEspluginGameId(gameType),
                                          pluginPath.u8string().c_str(),
                                          true,
                                          &isValid);

    if (returnCode != ESP_OK || !isValid) {
      // Try adding .ghost extension.
      auto ghostedFilename = pluginPath.u8string() + GHOST_FILE_EXTENSION;
      returnCode = esp_plugin_is_valid(
          GetEspluginGameId(gameType), ghostedFilename.c_str(), true, &isValid);
    }

    if (returnCode == ESP_OK && isValid) {
      return true;
    }
  }

  auto logger = getLogger();
  if (logger) {
    logger->debug("The file \"{}\" is not a valid plugin.",
                  pluginPath.filename().u8string());
  }

  return false;
}

uintmax_t Plugin::GetFileSize(std::filesystem::path pluginPath) {
  if (!std::filesystem::exists(pluginPath))
    pluginPath += GHOST_FILE_EXTENSION;

  return std::filesystem::file_size(pluginPath);
}

void Plugin::Load(const std::filesystem::path& path,
                  GameType gameType,
                  bool headerOnly) {
  ::Plugin* plugin = nullptr;
  auto ret = esp_plugin_new(
      &plugin, GetEspluginGameId(gameType), path.u8string().c_str());
  if (ret != ESP_OK) {
    throw FileAccessError(path.u8string() +
                          " : esplugin error code: " + std::to_string(ret));
  }

  esPlugin = std::unique_ptr<::Plugin, decltype(&esp_plugin_free)>(
      plugin, esp_plugin_free);

  ret = esp_plugin_parse(esPlugin.get(), headerOnly);
  if (ret != ESP_OK) {
    throw FileAccessError(path.u8string() +
                          " : esplugin error code: " + std::to_string(ret));
  }
}

std::string Plugin::GetDescription() const {
  char* description = nullptr;
  const auto ret = esp_plugin_description(esPlugin.get(), &description);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }
  if (description == nullptr) {
    return "";
  }

  std::string descriptionStr = description;
  esp_string_free(description);

  return descriptionStr;
}

std::string GetArchiveFileExtension(const GameType gameType) {
  if (gameType == GameType::fo4 || gameType == GameType::fo4vr)
    return ".ba2";
  else
    return ".bsa";
}

std::filesystem::path replaceExtension(std::filesystem::path path,
                                       const std::string& newExtension) {
  return path.replace_extension(std::filesystem::u8path(newExtension));
}

std::filesystem::path getTexturesArchivePath(std::filesystem::path pluginPath,
                                             const std::string& newExtension) {
  // replace_extension() with no argument just removes the existing extension.
  pluginPath.replace_extension();
  pluginPath += " - Textures" + newExtension;
  return pluginPath;
}

bool equivalent(const std::filesystem::path& path1,
                const std::filesystem::path& path2) {
  // If the paths are identical, they've got to be equivalent,
  // it doesn't matter if the paths exist or not.
  if (path1 == path2) {
    return true;
  }
  // If the paths are not identical, the filesystem might be case-insensitive
  // so check with the filesystem.
  try {
    return std::filesystem::equivalent(path1, path2);
  } catch (const std::filesystem::filesystem_error&) {
    // One of the paths checked for equivalence doesn't exist,
    // so they can't be equivalent.
    return false;
  } catch (const std::system_error&) {
    // This can be thrown if one or both of the paths contains a character
    // that can't be represented in Windows' multi-byte code page (e.g.
    // Windows-1252), even though Unicode paths shouldn't be a problem,
    // and throwing system_error is undocumented. Seems like a bug in MSVC's
    // implementation.
    return false;
  }
}

// Get whether the plugin loads an archive (BSA/BA2) or not.
bool Plugin::LoadsArchive(const GameType gameType,
                          const GameCache& gameCache,
                          const std::filesystem::path& pluginPath) {
  if (gameType == GameType::tes3) {
    return false;
  }

  const auto archiveExtension = GetArchiveFileExtension(gameType);

  if (gameType == GameType::tes5) {
    // Skyrim (non-SE) plugins can only load BSAs that have exactly the same
    // basename, ignoring file extensions.
    auto archiveFilename = replaceExtension(pluginPath, archiveExtension);

    return std::filesystem::exists(archiveFilename);
  } else if (gameType == GameType::tes5se || gameType == GameType::tes5vr) {
    // Skyrim SE can load BSAs that have exactly the same
    // basename, ignoring file extensions, and also BSAs with filenames of
    // the form "<basename> - Textures.bsa" (case-insensitively).
    // I'm assuming Skyrim VR works the same way as Skyrim SE.
    auto archiveFilename = replaceExtension(pluginPath, archiveExtension);
    auto texturesArchiveFilename =
        getTexturesArchivePath(pluginPath, archiveExtension);

    return std::filesystem::exists(archiveFilename) ||
           std::filesystem::exists(texturesArchiveFilename);
  } else if (gameType != GameType::tes4 ||
             boost::iends_with(pluginPath.filename().u8string(), ".esp")) {
    // Oblivion .esp files and FO3, FNV, FO4 plugins can load archives which
    // begin with the plugin basename.
    // I'm assuming that FO4 VR works the same way as FO4.

    auto basenameLength = pluginPath.stem().native().length();
    auto pluginExtension = pluginPath.extension().native();

    for (const auto& archivePath : gameCache.GetArchivePaths()) {
      // Need to check if it starts with the given plugin's basename,
      // but case insensitively. This is hard to do accurately, so
      // instead check if the plugin with the same length basename and
      // and the given plugin's file extension is equivalent.
      auto bsaPluginFilename =
          archivePath.filename().native().substr(0, basenameLength) +
          pluginExtension;
      auto bsaPluginPath = pluginPath.parent_path() / bsaPluginFilename;
      if (loot::equivalent(pluginPath, bsaPluginPath)) {
        return true;
      }
    }
  }

  return false;
}

unsigned int Plugin::GetEspluginGameId(GameType gameType) {
  switch (gameType) {
    case GameType::tes3:
      return ESP_GAME_MORROWIND;
    case GameType::tes4:
      return ESP_GAME_OBLIVION;
    case GameType::tes5:
      return ESP_GAME_SKYRIM;
    case GameType::tes5se:
      return ESP_GAME_SKYRIMSE;
    case GameType::fo3:
      return ESP_GAME_FALLOUT3;
    case GameType::fonv:
      return ESP_GAME_FALLOUTNV;
    default:
      return ESP_GAME_FALLOUT4;
  }
}

bool hasPluginFileExtension(std::string filename, GameType gameType) {
  if (boost::iends_with(filename, GHOST_FILE_EXTENSION)) {
    filename =
        filename.substr(0, filename.length() - GHOST_FILE_EXTENSION_LENGTH);
  }

  bool isEspOrEsm = boost::iends_with(filename, ".esp") ||
                    boost::iends_with(filename, ".esm");
  bool isEsl = (gameType == GameType::fo4 || gameType == GameType::fo4vr ||
                gameType == GameType::tes5se || gameType == GameType::tes5vr) &&
               boost::iends_with(filename, ".esl");

  return isEspOrEsm || isEsl;
}
}
