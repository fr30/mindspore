/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/nllloss_cpu_kernel.h"

#include <string>
#include <unordered_map>

#include "nnacl/errorcode.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kNLLLossInputsNum = 3;
constexpr size_t kNLLLossOutputsNum = 2;
const std::unordered_map<std::string, ReductionType> kReductionMap = {
  {MEAN, Reduction_Mean}, {SUM, Reduction_Sum}, {NONE, Reduction_None}};
}  // namespace

void NLLLossCpuKernelMod::InitKernel(const CNodePtr &kernel_node) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  kernel_name_ = common::AnfAlgo::GetCNodeName(kernel_node);
  KernelAttr kernel_attr = GetKernelAttrFromNode(kernel_node);
  bool is_match = MatchKernelAttr(kernel_attr, GetOpSupport()).first;
  if (!is_match) {
    MS_LOG(EXCEPTION) << kernel_name_ << " does not support this kernel data type: " << kernel_attr;
  }

  auto logits_shape = AnfAlgo::GetInputDeviceShape(kernel_node, 0);
  auto reduction = common::AnfAlgo::GetNodeAttr<std::string>(kernel_node, REDUCTION);
  auto pair = kReductionMap.find(reduction);
  if (pair == kReductionMap.end()) {
    MS_LOG(EXCEPTION) << "For " << kernel_name_
                      << ", the attr 'reduction' only support 'mean', 'sum' and 'none', but got " << reduction;
  }

  nllloss_param_.batch_ = LongToInt(logits_shape[0]);
  nllloss_param_.class_num_ = LongToInt(logits_shape[1]);
  nllloss_param_.reduction_type_ = pair->second;
}

bool NLLLossCpuKernelMod::Launch(const std::vector<kernel::AddressPtr> &inputs,
                                 const std::vector<kernel::AddressPtr> &workspace,
                                 const std::vector<kernel::AddressPtr> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(kNLLLossInputsNum, inputs.size(), kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(kNLLLossOutputsNum, outputs.size(), kernel_name_);

  const auto *logits = reinterpret_cast<float *>(inputs[0]->addr);
  const auto *labels = reinterpret_cast<int *>(inputs[1]->addr);
  const auto *weight = reinterpret_cast<float *>(inputs[2]->addr);
  auto *loss = reinterpret_cast<float *>(outputs[0]->addr);
  auto *total_weight = reinterpret_cast<float *>(outputs[1]->addr);

  int ret = NLLLoss(logits, labels, weight, loss, total_weight, &nllloss_param_);
  if (ret != static_cast<int>(NNACL_OK)) {
    MS_LOG(EXCEPTION) << "Launch " << kernel_name_ << " failed, the nnacl error code " << ret;
  }
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, NLLLoss, NLLLossCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
