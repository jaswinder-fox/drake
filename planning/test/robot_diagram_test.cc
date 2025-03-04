#include "drake/planning/robot_diagram.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "drake/common/find_resource.h"
#include "drake/common/test_utilities/expect_throws_message.h"
#include "drake/planning/robot_diagram_builder.h"

namespace drake {
namespace planning {
namespace {

using geometry::SceneGraph;
using multibody::MultibodyPlant;
using multibody::Parser;
using symbolic::Expression;
using systems::Context;
using systems::DiagramBuilder;
using systems::System;

std::unique_ptr<RobotDiagramBuilder<double>> MakeSampleDut() {
  auto builder = std::make_unique<RobotDiagramBuilder<double>>();
  builder->mutable_parser().AddAllModelsFromFile(FindResourceOrThrow(
      "drake/manipulation/models/iiwa_description/urdf/"
      "iiwa14_spheres_dense_collision.urdf"));
  return builder;
}

GTEST_TEST(RobotDiagramBuilderTest, TimeStep) {
  const double time_step = 0.01;
  auto builder = std::make_unique<RobotDiagramBuilder<double>>(time_step);
  EXPECT_EQ(builder->plant().time_step(), time_step);
}

GTEST_TEST(RobotDiagramBuilderTest, Getters) {
  std::unique_ptr<RobotDiagramBuilder<double>> dut = MakeSampleDut();

  DiagramBuilder<double>& mutable_builder = dut->mutable_builder();
  const DiagramBuilder<double>& builder = dut->builder();
  Parser& mutable_parser = dut->mutable_parser();
  const Parser& parser = dut->parser();
  MultibodyPlant<double>& mutable_plant = dut->mutable_plant();
  const MultibodyPlant<double>& plant = dut->plant();
  SceneGraph<double>& mutable_scene_graph = dut->mutable_scene_graph();
  const SceneGraph<double>& scene_graph = dut->scene_graph();

  // The getters for mutable vs readonly are consistent.
  EXPECT_EQ(&mutable_builder, &builder);
  EXPECT_EQ(&mutable_parser, &parser);
  EXPECT_EQ(&mutable_plant, &plant);
  EXPECT_EQ(&mutable_scene_graph, &scene_graph);

  // The inter-connects are consistent.
  EXPECT_EQ(&mutable_parser.plant(), &plant);
  EXPECT_THAT(builder.GetSystems(), ::testing::Contains(&plant));
  EXPECT_THAT(builder.GetSystems(), ::testing::Contains(&scene_graph));
}

GTEST_TEST(RobotDiagramBuilderTest, Lifecycle) {
  std::unique_ptr<RobotDiagramBuilder<double>> dut = MakeSampleDut();
  EXPECT_FALSE(dut->IsPlantFinalized());
  EXPECT_FALSE(dut->IsDiagramBuilt());

  dut->FinalizePlant();
  EXPECT_TRUE(dut->IsPlantFinalized());
  EXPECT_FALSE(dut->IsDiagramBuilt());

  auto robot_diagram = dut->BuildDiagram();
  EXPECT_TRUE(dut->IsDiagramBuilt());

  // The lifecycle of the built Diagram has no effect on the builder's ability
  // to report its own lifecycle status.
  robot_diagram.reset();
  EXPECT_TRUE(dut->IsDiagramBuilt());
}

GTEST_TEST(RobotDiagramBuilderTest, LifecycleFailFast) {
  std::unique_ptr<RobotDiagramBuilder<double>> dut = MakeSampleDut();
  auto robot_diagram = dut->BuildDiagram();
  robot_diagram.reset();

  EXPECT_TRUE(dut->IsDiagramBuilt());
  constexpr const char* error = ".*may no longer be used.*";
  DRAKE_EXPECT_THROWS_MESSAGE(dut->mutable_builder(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->builder(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->mutable_parser(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->parser(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->mutable_plant(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->plant(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->mutable_scene_graph(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->scene_graph(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->IsPlantFinalized(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->FinalizePlant(), error);
  DRAKE_EXPECT_THROWS_MESSAGE(dut->BuildDiagram(), error);
}

GTEST_TEST(RobotDiagramTest, SmokeTest) {
  std::unique_ptr<RobotDiagramBuilder<double>> dut = MakeSampleDut();
  std::unique_ptr<RobotDiagram<double>> robot_diagram = dut->BuildDiagram();
  ASSERT_NE(robot_diagram, nullptr);
  EXPECT_NE(robot_diagram->CreateDefaultContext(), nullptr);
}

GTEST_TEST(RobotDiagramTest, ToAutoDiff) {
  std::unique_ptr<RobotDiagram<double>> robot_diagram_double =
      MakeSampleDut()->BuildDiagram();
  std::unique_ptr<RobotDiagram<AutoDiffXd>> robot_diagram_autodiff =
      System<double>::ToAutoDiffXd(*robot_diagram_double);
  ASSERT_NE(robot_diagram_autodiff, nullptr);
  EXPECT_NE(robot_diagram_autodiff->CreateDefaultContext(), nullptr);
}

GTEST_TEST(RobotDiagramTest, ToSymbolic) {
  std::unique_ptr<RobotDiagram<double>> robot_diagram_double =
      MakeSampleDut()->BuildDiagram();
  std::unique_ptr<RobotDiagram<Expression>> robot_diagram_symbolic =
      System<double>::ToSymbolic(*robot_diagram_double);
  ASSERT_NE(robot_diagram_symbolic, nullptr);
  EXPECT_NE(robot_diagram_symbolic->CreateDefaultContext(), nullptr);
}

GTEST_TEST(RobotDiagramBuilderTest, AutoDiffFromBirthToDeath) {
  auto builder_autodiff = std::make_unique<RobotDiagramBuilder<AutoDiffXd>>();
  builder_autodiff->mutable_plant().AddModelInstance("mike");
  std::unique_ptr<RobotDiagram<AutoDiffXd>> robot_diagram_autodiff =
      builder_autodiff->BuildDiagram();
  ASSERT_NE(robot_diagram_autodiff, nullptr);
  EXPECT_NE(robot_diagram_autodiff->CreateDefaultContext(), nullptr);
  EXPECT_TRUE(robot_diagram_autodiff->plant().HasModelInstanceNamed("mike"));
}

GTEST_TEST(RobotDiagramTest, SystemGetters) {
  std::unique_ptr<RobotDiagram<double>> dut = MakeSampleDut()->BuildDiagram();

  const MultibodyPlant<double>& plant = dut->plant();
  SceneGraph<double>& mutable_scene_graph = dut->mutable_scene_graph();
  const SceneGraph<double>& scene_graph = dut->scene_graph();

  // The getters for mutable vs readonly are consistent.
  EXPECT_EQ(&mutable_scene_graph, &scene_graph);

  // The inter-connects are consistent.
  EXPECT_THAT(dut->GetSystems(), ::testing::Contains(&plant));
  EXPECT_THAT(dut->GetSystems(), ::testing::Contains(&scene_graph));
}

GTEST_TEST(RobotDiagramTest, ContextGetters) {
  std::unique_ptr<RobotDiagram<double>> dut = MakeSampleDut()->BuildDiagram();

  std::unique_ptr<Context<double>> root_context = dut->CreateDefaultContext();
  const Context<double>& plant_context =
      dut->plant_context(*root_context);
  const Context<double>& scene_graph_context =
      dut->scene_graph_context(*root_context);
  Context<double>& mutable_plant_context =
      dut->mutable_plant_context(root_context.get());
  Context<double>& mutable_scene_graph_context =
      dut->mutable_scene_graph_context(root_context.get());

  // The getters for mutable vs readonly are consistent.
  EXPECT_EQ(&mutable_plant_context, &plant_context);
  EXPECT_EQ(&mutable_scene_graph_context, &scene_graph_context);

  // The contexts are appropriate for their systems.
  EXPECT_NO_THROW(dut->plant().ValidateContext(plant_context));
  EXPECT_NO_THROW(dut->scene_graph().ValidateContext(scene_graph_context));
}

}  // namespace
}  // namespace planning
}  // namespace drake

