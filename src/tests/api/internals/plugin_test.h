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

#ifndef LOOT_TESTS_API_INTERNALS_PLUGIN_TEST
#define LOOT_TESTS_API_INTERNALS_PLUGIN_TEST

#include "api/game/game.h"
#include "api/plugin.h"
#include "loot/exception/error_categories.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginTest : public CommonGameTestFixture,
                   public testing::WithParamInterface<GameType> {
protected:
  PluginTest() :
      CommonGameTestFixture(GetParam()),
      emptyFile("EmptyFile.esm"),
      lowercaseBlankEsp("blank.esp"),
      nonAsciiEsp(u8"non\u00C1scii.esp"),
      otherNonAsciiEsp(u8"other non\u00C1scii.esp"),
      blankArchive("Blank" + GetArchiveFileExtension(GetParam())),
      blankSuffixArchive("Blank - Different - suffix" +
                         GetArchiveFileExtension(GetParam())),
      game_(GetParam(), gamePath, localPath) {}

  void SetUp() override {
    CommonGameTestFixture::SetUp();

    game_.LoadCurrentLoadOrderState();

    // Write out an empty file.
    touch(dataPath / emptyFile);
    ASSERT_TRUE(std::filesystem::exists(dataPath / emptyFile));

#ifndef _WIN32
    ASSERT_NO_THROW(std::filesystem::copy(dataPath / blankEsp,
                                          dataPath / lowercaseBlankEsp));
#endif

    // Make sure the plugins with non-ASCII filenames exists.
    ASSERT_NO_THROW(std::filesystem::copy_file(
        dataPath / blankEsp, dataPath / std::filesystem::u8path(nonAsciiEsp)));
    ASSERT_NO_THROW(std::filesystem::copy_file(
        dataPath / blankEsp,
        dataPath / std::filesystem::u8path(otherNonAsciiEsp)));

    if (GetParam() != GameType::fo4 && GetParam() != GameType::fo4vr &&
        GetParam() != GameType::tes5se && GetParam() != GameType::tes5vr &&
        GetParam() != GameType::starfield) {
      ASSERT_NO_THROW(
          std::filesystem::copy(dataPath / blankEsp, dataPath / blankEsl));
    }

    // Copy across archive files.
    std::filesystem::path blankMasterDependentArchive;
    if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
        GetParam() == GameType::starfield) {
      copyPlugin(getSourceArchivesPath(GetParam()), "Blank - Main.ba2");
      copyPlugin(getSourceArchivesPath(GetParam()), "Blank - Textures.ba2");

      blankMasterDependentArchive = "Blank - Master Dependent - Main.ba2";
      std::filesystem::copy_file(
          getSourceArchivesPath(GetParam()) / "Blank - Main.ba2",
          dataPath / blankMasterDependentArchive);
      ASSERT_TRUE(
          std::filesystem::exists(dataPath / blankMasterDependentArchive));
    } else if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
      touch(dataPath / blankArchive);

      blankMasterDependentArchive = "Blank - Master Dependent.bsa";
      touch(dataPath / blankMasterDependentArchive);
    } else {
      copyPlugin(getSourcePluginsPath(), blankArchive);

      // Also create a copy for Blank - Master Dependent.esp to test overlap.
      blankMasterDependentArchive = "Blank - Master Dependent.bsa";
      std::filesystem::copy_file(getSourcePluginsPath() / blankArchive,
                                 dataPath / blankMasterDependentArchive);
      ASSERT_TRUE(
          std::filesystem::exists(dataPath / blankMasterDependentArchive));
    }

    // Create dummy archive files.
    touch(dataPath / blankSuffixArchive);

    auto nonAsciiArchivePath =
        dataPath /
        std::filesystem::u8path(u8"non\u00E1scii" +
                                GetArchiveFileExtension(game_.GetType()));
    touch(dataPath / nonAsciiArchivePath);

    auto nonAsciiPrefixArchivePath =
        dataPath /
        std::filesystem::u8path(u8"other non\u00E1scii2 - suffix" +
                                GetArchiveFileExtension(game_.GetType()));
    touch(dataPath / nonAsciiPrefixArchivePath);

    game_.GetCache().CacheArchivePaths({dataPath / "Blank - Main.ba2",
                                        dataPath / "Blank - Textures.ba2",
                                        dataPath / blankArchive,
                                        dataPath / blankMasterDependentArchive,
                                        dataPath / blankSuffixArchive,
                                        dataPath / nonAsciiArchivePath,
                                        dataPath / nonAsciiPrefixArchivePath});
  }

  const std::string emptyFile;
  const std::string lowercaseBlankEsp;
  const std::string nonAsciiEsp;
  const std::string otherNonAsciiEsp;
  const std::string blankArchive;
  const std::string blankSuffixArchive;

  Game game_;

