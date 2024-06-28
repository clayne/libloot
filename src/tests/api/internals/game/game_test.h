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

#ifndef LOOT_TESTS_API_INTERNALS_GAME_GAME_TEST
#define LOOT_TESTS_API_INTERNALS_GAME_GAME_TEST

#include "api/game/game.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class GameTest : public CommonGameTestFixture {
protected:
  GameTest() : blankArchive("Blank" + GetArchiveFileExtension(GetParam())) {
    touch(dataPath / blankArchive);
  }

  void loadInstalledPlugins(Game& game, bool headersOnly) {
    if (GetParam() == GameType::starfield) {
      const std::vector<std::filesystem::path> plugins({
          // These are all ASCII filenames.
          masterFile,
          blankEsm,
          blankFullEsm,
          blankMasterDependentEsm,
          blankEsp,
          blankMasterDependentEsp,
      });
      game.LoadPlugins(plugins, headersOnly);
    } else {
      const std::vector<std::filesystem::path> plugins({
          // These are all ASCII filenames.
          masterFile,
          blankEsm,
          blankDifferentEsm,
          blankMasterDependentEsm,
          blankDifferentMasterDependentEsm,
          blankEsp,
          blankDifferentEsp,
          blankMasterDependentEsp,
          blankDifferentMasterDependentEsp,
          blankPluginDependentEsp,
          blankDifferentPluginDependentEsp,
      });
      game.LoadPlugins(plugins, headersOnly);
    }
  }

  const std::string blankArchive;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         GameTest,
                         ::testing::Values(GameType::tes4,
                                           GameType::tes5,
                                           GameType::fo3,
                                           GameType::fonv,
                                           GameType::fo4,
                                           GameType::tes5se,
                                           GameType::fo4vr,
                                           GameType::tes5vr,
                                           GameType::tes3,
                                           GameType::starfield));

TEST_P(GameTest, constructingShouldStoreTheGivenValues) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_EQ(GetParam(), game.GetType());
  EXPECT_EQ(dataPath, game.DataPath());
}

#ifndef _WIN32
TEST_P(GameTest,
       constructingShouldThrowOnLinuxIfLocalPathIsNotGivenExceptForMorrowind) {
  if (GetParam() == GameType::tes3) {
    EXPECT_NO_THROW(Game(GetParam(), dataPath.parent_path()));
  } else {
    EXPECT_THROW(Game(GetParam(), dataPath.parent_path()), std::system_error);
  }
}
#else
TEST_P(GameTest, constructingShouldNotThrowOnWindowsIfLocalPathIsNotGiven) {
  EXPECT_NO_THROW(Game(GetParam(), dataPath.parent_path()));
}
#endif

TEST_P(GameTest, constructingShouldNotThrowIfGameAndLocalPathsAreNotEmpty) {
  EXPECT_NO_THROW(Game(GetParam(), dataPath.parent_path(), localPath));
}

TEST_P(
    GameTest,
    constructingForFallout4FromMicrosoftStoreOrStarfieldShouldSetAdditionalDataPaths) {
  if (GetParam() == GameType::fo4) {
    // Create the file that indicates it's a Microsoft Store install.
    touch(dataPath.parent_path() / "appxmanifest.xml");
  }

  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  if (GetParam() == GameType::fo4) {
    const auto basePath = dataPath.parent_path() / ".." / "..";
    EXPECT_EQ(std::vector<std::filesystem::path>(
                  {basePath / "Fallout 4- Automatron (PC)" / "Content" / "Data",
                   basePath / "Fallout 4- Nuka-World (PC)" / "Content" / "Data",
                   basePath / "Fallout 4- Wasteland Workshop (PC)" / "Content" /
                       "Data",
                   basePath / "Fallout 4- High Resolution Texture Pack" /
                       "Content" / "Data",
                   basePath / "Fallout 4- Vault-Tec Workshop (PC)" / "Content" /
                       "Data",
                   basePath / "Fallout 4- Far Harbor (PC)" / "Content" / "Data",
                   basePath / "Fallout 4- Contraptions Workshop (PC)" /
                       "Content" / "Data"}),
              game.GetAdditionalDataPaths());
  } else if (GetParam() == GameType::starfield) {
    ASSERT_EQ(1, game.GetAdditionalDataPaths().size());

    const auto expectedSuffix = std::filesystem::u8path("Documents") /
                                "My Games" / "Starfield" / "Data";
    EXPECT_TRUE(boost::ends_with(game.GetAdditionalDataPaths()[0].u8string(),
                                 expectedSuffix.u8string()));
  } else {
    EXPECT_TRUE(game.GetAdditionalDataPaths().empty());
  }
}

