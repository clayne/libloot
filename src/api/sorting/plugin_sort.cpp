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

#include "plugin_sort.h"

#include <boost/algorithm/string.hpp>

#include "api/helpers/logging.h"
#include "api/sorting/group_sort.h"
#include "api/sorting/plugin_graph.h"

namespace loot {
std::vector<PluginSortingData> GetPluginsSortingData(
    const GameType gameType,
    const DatabaseInterface& db,
    const std::vector<const PluginInterface*> loadedPluginInterfaces,
    const std::vector<std::string>& loadOrder) {
  std::vector<PluginSortingData> pluginsSortingData;
  pluginsSortingData.reserve(loadedPluginInterfaces.size());

  for (const auto& pluginInterface : loadedPluginInterfaces) {
    if (!pluginInterface) {
      continue;
    }

    const auto plugin = dynamic_cast<const Plugin* const>(pluginInterface);

    if (!plugin) {
      throw std::logic_error(
          "Tried to case a PluginInterface pointer to a Plugin pointer.");
    }

    const auto masterlistMetadata =
        db.GetPluginMetadata(plugin->GetName(), false, true)
            .value_or(PluginMetadata(plugin->GetName()));
    const auto userMetadata = db.GetPluginUserMetadata(plugin->GetName(), true)
                                  .value_or(PluginMetadata(plugin->GetName()));

    const auto pluginSortingData = PluginSortingData(plugin,
                                                     masterlistMetadata,
                                                     userMetadata,
                                                     loadOrder,
                                                     gameType,
                                                     loadedPluginInterfaces);

    pluginsSortingData.push_back(pluginSortingData);
  }

  return pluginsSortingData;
}

std::unordered_map<std::string, Group> GetGroupsMap(
    const std::vector<Group> masterlistGroups,
    const std::vector<Group> userGroups) {
  const auto mergedGroups = MergeGroups(masterlistGroups, userGroups);

  std::unordered_map<std::string, Group> groupsMap;
  for (const auto& group : mergedGroups) {
    groupsMap.emplace(group.GetName(), group);
  }

  return groupsMap;
}

void ValidateSpecificAndHardcodedEdges(
    const std::vector<PluginSortingData>::const_iterator& begin,
    const std::vector<PluginSortingData>::const_iterator& firstNonMaster,
    const std::vector<PluginSortingData>::const_iterator& end,
    const std::vector<std::string>& hardcodedPlugins) {
  const auto isNonMaster = [&](const std::string& name) {
    return std::any_of(
        firstNonMaster, end, [&](const PluginSortingData& plugin) {
          return CompareFilenames(plugin.GetName(), name) == 0;
        });
  };

  for (auto it = begin; it != firstNonMaster; ++it) {
    for (const auto& master : it->GetMasters()) {
      if (isNonMaster(master)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(master, EdgeType::master),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }
    }

    for (const auto& file : it->GetMasterlistRequirements()) {
      const auto name = std::string(file.GetName());
      if (isNonMaster(name)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(name, EdgeType::masterlistRequirement),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }
    }

    for (const auto& file : it->GetUserRequirements()) {
      const auto name = std::string(file.GetName());
      if (isNonMaster(name)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(name, EdgeType::userRequirement),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }
    }

    for (const auto& file : it->GetMasterlistLoadAfterFiles()) {
      const auto name = std::string(file.GetName());
      if (isNonMaster(name)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(name, EdgeType::masterlistLoadAfter),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }
    }

    for (const auto& file : it->GetUserLoadAfterFiles()) {
      const auto name = std::string(file.GetName());
      if (isNonMaster(name)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(name, EdgeType::userLoadAfter),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }
    }
  }

  if (begin != firstNonMaster) {
    // There's at least one master, check that there are no hardcoded
    // non-masters.
    for (const auto& plugin : hardcodedPlugins) {
      if (isNonMaster(plugin)) {
        // Just report the cycle to the first master.
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(plugin, EdgeType::hardcoded),
            Vertex(begin->GetName(), EdgeType::masterFlag)});
      }
    }
  }
}

