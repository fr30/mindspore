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
#include "ops/uniform_candidate_sampler.h"
#include <string>
#include <algorithm>
#include <memory>
#include <set>
#include <vector>
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
namespace {
abstract::TupleShapePtr UCSInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  const int64_t input_num = 1;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_num, op_name);
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]);
  auto input_shape_ptr = input_args[kInputIndex0]->BuildShape();
  auto input_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_shape_ptr);
  auto input_shape = input_shape_map[kShape];
  auto min_shape = input_shape_map[kMinShape];
  auto max_shape = input_shape_map[kMaxShape];

  // Check UniformCandidateSampler input shape's dimension whether equal or greater than 2.
  int64_t batch_rank = 0;
  if (primitive->HasAttr(kBatchRank)) {
    auto value_ptr = primitive->GetAttr(kBatchRank);
    batch_rank = GetValue<int64_t>(value_ptr);
  }
  const int64_t input_dim = 2;
  if (batch_rank > 0) {
    // support vmap feature
    (void)CheckAndConvertUtils::CheckInteger("dimension of input", SizeToLong(input_shape.size()), kGreaterThan,
                                             input_dim, op_name);
  } else {
    (void)CheckAndConvertUtils::CheckInteger("dimension of input", SizeToLong(input_shape.size()), kEqual, input_dim,
                                             op_name);
  }

  bool x_not_dyn = std::all_of(input_shape.begin(), input_shape.end(),
                               [](int64_t value) { return value != abstract::Shape::SHP_ANY; });
  auto true_expected_count_shape = input_shape_ptr;
  if (batch_rank == 0 && !x_not_dyn && min_shape.size() != 0 && max_shape.size() != 0) {
    true_expected_count_shape = std::make_shared<abstract::Shape>(input_shape, min_shape, max_shape);
  }

  auto num_sampled = GetValue<int64_t>(primitive->GetAttr("num_sampled"));
  std::vector<int64_t> batch_lists;
  for (int64_t i = 0; i < batch_rank; i++) {
    batch_lists.emplace_back(input_shape[i]);
  }
  batch_lists.emplace_back(num_sampled);

  abstract::ShapePtr sampled_shape_ptr = std::make_shared<abstract::Shape>(batch_lists);
  auto output_shapes = std::make_shared<abstract::TupleShape>(
    std::vector<abstract::BaseShapePtr>{sampled_shape_ptr, true_expected_count_shape, sampled_shape_ptr});
  return output_shapes;
}

TuplePtr UCSInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]);
  auto input_type = input_args[kInputIndex0]->BuildType();
  std::set<TypePtr> check_list = {kInt32, kInt64};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("true_classes", input_type, check_list, op_name);
  return std::make_shared<Tuple>(std::vector<TypePtr>{input_type, kFloat32, kFloat32});
}
}  // namespace

void UniformCandidateSampler::Init(int64_t num_true, int64_t num_sampled, bool unique, int64_t range_max, int64_t seed,
                                   bool remove_accidental_hits) {
  set_num_true(num_true);
  set_num_sampled(num_sampled);
  set_unique(unique);
  set_range_max(range_max);
  set_seed(seed);
  set_remove_accidental_hits(remove_accidental_hits);
}

void UniformCandidateSampler::set_num_true(int64_t num_true) {
  (void)this->AddAttr("num_true", api::MakeValue(num_true));
}
void UniformCandidateSampler::set_num_sampled(int64_t num_sampled) {
  (void)this->AddAttr("num_sampled", api::MakeValue(num_sampled));
}
void UniformCandidateSampler::set_unique(bool unique) { (void)this->AddAttr("unique", api::MakeValue(unique)); }
void UniformCandidateSampler::set_range_max(int64_t range_max) {
  (void)this->AddAttr("range_max", api::MakeValue(range_max));
}
void UniformCandidateSampler::set_seed(int64_t seed) { (void)this->AddAttr("seed", api::MakeValue(seed)); }
void UniformCandidateSampler::set_remove_accidental_hits(bool remove_accidental_hits) {
  (void)this->AddAttr("remove_accidental_hits", api::MakeValue(remove_accidental_hits));
}

int64_t UniformCandidateSampler::get_num_true() const {
  auto value_ptr = GetAttr("num_true");
  return GetValue<int64_t>(value_ptr);
}
int64_t UniformCandidateSampler::get_num_sampled() const {
  auto value_ptr = GetAttr("num_sampled");
  return GetValue<int64_t>(value_ptr);
}
bool UniformCandidateSampler::get_unique() const {
  auto value_ptr = GetAttr("unique");
  return GetValue<bool>(value_ptr);
}
int64_t UniformCandidateSampler::get_range_max() const {
  auto value_ptr = GetAttr("range_max");
  return GetValue<int64_t>(value_ptr);
}
int64_t UniformCandidateSampler::get_seed() const {
  auto value_ptr = GetAttr("seed");
  return GetValue<int64_t>(value_ptr);
}
bool UniformCandidateSampler::get_remove_accidental_hits() const {
  auto value_ptr = GetAttr("remove_accidental_hits");
  return GetValue<bool>(value_ptr);
}

abstract::AbstractBasePtr UniformCandidateSamplerInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &prim,
                                                       const std::vector<abstract::AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  return abstract::MakeAbstract(UCSInferShape(prim, input_args), UCSInferType(prim, input_args));
}

MIND_API_OPERATOR_IMPL(UniformCandidateSampler, BaseOperator);
// register primivtive
REGISTER_PRIMITIVE_EVAL_IMPL(UniformCandidateSampler, prim::kPrimUniformCandidateSampler, UniformCandidateSamplerInfer,
                             nullptr, true);
}  // namespace ops
}  // namespace mindspore
