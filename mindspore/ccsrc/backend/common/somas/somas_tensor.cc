/**
 * Copyright 2020 Huawei Technologies Co., Ltd

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include "backend/common/somas/somas_tensor.h"

namespace mindspore {
namespace somas {
SomasTensor::SomasTensor(size_t id, size_t source_node_id, size_t source_stream_id, size_t real_size,
                         LifeLongType lifelong_value)
    : lifelong_value_(lifelong_value),
      between_streams_(false),
      contiguous_(false),
      type_(kUnknown),
      offset_(0),
      num_constraints_(0),
      ref_overlap_(false),
      id_(id),
      source_node_id_(source_node_id),
      source_stream_id_(source_stream_id),
      original_size_(real_size) {
  const size_t alignment = 512;
  const size_t alignment_complement = 31;
  aligned_size_ = (real_size > 0) ? ((real_size + alignment + alignment_complement) / alignment) * alignment : 0;
  solver_tensor_desc_ = std::make_shared<SomasSolverTensorDesc>(id_, aligned_size_, offset_, false);
}

SomasSolverTensorDescPtr SomasTensor::GetSolverTensorDesc() {
  if (contiguous_) {
    solver_tensor_desc_->Update(id_, aligned_size_, offset_, false, num_constraints_);
  } else {
    solver_tensor_desc_->Update(id_, aligned_size_, offset_, lifelong_value_ == kLifeLongGraphAll, num_constraints_);
  }
  if (aligned_size_ == 0) {  // ignore zero-size tensors for solver
    return nullptr;
  } else {
    return solver_tensor_desc_;
  }
}
}  // namespace somas
}  // namespace mindspore
