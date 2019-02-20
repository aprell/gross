#include "BGL.h"
#include "boost/graph/depth_first_search.hpp"
#include "gross/CodeGen/GraphScheduling.h"
#include "gross/Graph/Node.h"
#include "gross/Graph/NodeUtils.h"
#include <vector>

using namespace gross;

BasicBlock* GraphSchedule::NewBasicBlock() {
  auto* BB = new BasicBlock();
  Blocks.emplace_back(BB);
  return Blocks.back().get();
}

typename GraphSchedule::edge_iterator
GraphSchedule::edge_begin() {
  return edge_iterator(Blocks.begin(), Blocks.end());
}
typename GraphSchedule::edge_iterator
GraphSchedule::edge_end() {
  return edge_iterator(Blocks.end(), Blocks.end());
}
llvm::iterator_range<typename GraphSchedule::edge_iterator>
GraphSchedule::edges() {
  return llvm::make_range(edge_begin(), edge_end());
}
size_t GraphSchedule::edge_size() {
  return std::distance(edge_begin(), edge_end());
}

namespace gross {
namespace _internal {
class CFGBuilder {
  // SubGraph will travel nodes in reverse BFS order
  SubGraph SG;
  GraphSchedule& Schedule;

  std::vector<Node*> ControlNodes;

  BasicBlock* MapBlock(Node* N) {
    return Schedule.MapBlock(N);
  }

  void AddNodeToBlock(Node* N, BasicBlock* BB) {
    Schedule.AddNode(BB, N);
  }

  void BlockPlacement();

  void connectBlocks(BasicBlock* PredBB, BasicBlock* SuccBB) {
    PredBB->AddSuccBlock(SuccBB);
    SuccBB->AddPredBlock(PredBB);
  }
  void ConnectBlock(Node* CtrlNode);

public:
  CFGBuilder(const SubGraph& subgraph, GraphSchedule& schedule)
    : SG(subgraph),
      Schedule(schedule) {}

  void Run() {
    BlockPlacement();

    for(auto* N : ControlNodes) {
      ConnectBlock(N);
    }

    Schedule.SortRPO();
  }

  decltype(ControlNodes)& getCtrlNodes() { return ControlNodes; }
};

void CFGBuilder::BlockPlacement() {
  // we want normal BFS traveling order here
  std::vector<Node*> RPONodes;
  for(auto* N : SG.nodes())
    RPONodes.insert(RPONodes.cbegin(), N);

  for(auto* N : RPONodes) {
    switch(N->getOp()) {
    case IrOpcode::If: {
      assert(N->getNumControlInput() > 0);
      auto* PrevCtrl = N->getControlInput(0);
      auto* BB = MapBlock(PrevCtrl);
      assert(BB && "If node's previous control not visited yet?");
      AddNodeToBlock(N, BB);
      break;
    }
    case IrOpcode::Start:
    case IrOpcode::End:
    case IrOpcode::Loop: // loop header
    case IrOpcode::IfTrue:
    case IrOpcode::IfFalse:
    case IrOpcode::Merge: {
      auto* BB = Schedule.NewBasicBlock();
      AddNodeToBlock(N, BB);
      break;
    }
    default:
      continue;
    }
    ControlNodes.push_back(N);
  }
}

void CFGBuilder::ConnectBlock(Node* CtrlNode) {
  auto* EncloseBB = MapBlock(CtrlNode);
  assert(EncloseBB && "not enclosed in any BB?");

  switch(CtrlNode->getOp()) {
  default:
    // we ignore If node here
    return;
  case IrOpcode::Merge: {
    // merge from two branches
    NodeProperties<IrOpcode::Merge> NP(CtrlNode);
    Node* Branches[2] = { NP.TrueBranch(), NP.FalseBranch(true) };
    for(auto* Br : Branches) {
      auto* BB = MapBlock(Br);
      assert(BB && "branch not enclosed in any BB?");
      EncloseBB->AddPredBlock(BB);
      BB->AddSuccBlock(EncloseBB);
    }
    break;
  }
  case IrOpcode::IfTrue:
  case IrOpcode::IfFalse: {
    NodeProperties<IrOpcode::VirtIfBranches> NP(CtrlNode);
    auto* PrevBB = MapBlock(NP.BranchPoint());
    assert(PrevBB && "If node not enclosed in any BB?");
    connectBlocks(PrevBB, EncloseBB);
    break;
  }
  case IrOpcode::Loop: {
    for(auto* CI : CtrlNode->control_inputs()) {
      auto* PrevBB = MapBlock(CI);
      assert(PrevBB);
      connectBlocks(PrevBB, EncloseBB);
    }
    break;
  }
  case IrOpcode::End: {
    std::set<BasicBlock*> PrevBBs;
    BasicBlock* EntryBlock = nullptr;
    for(auto* CI : CtrlNode->control_inputs()) {
      auto* PrevBB = MapBlock(CI);
      assert(PrevBB);
      if(CI->getOp() == IrOpcode::Start) {
        EntryBlock = PrevBB;
        continue;
      }
      // eliminate duplicate
      PrevBBs.insert(PrevBB);
    }
    if(PrevBBs.empty() && EntryBlock) PrevBBs.insert(EntryBlock);
    for(auto* BB : PrevBBs) {
      connectBlocks(BB, EncloseBB);
    }
    break;
  }
  }
}
} // end namespace _internal
} // end namespace gross

struct GraphSchedule::RPOVisitor
  : public boost::default_dfs_visitor {
  void finish_vertex(BasicBlock* BB, const GraphSchedule& G) {
    Trace.insert(Trace.cbegin(), BB);
  }

  RPOVisitor() = delete;
  RPOVisitor(std::vector<BasicBlock*>& trace) : Trace(trace) {}

private:
  std::vector<BasicBlock*>& Trace;
};

void GraphSchedule::SortRPO() {
  RPOBlocks.clear();
  RPOVisitor Vis(RPOBlocks);
  std::unordered_map<BasicBlock*,boost::default_color_type> ColorStorage;
  StubColorMap<decltype(ColorStorage), BasicBlock> ColorMap(ColorStorage);
  boost::depth_first_search(*this, Vis, std::move(ColorMap));
}

GraphScheduler::GraphScheduler(Graph& graph) : G(graph) {}

void GraphScheduler::ComputeScheduledGraph() {
  // Phases:
  // 1. Build CFG. Insert fix nodes
  // 2. Insert rest of the nodes according to
  //    control dependencies.
}