private:
  static std::string GetArchiveFileExtension(const GameType gameType) {
    if (gameType == GameType::fo4 || gameType == GameType::fo4vr ||
        gameType == GameType::starfield)
      return ".ba2";
    else
      return ".bsa";
  }
};

class TestPlugin : public PluginSortingInterface {
public:
  TestPlugin() : name_("") {}

  TestPlugin(std::string_view name) : name_(name) {}

  std::string GetName() const override { return name_; }

  std::optional<float> GetHeaderVersion() const override {
    return std::optional<float>();
  }

  std::optional<std::string> GetVersion() const override {
    return std::optional<std::string>();
  }

  std::vector<std::string> GetMasters() const override { return masters_; }

  std::vector<std::string> GetBashTags() const override { return {}; }

  std::optional<uint32_t> GetCRC() const override {
    return std::optional<uint32_t>();
  }

  bool IsMaster() const override { return isMaster_; }

  bool IsLightPlugin() const override { return isLightPlugin_; }

  bool IsMediumPlugin() const override { return false; }

  bool IsUpdatePlugin() const override { return false; }

  bool IsBlueprintPlugin() const override { return isBlueprintPlugin_; }

  bool IsValidAsLightPlugin() const override { return false; }

  bool IsValidAsMediumPlugin() const override { return false; }

  bool IsValidAsUpdatePlugin() const override { return false; }

  bool IsEmpty() const override { return false; }

  bool LoadsArchive() const override { return false; }

  bool DoRecordsOverlap(const PluginInterface& plugin) const override {
    const auto otherPlugin = dynamic_cast<const TestPlugin*>(&plugin);
    return recordsOverlapWith.count(&plugin) != 0 ||
           otherPlugin->recordsOverlapWith.count(this) != 0;
  }

  size_t GetOverrideRecordCount() const override {
    return overrideRecordCount_;
  }

  size_t GetAssetCount() const override { return assetCount_; };

  bool DoAssetsOverlap(const PluginSortingInterface& plugin) const override {
    const auto otherPlugin = dynamic_cast<const TestPlugin*>(&plugin);
    return assetsOverlapWith.count(&plugin) != 0 ||
           otherPlugin->assetsOverlapWith.count(this) != 0;
  }

  void AddMaster(std::string_view master) {
    masters_.push_back(std::string(master));
  }

  void SetIsMaster(bool isMaster) { isMaster_ = isMaster; }

  void SetIsLightPlugin(bool isLightPlugin) { isLightPlugin_ = isLightPlugin; }

  void SetIsBlueprintPlugin(bool isBlueprintPlugin) {
    isBlueprintPlugin_ = isBlueprintPlugin;
  }

  void AddOverlappingRecords(const PluginInterface& plugin) {
    recordsOverlapWith.insert(&plugin);
  }

  void SetOverrideRecordCount(size_t overrideRecordCount) {
    overrideRecordCount_ = overrideRecordCount;
  }

  void AddOverlappingAssets(const PluginSortingInterface& plugin) {
    assetsOverlapWith.insert(&plugin);
  }

