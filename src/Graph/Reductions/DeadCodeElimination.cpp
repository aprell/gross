#include "gross/Graph/Reductions/DeadCodeElimination.h"
#include "gross/Graph/NodeUtils.h"

using namespace gross;
using namespace gross::graph_reduction;

// Eliminate dead inputs
GraphReduction DCEReducer::EliminateDead(Node* N) {
  unsigned Idx = 0;
  std::vector<unsigned> Worklist;

  // Value
  for(auto I = N->value_input_begin(), E = N->value_input_end();
      I != E; ++I, ++Idx) {
    if(NodeProperties<IrOpcode::Dead>(*I))
      Worklist.push_back(Idx);
  }
  for(auto Index : Worklist)
    N->removeValueInput(Index);
  Idx = 0;
  Worklist.clear();

  // Control
  for(auto I = N->control_input_begin(), E = N->control_input_end();
      I != E; ++I, ++Idx) {
    if(NodeProperties<IrOpcode::Dead>(*I))
      Worklist.push_back(Idx);
  }
  for(auto Index : Worklist)
    N->removeControlInput(Index);
  Idx = 0;
  Worklist.clear();

  // Effect
  for(auto I = N->effect_input_begin(), E = N->effect_input_end();
      I != E; ++I, ++Idx) {
    if(NodeProperties<IrOpcode::Dead>(*I))
      Worklist.push_back(Idx);
  }
  for(auto Index : Worklist)
    N->removeEffectInput(Index);

  return NoChange();
}

GraphReduction DCEReducer::Reduce(Node* N) {
  return EliminateDead(N);
}
