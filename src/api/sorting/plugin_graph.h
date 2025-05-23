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

#ifndef LOOT_API_SORTING_PLUGIN_SORTER
#define LOOT_API_SORTING_PLUGIN_SORTER

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <map>

#include "api/helpers/text.h"
#include "api/sorting/group_sort.h"
#include "api/sorting/plugin_sorting_data.h"
#include "loot/enum/edge_type.h"
#include "loot/metadata/group.h"

namespace loot {
typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::bidirectionalS,
                              PluginSortingData,
                              EdgeType>
    RawPluginGraph;
typedef boost::graph_traits<RawPluginGraph>::vertex_descriptor vertex_t;
typedef boost::graph_traits<RawPluginGraph>::vertex_iterator vertex_it;

std::string describeEdgeType(EdgeType edgeType);

class PathsCache {
public:
  bool IsPathCached(const vertex_t& fromVertex, const vertex_t& toVertex) const;
  void CachePath(const vertex_t& fromVertex, const vertex_t& toVertex);

private:
  boost::unordered_flat_map<vertex_t, boost::unordered_flat_set<vertex_t>>
      pathsCache_;
};

class ComparableFilenamesCache {
public:
  void Insert(const std::string& narrowString);
  const ComparableFilename& Get(const std::string& narrowString);
  const ComparableFilename& GetOrInsert(const std::string& narrowString);

private:
  boost::unordered_flat_map<std::string, ComparableFilename>
      comparableFilenamesCache_;
};

class PluginGraph {
public:
  size_t CountVertices() const;
  std::pair<vertex_it, vertex_it> GetVertices() const;
  std::optional<vertex_t> GetVertexByName(const std::string& name);

  const PluginSortingData& GetPlugin(const vertex_t& vertex) const;

  void CheckForCycles() const;
  std::vector<vertex_t> TopologicalSort() const;

  // If the path is not Hamiltonian, returns the first pair of vertices
  // in the path that do not have an edge between them.
  std::optional<std::pair<vertex_t, vertex_t>> IsHamiltonianPath(
      const std::vector<vertex_t>& path) const;
  std::vector<std::string> ToPluginNames(
      const std::vector<vertex_t>& path) const;

  bool EdgeExists(const vertex_t& fromVertex, const vertex_t& toVertex);
  bool PathExists(const vertex_t& fromVertex, const vertex_t& toVertex);
  bool IsPathCached(const vertex_t& fromVertex, const vertex_t& toVertex);

  std::optional<std::vector<vertex_t>> FindPath(const vertex_t& fromVertex,
                                                const vertex_t& toVertex);
  std::optional<EdgeType> GetEdgeType(const vertex_t& fromVertex,
                                      const vertex_t& toVertex);

  void AddEdge(const vertex_t& fromVertex,
               const vertex_t& toVertex,
               EdgeType edgeType);
  vertex_t AddVertex(const PluginSortingData& plugin);

  void AddSpecificEdges();
  void AddHardcodedPluginEdges(
      const std::vector<std::string>& hardcodedPlugins);
  void AddGroupEdges(const GroupGraph& groupGraph);
  void AddOverlapEdges();
  void AddTieBreakEdges();

private:
  RawPluginGraph graph_;
  PathsCache pathsCache_;
  ComparableFilenamesCache comparableFilenamesCache_;
};
}

#endif