  void SetAssetCount(size_t assetCount) { assetCount_ = assetCount; }

private:
  std::string name_;
  std::vector<std::string> masters_;
  std::set<const PluginInterface*> recordsOverlapWith;
  std::set<const PluginSortingInterface*> assetsOverlapWith;
  size_t overrideRecordCount_{0};
  size_t assetCount_{0};
  bool isMaster_{false};
  bool isLightPlugin_{false};
  bool isBlueprintPlugin_{false};
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(, PluginTest, ::testing::ValuesIn(ALL_GAME_TYPES));

TEST_P(PluginTest, constructorShouldTrimGhostExtensionExceptForOpenMW) {
  const auto pluginPath =
      game_.DataPath() / (blankMasterDependentEsm + ".ghost");

  if (GetParam() == GameType::openmw) {
    // This wasn't done for OpenMW during common setup.
    std::filesystem::rename(dataPath / blankMasterDependentEsm, pluginPath);
  }

  Plugin plugin(game_.GetType(), game_.GetCache(), pluginPath, true);

  if (GetParam() == GameType::openmw) {
    EXPECT_EQ(pluginPath.filename().u8string(), plugin.GetName());
  } else {
    EXPECT_EQ(blankMasterDependentEsm, plugin.GetName());
  }
}

TEST_P(PluginTest, loadingShouldHandleNonAsciiFilenamesCorrectly) {
  Plugin plugin(game_.GetType(),
                game_.GetCache(),
                game_.DataPath() / std::filesystem::u8path(nonAsciiEsp),
                true);

  EXPECT_EQ(nonAsciiEsp, plugin.GetName());
  EXPECT_EQ(nonAsciiEsp, plugin.GetName());
}

TEST_P(PluginTest, loadingWholePluginShouldReadFields) {
  const auto pluginName = GetParam() == GameType::openmw
                              ? blankMasterDependentEsm
                              : blankMasterDependentEsm + ".ghost";
  Plugin plugin(
      game_.GetType(), game_.GetCache(), game_.DataPath() / pluginName, false);

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
    Plugin master(
        game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, false);
    const auto pluginsMetadata = Plugin::GetPluginsMetadata({&master});

    EXPECT_NO_THROW(plugin.ResolveRecordIds(pluginsMetadata.get()));

    EXPECT_EQ(4, plugin.GetOverrideRecordCount());
  } else if (GetParam() == GameType::starfield) {
    Plugin master(game_.GetType(),
                  game_.GetCache(),
                  game_.DataPath() / blankFullEsm,
                  true);
    const auto pluginsMetadata = Plugin::GetPluginsMetadata({&master});

    EXPECT_NO_THROW(plugin.ResolveRecordIds(pluginsMetadata.get()));

    EXPECT_EQ(1, plugin.GetOverrideRecordCount());
  } else {
    EXPECT_EQ(4, plugin.GetOverrideRecordCount());
  }
}

TEST_P(PluginTest, loadingWholePluginShouldSucceedForOpenMWPlugins) {
  const auto omwgame = "Blank.omwgame";
  const auto omwaddon = "Blank.omwaddon";
  const auto omwscripts = "Blank.omwscripts";

  std::filesystem::rename(dataPath / blankEsm, dataPath / omwgame);
  std::filesystem::rename(dataPath / blankEsp, dataPath / omwaddon);
  std::ofstream out(dataPath / omwscripts);
  out.close();

  EXPECT_NO_THROW(
      Plugin(game_.GetType(), game_.GetCache(), dataPath / omwgame, false));
  EXPECT_NO_THROW(
      Plugin(game_.GetType(), game_.GetCache(), dataPath / omwaddon, false));
  if (GetParam() == GameType::openmw) {
    EXPECT_NO_THROW(Plugin(
        game_.GetType(), game_.GetCache(), dataPath / omwscripts, false));
  } else {
    EXPECT_THROW(
        Plugin(game_.GetType(), game_.GetCache(), dataPath / omwscripts, false),
        std::system_error);
  }
}

TEST_P(
    PluginTest,
    isLightPluginShouldBeTrueForAPluginWithEslFileExtensionForFallout4AndSkyrimSeAndFalseOtherwise) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / blankMasterDependentEsp,
                 true);
  Plugin plugin3(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsl, true);

  EXPECT_FALSE(plugin1.IsLightPlugin());
  EXPECT_FALSE(plugin2.IsLightPlugin());
  EXPECT_EQ(GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
                GetParam() == GameType::tes5se ||
                GetParam() == GameType::tes5vr ||
                GetParam() == GameType::starfield,
            plugin3.IsLightPlugin());
}

TEST_P(PluginTest, loadingAPluginThatDoesNotExistShouldThrow) {
  try {
    Plugin(game_.GetType(),
           game_.GetCache(),
           game_.DataPath() / "Blank\\.esp",
           true);
    FAIL();
  } catch (const std::system_error& e) {
    EXPECT_EQ(ESP_ERROR_FILE_NOT_FOUND, e.code().value());
    EXPECT_EQ(esplugin_category(), e.code().category());
  }
}

TEST_P(PluginTest, isValidShouldReturnTrueForAValidPlugin) {
  EXPECT_TRUE(Plugin::IsValid(game_.GetType(), game_.DataPath() / blankEsm));
}

TEST_P(PluginTest, isValidShouldReturnTrueForAValidNonAsciiPlugin) {
  EXPECT_TRUE(
      Plugin::IsValid(game_.GetType(),
                      game_.DataPath() / std::filesystem::u8path(nonAsciiEsp)));
}

TEST_P(PluginTest, isValidShouldReturnFalseForANonPluginFile) {
  EXPECT_FALSE(
      Plugin::IsValid(game_.GetType(), game_.DataPath() / nonPluginFile));
}

TEST_P(PluginTest, isValidShouldReturnFalseForAnEmptyFile) {
  EXPECT_FALSE(Plugin::IsValid(game_.GetType(), game_.DataPath() / emptyFile));
}

