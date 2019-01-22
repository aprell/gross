#ifndef GROSS_GRAPH_NODE_UTILS_H
#define GROSS_GRAPH_NODE_UTILS_H
#include "gross/Graph/Node.h"
#include "gross/Graph/Graph.h"
#include "gross/Support/type_traits.h"
#include <string>

namespace gross {
template<IrOpcode::ID Op>
class NodePropertiesBase {
protected:
  Node* NodePtr;

  NodePropertiesBase(Node* N)
    : NodePtr(N) {}

public:
  operator bool() const {
    return NodePtr && NodePtr->Op == Op;
  }
};

template<IrOpcode::ID Op>
struct NodeProperties : public NodePropertiesBase<Op> {
  NodeProperties(Node* N)
    : NodePropertiesBase<Op>(N) {}
};

#define NODE_PROPERTIES(OP) \
  template<>  \
  struct NodeProperties<IrOpcode::OP> : public NodePropertiesBase<IrOpcode::OP>

#define NODE_PROP_BASE(OP, NODE) \
  NodePropertiesBase<IrOpcode::OP>(NODE)

NODE_PROPERTIES(SrcVarDecl) {
  NodeProperties(Node *N)
    : NODE_PROP_BASE(SrcVarDecl, N) {}

  Node* getSymbolName() const {
    assert(NodePtr->getNumValueInput() > 0);
    return NodePtr->getValueInput(0);
  }
};

NODE_PROPERTIES(ConstantInt) {
  NodeProperties(Node *N)
    : NODE_PROP_BASE(ConstantInt, N) {}

  template<typename T>
  T as(const Graph& G) const {
    if(!*this) return T();
    if(auto* V = G.ConstNumberPool.find_value(NodePtr))
      return static_cast<T>(*V);
    else
      return T();
  }
};
NODE_PROPERTIES(ConstantStr) {
  NodeProperties(Node *N)
    : NODE_PROP_BASE(ConstantStr, N) {}

  const std::string& str(const Graph& G) const {
    assert(*this && "Invalid Node");
    const auto* V = G.ConstStrPool.find_value(NodePtr);
    assert(V && "string not found");
    return *V;
  }
  const std::string str_val(const Graph& G) const {
    if(!*this) return "";
    if(const auto* V = G.ConstStrPool.find_value(NodePtr))
      return *V;
    else
      return "";
  }
};

NODE_PROPERTIES(VirtSrcDesigAccess) {
  NodeProperties(Node *N)
    : NODE_PROP_BASE(VirtSrcDesigAccess, N) {}

  operator bool() const {
    if(!NodePtr) return false;
    auto Op = NodePtr->getOp();
    return Op == IrOpcode::SrcVarAccess ||
           Op == IrOpcode::SrcArrayAccess;
  }

  Node* decl() const {
    assert(NodePtr->getNumValueInput() > 0);
    return NodePtr->getValueInput(0);
  }

  Node* effect_dependency() const {
    if(NodePtr->getNumEffectInput() > 0)
      return NodePtr->getEffectInput(0);
    else
      return nullptr;
  }
};

template<>
struct NodeProperties<IrOpcode::SrcVarAccess>
  : public NodeProperties<IrOpcode::VirtSrcDesigAccess> {
  NodeProperties(Node *N)
    : NodeProperties<IrOpcode::VirtSrcDesigAccess>(N) {}

  operator bool() const {
    return NodePtr && NodePtr->Op == IrOpcode::SrcVarAccess;
  }
};

NODE_PROPERTIES(SrcAssignStmt) {
  NodeProperties(Node *N)
    : NODE_PROP_BASE(SrcAssignStmt, N) {}

  Node* source() const {
    assert(NodePtr->getNumValueInput() > 1);
    return NodePtr->getValueInput(1);
  }
  Node* dest() const {
    assert(NodePtr->getNumValueInput() > 0);
    return NodePtr->getValueInput(0);
  }
};

NODE_PROPERTIES(SrcArrayDecl) {
  NodeProperties(Node *N)
    : NODE_PROP_BASE(SrcArrayDecl, N) {}

  const std::string& ident_name(const Graph& G) const {
    assert(NodePtr->getNumValueInput() > 0);
    auto* SymStrNode = NodePtr->getValueInput(0);
    return NodeProperties<IrOpcode::ConstantStr>(SymStrNode)
           .str(G);
  }

  size_t dim_size() const {
    assert(NodePtr->getNumValueInput() > 0);
    return NodePtr->getNumValueInput() - 1U;
  }
  Node* dim(size_t idx) const {
    assert(idx < dim_size() && "dim index out-of-bound");
    return NodePtr->getValueInput(idx + 1);
  }
};

#undef NODE_PROP_BASE
#undef NODE_PROPERTIES

template<IrOpcode::ID OP>
struct NodeBuilder {
  Node* Build() {
    gross_unreachable("Unimplemented");
    return nullptr;
  }
};

template<>
struct NodeBuilder<IrOpcode::ConstantInt> {
  NodeBuilder(Graph* graph, int32_t val)
    : G(graph), Val(val) {}

