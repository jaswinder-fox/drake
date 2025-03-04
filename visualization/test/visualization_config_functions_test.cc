#include "drake/visualization/visualization_config_functions.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "drake/common/test_utilities/expect_throws_message.h"
#include "drake/lcm/drake_lcm.h"
#include "drake/systems/analysis/simulator.h"

using drake::geometry::DrakeVisualizerParams;
using drake::geometry::Rgba;
using drake::geometry::Role;
using drake::geometry::SceneGraph;
using drake::lcm::DrakeLcm;
using drake::multibody::AddMultibodyPlantSceneGraph;
using drake::multibody::MultibodyPlant;
using drake::systems::DiagramBuilder;
using drake::systems::Simulator;
using drake::systems::lcm::LcmBuses;

namespace drake {
namespace visualization {
namespace internal {
namespace {

// For some configuration defaults, we'd like our default config values to
// match up with other default params code in Drake. If these tests ever
// fail, we'll need to revisit whether the Config should change to match
// the Params, or whether we agree they can differ (and remove the test).
GTEST_TEST(VisualizationConfigTest, Defaults) {
  const VisualizationConfig config;
  const DrakeVisualizerParams params;
  EXPECT_EQ(config.publish_period, params.publish_period);
  EXPECT_EQ(config.default_illustration_color, params.default_color);
}

// Tests the mapping from default schema data to geometry params.
GTEST_TEST(VisualizationConfigFunctionsTest, ParamConversionDefault) {
  const VisualizationConfig config;
  const std::vector<DrakeVisualizerParams> params =
      ConvertVisualizationConfigToParams(config);
  ASSERT_EQ(params.size(), 2);

  EXPECT_EQ(params.at(0).role, Role::kIllustration);
  EXPECT_EQ(params.at(0).default_color, config.default_illustration_color);
  EXPECT_FALSE(params.at(0).show_hydroelastic);
  EXPECT_FALSE(params.at(0).use_role_channel_suffix);
  EXPECT_EQ(params.at(0).publish_period, config.publish_period);

  EXPECT_EQ(params.at(1).role, Role::kProximity);
  EXPECT_EQ(params.at(1).default_color, config.default_proximity_color);
  EXPECT_TRUE(params.at(1).show_hydroelastic);
  EXPECT_TRUE(params.at(1).use_role_channel_suffix);
  EXPECT_EQ(params.at(1).publish_period, config.publish_period);
}

// Tests the mapping from non-default schema data to geometry params.
GTEST_TEST(VisualizationConfigFunctionsTest, ParamConversionSpecial) {
  VisualizationConfig config;
  config.publish_period = 0.5;
  config.publish_proximity = false;
  config.default_illustration_color = Rgba(0.25, 0.25, 0.25, 0.25);
  const std::vector<DrakeVisualizerParams> params =
      ConvertVisualizationConfigToParams(config);
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params.at(0).role, Role::kIllustration);
  EXPECT_FALSE(params.at(0).show_hydroelastic);
  EXPECT_EQ(params.at(0).publish_period, 0.5);
  EXPECT_EQ(params.at(0).default_color, Rgba(0.25, 0.25, 0.25, 0.25));
}

// Tests everything disabled.
GTEST_TEST(VisualizationConfigFunctionsTest, ParamConversionAllDisabled) {
  VisualizationConfig config;
  config.publish_illustration = false;
  config.publish_proximity = false;
  const std::vector<DrakeVisualizerParams> params =
      ConvertVisualizationConfigToParams(config);
  EXPECT_EQ(params.size(), 0);
}

// Overall acceptance test with everything enabled.
GTEST_TEST(VisualizationConfigFunctionsTest, ApplyDefault) {
  // We'll monitor which LCM channel names appear.
  DrakeLcm drake_lcm;
  std::set<std::string> observed_channels;
  drake_lcm.SubscribeAllChannels(
      [&observed_channels](std::string_view channel, const void* /* buffer */,
                           int /* size */) {
        observed_channels.emplace(channel);
      });
  LcmBuses lcm_buses;
  lcm_buses.Add("default", &drake_lcm);

  // Add MbP and SG, then the default visualization.
  DiagramBuilder<double> builder;
  auto [plant, scene_graph] = AddMultibodyPlantSceneGraph(&builder, 0.0);
  plant.Finalize();
  const VisualizationConfig config;
  ApplyVisualizationConfig(config, &builder, &lcm_buses, &plant, &scene_graph);
  Simulator<double> simulator(builder.Build());

  // Simulate for a moment and make sure everything showed up.
  simulator.AdvanceTo(0.25);
  while (drake_lcm.HandleSubscriptions(1) > 0) {}
  EXPECT_THAT(observed_channels, testing::IsSupersetOf({
      "DRAKE_VIEWER_LOAD_ROBOT",
      "DRAKE_VIEWER_LOAD_ROBOT_PROXIMITY",
      "DRAKE_VIEWER_DRAW",
      "DRAKE_VIEWER_DRAW_PROXIMITY",
      "CONTACT_RESULTS"}));
}

// Overall acceptance test with nothing enabled.
GTEST_TEST(VisualizationConfigFunctionsTest, ApplyNothing) {
  // Disable everything.
  VisualizationConfig config;
  config.publish_illustration = false;
  config.publish_proximity = false;
  config.publish_contacts = false;

  // We'll fail in case any message is transmitted.
  DrakeLcm drake_lcm;
  drake_lcm.SubscribeAllChannels([](auto...) { GTEST_FAIL(); });
  LcmBuses lcm_buses;
  lcm_buses.Add("default", &drake_lcm);

  // Add MbP and SG, then fully-disabled visualization.
  DiagramBuilder<double> builder;
  auto [plant, scene_graph] = AddMultibodyPlantSceneGraph(&builder, 0.0);
  plant.Finalize();
  ApplyVisualizationConfig(config, &builder, &lcm_buses, &plant, &scene_graph);
  Simulator<double> simulator(builder.Build());

  // Simulate for a moment and make sure nothing showed up.
  simulator.AdvanceTo(0.25);
  drake_lcm.HandleSubscriptions(1);
}

// When the lcm pointer is directly provided, the lcm_buses can be nullptr
// and nothing crashes.
GTEST_TEST(VisualizationConfigFunctionsTest, IgnoredLcmBuses) {
  DrakeLcm drake_lcm;
  DiagramBuilder<double> builder;
  auto [plant, scene_graph] = AddMultibodyPlantSceneGraph(&builder, 0.0);
  plant.Finalize();
  VisualizationConfig config;
  config.lcm_bus = "will_be_ignored";
  ApplyVisualizationConfig(config, &builder, nullptr, nullptr, nullptr,
      &drake_lcm);
  // Guard against any crashes or dangling pointers during diagram construction.
  builder.Build();
}

// When the lcm pointer is directly provided, the lcm_bus config string can
// refer to a missing bus_name and nothing crashes.
GTEST_TEST(VisualizationConfigFunctionsTest, IgnoredLcmBusName) {
  DrakeLcm drake_lcm;
  const LcmBuses empty_lcm_buses;
  DiagramBuilder<double> builder;
  auto [plant, scene_graph] = AddMultibodyPlantSceneGraph(&builder, 0.0);
  plant.Finalize();
  VisualizationConfig config;
  config.lcm_bus = "will_be_ignored";
  ApplyVisualizationConfig(config, &builder, &empty_lcm_buses, nullptr, nullptr,
      &drake_lcm);
  // Guard against any crashes or dangling pointers during diagram construction.
  builder.Build();
}

// The AddDefault... sugar shouldn't crash.
GTEST_TEST(VisualizationConfigFunctionsTest, AddDefault) {
  DiagramBuilder<double> builder;
  auto [plant, scene_graph] = AddMultibodyPlantSceneGraph(&builder, 0.0);
  plant.Finalize();
  AddDefaultVisualization(&builder);
  Simulator<double> simulator(builder.Build());
  simulator.AdvanceTo(0.25);
}

// A missing plant causes an exception.
GTEST_TEST(VisualizationConfigFunctionsTest, NoPlant) {
  DiagramBuilder<double> builder;
  DRAKE_EXPECT_THROWS_MESSAGE(
      AddDefaultVisualization(&builder),
      ".*does not contain.*plant.*");
}

// A missing scene_graph causes an exception.
GTEST_TEST(VisualizationConfigFunctionsTest, NoSceneGraph) {
  DiagramBuilder<double> builder;
  auto* plant = builder.AddSystem<MultibodyPlant<double>>(0.0);
  plant->set_name("plant");
  plant->Finalize();
  DRAKE_EXPECT_THROWS_MESSAGE(
      AddDefaultVisualization(&builder),
      ".*does not contain.*scene_graph.*");
}

// Type confusion causes an exception.
GTEST_TEST(VisualizationConfigFunctionsTest, WrongSystemTypes) {
  DiagramBuilder<double> builder;
  auto [plant, scene_graph] = AddMultibodyPlantSceneGraph(&builder, 0.0);
  plant.set_name("scene_graph");
  scene_graph.set_name("plant");
  DRAKE_EXPECT_THROWS_MESSAGE(
      AddDefaultVisualization(&builder),
      ".*of the wrong type.*");
}

}  // namespace
}  // namespace internal
}  // namespace visualization
}  // namespace drake