TEST_P(PluginTest, isValidShouldReturnTrueForAnOpenMWOmwscriptsFile) {
  const auto omwscripts = "Blank.omwscripts";
  std::ofstream out(dataPath / omwscripts);
  out.close();

  if (GetParam() == GameType::openmw) {
    EXPECT_TRUE(
        Plugin::IsValid(game_.GetType(), game_.DataPath() / omwscripts));
  } else {
    EXPECT_FALSE(
        Plugin::IsValid(game_.GetType(), game_.DataPath() / omwscripts));
  }
}

TEST_P(PluginTest,
       getAssetCountShouldReturnNumberOfFilesInArchivesLoadedByPlugin) {
  const auto assetCount =
      Plugin(
          game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false)
          .GetAssetCount();

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
    EXPECT_EQ(0, assetCount);
  } else if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
             GetParam() == GameType::starfield) {
    EXPECT_EQ(2, assetCount);
  } else {
    EXPECT_EQ(1, assetCount);
  }
}

TEST_P(PluginTest, getAssetCountShouldReturnZeroIfOnlyPluginHeaderWasLoaded) {
  const auto assetCount =
      Plugin(
          game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, true)
          .GetAssetCount();

  EXPECT_EQ(0, assetCount);
}

TEST_P(PluginTest,
       doAssetsOverlapShouldReturnFalseOrThrowIfTheArgumentIsNotAPluginObject) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false);
  TestPlugin plugin2;

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
    EXPECT_FALSE(plugin1.DoAssetsOverlap(plugin2));
  } else {
    EXPECT_THROW(plugin1.DoAssetsOverlap(plugin2), std::invalid_argument);
  }
}

TEST_P(PluginTest,
       doAssetsOverlapShouldReturnFalseForTwoPluginsWithOnlyHeadersLoaded) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, true);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / blankMasterDependentEsp,
                 true);

  EXPECT_FALSE(plugin1.DoAssetsOverlap(plugin2));
  EXPECT_FALSE(plugin2.DoAssetsOverlap(plugin1));
}

TEST_P(PluginTest,
       doAssetsOverlapShouldReturnFalseIfThePluginsDoNotLoadTheSameAssetPath) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false);
  // Blank - Different.esp does not load any assets.
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / blankDifferentEsp,
                 false);

  EXPECT_FALSE(plugin1.DoAssetsOverlap(plugin2));
  EXPECT_FALSE(plugin2.DoAssetsOverlap(plugin1));
}

TEST_P(PluginTest,
       doAssetsOverlapShouldReturnTrueIfThePluginsLoadTheSameAssetPath) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / blankMasterDependentEsp,
                 false);

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
    // Morrowind plugins can't load assets.
    EXPECT_FALSE(plugin1.DoAssetsOverlap(plugin2));
    EXPECT_FALSE(plugin2.DoAssetsOverlap(plugin1));
  } else {
    EXPECT_TRUE(plugin1.DoAssetsOverlap(plugin2));
    EXPECT_TRUE(plugin2.DoAssetsOverlap(plugin1));
  }
}

class HasPluginFileExtensionTest : public ::testing::TestWithParam<GameType> {};

INSTANTIATE_TEST_SUITE_P(,
                         HasPluginFileExtensionTest,
                         ::testing::ValuesIn(ALL_GAME_TYPES));

TEST_P(HasPluginFileExtensionTest, shouldBeTrueIfFileEndsInDotEspOrDotEsm) {
  EXPECT_TRUE(hasPluginFileExtension("file.esp", GetParam()));
  EXPECT_TRUE(hasPluginFileExtension("file.esm", GetParam()));
  EXPECT_FALSE(hasPluginFileExtension("file.bsa", GetParam()));
}

TEST_P(HasPluginFileExtensionTest,
       shouldBeTrueIfFileEndsInDotEslOnlyForFallout4AndLater) {
  bool result = hasPluginFileExtension("file.esl", GetParam());

  EXPECT_EQ(GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
                GetParam() == GameType::tes5se ||
                GetParam() == GameType::tes5vr ||
                GetParam() == GameType::starfield,
            result);
}

TEST_P(HasPluginFileExtensionTest, shouldTrimGhostExtensionExceptForOpenMW) {
  if (GetParam() == GameType::openmw) {
    EXPECT_FALSE(hasPluginFileExtension("file.esp.ghost", GetParam()));
    EXPECT_FALSE(hasPluginFileExtension("file.esm.ghost", GetParam()));
  } else {
    EXPECT_TRUE(hasPluginFileExtension("file.esp.ghost", GetParam()));
    EXPECT_TRUE(hasPluginFileExtension("file.esm.ghost", GetParam()));
  }
  EXPECT_FALSE(hasPluginFileExtension("file.bsa.ghost", GetParam()));
}

