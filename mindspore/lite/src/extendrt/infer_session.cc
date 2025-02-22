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
#include "extendrt/infer_session.h"

#include "extendrt/single_op_session.h"
#include "plugin/factory/ms_factory.h"
#include "kernel/common_utils.h"
// #include "backend/common/session/session_basic.h"
#include "backend/graph_compiler/graph_partition.h"
#include "plugin/device/cpu/kernel/cpu_kernel_mod.h"
#include "extendrt/utils/kernel_graph_utils.h"
#include "include/common/utils/anfalgo.h"
#include "backend/common/session/anf_runtime_algorithm.h"
#include "extendrt/delegate/factory.h"
#include "extendrt/delegate/graph_executor/factory.h"
#include "extendrt/session/factory.h"

namespace mindspore {
static const std::vector<PrimitivePtr> ms_infer_cut_list = {prim::kPrimReturn,   prim::kPrimPartial,
                                                            prim::kPrimSwitch,   prim::kPrimMakeTuple,
                                                            prim::kPrimBpropCut, prim::kPrimSwitchLayer};
static bool is_infer_single_op = true;
static bool is_use_lite_session = false;

class DefaultInferSession : public InferSession {
 public:
  DefaultInferSession() = default;
  virtual ~DefaultInferSession() = default;
  Status Init(const std::shared_ptr<Context> context) override;
  Status CompileGraph(FuncGraphPtr graph, const void *data = nullptr, size_t size = 0) override;
  Status RunGraph() override;
  Status RunGraph(const std::vector<tensor::TensorPtr> &inputs, std::vector<tensor::TensorPtr> *outputs) override;
  Status Resize(const std::vector<tensor::TensorPtr> &inputs, const std::vector<std::vector<int64_t>> &dims) override;

  std::vector<tensor::TensorPtr> GetOutputs() override;
  std::vector<tensor::TensorPtr> GetInputs() override;
  std::vector<std::string> GetOutputNames() override;
  std::vector<std::string> GetInputNames() override;
  tensor::TensorPtr GetOutputByTensorName(const std::string &tensorName) override;
  tensor::TensorPtr GetInputByTensorName(const std::string &name) override;

 private:
  KernelGraphUtilsPtr kernel_graph_utils_;
  KernelGraphPtr kernel_graph_;
  std::vector<KernelGraphPtr> kernel_graphs_;
};

Status DefaultInferSession::Init(const std::shared_ptr<Context> context) {
  MS_LOG(INFO) << "DefaultInferSession::Init";
  kernel_graph_utils_ = std::make_shared<mindspore::KernelGraphUtils>();
  partition_ = std::make_shared<compile::GraphPartition>(ms_infer_cut_list, "ms");
  return kSuccess;
}
Status DefaultInferSession::CompileGraph(FuncGraphPtr graph, const void *data, size_t size) {
  MS_LOG(INFO) << "DefaultInferSession::CompileGraph";
  return kSuccess;
}

Status DefaultInferSession::RunGraph() { return kSuccess; }
Status DefaultInferSession::RunGraph(const std::vector<tensor::TensorPtr> &inputs,
                                     std::vector<tensor::TensorPtr> *outputs) {
  return kSuccess;
}
Status DefaultInferSession::Resize(const std::vector<tensor::TensorPtr> &inputs,
                                   const std::vector<std::vector<int64_t>> &dims) {
  return kSuccess;
}
std::vector<tensor::TensorPtr> DefaultInferSession::GetOutputs() { return std::vector<tensor::TensorPtr>(); }
std::vector<tensor::TensorPtr> DefaultInferSession::GetInputs() { return std::vector<tensor::TensorPtr>(); }
std::vector<std::string> DefaultInferSession::GetOutputNames() { return std::vector<std::string>(); }
std::vector<std::string> DefaultInferSession::GetInputNames() { return std::vector<std::string>(); }
tensor::TensorPtr DefaultInferSession::GetOutputByTensorName(const std::string &tensorName) { return nullptr; }
tensor::TensorPtr DefaultInferSession::GetInputByTensorName(const std::string &name) { return nullptr; }
std::shared_ptr<InferSession> InferSession::CreateSession(const std::shared_ptr<Context> context) {
  auto config = SelectSessionArg(context);
  return SessionRegistry::GetInstance()->GetSession(config.type_, config);
}

SessionConfig InferSession::SelectSessionArg(const std::shared_ptr<Context> &context) {
  SessionConfig config;
  config.context_ = context;
  if (context != nullptr) {
    if (context->GetDelegate() != nullptr) {
      config.delegates_.emplace_back(context->GetDelegate());
    }
    auto delegate_config = std::make_shared<mindspore::DelegateConfig>(context);
    auto &device_contexts = context->MutableDeviceInfo();
    for (auto device_context : device_contexts) {
      // delegate init
      MS_EXCEPTION_IF_NULL(device_context);
      // get graph executor delegate
      auto delegate = mindspore::DelegateRegistry::GetInstance()->GetDelegate(
        device_context->GetDeviceType(), device_context->GetProvider(), delegate_config);
      if (delegate == nullptr) {
        continue;
      }
      config.delegates_.emplace_back(delegate);
    }
  }

  if (!config.delegates_.empty()) {
    // create delegate session object
    config.type_ = kDelegateSession;
    return config;
  }
  if (is_infer_single_op) {
    config.type_ = kSingleOpSession;
    return config;
  }
  if (is_use_lite_session) {
    config.type_ = kLiteInferSession;
    return config;
  }
  config.type_ = kDefaultSession;
  return config;
}

static std::shared_ptr<InferSession> DefaultSessionCreator(const SessionConfig &config) {
  return std::make_shared<DefaultInferSession>();
}
REG_SESSION(kDefaultSession, DefaultSessionCreator);
}  // namespace mindspore
