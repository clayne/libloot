/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2016    WrinklyNinja

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

#ifndef LOOT_TESTS_API_INTERNALS_BSA_TEST
#define LOOT_TESTS_API_INTERNALS_BSA_TEST

#include <gtest/gtest.h>

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <fstream>

#include "api/bsa.h"

namespace loot::test {
TEST(GetAssetsInBethesdaArchive, shouldSupportV103BSAs) {
  const auto path = std::filesystem::u8path("./Oblivion/Data/Blank.bsa");

  const auto assets = GetAssetsInBethesdaArchive(path);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);
  EXPECT_EQ(0, assets.begin()->first);
  EXPECT_EQ(1, assets.at(0).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0).begin());
}

TEST(GetAssetsInBethesdaArchive, shouldSupportV104BSAs) {
  const auto path = std::filesystem::u8path("./Skyrim/Data/Blank.bsa");

  const auto assets = GetAssetsInBethesdaArchive(path);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);
  EXPECT_EQ(0x2E01002E, assets.begin()->first);
  EXPECT_EQ(1, assets.at(0x2E01002E).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0x2E01002E).begin());
}

TEST(GetAssetsInBethesdaArchive, shouldSupportV105BSAs) {
  const auto path = std::filesystem::u8path("./SkyrimSE/Data/Blank.bsa");

  const auto assets = GetAssetsInBethesdaArchive(path);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);
  EXPECT_EQ(0xB68102C964176E73, assets.begin()->first);
  EXPECT_EQ(1, assets.at(0xB68102C964176E73).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0xB68102C964176E73).begin());
}

TEST(GetAssetsInBethesdaArchive, shouldThrowIfFileCannotBeOpened) {
  const auto path = std::filesystem::u8path("invalid.bsa");

  EXPECT_THROW(GetAssetsInBethesdaArchive(path), std::runtime_error);
}

TEST(GetAssetsInBethesdaArchive, shouldSupportGeneralBA2s) {
  const auto path =
      std::filesystem::u8path("./Fallout 4/Data/Blank - Main.ba2");
  const uint64_t folderHash =
      std::hash<std::string>{}("dev\\git\\testing-plugins");
  const uint64_t fileHash = std::hash<std::string>{}("license.txt");

  const auto assets = GetAssetsInBethesdaArchive(path);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);

  ASSERT_EQ(1, assets.count(folderHash));

  EXPECT_EQ(1, assets.find(folderHash)->second.size());
  EXPECT_EQ(1, assets.find(folderHash)->second.count(fileHash));
}

TEST(GetAssetsInBethesdaArchive, shouldSupportTextureBA2s) {
  const auto path =
      std::filesystem::u8path("./Fallout 4/Data/Blank - Textures.ba2");
  const uint64_t folderHash =
      std::hash<std::string>{}("dev\\git\\testing-plugins");
  const uint64_t fileHash = std::hash<std::string>{}("blank.dds");

  const auto assets = GetAssetsInBethesdaArchive(path);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);

  ASSERT_EQ(1, assets.count(folderHash));

  EXPECT_EQ(1, assets.find(folderHash)->second.size());
  EXPECT_EQ(1, assets.find(folderHash)->second.count(fileHash));
}

class GetAssetsInBethesdaArchive_BA2Version
    : public ::testing::TestWithParam<char> {
protected:
  GetAssetsInBethesdaArchive_BA2Version() : path(GetArchivePath()) {
    const auto sourcePath =
        std::filesystem::u8path("./Fallout 4/Data/Blank - Main.ba2");
    std::filesystem::copy(sourcePath, path);

    std::fstream stream(
        path, std::ios_base::binary | std::ios_base::in | std::ios_base::out);
    stream.seekp(4);
    stream.put(GetParam());
    stream.close();
  }

  void TearDown() override { std::filesystem::remove(path); }

  const std::filesystem::path path;

private:
  std::filesystem::path GetArchivePath() {
    const auto tempFilename =
        "LOOT-test-" +
        boost::lexical_cast<std::string>((boost::uuids::random_generator())()) +
        ".ba2";

    return std::filesystem::temp_directory_path() / tempFilename;
  }
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         GetAssetsInBethesdaArchive_BA2Version,
                         ::testing::Values(1, 2, 3, 7, 8));

TEST_P(GetAssetsInBethesdaArchive_BA2Version, shouldSupportBA2Version) {
  const auto assets = GetAssetsInBethesdaArchive(path);

  EXPECT_FALSE(assets.empty());
}

TEST(GetAssetsInBethesdaArchives, shouldSkipFilesThatCannotBeRead) {
  std::vector<std::filesystem::path> paths(
      {std::filesystem::u8path("invalid.bsa"),
       std::filesystem::u8path("./Skyrim/Data/Blank.bsa")});

  const auto assets = GetAssetsInBethesdaArchives(paths);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);
  EXPECT_EQ(0x2E01002E, assets.begin()->first);
  EXPECT_EQ(1, assets.begin()->second.size());
  EXPECT_EQ(0x4670B6836C077365, *assets.begin()->second.begin());
}

TEST(GetAssetsInBethesdaArchives, shouldCombineAssetsFromEachLoadedArchive) {
  std::vector<std::filesystem::path> paths(
      {std::filesystem::u8path("./Oblivion/Data/Blank.bsa"),
       std::filesystem::u8path("./Skyrim/Data/Blank.bsa"),
       std::filesystem::u8path("./SkyrimSE/Data/Blank.bsa")});

  const auto assets = GetAssetsInBethesdaArchives(paths);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(3, assets.size());
  EXPECT_EQ(3, filesCount);

  EXPECT_EQ(1, assets.at(0).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0).begin());

  EXPECT_EQ(1, assets.at(0x2E01002E).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0x2E01002E).begin());

  EXPECT_EQ(1, assets.at(0xB68102C964176E73).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0xB68102C964176E73).begin());
}

TEST(DoAssetsIntersect, shouldReturnTrueIfTheSameFileExistsInTheSameFolder) {
  const auto path = std::filesystem::u8path("./Oblivion/Data/Blank.bsa");

  const auto assets = GetAssetsInBethesdaArchive(path);

  EXPECT_TRUE(DoAssetsIntersect(assets, assets));
}

TEST(DoAssetsIntersect,
     shouldReturnFalseIfTheSameFileExistsInDifferentFolders) {
  const auto path1 = std::filesystem::u8path("./Oblivion/Data/Blank.bsa");
  const auto assets1 = GetAssetsInBethesdaArchive(path1);

  const auto path2 = std::filesystem::u8path("./Skyrim/Data/Blank.bsa");
  const auto assets2 = GetAssetsInBethesdaArchive(path2);

  EXPECT_EQ(*assets2.at(0x2E01002E).begin(), *assets1.at(0).begin());

  EXPECT_FALSE(DoAssetsIntersect(assets1, assets2));
}
}

#endif
