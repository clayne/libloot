/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2018    WrinklyNinja

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

#include "plugin_sorting_data.h"

#include <loot/metadata/group.h>

#include <boost/algorithm/string.hpp>

#include "api/helpers/text.h"

namespace loot {
std::vector<const PluginInterface*> GetPluginsSubset(
    const std::vector<const PluginInterface*>& plugins,
    const std::vector<std::string>& pluginNames) {
  std::vector<const PluginInterface*> pluginsSubset;

  for (const auto& pluginName : pluginNames) {
    auto pos = std::find_if(plugins.begin(), plugins.end(), [&](auto plugin) {
      return CompareFilenames(plugin->GetName(), pluginName) == 0;
    });

    if (pos != plugins.end()) {
      pluginsSubset.push_back(*pos);
    }
  }

  return pluginsSubset;
}

PluginSortingData::PluginSortingData(
    const PluginSortingInterface* plugin,
    const PluginMetadata& masterlistMetadata,
    const PluginMetadata& userMetadata,
    const std::vector<std::string>& loadOrder,
    const GameType gameType,
    const std::vector<const PluginInterface*>& loadedPlugins) :
    plugin_(plugin),
    group_(userMetadata.GetGroup().value_or(
        masterlistMetadata.GetGroup().value_or(Group::DEFAULT_NAME))),
    masterlistLoadAfter_(masterlistMetadata.GetLoadAfterFiles()),
    userLoadAfter_(userMetadata.GetLoadAfterFiles()),
    masterlistReq_(masterlistMetadata.GetRequirements()),
    userReq_(userMetadata.GetRequirements()),
    groupIsUserMetadata_(userMetadata.GetGroup().has_value()) {
  if (plugin == nullptr) {
    return;
  }

  for (size_t i = 0; i < loadOrder.size(); i++) {
    if (CompareFilenames(plugin->GetName(), loadOrder.at(i)) == 0) {
      loadOrderIndex_ = i;
      break;
    }
  }

  if (gameType == GameType::tes3) {
    auto masterNames = plugin->GetMasters();
    if (masterNames.empty()) {
      overrideRecordCount_ = 0;
    } else {
      auto masters = GetPluginsSubset(loadedPlugins, masterNames);
      if (masters.size() == masterNames.size()) {
        overrideRecordCount_ = plugin->GetOverlapSize(masters);
      } else {
        // Not all masters are loaded, fall back to using the plugin's
        // total record count (Morrowind doesn't have groups). This is OK
        // because plugins with missing masters can't be loaded by the game,
        // so the correctness of their load order positions is less important
        // (it may not matter at all, depending on the sophistication/usage of
        // merge patches in Morrowind). It's better for LOOT to sort a load
        // order with missing masters with potentially poorer results than
        // for it to error out, as masters may be missing for a variety of
        // development & testing reasons.
        overrideRecordCount_ = plugin->GetRecordAndGroupCount();
      }
    }
  } else {
    overrideRecordCount_ = plugin->GetOverrideRecordCount();
  }
}

std::string PluginSortingData::GetName() const { return plugin_->GetName(); }

bool PluginSortingData::IsMaster() const {
  return plugin_ != nullptr &&
         (plugin_->IsMaster() ||
          (plugin_->IsLightPlugin() &&
           !boost::iends_with(plugin_->GetName(), ".esp")));
}

bool PluginSortingData::LoadsArchive() const {
  return plugin_ != nullptr && plugin_->LoadsArchive();
}

std::vector<std::string> PluginSortingData::GetMasters() const {
  if (plugin_ == nullptr) {
    return {};
  }

  return plugin_->GetMasters();
}

size_t PluginSortingData::GetOverrideRecordCount() const {
  return overrideRecordCount_;
}

bool PluginSortingData::DoRecordsOverlap(
    const PluginSortingData& plugin) const {
  return plugin_ != nullptr && plugin.plugin_ != nullptr &&
         plugin_->DoRecordsOverlap(*plugin.plugin_);
}

size_t PluginSortingData::GetAssetCount() const {
  return plugin_ == nullptr ? 0 : plugin_->GetAssetCount();
}

bool PluginSortingData::DoAssetsOverlap(const PluginSortingData& plugin) const {
  return plugin_ != nullptr && plugin.plugin_ != nullptr &&
         plugin_->DoAssetsOverlap(*plugin.plugin_);
}

std::string PluginSortingData::GetGroup() const { return group_; }

bool PluginSortingData::IsGroupUserMetadata() const {
  return groupIsUserMetadata_;
}

std::vector<PredecessorGroupPlugin>
PluginSortingData::GetPredecessorGroupPlugins() const {
  return predecessorGroupPlugins_;
}

void PluginSortingData::SetPredecessorGroupPlugins(
    std::vector<PredecessorGroupPlugin> plugins) {
  predecessorGroupPlugins_ = plugins;
}

const std::vector<File>& PluginSortingData::GetMasterlistLoadAfterFiles()
    const {
  return masterlistLoadAfter_;
}

const std::vector<File>& PluginSortingData::GetUserLoadAfterFiles() const {
  return userLoadAfter_;
}

const std::vector<File>& PluginSortingData::GetMasterlistRequirements() const {
  return masterlistReq_;
}

const std::vector<File>& PluginSortingData::GetUserRequirements() const {
  return userReq_;
}
const std::optional<size_t>& PluginSortingData::GetLoadOrderIndex() const {
  return loadOrderIndex_;
}
}
