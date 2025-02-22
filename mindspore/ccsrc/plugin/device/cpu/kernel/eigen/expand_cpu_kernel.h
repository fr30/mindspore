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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_EXPAND_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_EXPAND_CPU_KERNEL_H_

#include <string>
#include <vector>
#include <memory>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class ExpandCpuKernelMod : public DeprecatedNativeCpuKernelMod {
 public:
  ExpandCpuKernelMod() = default;
  ~ExpandCpuKernelMod() override = default;

  void InitKernel(const CNodePtr &kernel_node) override;

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs) override;

  size_t get_element_num(const std::vector<size_t> &shape);

  template <typename T>
  bool ExpandCompute(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &outputs);

  template <size_t RANK, typename T>
  bool ExpandCalculate(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &outputs);

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  std::string kernel_name_;
  std::vector<size_t> input_x_shape_;
  TypeId input_x_dtype_{kNumberTypeFloat32};
  std::vector<size_t> input_shape_;
  std::vector<size_t> output_y_shape_;
  std::vector<size_t> input_x_bcast_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_EXPAND_CPU_KERNEL_H_