TEST_P(HasPluginFileExtensionTest, shouldRecogniseOpenMWPluginExtensions) {
  EXPECT_EQ(GetParam() == GameType::openmw,
            hasPluginFileExtension("file.omwgame", GetParam()));
  EXPECT_EQ(GetParam() == GameType::openmw,
            hasPluginFileExtension("file.omwaddon", GetParam()));
  EXPECT_EQ(GetParam() == GameType::openmw,
            hasPluginFileExtension("file.omwscripts", GetParam()));
}

TEST(equivalent, shouldReturnTrueIfGivenEqualPathsThatExist) {
  auto path1 = std::filesystem::path("./testing-plugins/LICENSE");
  auto path2 = std::filesystem::path("./testing-plugins/LICENSE");

  ASSERT_EQ(path1, path2);
  ASSERT_TRUE(std::filesystem::exists(path1));

  EXPECT_TRUE(loot::equivalent(path1, path2));
}

TEST(equivalent, shouldReturnTrueIfGivenEqualPathsThatDoNotExist) {
  auto path1 = std::filesystem::path("LICENSE2");
  auto path2 = std::filesystem::path("LICENSE2");

  ASSERT_EQ(path1, path2);
  ASSERT_FALSE(std::filesystem::exists(path1));

  EXPECT_TRUE(loot::equivalent(path1, path2));
}

TEST(equivalent,
     shouldReturnFalseIfGivenCaseInsensitivelyEqualPathsThatDoNotExist) {
  auto upper = std::filesystem::path("LICENSE2");
  auto lower = std::filesystem::path("license2");

  ASSERT_TRUE(boost::iequals(upper.u8string(), lower.u8string()));
  ASSERT_FALSE(std::filesystem::exists(upper));
  ASSERT_FALSE(std::filesystem::exists(lower));

  EXPECT_FALSE(loot::equivalent(lower, upper));
}

TEST(equivalent, shouldReturnFalseIfGivenCaseInsensitivelyUnequalThatExist) {
  auto path1 = std::filesystem::path("./testing-plugins/LICENSE");
  auto path2 = std::filesystem::path("./testing-plugins/README.md");

  ASSERT_FALSE(boost::iequals(path1.u8string(), path2.u8string()));
  ASSERT_TRUE(std::filesystem::exists(path1));
  ASSERT_TRUE(std::filesystem::exists(path2));

  EXPECT_FALSE(loot::equivalent(path1, path2));
}

#ifdef _WIN32
TEST(equivalent, shouldReturnTrueIfGivenCaseInsensitivelyEqualPathsThatExist) {
  auto upper = std::filesystem::path("./testing-plugins/LICENSE");
  auto lower = std::filesystem::path("./testing-plugins/license");

  ASSERT_TRUE(boost::iequals(upper.u8string(), lower.u8string()));
  ASSERT_TRUE(std::filesystem::exists(upper));
  ASSERT_TRUE(std::filesystem::exists(lower));

  EXPECT_TRUE(loot::equivalent(lower, upper));
}

TEST(
    equivalent,
    shouldReturnTrueIfEqualPathsHaveCharactersThatAreUnrepresentableInTheSystemMultiByteCodePage) {
  auto path1 = std::filesystem::u8path(
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00C3\u00CE.txt");
  auto path2 = std::filesystem::u8path(
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00C3\u00CE.txt");

  EXPECT_TRUE(loot::equivalent(path1, path2));
}

TEST(
    equivalent,
    shouldReturnFalseIfCaseInsensitivelyEqualPathsHaveCharactersThatAreUnrepresentableInTheSystemMultiByteCodePage) {
  auto path1 = std::filesystem::u8path(
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00E3\u00CE.txt");
  auto path2 = std::filesystem::u8path(
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00C3\u00CE.txt");

  EXPECT_FALSE(loot::equivalent(path1, path2));
}
#else
TEST(equivalent, shouldReturnFalseIfGivenCaseInsensitivelyEqualPathsThatExist) {
  auto upper = std::filesystem::path("./testing-plugins/LICENSE");
  auto lower = std::filesystem::path("./testing-plugins/license");

  std::ofstream out(lower);
  out.close();

  ASSERT_TRUE(boost::iequals(upper.u8string(), lower.u8string()));
  ASSERT_TRUE(std::filesystem::exists(upper));
  ASSERT_TRUE(std::filesystem::exists(lower));

  EXPECT_FALSE(loot::equivalent(lower, upper));
}
#endif
}
}

#endif
