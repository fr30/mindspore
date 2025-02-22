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

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "src/extendrt/single_op_session.h"
#include "src/extendrt/infer_device_address.h"

#include "plugin/factory/ms_factory.h"
#include "include/common/utils/anfalgo.h"
#include "backend/common/session/anf_runtime_algorithm.h"
#include "kernel/common_utils.h"
#include "plugin/device/cpu/kernel/cpu_kernel_mod.h"
#include "src/extendrt/utils/kernel_build_utils.h"
#include "src/extendrt/kernel/ascend/plugin/ascend_kernel_plugin.h"
#include "extendrt/session/factory.h"
#include "extendrt/utils/runtime_utils.h"

namespace mindspore {
const size_t tensor_max_size = 0x1000000;

Status SingleOpInferSession::Init(const std::shared_ptr<Context> context) {
  MS_LOG(INFO) << "SingleOpInferSession::Init";
  MS_EXCEPTION_IF_NULL(context);
  kernel_graph_utils_ = std::make_shared<mindspore::KernelGraphUtils>();
  auto device_list = context->MutableDeviceInfo();
  for (const auto &device : device_list) {
    MS_EXCEPTION_IF_NULL(device);
    if (device->GetDeviceType() == DeviceType::kAscend) {
      kernel::AscendKernelPlugin::GetInstance().Register();
    }
  }
  return kSuccess;
}

Status SingleOpInferSession::CompileGraph(FuncGraphPtr graph, const void *data, size_t size) {
  MS_LOG(INFO) << "SingleOpInferSession::CompileGraph";
  std::vector<KernelGraphPtr> all_out_graph;
  kernel_graph_ = kernel_graph_utils_->ConstructKernelGraph(graph, &all_out_graph, mindspore::device::DeviceType::kCPU);
  MS_EXCEPTION_IF_NULL(kernel_graph_);

  auto &nodes = kernel_graph_->nodes();
  for (const auto &node : nodes) {
    std::string node_name = common::AnfAlgo::GetCNodeName(node);
    MS_LOG(INFO) << "SingleOpInferSession::Nodes " << node_name;
  }

  auto &kernel_nodes = kernel_graph_->execution_order();
  for (const auto &kernel_node : kernel_nodes) {
    mindspore::infer::SetKernelInfo(kernel_node);
    std::string kernel_name = common::AnfAlgo::GetCNodeName(kernel_node);
    std::shared_ptr<kernel::KernelMod> kernel_mod = kernel::Factory<kernel::KernelMod>::Instance().Create(kernel_name);
    if (kernel_mod == nullptr) {
      MS_LOG(EXCEPTION) << "Kernel mod is nullptr, kernel name: " << kernel_name;
    }
    MS_LOG(INFO) << "SingleOpInferSession::Kernels " << kernel_name;
    auto args = kernel::AbstractArgsFromCNode(kernel_node);
    mindspore::infer::CopyInputWeights(kernel_node, args.inputs);
    auto ret = kernel_mod->Init(args.op, args.inputs, args.outputs);
    MS_LOG(INFO) << "SingleOpInferSession::Kernels ret " << ret;
    if (!ret) {
      MS_LOG(EXCEPTION) << "kernel init failed " << kernel_name;
    }
    if (kernel_mod->Resize(args.op, args.inputs, args.outputs, kernel::GetKernelDepends(kernel_node)) ==
        kernel::KRET_RESIZE_FAILED) {
      MS_LOG(EXCEPTION) << "CPU kernel op [" << kernel_node->fullname_with_scope() << "] Resize failed.";
    }

    std::vector<size_t> input_size_list;
    std::vector<size_t> output_size_list;
    input_size_list.clear();
    output_size_list.clear();
    size_t input_num = common::AnfAlgo::GetInputTensorNum(kernel_node);
    for (size_t input_index = 0; input_index < input_num; ++input_index) {
      TypeId type_id = AnfAlgo::GetInputDeviceDataType(kernel_node, input_index);
      size_t type_size = GetTypeByte(TypeIdToType(type_id));
      auto shape = AnfAlgo::GetInputDeviceShape(kernel_node, input_index);
      size_t tensor_size =
        shape.empty() ? type_size : std::accumulate(shape.begin(), shape.end(), type_size, std::multiplies<size_t>());
      tensor_size = std::max(tensor_size, type_size);
      (void)input_size_list.emplace_back(tensor_size);
    }
    size_t output_num = common::AnfAlgo::GetOutputTensorNum(kernel_node);
    for (size_t output_index = 0; output_index < output_num; ++output_index) {
      TypeId type_id = AnfAlgo::GetOutputDeviceDataType(kernel_node, output_index);
      size_t type_size = GetTypeByte(TypeIdToType(type_id));
      auto shape = AnfAlgo::GetOutputDeviceShape(kernel_node, output_index);
      size_t tensor_size =
        shape.empty() ? type_size : std::accumulate(shape.begin(), shape.end(), type_size, std::multiplies<size_t>());
      tensor_size = std::max(tensor_size, type_size);
      (void)output_size_list.emplace_back(tensor_size);
    }
    kernel_mod->SetInputSizeList(input_size_list);
    kernel_mod->SetOutputSizeList(output_size_list);

    AnfAlgo::SetKernelMod(kernel_mod, kernel_node.get());
  }

  RuntimeUtils::AssignKernelGraphAddress(kernel_graph_);

  kernel_graph_utils_->GetModelInputsInfo(kernel_graph_->graph_id(), &inputs_, &input_names_);
  kernel_graph_utils_->GetModelOutputsInfo(kernel_graph_->graph_id(), &outputs_, &output_names_);

  return kSuccess;
}

Status SingleOpInferSession::RunGraph() { return kSuccess; }
Status SingleOpInferSession::RunGraph(const std::vector<tensor::TensorPtr> &inputs,
                                      std::vector<tensor::TensorPtr> *outputs) {
  MS_LOG(INFO) << "SingleOpInferSession::RunGraph with input and outputs";
  MS_EXCEPTION_IF_NULL(kernel_graph_);

  RuntimeUtils::CopyInputTensorsToKernelGraph(inputs, kernel_graph_);

  auto &kernel_nodes = kernel_graph_->execution_order();
  for (const auto &kernel_node : kernel_nodes) {
    std::string kernel_name = common::AnfAlgo::GetCNodeName(kernel_node);
    MS_LOG(INFO) << "SingleOpInferSession::RunGraph " << kernel_name;
    auto kernel_mod = AnfAlgo::GetKernelMod(kernel_node);
    MS_EXCEPTION_IF_NULL(kernel_mod);
    std::vector<kernel::AddressPtr> kernel_inputs;
    size_t input_num = common::AnfAlgo::GetInputTensorNum(kernel_node);
    for (size_t i = 0; i < input_num; ++i) {
      auto device_address = AnfAlgo::GetPrevNodeMutableOutputAddr(kernel_node, i);
      auto input = RuntimeUtils::GetAddressFromDevice(device_address);
      kernel_inputs.push_back(input);
    }
    std::vector<kernel::AddressPtr> kernel_outputs;
    size_t output_num = common::AnfAlgo::GetOutputTensorNum(kernel_node);
    for (size_t i = 0; i < output_num; ++i) {
      auto device_address = AnfAlgo::GetMutableOutputAddr(kernel_node, i);
      auto output = RuntimeUtils::GetAddressFromDevice(device_address);
      kernel_outputs.push_back(output);
    }
    std::vector<kernel::AddressPtr> kernel_workspaces;
    bool ret = true;
    try {
      ret = kernel_mod->Launch(kernel_inputs, kernel_workspaces, kernel_outputs, 0);
    } catch (std::exception &e) {
      MS_LOG(EXCEPTION) << e.what();
    }
    if (!ret) {
      MS_LOG(EXCEPTION) << "Launch kernel failed.";
    }
  }

  RuntimeUtils::CopyOutputTensorsFromKernelGraph(outputs, kernel_graph_);
  outputs_ = *outputs;

  return kSuccess;
}
Status SingleOpInferSession::Resize(const std::vector<tensor::TensorPtr> &inputs,
                                    const std::vector<std::vector<int64_t>> &dims) {
  return kSuccess;
}
std::vector<tensor::TensorPtr> SingleOpInferSession::GetOutputs() { return outputs_; }
std::vector<tensor::TensorPtr> SingleOpInferSession::GetInputs() { return inputs_; }
std::vector<std::string> SingleOpInferSession::GetOutputNames() { return output_names_; }
std::vector<std::string> SingleOpInferSession::GetInputNames() { return input_names_; }
tensor::TensorPtr SingleOpInferSession::GetOutputByTensorName(const std::string &tensorName) {
  for (size_t idx = 0; idx < output_names_.size(); ++idx) {
    if (output_names_[idx] == tensorName) {
      if (idx < outputs_.size()) {
        return outputs_[idx];
      }
    }
  }
  MS_LOG(ERROR) << "Can't found tensor name " << tensorName;
  return nullptr;
}
tensor::TensorPtr SingleOpInferSession::GetInputByTensorName(const std::string &name) { return nullptr; }

static std::shared_ptr<InferSession> SingleOpSessionCreator(const SessionConfig &config) {
  return std::make_shared<SingleOpInferSession>();
}
REG_SESSION(kSingleOpSession, SingleOpSessionCreator);
}  // namespace mindspore