  Node* Build() {
    if(auto* N = G->ConstNumberPool.find_node(Val))
      return N;
    else {
      // New constant Node
      Node* NewN = new Node(IrOpcode::ConstantInt);
      G->ConstNumberPool.insert({NewN, Val});
      G->InsertNode(NewN);
      return NewN;
    }
  }

public:
  Graph* G;
  int32_t Val;
};

template<>
struct NodeBuilder<IrOpcode::ConstantStr> {
  NodeBuilder(Graph* graph, const std::string& Name)
    : G(graph),
      SymName(Name) {}

  Node* Build(){
    if(auto* N = G->ConstStrPool.find_node(SymName))
      return N;
    else {
      // New constant Node
      Node* NewN = new Node(IrOpcode::ConstantStr);
      G->ConstStrPool.insert({NewN, SymName});
      G->InsertNode(NewN);
      return NewN;
    }
  }

private:
  Graph *G;
  const std::string& SymName;
};

template<>
struct NodeBuilder<IrOpcode::SrcVarDecl> {
  NodeBuilder(Graph *graph)
    : G(graph) {}

  NodeBuilder& SetSymbolName(const std::string& Name) {
    SymName = Name;
    return *this;
  }

  Node* Build() {
    Node* SymNameNode = NodeBuilder<IrOpcode::ConstantStr>(G, SymName).Build();
    // Value dependency
    Node* VarDeclNode = new Node(IrOpcode::SrcVarDecl, {SymNameNode});
    SymNameNode->Users.push_back(VarDeclNode);
    G->InsertNode(VarDeclNode);
    return VarDeclNode;
  }

private:
  Graph *G;
  std::string SymName;
};

template<>
struct NodeBuilder<IrOpcode::SrcArrayDecl> {
  NodeBuilder(Graph *graph)
    : G(graph) {}

  NodeBuilder& SetSymbolName(const std::string& Name) {
    SymName = Name;
    return *this;
  }

  // add new dimension
  NodeBuilder& AddDim(Node* DN) {
    Dims.push_back(DN);
    return *this;
  }
  NodeBuilder& AddConstDim(uint32_t Dim) {
    assert(Dim > 0);
    NodeBuilder<IrOpcode::ConstantInt> NB(G, static_cast<int32_t>(Dim));
    return AddDim(NB.Build());
  }
  NodeBuilder& ResetDims() {
    Dims.clear();
    return *this;
  }

  Node* Build() {
    Node* SymNode = NodeBuilder<IrOpcode::ConstantStr>(G, SymName).Build();
    // value dependency
    // 0: symbol string
    // 1..N: dimension expression
    std::vector<Node*> ValDeps{SymNode};
    ValDeps.insert(ValDeps.end(), Dims.begin(), Dims.end());
    Node* ArrDeclNode = new Node(IrOpcode::SrcArrayDecl, ValDeps);
    for(auto* N : ValDeps)
      N->Users.push_back(ArrDeclNode);
    G->InsertNode(ArrDeclNode);
    return ArrDeclNode;
  }

private:
  Graph *G;
  std::string SymName;
  std::vector<Node*> Dims;
};

namespace _details {
// (Simple)Binary Ops
template<IrOpcode::ID OC,
         class SubT = NodeBuilder<OC>>
struct BinOpNodeBuilder {
  BinOpNodeBuilder(Graph* graph) : G(graph) {}

  SubT& LHS(Node* N) {
    LHSNode = N;
    return downstream();
  }
  SubT& RHS(Node* N) {
    RHSNode = N;
    return downstream();
  }

  Node* Build() {
    auto* BinOp = new Node(OC, {LHSNode, RHSNode});
    LHSNode->Users.push_back(BinOp);
    RHSNode->Users.push_back(BinOp);
    G->InsertNode(BinOp);
    return BinOp;
  }

protected:
  Graph *G;
  Node *LHSNode, *RHSNode;

