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
PluginSortingData::PluginSortingData(const PluginSortingInterface* plugin,
                                     const PluginMetadata& masterlistMetadata,
                                     const PluginMetadata& userMetadata,
                                     const size_t loadOrderIndex) :
    plugin_(plugin),
    name_(plugin == nullptr ? std::string() : plugin->GetName()),
    isMaster_(plugin != nullptr && plugin->IsMaster()),
    group_(userMetadata.GetGroup().value_or(
        masterlistMetadata.GetGroup().value_or(std::string(Group::DEFAULT_NAME)))),
    masterlistLoadAfter_(masterlistMetadata.GetLoadAfterFiles()),
    userLoadAfter_(userMetadata.GetLoadAfterFiles()),
    masterlistReq_(masterlistMetadata.GetRequirements()),
    userReq_(userMetadata.GetRequirements()),
    loadOrderIndex_(loadOrderIndex),
    overrideRecordCount_(plugin == nullptr ? 0
                                           : plugin->GetOverrideRecordCount()),
    groupIsUserMetadata_(userMetadata.GetGroup().has_value()) {}

const std::string& PluginSortingData::GetName() const { return name_; }

bool PluginSortingData::IsMaster() const { return isMaster_; }

bool PluginSortingData::IsBlueprintMaster() const {
  return isMaster_ && plugin_->IsBlueprintPlugin();
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

size_t PluginSortingData::GetLoadOrderIndex() const { return loadOrderIndex_; }
}
