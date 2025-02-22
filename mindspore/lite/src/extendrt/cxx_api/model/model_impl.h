/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_LITE_SRC_EXTENDRT_CXX_API_MODEL_MODEL_IMPL_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_CXX_API_MODEL_MODEL_IMPL_H_
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include "include/api/context.h"
#include "include/api/model.h"
#include "include/api/graph.h"
#include "include/api/serialization.h"
#include "extendrt/cxx_api/graph/graph_data.h"
#include "include/common/utils/utils.h"
#include "ir/func_graph.h"
#include "extendrt/infer_session.h"
namespace mindspore {
class ModelImpl {
 public:
  ModelImpl() : graph_(nullptr), session_(nullptr), context_(nullptr) {}
  ~ModelImpl() = default;

  Status Build(const void *model_data, size_t data_size, ModelType model_type,
               const std::shared_ptr<Context> &model_context);
  Status Build(const std::string &model_path, ModelType model_type, const std::shared_ptr<Context> &model_context);
  Status Resize(const std::vector<MSTensor> &inputs, const std::vector<std::vector<int64_t>> &dims);
  bool HasPreprocess();
  Status Preprocess(const std::vector<std::vector<MSTensor>> &inputs, std::vector<MSTensor> *outputs);
  Status Predict(const std::vector<MSTensor> &inputs, std::vector<MSTensor> *outputs);
  Status Predict();
  Status PredictWithPreprocess(const std::vector<std::vector<MSTensor>> &inputs, std::vector<MSTensor> *outputs);
  std::vector<MSTensor> GetInputs();
  std::vector<MSTensor> GetOutputs();
  MSTensor GetInputByTensorName(const std::string &name);
  std::vector<std::string> GetOutputTensorNames();
  MSTensor GetOutputByTensorName(const std::string &name);

 private:
  friend class Model;
  friend class Serialization;
  std::shared_ptr<Graph> graph_ = nullptr;
  std::shared_ptr<InferSession> session_ = nullptr;
  std::shared_ptr<Context> context_ = nullptr;
};
}  // namespace mindspore
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_CXX_API_MODEL_MODEL_IMPL_H_
