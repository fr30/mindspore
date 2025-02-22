/**
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CCSRC_PIPELINE_PYNATIVE_PYNATIVE_EXECUTE_H_
#define MINDSPORE_CCSRC_PIPELINE_PYNATIVE_PYNATIVE_EXECUTE_H_

#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <stack>
#include <set>
#include <map>

#include "utils/hash_map.h"
#include "utils/hash_set.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/numpy.h"
#include "pybind_api/ir/base_ref_py.h"
#include "ir/anf.h"
#include "frontend/optimizer/ad/kpynative.h"
#include "frontend/operator/composite/composite.h"
#include "pipeline/jit/resource.h"
#include "pipeline/pynative/base.h"
#include "pipeline/pynative/pynative_cache.h"
#include "utils/ms_context.h"

namespace mindspore::pynative {
namespace py = pybind11;
using OpInfoWithTensorId = mindspore::HashMap<std::string, std::vector<std::string>>;
using TensorIdWithTensorObject = mindspore::HashMap<std::string, std::vector<tensor::TensorPtr>>;
using OpInfoWithMsFuncForwardTensors = mindspore::HashMap<std::string, std::vector<tensor::TensorPtr>>;
using CellIdWithBackwardHookOp = mindspore::HashMap<std::string, std::vector<AnfNodePtr>>;

py::object RealRunOp(const py::args &args);

struct GraphInfo {
  GraphInfo() = default;
  ~GraphInfo() = default;
  std::string cell_id;
  AnfNodePtr output;
  OrderedMap<std::string, ParameterPtr> params;  // hold input parameters and cell weights
  mindspore::HashMap<std::string, std::pair<AnfNodePtr, std::vector<int64_t>>> node_map;
  explicit GraphInfo(std::string id) : cell_id(std::move((id))) {}
};
using GraphInfoPtr = std::shared_ptr<GraphInfo>;

struct DynamicShapeInfo {
  bool HasFeedDynamicInput() const { return !feed_dynamic_input.empty(); }
  OrderedMap<std::string, abstract::AbstractBasePtr> obj_id_with_dynamic_output_abs;
  void reset() { obj_id_with_dynamic_output_abs.clear(); }
  mindspore::HashMap<std::string, std::vector<abstract::AbstractBasePtr>> feed_dynamic_input;
};
using DynamicShapeInfoPtr = std::shared_ptr<DynamicShapeInfo>;

struct CellSelfInfo {
  CellSelfInfo() = default;
  ~CellSelfInfo() = default;
  CellSelfInfo(std::string cell_self_id, std::vector<std::string> args_id, std::vector<abstract::ShapePtr> args_shape,
               std::vector<TypePtr> args_type)
      : cell_self_id(std::move(cell_self_id)),
        args_id(std::move(args_id)),
        args_shape(std::move(args_shape)),
        args_type(std::move(args_type)) {}

  std::string cell_self_id;
  std::vector<std::string> args_id;
  std::vector<abstract::ShapePtr> args_shape;
  std::vector<TypePtr> args_type;
};
using CellSelfInfoPtr = std::shared_ptr<CellSelfInfo>;

class TopCellInfo {
 public:
  TopCellInfo() = default;
  ~TopCellInfo() = default;
  TopCellInfo(bool topest, size_t grad_order, pipeline::ResourcePtr r, FuncGraphPtr fg, FuncGraphPtr df,
              std::string cellid, std::string already_run_cell_id)
      : is_topest_(topest),
        grad_order_(grad_order),
        resource_(std::move(r)),
        fg_(std::move(fg)),
        df_builder_(std::move(df)),
        cell_id_(std::move(cellid)),
        already_run_cell_id_(std::move(already_run_cell_id)) {}

  bool is_init_kpynative() const { return is_init_kpynative_; }
  void set_init_kpynative(bool init) { is_init_kpynative_ = init; }
  bool is_topest() const { return is_topest_; }
  size_t grad_order() const { return grad_order_; }
  void set_grad_order(size_t grad_order) { grad_order_ = grad_order; }
  bool dynamic_graph_structure() const { return dynamic_graph_structure_; }
  void set_dynamic_graph_structure(bool dynamic_graph_structure) { dynamic_graph_structure_ = dynamic_graph_structure; }
  bool dynamic_shape() const { return dynamic_shape_; }
  void set_dynamic_shape(bool dynamic_shape) { dynamic_shape_ = dynamic_shape; }
  bool hook_changed() const { return hook_changed_; }
  void set_hook_changed(bool hook_changed) { hook_changed_ = hook_changed; }
  void set_sub_cell_hook_changed(const std::string &sub_cell) { (void)sub_cell_hook_changed_.emplace(sub_cell); }
  const CellIdWithBackwardHookOp &cell_backward_hook_op() const { return cell_backward_hook_op_; }
  void RecordCellBackwardHookOp(const std::string &cell_order, const AnfNodePtr &hook_op);
  void ClearCellHookOp() { cell_backward_hook_op_.clear(); }
  bool vm_compiled() const { return vm_compiled_; }
  void set_vm_compiled(bool vm_compiled) { vm_compiled_ = vm_compiled; }
  bool ms_function_flag() const { return ms_function_flag_; }
  void set_ms_function_flag(bool ms_function_flag) { ms_function_flag_ = ms_function_flag; }
  bool need_compile_graph() const { return need_compile_graph_; }
  void set_need_compile_graph(bool need_compile_graph) { need_compile_graph_ = need_compile_graph; }
  bool forward_already_run() const { return forward_already_run_; }
  void set_forward_already_run(bool set_forward_already_run) { forward_already_run_ = set_forward_already_run; }
  void set_is_dynamic_structure(bool is_dynamic_structure) { is_dynamic_structure_ = is_dynamic_structure; }
  bool is_dynamic_structure() const { return is_dynamic_structure_; }
  pipeline::ResourcePtr resource() const { return resource_; }
  FuncGraphPtr df_builder() const { return df_builder_; }
  FuncGraphPtr fg() const { return fg_; }
  void set_fg(const FuncGraphPtr &fg) { fg_ = fg; }
  size_t op_num() const { return op_num_; }
  void set_op_num(size_t op_num) { op_num_ = op_num; }
  const std::string &cell_id() const { return cell_id_; }
  void set_cell_id(const std::string &cell_id) { cell_id_ = cell_id; }
  const std::string &already_run_cell_id() const { return already_run_cell_id_; }
  void set_already_run_cell_id(const std::string &already_run_cell_id) { already_run_cell_id_ = already_run_cell_id; }
  const std::string &input_args_id() const { return input_args_id_; }
  void set_input_args_id(const std::string &input_args_id) { input_args_id_ = input_args_id; }
  const std::string &all_op_info() const { return all_op_info_; }
  void set_all_op_info(const std::string &all_op_info) { all_op_info_ = all_op_info; }
  const std::string &grad_operation() const { return grad_operation_; }
  void set_grad_operation(const std::string &grad_operation) { grad_operation_ = grad_operation; }
  const abstract::AbstractBasePtr &last_output_abs() const { return last_output_abs_; }
  void set_last_output_abs(const abstract::AbstractBasePtr &last_output_abs) { last_output_abs_ = last_output_abs; }
  CellSelfInfoPtr cell_self_info() const { return cell_self_info_; }
  void set_cell_self_info(const CellSelfInfoPtr &cell_self_info) { cell_self_info_ = cell_self_info; }
  void SetCellSelfInfoForTopCell(const py::object &cell, const py::args &args);
  void EraseFromSubCellList(const std::string &cell_id) { (void)sub_cell_list_.erase(cell_id); }
  void SetSubCellList(const std::string &cell_id) { (void)sub_cell_list_.emplace(cell_id); }
  const mindspore::HashSet<std::string> &sub_cell_list() const { return sub_cell_list_; }
  bool IsSubCell(const std::string &cell_id) const;
  void CheckSubCellHookChanged();
  void SetGraphInfoMap(const FuncGraphPtr &fg, const GraphInfoPtr &graph_info) { graph_info_map_[fg] = graph_info; }
  const OrderedMap<FuncGraphPtr, GraphInfoPtr> &graph_info_map() const { return graph_info_map_; }
  void SetOpInfoWithTensorId(const std::string &op_info, const std::string &tensor_id) {
    (void)op_info_with_tensor_id_[op_info].emplace_back(tensor_id);
  }
  const OpInfoWithTensorId &op_info_with_tensor_id() const { return op_info_with_tensor_id_; }
  const TensorIdWithTensorObject &tensor_id_with_tensor_object() const { return tensor_id_with_tensor_object_; }
  void SetTensorIdWithTensorObject(const std::string &tensor_id, const tensor::TensorPtr &out_tensor) {
    (void)tensor_id_with_tensor_object_[tensor_id].emplace_back(out_tensor);
  }
  ad::KPynativeCellPtr k_pynative_cell_ptr() const { return k_pynative_cell_ptr_; }
  void set_k_pynative_cell_ptr(const ad::KPynativeCellPtr &k_pynative_cell_ptr) {
    k_pynative_cell_ptr_ = k_pynative_cell_ptr;
  }
  const OpInfoWithMsFuncForwardTensors &op_info_with_ms_func_forward_tensors() const {
    return op_info_with_ms_func_forward_tensors_;
  }
  void set_op_info_with_ms_func_forward_tensors(const std::string &op_info,
                                                const std::vector<tensor::TensorPtr> &forward_tensors) {
    op_info_with_ms_func_forward_tensors_[op_info] = forward_tensors;
  }
  void ClearDeviceMemory();
  void Clear();

 private:
  bool is_topest_{false};
  bool dynamic_graph_structure_{false};
  bool dynamic_shape_{false};
  bool vm_compiled_{false};
  bool hook_changed_{false};
  bool ms_function_flag_{false};
  bool is_init_kpynative_{false};
  bool forward_already_run_{false};
  bool need_compile_graph_{false};
  bool is_dynamic_structure_{false};
  size_t op_num_{0};
  size_t grad_order_{0};
  pipeline::ResourcePtr resource_{nullptr};
  FuncGraphPtr fg_{nullptr};
  FuncGraphPtr df_builder_{nullptr};
  ad::KPynativeCellPtr k_pynative_cell_ptr_{nullptr};
  std::string cell_id_;
  std::string already_run_cell_id_;
  std::string input_args_id_;
  std::string all_op_info_;
  std::string grad_operation_;
  abstract::AbstractBasePtr last_output_abs_;
  CellSelfInfoPtr cell_self_info_{nullptr};
  OrderedMap<FuncGraphPtr, GraphInfoPtr> graph_info_map_;
  mindspore::HashSet<std::string> sub_cell_list_;
  // Record `register hook` or `remove hook` function has been called by sub cell
  // The record range between the begin and end of top cell.
  mindspore::HashSet<std::string> sub_cell_hook_changed_;
  // Record backward hook ops for each cell object.
  // Each cell object has two backward hook ops.
  CellIdWithBackwardHookOp cell_backward_hook_op_;
  OpInfoWithTensorId op_info_with_tensor_id_;
  TensorIdWithTensorObject tensor_id_with_tensor_object_;
  OpInfoWithMsFuncForwardTensors op_info_with_ms_func_forward_tensors_;
};
using TopCellInfoPtr = std::shared_ptr<TopCellInfo>;

class ForwardExecutor;
using ForwardExecutorPtr = std::shared_ptr<ForwardExecutor>;
using ForwardExecutorWeakPtr = std::weak_ptr<ForwardExecutor>;

class GradExecutor;
using GradExecutorPtr = std::shared_ptr<GradExecutor>;
using GradExecutorWeakPtr = std::weak_ptr<GradExecutor>;

class GradExecutor {
 public:
  GradExecutor() = default;
  ~GradExecutor() = default;
  explicit GradExecutor(const ForwardExecutorPtr &forward_executor = nullptr)
      : forward_executor_(ForwardExecutorWeakPtr(forward_executor)) {}

  std::function<void(const py::object *, const py::object &, const py::args &)> InitGraph = [this](auto &&PH1,
                                                                                                   auto &&PH2,
                                                                                                   auto &&PH3) {
    NewGraphInner(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), std::forward<decltype(PH3)>(PH3));
  };
  std::function<void(const py::object *, const py::object &, const py::object &, const py::args &)> LinkGraph =
    [this](auto &&PH1, auto &&PH2, auto &&PH3, auto &&PH4) {
      EndGraphInner(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2),
                    std::forward<decltype(PH3)>(PH3), std::forward<decltype(PH4)>(PH4));
    };
  std::function<void(const py::object *, const prim::GradOperationPtr &, const py::object &, const py::object &,
                     const py::object &, const py::args &)>
    GradGraph = [this](auto &&PH1, auto &&PH2, auto &&PH3, auto &&PH4, auto &&PH5, auto &&PH6) {
      GradNetInner(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), std::forward<decltype(PH3)>(PH3),
                   std::forward<decltype(PH4)>(PH4), std::forward<decltype(PH5)>(PH5),
                   std::forward<decltype(PH6)>(PH6));
    };
  std::function<void(py::object *, const py::object &, const py::object &sens_param, const py::tuple &)> RunGraph =
    [this](auto &&PH1, auto &&PH2, auto &&PH3, auto &&PH4) {
      RunGradGraph(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), std::forward<decltype(PH3)>(PH3),
                   std::forward<decltype(PH4)>(PH4));
    };

  FuncGraphPtr curr_g() const;
  const TopCellInfoPtr &top_cell() const;
  void CheckNeedCompileGraph();
  void PushHighOrderGraphStack(const TopCellInfoPtr &top_cell);
  size_t GetHighOrderStackSize() const { return high_order_stack_.size(); }
  TopCellInfoPtr GetTopCell(const string &already_run_cell_id);
  std::string GetCurCellOrder() const;
  void EnableOpGraphCache(bool is_enable);
  void SetHookChanged(const py::object &cell);
  bool need_renormalize() const { return need_renormalize_; }
  bool enable_op_cache() const { return enable_op_cache_; }
  bool grad_is_running() const { return grad_is_running_; }
  void set_top_cell(TopCellInfoPtr top_cell) { top_cell_ = std::move(top_cell); }
  bool grad_flag() const { return grad_flag_; }
  void set_grad_flag(bool flag) { grad_flag_ = flag; }
  void set_graph_phase(const std::string &graph_phase) { graph_phase_ = graph_phase; }
  bool in_cell_with_custom_bprop_() const { return custom_bprop_cell_count_ > 0; }
  AnfNodePtr GetInput(const py::object &obj, bool op_mask) const;
  std::string GetCellId(const py::object &cell, const py::args &args) const;
  void RecordGradOpInfo(const OpExecInfoPtr &op_exec_info) const;
  bool need_construct_graph() const { return !cell_stack_.empty() && grad_flag_; }
  // Construct grad graph for ms_function
  bool eliminate_forward() const { return eliminate_forward_; }
  void set_eliminate_forward(bool eliminate_forward) { eliminate_forward_ = eliminate_forward; }
  py::object GradMsFunction(const py::object &out, const py::args &args);
  void GradMsFunctionInner(const std::string &phase, const py::object &out, const py::args &args,
                           const FuncGraphPtr &ms_func_graph, const FuncGraphPtr &grad_graph);
  void SaveDynShapeAbsForMsFunction(const py::args &args, const py::object &out,
                                    const FuncGraphPtr &ms_func_graph) const;
  void UpdateMsFunctionForwardTensors(const OpExecInfoPtr &op_exec_info, const ValuePtr &new_forward_value) const;
  CNodePtr MakeAdjointForMsFunction(const FuncGraphPtr &ms_func_graph, const FuncGraphPtr &grad_graph,
                                    const py::object &actual_out, const py::args &args,
                                    const ValuePtr &actual_out_v) const;
  void MakeCNodeForMsFunction(const FuncGraphPtr &ms_func_graph, const py::args &args, ValuePtrList *input_values,
                              CNodePtr *ms_function_cnode) const;
  void SaveOutputNodeMap(const std::string &obj_id, const py::object &out_real, const CNodePtr &cnode);
  void DoOpGrad(const OpExecInfoPtr &op_exec_info, const CNodePtr &cnode, const ValuePtr &op_out);
  // Update forward tensors info
  void UpdateForwardTensorInfoInBpropGraph(const string &op_info, const ValuePtr &op_out);
  void SaveForwardTensorInfoInBpropGraph(const pipeline::ResourcePtr &resource) const;
  py::object CheckGraph(const py::object &cell, const py::args &args);
  void ChangeTopCellInfo(const TopCellInfoPtr &top_cell, size_t args_size);
  TopCellInfoPtr ChangeTopCellToDynamicShapeByAuto(const TopCellInfoPtr &top_cell,
                                                   const std::vector<ShapeVector> &new_args_shape,
                                                   const py::object &cell, const py::args &args);
  TopCellInfoPtr ChangeTopCellToDynamicShapeBySetInputs(const TopCellInfoPtr &top_cell,
                                                        const std::vector<ShapeVector> &new_args_shape,
                                                        const py::object &cell);
  void UpdateTopCellId(const py::args &args);
  TopCellInfoPtr GetTopCellWithDynamicShape(const py::object &cell, const py::args &args, bool is_auto);
  void CheckPreviousTopCellCanBeDynamicShape(const py::object &cell, const py::args &args);
  void RunGradGraph(py::object *ret, const py::object &cell, const py::object &sens_param, const py::tuple &args);
  py::object CheckAlreadyRun(const prim::GradOperationPtr &grad, const py::object &cell, const py::args &args);
  void EraseTopCellFromTopCellList(const TopCellInfoPtr &top_cell);
  void ClearGrad(const py::object &cell, const py::args &args);
  void ClearRes();
  void ClearCellRes(const std::string &cell_id = "");

 private:
  ForwardExecutorPtr forward() const;
  // Higher derivative
  inline bool IsNestedGrad() const;
  void SwitchTopcell();
  void DoParameterReplace(const FuncGraphPtr &first_grad_fg, const py::tuple &forward_args,
                          std::vector<AnfNodePtr> *inputs, ValuePtrList *weights_args);
  void MakeNestedCnode(const py::object &cell, const py::tuple &forward_args, const pipeline::ResourcePtr &resource,
                       const py::object &out);
  void PushCellStack(const std::string &cell_id);
  void PopCellStack();
  TopCellInfoPtr PopHighOrderGraphStack();
  void HandleInputArgsForTopCell(const py::args &args, bool is_bprop_top);
  void InitResourceAndDfBuilder(const std::string &cell_id, const py::object &cell, const py::args &args);
  void MakeNewTopGraph(const string &cell_id, const py::object &cell, const py::args &args, bool is_topest);
  void UpdateTopCellInfo(bool forward_already_run, bool need_compile_graph, bool vm_compiled) const;
  // Manage resource when run grad process.
  bool IsBpropGraph(const std::string &cell_id);
  bool IsCellObjIdEq(const std::string &l_cell_id, const std::string &r_cell_id) const;
  void DumpGraphIR(const std::string &filename, const FuncGraphPtr &graph);
  void NewGraphInner(const py::object *ret, const py::object &cell, const py::args &args);
  void EndGraphInner(const py::object *ret, const py::object &cell, const py::object &out, const py::args &args);
  void SetForwardLastNodeInfo(const py::object &out) const;
  ValuePtr GetSensValueForDynamicShapeOutput(const py::object &out, const AnfNodePtr &node) const;
  void UpdateSensValueForDynamicShapeOutput(const py::object &out) const;
  void DoGradForCustomBprop(const py::object &cell, const py::object &out, const py::args &args);
  std::string GetAlreadyRunCellId(const std::string &cell_id);
  std::string GetGradCellId(bool has_sens, const py::object &cell, const py::args &args);
  void GradNetInner(const py::object *ret, const prim::GradOperationPtr &grad, const py::object &cell,
                    const py::object &weights, const py::object &grad_position, const py::args &args);
  FuncGraphPtr GetBpropGraph(const prim::GradOperationPtr &grad, const py::object &cell,
                             const std::vector<AnfNodePtr> &weights, const std::vector<size_t> &grad_position,
                             size_t arg_size, const py::args &args);
  std::vector<AnfNodePtr> GetWeightsArgs(const py::object &weights, const FuncGraphPtr &df_builder) const;
  void CheckParamShapeAndType(const AnfNodePtr &param, const ParameterPtr &param_node,
                              const abstract::AbstractBasePtr &input_abs,
                              const abstract::AbstractBasePtr &param_tensor_abs, const std::string &input_shape);
  void UpdateParamAbsByArgs(const py::list &args, const FuncGraphPtr &bprop_graph);
  std::vector<size_t> GetGradPositionArgs(const py::object &grad_position) const;
  void ShallowCopySensValue(const py::tuple &input_args, bool has_sens, VectorRef *run_args) const;
  // Manage resource for construct forward graph.
  const std::string &graph_phase() const { return graph_phase_; }
  AnfNodePtr GetObjNode(const py::object &obj, const std::string &obj_id) const;
  AnfNodePtr MakeValueNode(const py::object &obj, const std::string &obj_id) const;
  AnfNodePtr CreateMakeTupleNode(const py::object &obj, const std::string &obj_id) const;
  AnfNodePtr CreateTupleGetItemNode(const std::string &obj_id) const;
  void SetTupleItemArgsToGraphInfoMap(const FuncGraphPtr &g, const py::object &args, const AnfNodePtr &node,
                                      const std::vector<int64_t> &index_sequence, bool is_param = false) const;
  void SetTupleArgsToGraphInfoMap(const FuncGraphPtr &g, const py::object &args, const AnfNodePtr &node,
                                  bool is_param = false) const;
  void SetParamNodeMapInGraphInfoMap(const FuncGraphPtr &g, const std::string &id, const ParameterPtr &param) const {
    auto &graph_info = top_cell()->graph_info_map().at(g);
    MS_EXCEPTION_IF_NULL(graph_info);
    graph_info->params[id] = param;
  }
  void SetNodeMapInGraphInfoMap(const FuncGraphPtr &g, const std::string &id, const AnfNodePtr &node,
                                int64_t index = -1) const {
    auto &graph_info = top_cell()->graph_info_map().at(g);
    MS_EXCEPTION_IF_NULL(graph_info);
    graph_info->node_map[id] = std::make_pair(node, std::vector<int64_t>{index});
  }
  void SetNodeMapInGraphInfoMap(const FuncGraphPtr &g, const std::string &id, const AnfNodePtr &node,
                                const std::vector<int64_t> &index) const {
    auto &graph_info = top_cell()->graph_info_map().at(g);
    MS_EXCEPTION_IF_NULL(graph_info);
    graph_info->node_map[id] = std::make_pair(node, index);
  }
  void MarkMsFunctionNodes(const pipeline::ResourcePtr &resource);

  bool grad_flag_{false};
  bool enable_op_cache_{true};
  bool grad_is_running_{false};
  bool need_renormalize_{false};
  bool eliminate_forward_{true};
  int custom_bprop_cell_count_{0};
  size_t cell_order_{0};
  size_t grad_order_{0};
  size_t top_cell_switch_counts_{0};

  // The graph phase is used to obtain backend graph that is complied by ms_function
  std::string graph_phase_;
  // The cell run check graph which will be top cell
  std::string check_graph_cell_id_;
  std::string grad_operation_;
  TopCellInfoPtr top_cell_{nullptr};
  // Records forwrad cell, the bottom is top cell
  std::stack<std::string> cell_stack_;
  // Stores parameter in ms_function
  std::vector<std::string> ms_function_params_;
  // For high grad of bprop
  std::stack<std::pair<std::string, bool>> bprop_grad_stack_;
  std::vector<std::string> bprop_cell_list_;
  // For high grad order
  std::stack<TopCellInfoPtr> high_order_stack_;
  // Use vector for keep order
  std::vector<TopCellInfoPtr> top_cell_list_;
  // Record all top cell which has been ran
  mindspore::HashMap<std::string, TopCellInfoPtr> already_run_top_cell_;
  // Use vector for keep order
  ForwardExecutorWeakPtr forward_executor_;
};

class ForwardExecutor {
 public:
  ForwardExecutor() = default;
  ~ForwardExecutor() = default;

  std::function<void(py::object *, const OpExecInfoPtr &)> RunOpS = [this](auto &&PH1, auto &&PH2) {
    RunOpInner(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
  };

  void RunOpInner(py::object *ret, const OpExecInfoPtr &op_exec_info);
  OpExecInfoPtr GenerateOpExecInfo(const py::args &args) const;
  void set_grad_executor(const GradExecutorPtr &grad_executor) { grad_executor_ = GradExecutorWeakPtr(grad_executor); }
  void ClearNodeAbsMap() { node_abs_map_.clear(); }
  void EraseFromNodeAbsMap(const std::string &id) { (void)node_abs_map_.erase(id); }
  void SetNodeAbsMap(const std::string &id, const abstract::AbstractBasePtr &abs) { node_abs_map_[id] = abs; }
  const mindspore::HashMap<std::string, abstract::AbstractBasePtr> &node_abs_map() const { return node_abs_map_; }
  void ClearRes();
  CNodePtr ConstructForwardGraph(const OpExecInfoPtr &op_exec_info);
  // Replace input hook node with its input node when not in its own cell scope.
  AnfNodePtr GetRealInputNodeBySkipHook(const AnfNodePtr &input_node) const;
  void set_lazy_build(bool lazy_build) { lazy_build_ = lazy_build; }
  DynamicShapeInfoPtr dynamic_shape_info_ptr();
  void SetDynamicInput(const py::object &cell, const py::args &args);
  void SetFeedDynamicInputAbs(const py::object &cell, const py::args &args);
  void ResetDynamicAbsMap();
  py::object GetDynamicInput(const py::object &actual_input);
  bool IsFirstCell() const { return forward_cell_stack_.empty(); }
  void PushForwardCell(const py::object &cell) { forward_cell_stack_.push(cell); }
  void PopForwardCell() { forward_cell_stack_.pop(); }

 private:
  GradExecutorPtr grad() const;
  MsBackendPolicy GetBackendPolicy(const OpExecInfoPtr &op_exec_info) const;
  py::tuple RunOpWithInitBackendPolicy(const OpExecInfoPtr &op_exec_info);
  void RunMixedPrecisionCastOp(const OpExecInfoPtr &op_exec_info, py::object *ret);
  py::object RunOpInVM(const OpExecInfoPtr &op_exec_info);
  py::object RunOpInMs(const OpExecInfoPtr &op_exec_info);
  py::object RunOpWithBackendPolicy(MsBackendPolicy backend_policy, const OpExecInfoPtr &op_exec_info);
  void SetNonCostantValueAbs(const AbstractBasePtr &abs, const std::string &id);
  AbstractBasePtr GetTupleInputAbstract(const OpExecInfoPtr &op_exec_info, const py::object &obj, const std::string &id,
                                        size_t input_index);
  AbstractBasePtr GetInputObjAbstract(const OpExecInfoPtr &op_exec_info, size_t i, const py::object &obj);
  void GetInputsArgsSpec(const OpExecInfoPtr &op_exec_info, abstract::AbstractBasePtrList *args_spec_list);
  void GetOpOutputAbstract(const OpExecInfoPtr &op_exec_info, const abstract::AbstractBasePtrList &args_spec_list,
                           bool *prim_cache_hit);
  void GetOpOutput(const OpExecInfoPtr &op_exec_info, const abstract::AbstractBasePtrList &args_spec_list,
                   const CNodePtr &cnode, bool prim_cache_hit, py::object *ret);
  void DoNopOutput(const OpExecInfoPtr &op_exec_info, ValuePtr *out_real_value) const;
  // Mix precision and Implicit transform
  void SetCastForInputs(const OpExecInfoPtr &op_exec_info);
  void SetTensorMixPrecisionCast(const OpExecInfoPtr &op_exec_info);
  void SetImplicitCast(const OpExecInfoPtr &op_exec_info);
  py::object DoParamMixPrecisionCast(bool *is_cast, const py::object &obj, const std::string &op_name, size_t index);
  py::object DoParamMixPrecisionCastTuple(bool *is_cast, const py::tuple &tuple, const std::string &op_name,
                                          size_t index);
  py::object DoAutoCastTuple(const py::tuple &tuple, const TypeId &type_id, const std::string &op_name, size_t index);
  py::object DoAutoCast(const py::object &arg, const TypeId &type_id, const std::string &op_name, size_t index);
  void DoSignatureCast(const PrimitivePyPtr &prim, const mindspore::HashMap<SignatureEnumDType, TypeId> &dst_type,
                       const std::vector<SignatureEnumDType> &dtypes, const OpExecInfoPtr &op_exec_info);
  void CheckIfNeedSyncForHeterogeneous(const std::string &cur_target);
  void SaveOutputDynamicShape(const OpExecInfoPtr &op_exec_info, const AbstractBasePtr &real_abs,
                              const py::object &obj);

 private:
  bool lazy_build_{false};
  std::string last_target_{"Unknown"};
  GradExecutorWeakPtr grad_executor_;
  DynamicShapeInfoPtr dynamic_shape_info_ptr_{nullptr};
  std::stack<py::object> forward_cell_stack_;
  PrimAbsCache prim_abs_list_;
  ImplicitCastCache implicit_cast_map_;
  mindspore::HashMap<std::string, abstract::AbstractBasePtr> node_abs_map_;
};

class PynativeExecutor : public std::enable_shared_from_this<PynativeExecutor> {
 public:
  static std::shared_ptr<PynativeExecutor> GetInstance() {
    std::lock_guard<std::mutex> i_lock(instance_lock_);
    if (executor_ == nullptr) {
      executor_ = std::shared_ptr<PynativeExecutor>(new (std::nothrow) PynativeExecutor());
      forward_executor_ = std::make_shared<ForwardExecutor>();
      grad_executor_ = std::make_shared<GradExecutor>(forward_executor_);
      forward_executor_->set_grad_executor(grad_executor_);
    }
    return executor_;
  }
  ~PynativeExecutor() = default;
  PynativeExecutor(const PynativeExecutor &) = delete;
  PynativeExecutor &operator=(const PynativeExecutor &) = delete;
  GradExecutorPtr grad_executor() const;
  ForwardExecutorPtr forward_executor() const;

  bool grad_flag() const;
  void set_grad_flag(bool flag) const;
  void SetDynamicInput(const py::object &cell, const py::args &args);
  py::object GetDynamicInput(const py::object &actual_input) const;
  void set_graph_phase(const std::string &graph_phase);
  void set_py_exe_path(const py::object &py_exe_path);
  void set_kernel_build_server_dir(const py::object &kernel_build_server_dir);
  void SetHookChanged(const py::object &cell) const;
  void NewGraph(const py::object &cell, const py::args &args);
  void EndGraph(const py::object &cell, const py::object &out, const py::args &args);
  void GradNet(const prim::GradOperationPtr &grad, const py::object &cell, const py::object &weights,
               const py::object &grad_position, const py::args &args);
  py::object GradMsFunction(const py::object &out, const py::args &args);
  py::object CheckGraph(const py::object &cell, const py::args &args);
  py::object CheckAlreadyRun(const prim::GradOperationPtr &grad, const py::object &cell, const py::args &args);
  void set_grad_position(const prim::GradOperationPtr &grad, const py::object &grad_position);
  py::object Run(const py::object &cell, const py::object &sens_param, const py::tuple &args);

  // Used by graph clean
  // Cell destruct will call
  void ClearCell(const py::object &cell);
  void ClearGrad(const py::object &cell, const py::args &args);
  // Abnormal existed
  void ClearRes();
  // Sync stream
  void Sync();
  void SetLazyBuild(bool enable);
  void ExecuteLazyTask();
  bool IsFirstCell() const;

 private:
  PynativeExecutor() = default;

  static std::shared_ptr<PynativeExecutor> executor_;
  static std::mutex instance_lock_;
  static ForwardExecutorPtr forward_executor_;
  static GradExecutorPtr grad_executor_;
};

using PynativeExecutorPtr = std::shared_ptr<PynativeExecutor>;
}  // namespace mindspore::pynative
#endif  // MINDSPORE_CCSRC_PIPELINE_PYNATIVE_PYNATIVE_EXECUTE_H_
