#ifndef GROSS_GRAPH_GRAPH_H
#define GROSS_GRAPH_GRAPH_H
#include "gross/Support/iterator_range.h"
#include "gross/Support/Graph.h"
#include "gross/Graph/Node.h"
#include "gross/Graph/Attribute.h"
#include <memory>
#include <iostream>
#include <utility>
#include <vector>

namespace gross {
// lightweight nodes holder for part of the Graph
class SubGraph {
  template<class T>
  friend struct std::hash;

  // only hold the tail node, lazily travel the graph when needed
  Node* TailNode;

public:
  SubGraph() : TailNode(nullptr) {}

  explicit SubGraph(Node* Tail) : TailNode(Tail) {}

  bool operator==(const SubGraph& Other) const {
    return TailNode == Other.TailNode;
  }

  using node_iterator = lazy_node_iterator<SubGraph, false>;
  using const_node_iterator = lazy_node_iterator<SubGraph, true>;
  static Node* GetNodeFromIt(const node_iterator& NodeIt) { return *NodeIt; }
  static const Node* GetNodeFromIt(const const_node_iterator& NodeIt) {
    return *NodeIt;
  }
  node_iterator node_begin() { return node_iterator(TailNode); }
  node_iterator node_end() { return node_iterator(); }
  llvm::iterator_range<node_iterator> nodes() {
    return llvm::make_range(node_begin(), node_end());
  }
  const_node_iterator node_cbegin() const {
    return const_node_iterator(const_cast<const Node*>(TailNode));
  }
  const_node_iterator node_cend() const { return const_node_iterator(); }
  size_t node_size() const;

  using edge_iterator = lazy_edge_iterator<SubGraph>;
  edge_iterator edge_begin();
  edge_iterator edge_end();
  size_t edge_size();
  size_t edge_size() const { return const_cast<SubGraph*>(this)->edge_size(); }
};
} // end namespace gross

namespace std {
template<>
struct hash<gross::SubGraph> {
  using argument_type = gross::SubGraph;
  using result_type = std::size_t;
  result_type operator()(argument_type const& SG) const noexcept {
    return std::hash<gross::Node*>{}(SG.TailNode);
  }
};
} //end namespace std

namespace gross {
// map from vertex or edge to an unique id
template<class GraphT, class PropertyTag>
struct graph_id_map {};

// Owner of nodes
class Graph {
  template<IrOpcode::ID Op>
  friend class NodeBuilder;
  template<IrOpcode::ID Op>
  friend struct NodeProperties;
  friend class NodeMarkerBase;

  std::vector<std::unique_ptr<Node>> Nodes;

  // Constant pools
  NodeBiMap<std::string> ConstStrPool;
  NodeBiMap<int32_t> ConstNumberPool;
  Node* DeadNode;

  // functions
  std::vector<SubGraph> SubRegions;
  // SubGraph should be trivial constructable so just use value
  // as key
  NodeBiMap<SubGraph> FuncStubPool;

  // attribute storage (owner of attribute implements)
  std::unordered_map<Node*, std::unique_ptr<AttributeConcept>> Attributes;

  // recording state of NodeMarkers
  typename Node::MarkerTy MarkerMax;

public:
  Graph()
    : DeadNode(nullptr),
      MarkerMax(0U) {}

  using node_iterator = typename decltype(Nodes)::iterator;
  using const_node_iterator = typename decltype(Nodes)::const_iterator;
  static Node* GetNodeFromIt(const node_iterator& NodeIt) {
    return NodeIt->get();
  }
  static const Node* GetNodeFromIt(const const_node_iterator& NodeIt) {
    return NodeIt->get();
  }
  node_iterator node_begin() { return Nodes.begin(); }
  const_node_iterator node_cbegin() const { return Nodes.cbegin(); }
  node_iterator node_end() { return Nodes.end(); }
  const_node_iterator node_cend() const { return Nodes.cend(); }
  Node* getNode(size_t idx) const { return Nodes.at(idx).get(); }
  size_t node_size() const { return Nodes.size(); }

  using edge_iterator = lazy_edge_iterator<Graph>;
  edge_iterator edge_begin();
  edge_iterator edge_end();
  size_t edge_size();
  size_t edge_size() const { return const_cast<Graph*>(this)->edge_size(); }

  using subregion_iterator = typename decltype(SubRegions)::iterator;
  llvm::iterator_range<subregion_iterator> subregions() {
    return llvm::make_range(SubRegions.begin(), SubRegions.end());
  }

  void InsertNode(Node* N);
  const_node_iterator RemoveNode(const_node_iterator It) {
    return Nodes.erase(It);
  }

  void AddSubRegion(const SubGraph& SubG);
  void AddSubRegion(SubGraph&& SubG);

  size_t getNumConstStr() const {
    return ConstStrPool.size();
  }
  size_t getNumConstNumber() const {
    return ConstNumberPool.size();
  }

  // dump to GraphViz graph
  void dumpGraphviz(std::ostream& OS);
};
} // end namespace gross
#endif