  SubT& downstream() { return *static_cast<SubT*>(this); }
};
} // end namespace _details

#define TRIVIAL_BIN_OP_BUILDER(OC)  \
template<>  \
struct NodeBuilder<IrOpcode::OC> :  \
  public _details::BinOpNodeBuilder<IrOpcode::OC> { \
  NodeBuilder(Graph* graph) : BinOpNodeBuilder(graph) {}  \
}

TRIVIAL_BIN_OP_BUILDER(BinAdd);
TRIVIAL_BIN_OP_BUILDER(BinSub);
TRIVIAL_BIN_OP_BUILDER(BinMul);
TRIVIAL_BIN_OP_BUILDER(BinDiv);
TRIVIAL_BIN_OP_BUILDER(BinLe);
TRIVIAL_BIN_OP_BUILDER(BinLt);
TRIVIAL_BIN_OP_BUILDER(BinGe);
TRIVIAL_BIN_OP_BUILDER(BinGt);
TRIVIAL_BIN_OP_BUILDER(BinEq);
TRIVIAL_BIN_OP_BUILDER(BinNe);
#undef TRIVIAL_BIN_OP_BUILDER

template<>
struct NodeBuilder<IrOpcode::SrcVarAccess> {
  NodeBuilder(Graph* graph) :
    G(graph),
    VarDecl(nullptr), EffectDep(nullptr) {}

  NodeBuilder& Decl(Node* N) {
    VarDecl = N;
    return *this;
  }

  NodeBuilder& Effect(Node* N) {
    EffectDep = N;
    return *this;
  }

  Node* Build(bool Verify = true) {
    if(Verify) {
      NodeProperties<IrOpcode::SrcVarDecl> NP(VarDecl);
      assert(NP && "original decl should be SrcVarDecl");
    }

    std::vector<Node*> Effects;
    if(EffectDep) Effects.push_back(EffectDep);
    auto* N = new Node(IrOpcode::SrcVarAccess,
                       {VarDecl},// value inputs
                       {}/*control inputs*/,
                       Effects/*effect inputs*/);
    VarDecl->Users.push_back(N);
    if(EffectDep) EffectDep->Users.push_back(N);
    G->InsertNode(N);
    return N;
  }

private:
  Graph* G;
  Node *VarDecl, *EffectDep;
};

template<>
struct NodeBuilder<IrOpcode::SrcArrayAccess> {
  NodeBuilder(Graph *graph) :
    G(graph),
    VarDecl(nullptr), EffectDep(nullptr) {}

  NodeBuilder& Decl(Node* D) {
    VarDecl = D;
    return *this;
  }
  NodeBuilder& Effect(Node* N) {
    EffectDep = N;
    return *this;
  }

  NodeBuilder& AppendAccessDim(Node* DimExpr) {
    Dims.push_back(DimExpr);
    return *this;
  }
  NodeBuilder& ResetDims() {
    Dims.clear();
    return *this;
  }

  Node* Build(bool Verify=true) {
    if(Verify) {
      // Make sure the amount of dims
      // matches that of original array decl
      NodeProperties<IrOpcode::SrcArrayDecl> NP(VarDecl);
      assert(NP && NP.dim_size() == Dims.size() &&
             "illegal array decl");
    }

    // value dependency
    // 0: array decl
    // 1..N: dimension expression
    std::vector<Node*> ValDeps{VarDecl};
    ValDeps.insert(ValDeps.end(), Dims.begin(), Dims.end());
    std::vector<Node*> EffectDeps;
    if(EffectDep) EffectDeps.push_back(EffectDep);
    Node* ArrAccessNode = new Node(IrOpcode::SrcArrayAccess,
                                   ValDeps, // value dependencies
                                   {}, // control dependencies
                                   EffectDeps); // effect dependencies
    for(auto* N : ValDeps)
      N->Users.push_back(ArrAccessNode);
    if(EffectDep) EffectDep->Users.push_back(ArrAccessNode);
    G->InsertNode(ArrAccessNode);
    return ArrAccessNode;
  }

private:
  Graph *G;
  Node *VarDecl, *EffectDep;
  std::vector<Node*> Dims;
};

template<>
struct NodeBuilder<IrOpcode::SrcAssignStmt> {
  NodeBuilder(Graph *graph) : G(graph) {}

  NodeBuilder& Dest(Node* N) {
    DestNode = N;
    return *this;
  }
  NodeBuilder& Src(Node* N) {
    SrcNode = N;
    return *this;
  }

  Node* Build() {
    auto* N = new Node(IrOpcode::SrcAssignStmt,
                       {DestNode, SrcNode});
    DestNode->Users.push_back(N);
    SrcNode->Users.push_back(N);
    G->InsertNode(N);
    return N;
  }

private:
  Graph *G;
  Node *DestNode, *SrcNode;
};
} // end namespace gross
#endif
