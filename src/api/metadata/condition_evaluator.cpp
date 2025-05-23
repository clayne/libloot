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
    <http://www.gnu.org/licenses/>.
    */

#include "api/metadata/condition_evaluator.h"

#include <sstream>

#include "api/helpers/crc.h"
#include "api/helpers/logging.h"
#include "loot/exception/condition_syntax_error.h"
#include "loot/exception/error_categories.h"

namespace loot {
void HandleError(std::string_view operation, int returnCode) {
  if (returnCode == LCI_OK) {
    return;
  }

  const char* message = nullptr;
  std::string err;
  lci_get_error_message(&message);
  if (message == nullptr) {
    err = fmt::format("Failed to {}. Error code: {}", operation, returnCode);
  } else {
    err = fmt::format("Failed to {}. Details: {}", operation, message);
  }

  auto logger = getLogger();
  if (logger) {
    logger->error(err);
  }

  throw ConditionSyntaxError(
      returnCode, loot_condition_interpreter_category(), err);
}

int mapGameType(GameType gameType) {
  switch (gameType) {
    case GameType::tes3:
      return LCI_GAME_MORROWIND;
    case GameType::tes4:
    case GameType::oblivionRemastered:
      return LCI_GAME_OBLIVION;
    case GameType::tes5:
      return LCI_GAME_SKYRIM;
    case GameType::tes5se:
      return LCI_GAME_SKYRIM_SE;
    case GameType::tes5vr:
      return LCI_GAME_SKYRIM_VR;
    case GameType::fo3:
      return LCI_GAME_FALLOUT_3;
    case GameType::fonv:
      return LCI_GAME_FALLOUT_NV;
    case GameType::fo4:
      return LCI_GAME_FALLOUT_4;
    case GameType::fo4vr:
      return LCI_GAME_FALLOUT_4_VR;
    case GameType::starfield:
      return LCI_GAME_STARFIELD;
    case GameType::openmw:
      return LCI_GAME_OPENMW;
    default:
      throw std::runtime_error(
          "Unrecognised game type encountered while mapping for condition "
          "evaluation.");
  }
}

ConditionEvaluator::ConditionEvaluator(const GameType gameType,
                                       const std::filesystem::path& dataPath) :
    lciState_(std::unique_ptr<lci_state, decltype(&lci_state_destroy)>(
        nullptr,
        lci_state_destroy)) {
  lci_state* state = nullptr;

  int result = lci_state_create(
      &state, mapGameType(gameType), dataPath.u8string().c_str());
  HandleError("create state object for condition evaluation", result);

  lciState_ = std::unique_ptr<lci_state, decltype(&lci_state_destroy)>(
      state, lci_state_destroy);
}

bool ConditionEvaluator::Evaluate(const std::string& condition) {
  if (condition.empty())
    return true;

  auto logger = getLogger();
  if (logger) {
    logger->trace("Evaluating condition: {}", condition);
  }

  const int result = lci_condition_eval(condition.c_str(), lciState_.get());
  if (result != LCI_RESULT_FALSE && result != LCI_RESULT_TRUE) {
    HandleError("evaluate condition \"" + condition + "\"", result);
  }

  return result == LCI_RESULT_TRUE;
}

std::optional<PluginMetadata> ConditionEvaluator::EvaluateAll(
    const PluginMetadata& pluginMetadata) {
  PluginMetadata evaluatedMetadata(pluginMetadata.GetName());
  evaluatedMetadata.SetLocations(pluginMetadata.GetLocations());

  if (pluginMetadata.GetGroup()) {
    evaluatedMetadata.SetGroup(pluginMetadata.GetGroup().value());
  }

  std::vector<File> files;
  for (const auto& file : pluginMetadata.GetLoadAfterFiles()) {
    if (Evaluate(file.GetCondition()))
      files.push_back(file);
  }
  evaluatedMetadata.SetLoadAfterFiles(files);

  files.clear();
  for (const auto& file : pluginMetadata.GetRequirements()) {
    if (Evaluate(file.GetCondition()))
      files.push_back(file);
  }
  evaluatedMetadata.SetRequirements(files);

  files.clear();
  for (const auto& file : pluginMetadata.GetIncompatibilities()) {
    if (Evaluate(file.GetCondition()))
      files.push_back(file);
  }
  evaluatedMetadata.SetIncompatibilities(files);

  std::vector<Message> messages;
  for (const auto& message : pluginMetadata.GetMessages()) {
    if (Evaluate(message.GetCondition()))
      messages.push_back(message);
  }
  evaluatedMetadata.SetMessages(messages);

  std::vector<Tag> tags;
  for (const auto& tag : pluginMetadata.GetTags()) {
    if (Evaluate(tag.GetCondition()))
      tags.push_back(tag);
  }
  evaluatedMetadata.SetTags(tags);

  if (!evaluatedMetadata.IsRegexPlugin()) {
    std::vector<PluginCleaningData> infoVector;
    for (const auto& info : pluginMetadata.GetDirtyInfo()) {
      if (Evaluate(info, pluginMetadata.GetName()))
        infoVector.push_back(info);
    }
    evaluatedMetadata.SetDirtyInfo(infoVector);

    infoVector.clear();
    for (const auto& info : pluginMetadata.GetCleanInfo()) {
      if (Evaluate(info, pluginMetadata.GetName()))
        infoVector.push_back(info);
    }
    evaluatedMetadata.SetCleanInfo(infoVector);
  }

  if (evaluatedMetadata.HasNameOnly()) {
    return std::nullopt;
  }

  return evaluatedMetadata;
}

void ConditionEvaluator::ClearConditionCache() {
  const int result = lci_state_clear_condition_cache(lciState_.get());
  HandleError("clear the condition cache", result);
}

void ConditionEvaluator::RefreshActivePluginsState(
    const std::vector<std::string>& activePluginNames) {
  ClearConditionCache();

  std::vector<const char*> activePluginNameCStrings;
  for (auto& pluginName : activePluginNames) {
    activePluginNameCStrings.push_back(pluginName.c_str());
  }

  const int result =
      lci_state_set_active_plugins(lciState_.get(),
                                   activePluginNameCStrings.data(),
                                   activePluginNameCStrings.size());
  HandleError("cache active plugins for condition evaluation", result);
}

void ConditionEvaluator::RefreshLoadedPluginsState(
    const std::vector<std::shared_ptr<const PluginInterface>>& plugins) {
  ClearConditionCache();

  std::vector<std::string> pluginNames;
  std::vector<std::string> pluginVersionStrings;
  std::vector<uint32_t> crcs;
  for (auto plugin : plugins) {
    pluginNames.push_back(plugin->GetName());
    pluginVersionStrings.push_back(plugin->GetVersion().value_or(""));
    crcs.push_back(plugin->GetCRC().value_or(0));
  }

  std::vector<plugin_version> pluginVersions;
  std::vector<plugin_crc> pluginCrcs;
  for (size_t i = 0; i < pluginNames.size(); ++i) {
    if (!pluginVersionStrings.at(i).empty()) {
      plugin_version pluginVersion;
      pluginVersion.plugin_name = pluginNames.at(i).c_str();
      pluginVersion.version = pluginVersionStrings.at(i).c_str();
      pluginVersions.push_back(pluginVersion);
    }

    if (crcs.at(i) != 0) {
      plugin_crc pluginCrc;
      pluginCrc.plugin_name = pluginNames.at(i).c_str();
      pluginCrc.crc = crcs.at(i);
      pluginCrcs.push_back(pluginCrc);
    }
  }

  int result = lci_state_set_plugin_versions(
      lciState_.get(), pluginVersions.data(), pluginVersions.size());
  HandleError("cache plugin versions for condition evaluation", result);

  result = lci_state_set_crc_cache(
      lciState_.get(), pluginCrcs.data(), pluginCrcs.size());
  HandleError("fill CRC cache for condition evaluation", result);
}

void ConditionEvaluator::SetAdditionalDataPaths(
    const std::vector<std::filesystem::path>& dataPaths) {
  std::vector<std::string> dataPathStrings;
  for (const auto& dataPath : dataPaths) {
    dataPathStrings.push_back(dataPath.u8string());
  }

  std::vector<const char*> dataPathCStrings;
  for (const auto& dataPath : dataPathStrings) {
    dataPathCStrings.push_back(dataPath.c_str());
  }

  int result = lci_state_set_additional_data_paths(
      lciState_.get(), dataPathCStrings.data(), dataPathCStrings.size());
  HandleError("create state object for condition evaluation", result);
}

bool ConditionEvaluator::Evaluate(const PluginCleaningData& cleaningData,
                                  std::string_view pluginName) {
  if (pluginName.empty())
    return false;

  return Evaluate(fmt::format(
      "checksum(\"{}\", {})", pluginName, CrcToString(cleaningData.GetCRC())));
}

void ParseCondition(const std::string& condition) {
  if (condition.empty()) {
    return;
  }

  auto logger = getLogger();
  if (logger) {
    logger->trace("Testing condition syntax: {}", condition);
  }

  const int result = lci_condition_parse(condition.c_str());
  HandleError("parse condition \"" + condition + "\"", result);
}
}
