#include "gurka.h"
#include "test.h"

using namespace valhalla;

std::string deprecated_costing_methods[3] = {"auto_shorter", "hov", "auto_data_fix"};

// test case for routes endpoint
TEST(warnings, routes_endpoint) {
  const std::string ascii_map = R"(
          A----------B-------------C-----P
          |          |                   |
          |          |                   |
          |          3--------U     G----S
          |          |        |     |    |
          D----------E--------4-----F----L
     )";
  const gurka::ways ways = {
      {"AB", {{"highway", "primary"}, {"name", "RT 1"}}},
      {"BC", {{"highway", "motorway"}, {"name", "RT 2"}}},
      {"CP", {{"highway", "motorway"}, {"name", "RT 3"}}},
      {"AD", {{"highway", "motorway"}, {"name", "RT 4"}}},
      {"B3", {{"highway", "motorway"}, {"name", "RT 5"}}},
      {"3E", {{"highway", "motorway"}, {"name", "RT 6"}}},
      {"3U", {{"highway", "motorway"}, {"name", "RT 7"}}},
      {"E4", {{"highway", "motorway"}, {"name", "RT 8"}}},
      {"U4", {{"highway", "motorway"}, {"name", "RT 9"}}},
      {"4F", {{"highway", "motorway"}, {"name", "RT 10"}}},
      {"GF", {{"highway", "motorway"}, {"name", "RT 11"}}},
      {"GS", {{"highway", "motorway"}, {"name", "RT 12"}}},
      {"PS", {{"highway", "motorway"}, {"name", "RT 13"}}},
      {"FL", {{"highway", "motorway"}, {"name", "RT 14"}}},
      {"SL", {{"highway", "motorway"}, {"name", "RT 15"}}},
  };
  const double gridsize = 100;
  const auto layout = gurka::detail::map_to_coordinates(ascii_map, gridsize);
  auto map = gurka::buildtiles(layout, ways, {}, {}, "test/data/routes_warnings");
  for (int i = 0; i < 3; i++) {
    valhalla::Api result = gurka::do_action(valhalla::Options::route, map, {"A", "L"},
                                            deprecated_costing_methods[i], {{"/best_paths", "2"}});
    ASSERT_TRUE(result.info().warnings_size() != 0);
    EXPECT_EQ(result.info().warnings_size(), 2);
  }
}

// test case for locate endpoint
TEST(location_warnings, locate_endpoint) {
  const std::string ascii_map = R"(
            1------------2-----------A
            |            |           |
            |            |           |
            a------------b-----------c
            |            |           |
            |            B-----------K
            |  E---------|           |
            | /          |           6
            |/           |           |
            D------------F-----------3
  )";
  const gurka::ways ways = {
      {"AE", {{"highway", "primary"}, {"name", "RT 1"}}},
      {"BE", {{"highway", "motorway"}, {"name", "RT 2"}}},
      {"DE", {{"highway", "motorway"}, {"name", "RT 3"}}},
      {"BF", {{"highway", "motorway"}, {"name", "RT 4"}}},
      {"DF", {{"highway", "motorway"}, {"name", "RT 5"}}},
  };
  const double gridsize = 100;
  const auto layout = gurka::detail::map_to_coordinates(ascii_map, gridsize);
  auto map = gurka::buildtiles(layout, ways, {}, {}, "test/data/locate_warnings");
  for (int i = 0; i < 3; i++) {
    valhalla::Api result =
        gurka::do_action(valhalla::Options::locate, map, {"6"}, deprecated_costing_methods[i]);
    ASSERT_TRUE(result.info().warnings_size() != 0);
    EXPECT_EQ(result.info().warnings_size(), 1);
  }
}

// test case for isochrone endpoint
TEST(isochrone_warnings, isochrone_endpoint) {
  const std::string ascii_map = R"(
            E------------M-----------A--------Z
            |            |                    |
            |            |                    |
            3------------N-----------C--------|
            |            |           |        |
            |            B           2--------|
            |                                 |
            |                                 |
            |                                /  
            P------------4-----------1------/
  )";
  const gurka::ways ways = {
      {"EM", {{"highway", "primary"}, {"name", "RT 1"}}},
      {"MA", {{"highway", "motorway"}, {"name", "RT 2"}}},
      {"AZ", {{"highway", "motorway"}, {"name", "RT 3"}}},
      {"NC", {{"highway", "motorway"}, {"name", "RT 4"}}},
      {"NB", {{"highway", "motorway"}, {"name", "RT 5"}}},
  };
  const double gridsize = 100;
  const auto layout = gurka::detail::map_to_coordinates(ascii_map, gridsize);
  auto map = gurka::buildtiles(layout, ways, {}, {}, "test/data/isochrone_warnings");
  for (int i = 0; i < 3; i++) {
    valhalla::Api result =
        gurka::do_action(valhalla::Options::isochrone, map, {"B"}, deprecated_costing_methods[i],
                         {{"/contours/0/time", "10"}, {"/denoise", "0"}, {"/generalize", "0"}});
    ASSERT_FALSE(result.info().warnings_size() == 0);
    EXPECT_EQ(result.info().warnings_size(), 1);
  }
}