TEST_P(GameTest, setAdditionalDataPathsShouldClearTheConditionCache) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  PluginMetadata metadata(blankEsm);
  metadata.SetLoadAfterFiles({File("plugin.esp", "", "file(\"plugin.esp\")")});
  game.GetDatabase().SetPluginUserMetadata(metadata);

  auto evaluatedMetadata =
      game.GetDatabase().GetPluginUserMetadata(blankEsm, true).value();
  EXPECT_TRUE(evaluatedMetadata.GetLoadAfterFiles().empty());

  const auto dataFilePath =
      dataPath.parent_path().parent_path() / "Data" / "plugin.esp";
  touch(dataFilePath);
  game.SetAdditionalDataPaths({dataFilePath.parent_path()});

  evaluatedMetadata =
      game.GetDatabase().GetPluginUserMetadata(blankEsm, true).value();
  EXPECT_FALSE(evaluatedMetadata.GetLoadAfterFiles().empty());
}

TEST_P(GameTest,
       setAdditionalDataPathsShouldUpdateWhereLoadOrderPluginsAreFound) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  // Set no additional data paths to avoid picking up non-test plugins on PCs
  // which have Starfield or Fallout 4 installed.
  game.SetAdditionalDataPaths({});
  game.LoadCurrentLoadOrderState();
  auto loadOrder = game.GetLoadOrder();

  const auto filename = "plugin.esp";
  const auto dataFilePath =
      dataPath.parent_path().parent_path() / "Data" / filename;
  std::filesystem::create_directories(dataFilePath.parent_path());
  std::filesystem::copy_file(getSourcePluginsPath() / blankEsp, dataFilePath);
  ASSERT_TRUE(std::filesystem::exists(dataFilePath));

  if (GetParam() == GameType::starfield) {
    std::filesystem::copy_file(getSourcePluginsPath() / blankEsp,
                               dataPath / filename);
    ASSERT_TRUE(std::filesystem::exists(dataPath / filename));
  }

  std::filesystem::last_write_time(
      dataFilePath,
      std::filesystem::file_time_type::clock().now() + std::chrono::hours(1));

  game.SetAdditionalDataPaths({dataFilePath.parent_path()});
  game.LoadCurrentLoadOrderState();

  loadOrder.push_back(filename);

  EXPECT_EQ(loadOrder, game.GetLoadOrder());
}

TEST_P(GameTest, isValidPluginShouldResolveRelativePathsRelativeToDataPath) {
  const Game game(GetParam(), dataPath.parent_path(), localPath);

  const auto path = ".." / dataPath.filename() / blankEsm;

  EXPECT_TRUE(game.IsValidPlugin(path));
}

TEST_P(GameTest, isValidPluginShouldUseAbsolutePathsAsGiven) {
  const Game game(GetParam(), dataPath.parent_path(), localPath);

  ASSERT_TRUE(dataPath.is_absolute());

  const auto path = dataPath / std::filesystem::u8path(blankEsm);

  EXPECT_TRUE(game.IsValidPlugin(path));
}

