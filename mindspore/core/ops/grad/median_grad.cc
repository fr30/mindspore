/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "ops/grad/median_grad.h"
#include <string>
#include <algorithm>
#include <memory>
#include <set>
#include <vector>
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr MedianGradInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  auto y_grad_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->BuildShape())[kShape];
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->BuildShape())[kShape];
  auto y_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[2]->BuildShape())[kShape];
  CheckAndConvertUtils::Check("y_grad shape", y_grad_shape, kEqual, y_shape, op_name, ValueError);
  return std::make_shared<abstract::Shape>(x_shape);
}

TypePtr MedianGradInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  if (std::any_of(input_args.begin(), input_args.end(), [](const AbstractBasePtr &a) { return a == nullptr; })) {
    MS_LOG(EXCEPTION) << "nullptr";
  }
  const std::set<TypePtr> valid_types = {kInt16, kInt32, kInt64, kFloat32, kFloat64};
  auto type = CheckAndConvertUtils::CheckTensorTypeValid("x", input_args[1]->BuildType(), valid_types, prim->name());
  auto type_id = type->type_id();
  TypePtr const base_type = kFloat64;
  if ((type_id == base_type->type_id() || type_id == base_type->generic_type_id() ||
       type_id == base_type->object_type() || type_id == base_type->meta_type())) {
    return std::make_shared<TensorType>(kFloat64);
  }
  return std::make_shared<TensorType>(kFloat32);
}
}  // namespace

MIND_API_OPERATOR_IMPL(MedianGrad, BaseOperator);
AbstractBasePtr MedianGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t input_num = 4;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto infer_type = MedianGradInferType(primitive, input_args);
  auto infer_shape = MedianGradInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}
REGISTER_PRIMITIVE_EVAL_IMPL(MedianGrad, prim::kPrimMedianGrad, MedianGradInfer, nullptr, true);
}  // namespace ops
}  // namespace mindspore
