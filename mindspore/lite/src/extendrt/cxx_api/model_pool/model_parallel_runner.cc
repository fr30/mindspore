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
#include "include/api/model_parallel_runner.h"
#include "src/extendrt/cxx_api/model_pool/model_pool.h"
#include "src/extendrt/cxx_api/model_pool/runner_config.h"
#include "src/common/log_adapter.h"
#include "src/litert/cpu_info.h"
namespace mindspore {
namespace {
constexpr size_t kMaxSectionNum = 100;
constexpr size_t kMaxConfigNumPerSection = 1000;
}  // namespace
#ifdef USE_GLOG
extern "C" {
extern void mindspore_log_init();
}
#endif

RunnerConfig::RunnerConfig() : data_(std::make_shared<Data>()) {}

void RunnerConfig::SetWorkersNum(int32_t workers_num) { data_->workers_num = workers_num; }

void RunnerConfig::SetContext(const std::shared_ptr<Context> &context) { data_->context = context; }

int32_t RunnerConfig::GetWorkersNum() const { return data_->workers_num; }

std::shared_ptr<Context> RunnerConfig::GetContext() const { return data_->context; }

void RunnerConfig::SetConfigInfo(const std::vector<char> &section,
                                 const std::map<std::vector<char>, std::vector<char>> &config) {
  if (data_->config_info.size() > kMaxSectionNum) {
    return;
  }
  if (config.size() > kMaxConfigNumPerSection) {
    return;
  }
  data_->config_info[CharToString(section)] = MapVectorCharToString(config);
  return;
}

std::map<std::vector<char>, std::map<std::vector<char>, std::vector<char>>> RunnerConfig::GetConfigInfoChar() const {
  return MapMapStringToChar(data_->config_info);
}

Status ModelParallelRunner::Init(const std::vector<char> &model_path,
                                 const std::shared_ptr<RunnerConfig> &runner_config) {
#ifdef USE_GLOG
  mindspore::mindspore_log_init();
#endif
  if (!PlatformInstructionSetSupportCheck()) {
    return kLiteNotSupport;
  }
  model_pool_ = std::make_shared<ModelPool>();
  if (model_pool_ == nullptr) {
    MS_LOG(ERROR) << "model pool is nullptr.";
    return kLiteNullptr;
  }
  auto status = model_pool_->InitByPath(CharToString(model_path), runner_config);
  if (status != kSuccess) {
    MS_LOG(ERROR) << "model runner init failed.";
    return kLiteError;
  }
  return status;
}

Status ModelParallelRunner::Init(const void *model_data, size_t data_size,
                                 const std::shared_ptr<RunnerConfig> &runner_config) {
#ifdef USE_GLOG
  mindspore::mindspore_log_init();
#endif
  if (!PlatformInstructionSetSupportCheck()) {
    return kLiteNotSupport;
  }
  model_pool_ = std::make_shared<ModelPool>();
  if (model_pool_ == nullptr) {
    MS_LOG(ERROR) << "model pool is nullptr.";
    return kLiteNullptr;
  }
  auto status = model_pool_->InitByBuf(static_cast<const char *>(model_data), data_size, runner_config);
  if (status != kSuccess) {
    MS_LOG(ERROR) << "model runner init failed.";
    return kLiteError;
  }
  return status;
}

std::vector<MSTensor> ModelParallelRunner::GetInputs() { return model_pool_->GetInputs(); }

std::vector<MSTensor> ModelParallelRunner::GetOutputs() { return model_pool_->GetOutputs(); }

Status ModelParallelRunner::Predict(const std::vector<MSTensor> &inputs, std::vector<MSTensor> *outputs,
                                    const MSKernelCallBack &before, const MSKernelCallBack &after) {
  if (outputs == nullptr) {
    MS_LOG(ERROR) << "predict output is nullptr.";
    return kLiteNullptr;
  }
  auto status = model_pool_->Predict(inputs, outputs, before, after);
  if (status != kSuccess) {
    MS_LOG(ERROR) << "model runner predict failed.";
    return status;
  }
  return kSuccess;
}
}  // namespace mindspore