TEST_P(
    GameTest,
    loadPluginsWithHeadersOnlyTrueShouldLoadTheHeadersOfAllInstalledPlugins) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_NO_THROW(loadInstalledPlugins(game, true));
  if (GetParam() == GameType::starfield) {
    EXPECT_EQ(6, game.GetCache().GetPlugins().size());
  } else {
    EXPECT_EQ(11, game.GetCache().GetPlugins().size());
  }

  // Check that one plugin's header has been read.
  ASSERT_NO_THROW(game.GetPlugin(masterFile));
  auto plugin = game.GetPlugin(masterFile);
  EXPECT_EQ("5.0", plugin->GetVersion().value());

  // Check that only the header has been read.
  EXPECT_FALSE(plugin->GetCRC());
}

TEST_P(GameTest, loadPluginsWithANonPluginShouldNotAddItToTheLoadedPlugins) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  ASSERT_THROW(game.LoadPlugins(
                   std::vector<std::filesystem::path>({nonPluginFile}), false),
               std::invalid_argument);

  ASSERT_TRUE(game.GetLoadedPlugins().empty());
}

TEST_P(GameTest,
       loadPluginsWithAnInvalidPluginShouldNotAddItToTheLoadedPlugins) {
  ASSERT_FALSE(std::filesystem::exists(dataPath / invalidPlugin));
  ASSERT_NO_THROW(std::filesystem::copy_file(dataPath / blankEsm,
                                             dataPath / invalidPlugin));
  ASSERT_TRUE(std::filesystem::exists(dataPath / invalidPlugin));
  std::ofstream out(dataPath / invalidPlugin, std::fstream::app);
  out << "GRUP0";
  out.close();

  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  ASSERT_NO_THROW(game.LoadPlugins(
      std::vector<std::filesystem::path>({invalidPlugin}), false));

  ASSERT_TRUE(game.GetLoadedPlugins().empty());
}

TEST_P(GameTest,
       loadPluginsWithHeadersOnlyFalseShouldFullyLoadAllInstalledPlugins) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_NO_THROW(loadInstalledPlugins(game, false));
  if (GetParam() == GameType::starfield) {
    EXPECT_EQ(6, game.GetCache().GetPlugins().size());
  } else {
    EXPECT_EQ(11, game.GetCache().GetPlugins().size());
  }

  // Check that one plugin's header has been read.
  ASSERT_NO_THROW(game.GetPlugin(blankEsm));
  auto plugin = game.GetPlugin(blankEsm);
  EXPECT_EQ("5.0", plugin->GetVersion().value());

  // Check that not only the header has been read.
  EXPECT_EQ(blankEsmCrc, plugin->GetCRC().value());
}

TEST_P(
    GameTest,
    loadPluginsShouldFindAndCacheArchivesForLoadDetectionWhenLoadingPlugins) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_NO_THROW(loadInstalledPlugins(game, false));

  auto expected = std::set<std::filesystem::path>({dataPath / blankArchive});
  EXPECT_EQ(expected, game.GetCache().GetArchivePaths());
}

TEST_P(GameTest, loadPluginsShouldFindArchivesInAdditionalDataPaths) {
  // Create a couple of external archive files.
  const std::string archiveFileExtension =
      GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
              GetParam() == GameType::starfield
          ? ".ba2"
          : ".bsa";

  const auto ba2Path1 =
      dataPath.parent_path() /
      ("../../Fallout 4- Far Harbor (PC)/Content/Data/DLCCoast - Main" +
       archiveFileExtension);
  const auto ba2Path2 =
      dataPath.parent_path() /
      ("../../Fallout 4- Nuka-World (PC)/Content/Data/DLCNukaWorld "
       "- Voices_it" +
       archiveFileExtension);
  touch(ba2Path1);
  touch(ba2Path2);

  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  game.SetAdditionalDataPaths({ba2Path1.parent_path(), ba2Path2.parent_path()});

  EXPECT_NO_THROW(loadInstalledPlugins(game, true));

  const auto archivePaths = game.GetCache().GetArchivePaths();

  EXPECT_EQ(std::set<std::filesystem::path>(
                {ba2Path1, ba2Path2, dataPath / blankArchive}),
            archivePaths);
}