// test case for transit available endpoint
TEST(transit_available_warnings, transit_available_endpoint) {
  const std::string ascii_map = R"(
            A------------B-----------C--------D
            |            |           |        |
            |            |           |        |
            E------------1-----------s--------H-------------------------Q
            |            |           |        |
            |            |           |        |
            I------------2-----------t--------L-------------------------R
            |            |           |        |
            |            |           |        |  
            M------------N-----------O--------P
  )";
  const gurka::ways ways = {
      {"AD", {{"highway", "primary"}, {"name", "RT 1"}}},
      {"EQ", {{"highway", "motorway"}, {"name", "RT 2"}}},
      {"IR", {{"highway", "motorway"}, {"name", "RT 3"}}},
      {"MP", {{"highway", "motorway"}, {"name", "RT 4"}}},
      {"DP", {{"highway", "motorway"}, {"name", "RT 5"}}},
      {"AM", {{"highway", "motorway"}, {"name", "RT 6"}}},
  };
  const double gridsize = 100;
  const auto layout = gurka::detail::map_to_coordinates(ascii_map, gridsize);
  auto map = gurka::buildtiles(layout, ways, {}, {}, "test/data/transit_available_warnings");
  valhalla::Api result = gurka::do_action(valhalla::Options::transit_available, map, {"A"}, "",
                                          {{"/locations/0/radius", "5"}, {"/best_paths", "2"}});
  ASSERT_FALSE(result.info().warnings_size() == 0);
  EXPECT_EQ(result.info().warnings_size(), 1);
}

// test case for height endpoint
TEST(height_warnings, height_endpoint) {
  const std::string ascii_map = R"(
            Z---------------------------------P
            |                                 |
            |                                 |
            D                                 |
            |                                 |
            |                                 |
            G----------------------------------
            |            |           |        |
            |            |           |        |  
            N---------------------------------Q
  )";
  const gurka::ways ways = {
      {"ZP", {{"highway", "primary"}, {"name", "RT 1"}}},
      {"NQ", {{"highway", "motorway"}, {"name", "RT 2"}}},
      {"PQ", {{"highway", "motorway"}, {"name", "RT 3"}}},
  };
  const double gridsize = 100;
  const auto layout = gurka::detail::map_to_coordinates(ascii_map, gridsize);
  auto map = gurka::buildtiles(layout, ways, {}, {}, "test/data/height_warnings");
  // using best_paths which is not necessary or the elevation API but should generate a warning
  valhalla::Api result = gurka::do_action(valhalla::Options::height, map, {"D", "G"}, "",
                                          {{"/resample_distance", "15"}, {"/best_paths", "2"}});
  ASSERT_FALSE(result.info().warnings_size() == 0);
  EXPECT_EQ(result.info().warnings_size(), 1);
}

// test case for map_matching endpoint
TEST(map_matching_warnings, map_matching_endpoint) {
  const std::string ascii_map = R"(
      ------A-------------       X
      |                  |       |          ---------------D----------E   
      |     P-----G      |       |          |                         |
      |     |     |      |       |          |                         |
      2     |     |      C-----------------B                          | 
      |     |     |                                                   |
      ------S     ----------------------------------------------------F
  )";

  const gurka::ways ways = {
      {"GF", {{"highway", "primary"}, {"name", "RT 1"}}},
      {"AE", {{"highway", "motorway"}, {"name", "RT 2"}}},
      {"PS", {{"highway", "motorway"}, {"name", "RT 3"}}},
  };
  const double gridsize = 100;
  const auto layout = gurka::detail::map_to_coordinates(ascii_map, gridsize);
  auto map = gurka::buildtiles(layout, ways, {}, {}, "test/data/map_matching_warnings");
  for (int i = 0; i < 3; i++) {
    valhalla::Api trace_route_result =
        gurka::do_action(valhalla::Options::trace_route, map, {"D", "E", "F"},
                         deprecated_costing_methods[i]);
    valhalla::Api trace_attributes_result =
        gurka::do_action(valhalla::Options::trace_attributes, map,
                         {"A", "C", "B", "E", "E", "F", "G", "P", "S"},
                         deprecated_costing_methods[i]);
    ASSERT_FALSE(trace_route_result.info().warnings_size() == 0);
    EXPECT_EQ(trace_route_result.info().warnings_size(), 1);
    ASSERT_TRUE(trace_attributes_result.info().warnings_size() != 0);
    EXPECT_EQ(trace_attributes_result.info().warnings_size(), 1);
  }
}

// test case for matrix endpoint
TEST(matrix_warnings, matrix_endpoint) {
  const std::string ascii_map = R"(
      A----------------------------------D----------E         
      |                                  |   
      |                                  |
      |                                  |
      |                                  |----------------------------G
      |                                                               |
      |                                                               |
      |                                                               |
      B----------------------------------------------------------------     
  )";

  const gurka::ways ways = {
      {"AB", {{"highway", "primary"}, {"name", "RT 1"}}},
      {"DG", {{"highway", "motorway"}, {"name", "RT 2"}}},
      {"BG", {{"highway", "motorway"}, {"name", "RT 3"}}},
  };
  const double gridsize = 100;
  const auto layout = gurka::detail::map_to_coordinates(ascii_map, gridsize);
  auto map = gurka::buildtiles(layout, ways, {}, {}, "test/data/matrix_warnings");
  Api request;
  for (int i = 0; i < 3; i++) {
    std::string request_str = R"({"sources":[{"lat":)" + std::to_string(map.nodes["A"].lat()) +
                              R"(,"lon":)" + std::to_string(map.nodes["A"].lng()) +
                              R"(}],"targets":[{"lat":)" + std::to_string(map.nodes["G"].lat()) +
                              R"(,"lon":)" + std::to_string(map.nodes["G"].lng()) +
                              R"(}],"costing":")" + deprecated_costing_methods[i] + R"("})";
    ParseApi(request_str, Options::sources_to_targets, request);
    ASSERT_FALSE(request.info().warnings_size() == 0);
    EXPECT_EQ(request.info().warnings_size(), 1);
  }
}