std::vector<std::string> SortPlugins(
    const std::vector<PluginSortingData>::const_iterator& begin,
    const std::vector<PluginSortingData>::const_iterator& end,
    const std::vector<std::string>& hardcodedPlugins,
    const std::unordered_map<std::string, Group>& groupsMap,
    const std::unordered_map<std::string, std::vector<PredecessorGroup>>&
        predecessorGroupsMap) {
  PluginGraph graph;

  for (auto it = begin; it != end; ++it) {
    graph.AddVertex(*it);
  }

  // Now add the interactions between plugins to the graph as edges.
  graph.AddSpecificEdges();
  graph.AddHardcodedPluginEdges(hardcodedPlugins);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Check for cycles now because from this point on edges are only added if
  // they don't cause cycles, and adding tie-break edges is by far the slowest
  // part of the process, so if there is a cycle checking now will provide
  // quicker feedback than checking later.
  graph.CheckForCycles();

  graph.AddOverlapEdges();
  graph.AddTieBreakEdges();

  // Check for cycles again, just in case there's a bug that lets some occur.
  // The check doesn't take a significant amount of time.
  graph.CheckForCycles();

  const auto path = graph.TopologicalSort();

  const auto result = graph.IsHamiltonianPath(path);
  const auto logger = getLogger();
  if (result.has_value() && logger) {
    logger->error("The path is not unique. No edge exists between {} and {}.",
                  graph.GetPlugin(result.value().first).GetName(),
                  graph.GetPlugin(result.value().second).GetName());
  }

  // Output a plugin list using the sorted vertices.
  return graph.ToPluginNames(path);
}

std::vector<std::string> SortPlugins(
    std::vector<PluginSortingData>&& pluginsSortingData,
    const GameType gameType,
    const std::vector<Group> masterlistGroups,
    const std::vector<Group> userGroups,
    const std::vector<std::string>& earlyLoadingPlugins) {
  // If there aren't any plugins, exit early, because sorting assumes
  // there is at least one plugin.
  if (pluginsSortingData.empty()) {
    return {};
  }

  // Sort the plugins according to into their existing load order, or
  // lexicographical ordering for pairs of plugins without load order positions.
  // This ensures a consistent iteration order for vertices given the same input
  // data. The vertex iteration order can affect what edges get added and so
  // the final sorting result, so consistency is important.
  // This order needs to be independent of any state (e.g. the current load
  // order) so that sorting and applying the result doesn't then produce a
  // different result if you then sort again.
  std::sort(pluginsSortingData.begin(),
            pluginsSortingData.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.GetName() < rhs.GetName();
            });

  // Create some shared data structures.
  const auto groupsMap = GetGroupsMap(masterlistGroups, userGroups);
  const auto predecessorGroupsMap =
      GetPredecessorGroups(masterlistGroups, userGroups);

  // Some parts of sorting are O(N^2) for N plugins, and master flags cause
  // O(M*N) edges to be added for M masters and N non-masters, which can be
  // two thirds of all edges added. The cost of each bidirectional search
  // scales with the number of edges, so reducing edges makes searches
  // faster.
  // As such, sort plugins using two separate graphs for masters and
  // non-masters. This means that any edges that go from a non-master to a
  // master are effectively ignored, so won't cause cyclic interaction errors.
  // Edges going the other way will also effectively be ignored, but that
  // shouldn't have a noticeable impact.
  const auto firstNonMasterIt = std::stable_partition(
      pluginsSortingData.begin(),
      pluginsSortingData.end(),
      [](const PluginSortingData& plugin) { return plugin.IsMaster(); });

  ValidateSpecificAndHardcodedEdges(pluginsSortingData.begin(),
                                    firstNonMasterIt,
                                    pluginsSortingData.end(),
                                    earlyLoadingPlugins);

  auto newLoadOrder = SortPlugins(pluginsSortingData.begin(),
                                  firstNonMasterIt,
                                  earlyLoadingPlugins,
                                  groupsMap,
                                  predecessorGroupsMap);

  const auto newNonMastersLoadOrder = SortPlugins(firstNonMasterIt,
                                                  pluginsSortingData.end(),
                                                  earlyLoadingPlugins,
                                                  groupsMap,
                                                  predecessorGroupsMap);

  newLoadOrder.insert(newLoadOrder.end(),
                      newNonMastersLoadOrder.begin(),
                      newNonMastersLoadOrder.end());

  return newLoadOrder;
}

std::vector<std::string> SortPlugins(
    Game& game,
    const std::vector<std::string>& loadOrder) {
  auto pluginsSortingData = GetPluginsSortingData(
      game.GetType(), game.GetDatabase(), game.GetLoadedPlugins(), loadOrder);

  const auto logger = getLogger();
  if (logger) {
    logger->debug("Current load order:");
    for (const auto& plugin : loadOrder) {
      logger->debug("\t{}", plugin);
    }
  }

  const auto newLoadOrder =
      SortPlugins(std::move(pluginsSortingData),
                  game.GetType(),
                  game.GetDatabase().GetGroups(false),
                  game.GetDatabase().GetUserGroups(),
                  game.GetLoadOrderHandler().GetEarlyLoadingPlugins());

  if (logger) {
    logger->debug("Calculated order:");
    for (const auto& name : newLoadOrder) {
      logger->debug("\t{}", name);
    }
  }

  return newLoadOrder;
}
}