TEST_P(GameTest, loadPluginsShouldClearTheArchivesCacheBeforeFindingArchives) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_NO_THROW(loadInstalledPlugins(game, false));
  EXPECT_NO_THROW(loadInstalledPlugins(game, false));
  EXPECT_EQ(1, game.GetCache().GetArchivePaths().size());
}

TEST_P(
    GameTest,
    loadPluginsShouldNotThrowIfAFilenameHasNonWindows1252EncodableCharacters) {
  touch(dataPath /
        std::filesystem::u8path(
            u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00C3\u00CE.txt"));

  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_NO_THROW(loadInstalledPlugins(game, false));
}

TEST_P(GameTest,
       loadPluginsShouldThrowIfGivenVectorElementsWithTheSameFilename) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  const auto dataPluginPath = dataPath / std::filesystem::u8path(blankEsm);
  const auto sourcePluginPath =
      getSourcePluginsPath() / std::filesystem::u8path(blankEsm);

  EXPECT_THROW(game.LoadPlugins(std::vector<std::filesystem::path>(
                                    {dataPluginPath, sourcePluginPath}),
                                true),
               std::invalid_argument);
}

TEST_P(GameTest, loadPluginsShouldResolveRelativePathsRelativeToDataPath) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  const auto relativePath = ".." / dataPath.filename() / blankEsm;

  game.LoadPlugins(std::vector<std::filesystem::path>({relativePath}), true);

  EXPECT_NE(nullptr, game.GetPlugin(blankEsm));
}

TEST_P(GameTest, loadPluginsShouldUseAbsolutePathsAsGiven) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  const auto absolutePath = dataPath / std::filesystem::u8path(blankEsm);

  game.LoadPlugins(std::vector<std::filesystem::path>({absolutePath}), true);

  EXPECT_NE(nullptr, game.GetPlugin(blankEsm));
}

TEST_P(GameTest, sortPluginsShouldHandlePluginPathsThatAreNotJustFilenames) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  const auto absolutePath = dataPath / std::filesystem::u8path(blankEsm);

  const auto newLoadOrder =
      game.SortPlugins(std::vector<std::filesystem::path>({absolutePath}));

  EXPECT_EQ(std::vector<std::string>{blankEsm}, newLoadOrder);
}

TEST_P(GameTest, shouldShowBlankEsmAsActiveIfItHasNotBeenLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);
  game.LoadCurrentLoadOrderState();

  EXPECT_TRUE(game.IsPluginActive(blankEsm));
}

TEST_P(GameTest, shouldShowBlankEspAsInactiveIfItHasNotBeenLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);
  game.LoadCurrentLoadOrderState();

  EXPECT_FALSE(game.IsPluginActive(blankEsp));
}

TEST_P(GameTest, shouldShowBlankEsmAsActiveIfItsHeaderHasBeenLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);
  game.LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(loadInstalledPlugins(game, true));

  EXPECT_TRUE(game.IsPluginActive(blankEsm));
}

TEST_P(GameTest, shouldShowBlankEspAsInactiveIfItsHeaderHasBeenLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);
  game.LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(loadInstalledPlugins(game, true));

  EXPECT_FALSE(game.IsPluginActive(blankEsp));
}

TEST_P(GameTest, shouldShowBlankEsmAsActiveIfItHasBeenFullyLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);
  game.LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(loadInstalledPlugins(game, false));

  EXPECT_TRUE(game.IsPluginActive(blankEsm));
}

TEST_P(GameTest, shouldShowBlankEspAsInactiveIfItHasBeenFullyLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);
  game.LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(loadInstalledPlugins(game, false));

  EXPECT_FALSE(game.IsPluginActive(blankEsp));
}
}
}

#endif
