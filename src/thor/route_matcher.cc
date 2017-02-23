#include <vector>
#include <algorithm>
#include <exception>
#include "midgard/logging.h"
#include "baldr/errorcode_util.h"

#include "thor/route_matcher.h"
#include "thor/service.h"

using namespace valhalla::baldr;
using namespace valhalla::sif;
using namespace valhalla::thor;

namespace valhalla {
namespace thor {

/*
 * An “edge-walking” method for use when the input shape is exact
 * shape from a prior Valhalla route. This will walk the input shape
 * and compare to Valhalla edge’s end node positions to form the list of edges.
 *
 */

namespace {

// Minimum tolerance for edge length
constexpr float kMinLengthTolerance = 10.0f;

// Get the length to compare to the edge length
float length_comparison(const float length, const bool exact_match) {
  // Alter tolerance based on exact_match flag
  float t, max_t;
  if (exact_match) {
    t = length * 0.05f;
    max_t = 25.0f;
  } else {
    t = length * 0.1f;
    max_t = 100.0f;
  }
  float tolerance = (t < kMinLengthTolerance) ? kMinLengthTolerance :
                      (t > max_t) ? max_t : t;
  return length + tolerance;
}

// Get a map of end edges and the start node of each edge. This is used
// to terminate the edge walking method.
std::unordered_map<GraphId, PathLocation::PathEdge> GetEndEdges(
        GraphReader& reader,
        const std::vector<PathLocation>& correlated) {
  std::unordered_map<GraphId, PathLocation::PathEdge> end_nodes;
  for (const auto& edge : correlated.back().edges) {
    // If destination is at a node - skip any outbound edge
    if (edge.begin_node() || !edge.id.Is_Valid()) {
      continue;
    }

    // Add the node which we look for to terminate edge walking. This is
    // generally the start of the end edge, unless the end edge ends ends
    // at a node (not partially along the edge)
    if (edge.end_node()) {
      // If this edge ends at a node add its end node
      auto* tile = reader.GetGraphTile(edge.id);
      auto* directededge = tile->directededge(edge.id);
      end_nodes.insert({directededge->endnode(), edge});
    } else {
      // Get the start node of this edge
      auto* opp_edge = reader.GetOpposingEdge(edge.id);
      if (opp_edge == nullptr) {
        throw std::runtime_error("Couldn't get the opposing edge");
      }
      end_nodes.insert({opp_edge->endnode(), edge});
    }
  }
  if (end_nodes.size() == 0) {
    throw std::runtime_error("No valid end edges are found");
  }
  return end_nodes;
}


bool expand_from_node(const std::shared_ptr<DynamicCost>* mode_costing,
                      const TravelMode& mode, GraphReader& reader,
                      const std::vector<midgard::PointLL>& shape,
                      const std::vector<float>& distances,
                      size_t& correlated_index, const GraphTile* tile,
                      const GraphId& node,
                      std::unordered_map<GraphId, PathLocation::PathEdge>& end_nodes,
                      EdgeLabel& prev_edge_label, float& elapsed_time,
                      std::vector<PathInfo>& path_infos,
                      const bool from_transition, GraphId& end_node) {

  // If node equals stop node then when are done expanding
  auto n = end_nodes.find(node);
  if (n != end_nodes.end()) {
    end_node = node;
    return true;
  }

  const NodeInfo* node_info = tile->node(node);
  GraphId edge_id(node.tileid(), node.level(), node_info->edge_index());
  const DirectedEdge* de = tile->directededge(node_info->edge_index());
  for (uint32_t i = 0; i < node_info->edge_count(); i++, de++, edge_id++) {
    // Skip shortcuts and transit connection edges
    // TODO - later might allow transit connections for multi-modal
    if (de->is_shortcut() || de->use() == Use::kTransitConnection) {
      continue;
    }

    // Look back in path_infos by 1-2 edges to make sure we aren't in a loop.
    // A loop can occur if we have edges shorter than the lat,lng tolerance.
    uint32_t n = path_infos.size();
    if (n > 1 &&  (edge_id == path_infos[n-2].edgeid ||
        edge_id == path_infos[n-1].edgeid)) {
      continue;
    }

    // Process transition edge if previous edge was not from a transition
    if (de->trans_down() || de->trans_up()) {
      if (from_transition) {
        continue;
      } else {
        const GraphTile* end_node_tile = reader.GetGraphTile(de->endnode());
        if (end_node_tile == nullptr) {
          continue;
        }
        if (expand_from_node(mode_costing, mode, reader, shape, distances,
                             correlated_index, end_node_tile, de->endnode(),
                             end_nodes, prev_edge_label, elapsed_time,
                             path_infos, true, end_node)) {
          return true;
        } else {
          continue;
        }
      }
    }

    // Get the end node LL and set up the length comparison
    const GraphTile* end_node_tile = reader.GetGraphTile(de->endnode());
    if (end_node_tile == nullptr) {
      continue;
    }
    PointLL de_end_ll = end_node_tile->node(de->endnode())->latlng();
    float de_length = length_comparison(de->length(), true);

    // Process current edge until shape matches end node
    // or shape length is longer than the current edge. Increment to the
    // next shape point after the correlated index.
    size_t index = correlated_index + 1;
    float length = 0.0f;
    while (index < shape.size()) {
      // Exclude edge if length along shape is longer than the edge length
      length += distances.at(index);
      if (length > de_length) {
        break;
      }

      // Found a match if shape equals directed edge LL within tolerance
      if (shape.at(index).ApproximatelyEqual(de_end_ll)) {
        // Update the elapsed time based on transition cost
        elapsed_time += mode_costing[static_cast<int>(mode)]->TransitionCost(
            de, node_info, prev_edge_label).secs;

        // Update the elapsed time based on edge cost
        elapsed_time += mode_costing[static_cast<int>(mode)]->EdgeCost(de).secs;

        // Add edge and update correlated index
        path_infos.emplace_back(mode, std::round(elapsed_time), edge_id, 0);

        // Set previous edge label
        prev_edge_label = {kInvalidLabel, edge_id, de, {}, 0, 0, mode, 0};

        // Continue walking shape to find the end edge...
        if (expand_from_node(mode_costing, mode, reader, shape, distances,
                                 index, end_node_tile, de->endnode(),
                                 end_nodes, prev_edge_label, elapsed_time,
                                 path_infos, false, end_node)) {
          return true;
        } else {
          // Match failed along this edge, pop the last entry off path_infos
          // and try the next edge
          path_infos.pop_back();
          break;
        }
      }
      index++;
    }
  }
  return false;
}

}

// For the route path using an edge walking method.
bool RouteMatcher::FormPath(
    const std::shared_ptr<DynamicCost>* mode_costing,
    const sif::TravelMode& mode, GraphReader& reader,
    const std::vector<midgard::PointLL>& shape,
    const std::vector<PathLocation>& correlated,
    std::vector<PathInfo>& path_infos) {
  float elapsed_time = 0.f;

  // Form distances between shape points
  std::vector<float> distances;
  distances.push_back(0.0f);
  for (size_t i = 1; i < shape.size(); i++) {
    distances.push_back(shape[i].Distance(shape[i-1]));
  }

  // Process and validate end edges (can be more than 1). Create a map of
  // the end edges' start nodes and the edge information.
  auto end_nodes = GetEndEdges(reader, correlated);

  // Iterate through start edges
  for (const auto& edge : correlated.front().edges) {
    // If origin is at a node - skip any inbound edge
    if (edge.end_node()) {
      continue;
    }

    // Process and validate begin edge
    if (!edge.id.Is_Valid()) {
      throw std::runtime_error("Invalid begin edge id");
    }
    const GraphTile* begin_edge_tile = reader.GetGraphTile(edge.id);
    if (begin_edge_tile == nullptr) {
      throw std::runtime_error("Begin tile is null");
    }

    // Process directed edge and info
    const DirectedEdge* de = begin_edge_tile->directededge(edge.id);
    const GraphTile* end_node_tile = reader.GetGraphTile(de->endnode());
    if (begin_edge_tile == nullptr) {
      throw std::runtime_error("End node tile is null");
    }
    PointLL de_end_ll = end_node_tile->node(de->endnode())->latlng();

    // Initialize indexes and shape
    size_t index = 0;
    float length = 0.0f;
    float de_length = length_comparison(de->length() * (1 - edge.dist), true);
    EdgeLabel prev_edge_label;
    // Loop over shape to form path from matching edges
    while (index < shape.size()) {
      length += distances.at(index);
      if (length > de_length) {
        break;
      }

      // Check if shape is within tolerance at the end node
      if (shape.at(index).ApproximatelyEqual(de_end_ll)) {

        // Update the elapsed time edge cost at begin edge
        elapsed_time += mode_costing[static_cast<int>(mode)]->EdgeCost(de).secs
            * (1 - edge.dist);

        // Add begin edge
        path_infos.emplace_back(mode, std::round(elapsed_time),
                                edge.id, 0);

        // Set previous edge label
        prev_edge_label = {kInvalidLabel, edge.id, de, {}, 0, 0, mode, 0};

        // Continue walking shape to find the end node
        GraphId end_node;
        if (expand_from_node(mode_costing, mode, reader, shape, distances, index,
                             end_node_tile, de->endnode(), end_nodes,
                             prev_edge_label, elapsed_time, path_infos,
                             false, end_node)) {
          // If node equals stop node then when are done expanding - get
          // the matching end edge
          auto n = end_nodes.find(end_node);
          if (n == end_nodes.end()) {
            return false;
          }

          // If the end edge distance = 1.0 then we are done
          if (n->second.dist == 1.0f) {
            return true;
          }

          // Get the end edge and add transition time and partial time along
          // the destination edge.
          const GraphTile* end_edge_tile = reader.GetGraphTile(n->second.id);
          if (end_edge_tile == nullptr) {
            throw std::runtime_error("End edge tile is null");
          }
          const DirectedEdge* end_de = end_edge_tile->directededge(n->second.id);

          // Update the elapsed time based on transition cost
          elapsed_time += mode_costing[static_cast<int>(mode)]->TransitionCost(
              end_de, end_edge_tile->node(n->first), prev_edge_label).secs;

          // Update the elapsed time based on edge cost
          elapsed_time += mode_costing[static_cast<int>(mode)]->EdgeCost(end_de).secs *
                                  n->second.dist;

          // Add end edge
          path_infos.emplace_back(mode, std::round(elapsed_time),
                                  n->second.id, 0);

          return true;
        } else {
          // Did not find end edge - so get out
          return false;
        }
      }
      index++;
    }

    // Did not find the end of the origin edge. Check for special case where
    // end is along the same edge.
    for (auto end : end_nodes) {
      if (end.second.id == edge.id) {
        // Update the elapsed time based on edge cost
        elapsed_time += mode_costing[static_cast<int>(mode)]->EdgeCost(de).secs *
                               (end.second.dist - edge.dist);

        // Add end edge
        path_infos.emplace_back(mode, std::round(elapsed_time), edge.id, 0);
        return true;
      }
    }
  }
  // TODO - would be nice to know this, but if map-matching fallback is specified
  // this would not fall back.
//  throw std::runtime_error("RouteMatcher::FormPath could not match to begin edge");
  return false;
}

}
}
