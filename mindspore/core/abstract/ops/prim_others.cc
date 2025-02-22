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

#include <string>

#include "ir/dtype.h"
#include "utils/ms_utils.h"
#include "abstract/param_validator.h"
#include "abstract/ops/infer_functions.h"
#include "abstract/utils.h"
#include "utils/anf_utils.h"
#include "utils/ms_context.h"
#include "utils/symbolic.h"
#include "utils/shape_utils.h"
#include "ops/real_div.h"
#include "ops/add.h"
#include "ops/mul.h"
#include "ops/sub.h"
#include "ops/square.h"
#include "ops/assign.h"

namespace {
constexpr auto kRankSize = "rank_size";
inline void CheckSparseShape(ShapeVector sparse_shp, ShapeVector dense_shp) {
  constexpr auto kCSRMulBatchPos = 2;
  int dlen = mindspore::SizeToInt(sparse_shp.size()) - mindspore::SizeToInt(dense_shp.size());
  if (dlen < 0) {
    MS_EXCEPTION(mindspore::ValueError) << "Currently, only support dense tensor broadcast to sparse tensor, "
                                        << "but sparse tensor has " << sparse_shp.size() << " dimensions, "
                                        << "and dense tensor has " << dense_shp.size() << " dimensions, ";
  }
  for (int i = 0; i < dlen; i++) {
    (void)dense_shp.insert(dense_shp.begin(), 1);
  }
  if (sparse_shp.size() != dense_shp.size()) {
    MS_LOG(EXCEPTION) << "Failure: sparse_shp.size() != dense_shp.size().";
  }
  if (sparse_shp.size() < 1) {
    MS_LOG(EXCEPTION) << "Failure: dense tensor and sparse tensor shapes cannot be zero.";
  }
  for (size_t i = 0; i < sparse_shp.size(); i++) {
    auto s = sparse_shp[i];
    auto d = dense_shp[i];
    if (i < kCSRMulBatchPos) {
      if (d != s && d != 1) {
        MS_EXCEPTION(mindspore::ValueError) << "Dense shape cannot broadcast to sparse shape.";
      }
    } else {
      if (d != s) {
        MS_EXCEPTION(mindspore::ValueError)
          << "Currently, sparse shape and dense shape must equal in feature dimensions.";
      }
    }
  }
}
inline void CheckSparseShape(const size_t shape_size, const size_t expected_dim, const std::string &arg_name) {
  if (shape_size != expected_dim) {
    MS_EXCEPTION(mindspore::ValueError) << arg_name << " must be a " << expected_dim
                                        << "-dimensional tensor, but got a " << shape_size << "-dimensional tensor.";
  }
}
inline void CheckSparseIndicesDtype(const mindspore::TypePtr data_type, const std::string &arg_name) {
  if (!(data_type->equal(mindspore::kInt16) || data_type->equal(mindspore::kInt32) ||
        data_type->equal(mindspore::kInt64))) {
    MS_EXCEPTION(mindspore::TypeError) << "The dtype of " << arg_name << " must be Int16 or Int32 or Int64, but got "
                                       << data_type->ToString() << ".";
  }
}
inline void CheckSparseIndicesDtypeInt32(const mindspore::TypePtr data_type, const std::string &arg_name) {
  if (!data_type->equal(mindspore::kInt32)) {
    MS_EXCEPTION(mindspore::TypeError) << "The dtype of " << arg_name << " only support Int32 for now, but got "
                                       << data_type->ToString() << ".";
  }
}
}  // namespace

namespace mindspore {
namespace abstract {
constexpr auto kCSRDenseShape = "dense_shape";
constexpr auto kCSRAxis = "axis";
constexpr auto kCSRAvgRows = "csr_avg_rows";
constexpr auto kIsCSR = "is_csr";

AbstractBasePtr InferImplIdentity(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                  const AbstractBasePtrList &args_spec_list) {
  // An object of a subclass of AbstractBase
  CheckArgsSize(primitive->name(), args_spec_list, 1);
  return args_spec_list[0];
}

AbstractBasePtr InferImplEnvironCreate(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                       const AbstractBasePtrList &args_spec_list) {
  // args: None.
  CheckArgsSize(primitive->name(), args_spec_list, 0);
  static const AbstractBasePtr abs_env = std::make_shared<AbstractScalar>(kAnyValue, std::make_shared<EnvType>());
  return abs_env;
}

AbstractBasePtr InferImplEnvironGet(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const AbstractBasePtrList &args_spec_list) {
  MS_EXCEPTION_IF_NULL(primitive);
  // args: Three objects of a subclass of AbstractBase, env, key, default_value(default).
  CheckArgsSize(primitive->name(), args_spec_list, kSizeThree);
  auto key = args_spec_list[kIndexOne];
  auto default_value = args_spec_list[kIndexTwo];
  TypePtr type = key->GetTypeTrack();
  MS_EXCEPTION_IF_NULL(type);
  if (type->type_id() != kObjectTypeSymbolicKeyType) {
    MS_LOG(EXCEPTION) << "EnvironGet evaluator args[1] should be a SymbolicKeyInstance but: " << key->ToString();
  }

  MS_LOG(DEBUG) << "key: " << key->ToString() << ", value: " << default_value->ToString();
  if (default_value->isa<AbstractTensor>() && EnvSetSparseResultMgr::GetInstance().Get()) {
    auto tensor_value = default_value->cast<AbstractTensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor_value);
    return std::make_shared<AbstractUndetermined>(tensor_value->element()->Clone(), tensor_value->shape()->Clone());
  }

  if (!key->GetValueTrack()->isa<SymbolicKeyInstance>()) {
    return default_value;
  }
  ValuePtr key_value_ptr = key->GetValueTrack();
  MS_EXCEPTION_IF_NULL(key_value_ptr);
  auto key_value_track = key_value_ptr->cast<SymbolicKeyInstancePtr>();
  auto expected = key_value_track->abstract();
  MS_EXCEPTION_IF_NULL(expected);
  (void)expected->Join(default_value);
  // If expected is AbstractRef, return it's AbstractTensor as Value type other than Reference type.
  if (expected->isa<AbstractRefTensor>()) {
    const auto &abs_ref = expected->cast<AbstractRefPtr>();
    MS_EXCEPTION_IF_NULL(abs_ref);
    return abs_ref->CloneAsTensor();
  }
  return expected;
}

AbstractBasePtr InferImplEnvironSet(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const AbstractBasePtrList &args_spec_list) {
  // args: Three objects of a subclass of AbstractBase, env, key, value.
  CheckArgsSize(primitive->name(), args_spec_list, kSizeThree);

  auto key = args_spec_list[kIndexOne];
  ValuePtr key_value_ptr = key->GetValueTrack();
  MS_EXCEPTION_IF_NULL(key_value_ptr);
  auto key_value_track = key_value_ptr->cast<SymbolicKeyInstancePtr>();
  if (key_value_track == nullptr) {
    MS_LOG(EXCEPTION) << "EnvironSet evaluator args[1] expected should be able to cast to SymbolicKeyInstancePtrbut: "
                      << key_value_ptr->ToString();
  }
  auto expected = key_value_track->abstract();
  MS_EXCEPTION_IF_NULL(expected);

  auto value = args_spec_list[kIndexTwo];
  MS_LOG(DEBUG) << "key: " << key->ToString() << ", value: " << value->ToString();
  if (value->isa<AbstractUndetermined>() && !value->isa<AbstractTensor>()) {
    EnvSetSparseResultMgr::GetInstance().Set(true);
  }
  return std::make_shared<AbstractScalar>(kAnyValue, std::make_shared<EnvType>());
}

AbstractBasePtr InferImplEnvironAdd(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const AbstractBasePtrList &args_spec_list) {
  // args: Three objects of a subclass of AbstractBase, env, key, dflt(default).
  CheckArgsSize(primitive->name(), args_spec_list, 2);
  return std::make_shared<AbstractScalar>(kAnyValue, std::make_shared<EnvType>());
}

AbstractBasePtr InferImplEnvironDestroyAll(const AnalysisEnginePtr &, const PrimitivePtr &,
                                           const AbstractBasePtrList &) {
  return std::make_shared<abstract::AbstractScalar>(kAnyValue, std::make_shared<Bool>());
}

AbstractBasePtr InferImplStateSetItem(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                      const AbstractBasePtrList &args_spec_list) {
  // args: Two objects of a subclass of AbstractBase, key and value.
  CheckArgsSize(primitive->name(), args_spec_list, 2);

  TypePtr type = args_spec_list[0]->GetTypeTrack();
  MS_EXCEPTION_IF_NULL(type);
  if (type->type_id() != kObjectTypeRefKey && type->type_id() != kObjectTypeSymbolicKeyType) {
    MS_LOG(EXCEPTION) << "First input of StateSetItem should be a RefKey or SymbolicKeyType but a " << type->ToString();
  }
  return std::make_shared<AbstractScalar>(kAnyValue, kBool);
}

AbstractBasePtr InferImplDepend(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                const AbstractBasePtrList &args_spec_list) {
  CheckArgsSize(primitive->name(), args_spec_list, 2);

  // If the dependent has a value, just return depended node.
  // If depended node is not Any, the dependent maybe eliminated.
  auto dependant_abstract = args_spec_list[1];
  auto dependant_value = dependant_abstract->BuildValue();
  MS_EXCEPTION_IF_NULL(dependant_value);
  if (dependant_value != kAnyValue) {
    return args_spec_list[0];
  }
  auto depends = args_spec_list[0];

  if (depends->isa<AbstractRefTensor>()) {
    auto abs_ref = depends->cast<AbstractRefPtr>();
    auto tensor_abs = abs_ref->ref();
    MS_EXCEPTION_IF_NULL(tensor_abs);
    return std::make_shared<AbstractRefTensor>(tensor_abs->Broaden()->cast<AbstractTensorPtr>(),
                                               abs_ref->ref_key_value());
  }

  auto depends_abs = depends->Broaden();  // Avoid eliminating the dependent node.
  if (!MsContext::GetInstance()->get_param<bool>(MS_CTX_GRAD_FOR_SCALAR)) {
    // For scalar, need to set value to kAnyValue, because broaden scalar will not change the value.
    if (depends_abs->isa<AbstractScalar>()) {
      depends_abs->set_value(kAnyValue);
    }
  }
  return depends_abs;
}

AbstractBasePtr InferImplUpdateState(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                     const AbstractBasePtrList &args_spec_list) {
  if (args_spec_list.empty()) {
    MS_LOG(EXCEPTION) << primitive->name() << " input args size should be at least 1, but got 0";
  }
  MS_EXCEPTION_IF_NULL(args_spec_list[0]);
  return args_spec_list[0]->Broaden();
}

AbstractBasePtr InferImplMakeRowTensor(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                       const AbstractBasePtrList &args_spec_list) {
  // Inputs: two tensors and a tuple.
  const std::string op_name = primitive->name();
  constexpr size_t size_expected = 3;
  CheckArgsSize(op_name, args_spec_list, size_expected);
  auto indices = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  auto values = CheckArg<AbstractTensor>(op_name, args_spec_list, 1);
  auto dense_shape = CheckArg<AbstractTuple>(op_name, args_spec_list, 2);

  auto indices_dtype = indices->element()->BuildType();
  if (!indices_dtype->isa<Int>()) {
    MS_EXCEPTION(TypeError) << "The dtype of indices must be a Int, but got " << indices_dtype->ToString();
  }
  auto indices_shp = indices->shape()->shape();
  if (indices_shp.size() != 1) {
    MS_EXCEPTION(TypeError) << "Indices must be a 1 dimension tensor, but got a " << indices_shp.size()
                            << " dimension tensor";
  }
  auto values_shp = values->shape()->shape();
  if (indices_shp[0] != values_shp[0]) {
    MS_EXCEPTION(TypeError) << "The first dimension of indices must be the same with the first dimension of values "
                            << values_shp[0] << ", but got " << indices_shp[0];
  }

  for (const auto &elem_type : dense_shape->ElementsType()) {
    if (!elem_type->isa<Int>()) {
      MS_EXCEPTION(TypeError) << "The element type of dense_shape must be Int, but got " << elem_type->ToString();
    }
  }
  auto dense_shape_value = dense_shape->BuildValue();
  MS_EXCEPTION_IF_NULL(dense_shape_value);
  auto dense_shape_valuetuple = dense_shape_value->cast<ValueTuplePtr>();
  MS_EXCEPTION_IF_NULL(dense_shape_valuetuple);
  auto shp = dense_shape_valuetuple->value();
  ShapeVector dense_shape_vec;
  (void)std::transform(std::begin(shp), std::end(shp), std::back_inserter(dense_shape_vec),
                       [](const ValuePtr &e) -> int64_t {
                         auto elem = GetValue<int64_t>(e);
                         return elem;
                       });
  if (dense_shape_vec.size() != values_shp.size()) {
    MS_EXCEPTION(TypeError) << "The size of dense_shape must be the same with the dimension of values "
                            << values_shp.size() << ", but got " << dense_shape_valuetuple->size();
  }
  for (size_t i = 0; i < dense_shape_vec.size(); i++) {
    if (dense_shape_vec[i] < 0) {
      MS_EXCEPTION(TypeError) << "The " << i << "th element of dense_shape must be positive, but got "
                              << dense_shape_vec[i];
    }
    // The 0th mode might be less or exceed dense_shape[0] due to duplicated selection
    if (i != 0 && dense_shape_vec[i] != values_shp[i]) {
      MS_EXCEPTION(TypeError) << "The " << i << "th element of dense_shape must be same with the " << i
                              << "th dimension of values " << values_shp[i] << ", but got " << dense_shape_vec[i];
    }
  }
  auto ret = std::make_shared<AbstractRowTensor>(values->element()->BuildType(), dense_shape_vec);
  ret->set_indices(indices);
  ret->set_values(values);
  ret->set_dense_shape(dense_shape);
  return ret;
}

AbstractBasePtr InferImplRowTensorGetValues(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                            const AbstractBasePtrList &args_spec_list) {
  // Inputs: two tensors and a tuple.
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto row_tensor = CheckArg<AbstractRowTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(row_tensor->values());
  return row_tensor->values();
}

AbstractBasePtr InferImplRowTensorGetIndices(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                             const AbstractBasePtrList &args_spec_list) {
  // Inputs: two tensors and a tuple.
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto row_tensor = CheckArg<AbstractRowTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(row_tensor->indices());
  return row_tensor->indices();
}

AbstractBasePtr InferImplRowTensorGetDenseShape(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                const AbstractBasePtrList &args_spec_list) {
  // Inputs: two tensors and a tuple.
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto row_tensor = CheckArg<AbstractRowTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(row_tensor->dense_shape());
  return row_tensor->dense_shape();
}

AbstractBasePtr InferImplRowTensorAdd(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                      const AbstractBasePtrList &args_spec_list) {
  // Inputs: row tensor and tensor.
  const std::string op_name = primitive->name();
  constexpr size_t args_size = 2;
  CheckArgsSize(op_name, args_spec_list, args_size);
  auto row_tensor = CheckArg<AbstractRowTensor>(op_name, args_spec_list, 0);
  auto tensor = CheckArg<AbstractTensor>(op_name, args_spec_list, 1);
  MS_EXCEPTION_IF_NULL(row_tensor->dense_shape());
  MS_EXCEPTION_IF_NULL(tensor->shape());
  return args_spec_list[0];
}

AbstractBasePtr InferImplMakeCOOTensor(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                       const AbstractBasePtrList &args_spec_list) {
  // Inputs: two tensors and a tuple.
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, kSizeThree);
  auto indices = CheckArg<AbstractTensor>(op_name, args_spec_list, kIndexZero);
  auto values = CheckArg<AbstractTensor>(op_name, args_spec_list, kIndexOne);
  auto dense_shape = CheckArg<AbstractTuple>(op_name, args_spec_list, kIndexTwo);

  auto indices_dtype = indices->element()->BuildType();
  CheckSparseIndicesDtype(indices_dtype, "Indices");

  auto indices_shp = indices->shape()->shape();
  CheckSparseShape(indices_shp.size(), kSizeTwo, "Indices");

  auto values_shp = values->shape()->shape();
  CheckSparseShape(values_shp.size(), kSizeOne, "Values");

  if (indices_shp[kIndexZero] != values_shp[kIndexZero]) {
    MS_EXCEPTION(ValueError) << "For COOTensor, `indices.shape[" << kIndexZero << "]` must be equal to `values.shape["
                             << kIndexZero << "]`, but got `indices.shape[" << kIndexZero
                             << "]`: " << indices_shp[kIndexZero] << " and `values.shape[" << kIndexZero
                             << "]`: " << values_shp[kIndexZero];
  }
  constexpr int64_t kDimTwo = 2;
  if (indices_shp[kIndexOne] != kDimTwo) {
    MS_EXCEPTION(ValueError) << "For COOTensor, `indices.shape[" << kIndexOne << "]` must be " << kDimTwo << ",but got "
                             << indices_shp[kIndexOne];
  }

  for (const auto &elem_type : dense_shape->ElementsType()) {
    if (!elem_type->isa<Int>()) {
      MS_EXCEPTION(TypeError) << "For COOTensor, the element type of `shape` must be Int, but got "
                              << elem_type->ToString();
    }
  }
  auto dense_shape_value = dense_shape->BuildValue()->cast<ValueTuplePtr>();
  MS_EXCEPTION_IF_NULL(dense_shape_value);
  auto shp = dense_shape_value->value();
  auto min_elem = *std::min_element(std::begin(shp), std::end(shp));
  if (min_elem <= 0) {
    MS_EXCEPTION(ValueError) << "For COOTensor, the element of `shape` must be positive integer. But got " << min_elem
                             << "int it";
  }
  ShapeVector dense_shape_vec;
  (void)std::transform(std::begin(shp), std::end(shp), std::back_inserter(dense_shape_vec),
                       [](const ValuePtr &e) -> int64_t {
                         auto elem = GetValue<int64_t>(e);
                         return elem;
                       });
  if (LongToSize(indices_shp[kIndexOne]) != dense_shape_vec.size()) {
    MS_EXCEPTION(TypeError) << "For COOTensor, `indices.shape[" << indices_shp << "]` must be equal to the second "
                            << "dimension of `indices`: " << dense_shape_vec.size() << " but got "
                            << indices_shp[kIndexOne];
  }
  for (auto dense_shape_elem : dense_shape_vec) {
    if (dense_shape_elem <= 0) {
      MS_EXCEPTION(TypeError) << "For COOTensor, the element of `shape` must be positive, but got "
                              << dense_shape_value->ToString();
    }
  }
  AbstractBasePtrList element_list{indices, values, dense_shape};
  return std::make_shared<abstract::AbstractCOOTensor>(element_list);
}

AbstractBasePtr InferImplCOOTensorGetValues(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                            const AbstractBasePtrList &args_spec_list) {
  // Inputs: two tensors and a tuple.
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto sparse_tensor = CheckArg<AbstractCOOTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(sparse_tensor->values());
  return sparse_tensor->values();
}

AbstractBasePtr InferImplCOOTensorGetIndices(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                             const AbstractBasePtrList &args_spec_list) {
  // Inputs: two tensors and a tuple.
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto sparse_tensor = CheckArg<AbstractCOOTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(sparse_tensor->indices());
  return sparse_tensor->indices();
}

AbstractBasePtr InferImplCOOTensorGetDenseShape(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                const AbstractBasePtrList &args_spec_list) {
  // Inputs: two tensors and a tuple.
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto sparse_tensor = CheckArg<AbstractCOOTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(sparse_tensor->shape());
  return sparse_tensor->shape();
}

ShapeVector ConvertToShapeVector(const AbstractTuplePtr &shape) {
  auto shape_value = shape->BuildValue()->cast<ValueTuplePtr>();
  MS_EXCEPTION_IF_NULL(shape_value);
  ShapeVector shape_vec;
  (void)std::transform(std::begin(shape_value->value()), std::end(shape_value->value()), std::back_inserter(shape_vec),
                       [](const ValuePtr &e) -> int64_t {
                         auto elem = GetValue<int64_t>(e);
                         return elem;
                       });
  return shape_vec;
}

AbstractBasePtr InferImplCSRElementWise(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                        const AbstractBasePtrList &args_spec_list) {
  // Inputs: a sparse tensor and a dense tensor.
  constexpr auto kCSRElementwiseInputsNum = 5;
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, kCSRElementwiseInputsNum);
  auto indptr = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  auto indices = CheckArg<AbstractTensor>(op_name, args_spec_list, 1);
  auto values = CheckArg<AbstractTensor>(op_name, args_spec_list, 2);
  auto shape = CheckArg<AbstractTuple>(op_name, args_spec_list, 3);
  auto dense = CheckArg<AbstractTensor>(op_name, args_spec_list, 4);
  MS_EXCEPTION_IF_NULL(indptr);
  MS_EXCEPTION_IF_NULL(indices);
  MS_EXCEPTION_IF_NULL(values);
  MS_EXCEPTION_IF_NULL(shape);
  MS_EXCEPTION_IF_NULL(dense);

  CheckSparseIndicesDtypeInt32(indptr->element()->BuildType(), "Indptr");
  CheckSparseIndicesDtypeInt32(indices->element()->BuildType(), "Indices");

  ShapeVector sparse_shape = ConvertToShapeVector(shape);
  auto dense_shape = dense->shape()->shape();
  CheckSparseShape(sparse_shape, dense_shape);
  auto ret = values->Broaden();
  // SetAttr
  auto nnz_vec = indices->shape()->shape();
  auto csr_avg_rows = nnz_vec[0] / dense_shape[0];
  primitive->set_attr(kCSRAvgRows, MakeValue(csr_avg_rows));
  primitive->set_attr(kIsCSR, MakeValue(true));
  return ret;
}

AbstractBasePtr InferImplCSRMV(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                               const AbstractBasePtrList &args_spec_list) {
  constexpr auto kCSRMVInputsNum = 5;
  constexpr auto kCSRMVShapeSize = 2;
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, kCSRMVInputsNum);
  auto indptr = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  auto indices = CheckArg<AbstractTensor>(op_name, args_spec_list, 1);
  auto values = CheckArg<AbstractTensor>(op_name, args_spec_list, 2);
  auto shape = CheckArg<AbstractTuple>(op_name, args_spec_list, 3);
  auto dense = CheckArg<AbstractTensor>(op_name, args_spec_list, 4);
  MS_EXCEPTION_IF_NULL(indptr);
  MS_EXCEPTION_IF_NULL(indices);
  MS_EXCEPTION_IF_NULL(values);
  MS_EXCEPTION_IF_NULL(shape);
  MS_EXCEPTION_IF_NULL(dense);

  CheckSparseIndicesDtypeInt32(indptr->element()->BuildType(), "Indptr");
  CheckSparseIndicesDtypeInt32(indices->element()->BuildType(), "Indices");

  ShapeVector sparse_shape = ConvertToShapeVector(shape);
  ShapeVector dense_shape = dense->shape()->shape();
  if (sparse_shape.size() != kCSRMVShapeSize || dense_shape.size() != kCSRMVShapeSize) {
    MS_EXCEPTION(ValueError) << "Currently, only support " << kCSRMVShapeSize << "-D inputs! "
                             << "But csr tensor has " << sparse_shape.size() << " dimensions, "
                             << "and dense tensor has " << dense_shape.size() << " dimension(s). ";
  }
  if (dense_shape[kIndexZero] != sparse_shape[kIndexOne] || dense_shape[kIndexOne] != 1) {
    MS_EXCEPTION(ValueError) << "The dense_vector's shape should be (" << sparse_shape[kIndexOne] << ", 1)"
                             << ", but its current shape is: "
                             << "(" << dense_shape[kIndexZero] << ", " << dense_shape[kIndexOne] << ").";
  }

  ShapeVector out_shape = {sparse_shape[kIndexZero], dense_shape[kIndexOne]};
  auto ret = std::make_shared<AbstractTensor>(values->element()->BuildType(), out_shape);
  // SetAttr
  auto nnz_vec = indices->shape()->shape();
  auto csr_avg_rows = nnz_vec[kIndexZero] / dense_shape[kIndexZero];
  primitive->set_attr(kCSRAvgRows, MakeValue(csr_avg_rows));
  primitive->set_attr(kIsCSR, MakeValue(true));
  return ret;
}

AbstractBasePtr InferImplCSRReduceSum(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                      const AbstractBasePtrList &args_spec_list) {
  // Inputs: a sparse tensor and an axis.
  constexpr auto kCSRReduceSumInputsNum = 5;
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, kCSRReduceSumInputsNum);
  auto indptr = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  auto indices = CheckArg<AbstractTensor>(op_name, args_spec_list, 1);
  auto values = CheckArg<AbstractTensor>(op_name, args_spec_list, 2);
  auto shape = CheckArg<AbstractTuple>(op_name, args_spec_list, 3);
  auto axis = CheckArg<AbstractScalar>(op_name, args_spec_list, 4);
  MS_EXCEPTION_IF_NULL(indptr);
  MS_EXCEPTION_IF_NULL(indices);
  MS_EXCEPTION_IF_NULL(values);
  MS_EXCEPTION_IF_NULL(shape);
  MS_EXCEPTION_IF_NULL(axis);

  CheckSparseIndicesDtypeInt32(indptr->element()->BuildType(), "Indptr");
  CheckSparseIndicesDtypeInt32(indices->element()->BuildType(), "Indices");

  ShapeVector sparse_shape = ConvertToShapeVector(shape);
  ShapeVector out_shape = sparse_shape;
  MS_EXCEPTION_IF_NULL(axis->BuildValue());
  if (axis->BuildValue()->isa<Int32Imm>() || axis->BuildValue()->isa<Int64Imm>()) {
    int64_t axis_value = GetValue<int64_t>(axis->BuildValue());
    int64_t dim = static_cast<int64_t>(sparse_shape.size());
    if (axis_value != 1 && axis_value != 1 - dim) {
      MS_EXCEPTION(ValueError) << "For CSRReduceSum, `axis` should be 1 or 1-dim. But got `axis`: " << axis_value
                               << "and `1- dim`: " << 1 - dim;
    }
    if (axis_value < 0) {
      axis_value += dim;
    }
    out_shape[LongToSize(axis_value)] = 1;
    primitive->set_attr(kCSRAxis, MakeValue(axis_value));
  } else {
    MS_EXCEPTION(TypeError) << "For CSRReduceSum, `axis` should be int32 or int64, but got "
                            << axis->BuildType()->ToString();
  }

  MS_EXCEPTION_IF_NULL(values->element());
  auto ret = std::make_shared<AbstractTensor>(values->element()->BuildType(), out_shape);
  // SetAttr
  auto nnz_vec = indices->shape()->shape();
  auto csr_avg_rows = nnz_vec[0] / sparse_shape[0];
  primitive->set_attr(kCSRAvgRows, MakeValue(csr_avg_rows));
  primitive->set_attr(kIsCSR, MakeValue(true));
  return ret;
}

AbstractBasePtr InferImplCSRGather(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                   const AbstractBasePtrList &args_spec_list) {
  // Inputs: the indptr and indices of a sparse csr tensor, a dense tensor, and the shape of the sparse tensor.
  constexpr size_t csr_row_num = 2;
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, kSizeFour);
  auto indptr = CheckArg<AbstractTensor>(op_name, args_spec_list, kIndexZero);
  auto indices = CheckArg<AbstractTensor>(op_name, args_spec_list, kIndexOne);
  auto dense = CheckArg<AbstractTensor>(op_name, args_spec_list, kIndexTwo);
  auto sparse_shape = CheckArg<AbstractTuple>(op_name, args_spec_list, kIndexThree);
  MS_EXCEPTION_IF_NULL(indptr);
  MS_EXCEPTION_IF_NULL(indices);
  MS_EXCEPTION_IF_NULL(dense);
  MS_EXCEPTION_IF_NULL(sparse_shape);

  CheckSparseIndicesDtypeInt32(indptr->element()->BuildType(), "Indptr");
  CheckSparseIndicesDtypeInt32(indices->element()->BuildType(), "Indices");

  auto shape_value = sparse_shape->BuildValue()->cast<ValueTuplePtr>();
  MS_EXCEPTION_IF_NULL(shape_value);
  auto nnz_vec = indices->shape()->shape();
  int64_t csr_avg_rows = nnz_vec[0] / GetValue<int64_t>(shape_value->value()[0]);
  primitive->set_attr(kCSRAvgRows, MakeValue(csr_avg_rows));
  primitive->set_attr(kIsCSR, MakeValue(true));

  MS_EXCEPTION_IF_NULL(indices->shape());
  ShapeVector out_shape = indices->shape()->shape();
  MS_EXCEPTION_IF_NULL(dense->shape());
  ShapeVector dense_shape = dense->shape()->shape();
  for (size_t i = csr_row_num; i < dense_shape.size(); ++i) {
    out_shape.push_back(dense_shape[i]);
  }
  MS_EXCEPTION_IF_NULL(dense->element());
  auto ret = std::make_shared<AbstractTensor>(dense->element()->BuildType(), out_shape);
  return ret;
}

AbstractBasePtr InferImplCSR2COO(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const AbstractBasePtrList &args_spec_list) {
  // Inputs: the indptr of a sparse csr tensor, and the number of non-zero elements.
  constexpr auto kCSRArgsSize = 2;
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, kCSRArgsSize);
  auto indptr = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  CheckSparseIndicesDtypeInt32(indptr->element()->BuildType(), "Indptr");

  auto nnz = CheckArg<AbstractScalar>(op_name, args_spec_list, 1);
  MS_EXCEPTION_IF_NULL(indptr);
  MS_EXCEPTION_IF_NULL(nnz);

  MS_EXCEPTION_IF_NULL(nnz->BuildValue());
  ShapeVector out_shape;
  if (nnz->BuildValue()->isa<Int32Imm>() || nnz->BuildValue()->isa<Int64Imm>()) {
    int64_t nnz_value = GetValue<int64_t>(nnz->BuildValue());
    out_shape.push_back(nnz_value);
  } else {
    MS_EXCEPTION(ValueError) << "Currently, only support Integer nnz.";
  }

  MS_EXCEPTION_IF_NULL(indptr->shape());
  auto num_rows = indptr->shape()->shape()[0] - 1;
  int csr_avg_rows = GetValue<int64_t>(nnz->BuildValue()) / num_rows;
  primitive->set_attr(kCSRAvgRows, MakeValue(csr_avg_rows));
  primitive->set_attr(kIsCSR, MakeValue(true));

  MS_EXCEPTION_IF_NULL(indptr->element());
  auto ret = std::make_shared<AbstractTensor>(indptr->element()->BuildType(), out_shape);
  return ret;
}

AbstractBasePtr InferImplCOO2CSR(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const AbstractBasePtrList &args_spec_list) {
  // Inputs: the row indices of a sparse coo tensor, and the size of its first dimension.
  constexpr auto kCSRArgsSize = 2;
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, kCSRArgsSize);
  auto row_indices = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  auto height = CheckArg<AbstractScalar>(op_name, args_spec_list, 1);
  MS_EXCEPTION_IF_NULL(row_indices);
  MS_EXCEPTION_IF_NULL(height);
  CheckSparseIndicesDtypeInt32(row_indices->element()->BuildType(), "row_indices");
  MS_EXCEPTION_IF_NULL(height->BuildValue());
  ShapeVector out_shape;
  if (height->BuildValue()->isa<Int32Imm>() || height->BuildValue()->isa<Int64Imm>()) {
    int64_t height_value = GetValue<int64_t>(height->BuildValue());
    out_shape.push_back(height_value + 1);
  } else {
    MS_EXCEPTION(ValueError) << "Currently, only support Integer height.";
  }

  MS_EXCEPTION_IF_NULL(row_indices->element());
  auto ret = std::make_shared<AbstractTensor>(row_indices->element()->BuildType(), out_shape);
  return ret;
}

AbstractBasePtr InferImplMakeCSRTensor(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                       const AbstractBasePtrList &args_spec_list) {
  // Inputs: three tensors and a tuple.
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, kSizeFour);
  auto indptr = CheckArg<AbstractTensor>(op_name, args_spec_list, kIndexZero);
  auto indices = CheckArg<AbstractTensor>(op_name, args_spec_list, kIndexOne);
  auto values = CheckArg<AbstractTensor>(op_name, args_spec_list, kIndexTwo);
  auto shape = CheckArg<AbstractTuple>(op_name, args_spec_list, kIndexThree);

  auto indptr_dtype = indptr->element()->BuildType();
  auto indices_dtype = indices->element()->BuildType();
  CheckSparseIndicesDtype(indptr_dtype, "indptr");
  CheckSparseIndicesDtype(indices_dtype, "indices");

  auto indptr_shp = indptr->shape()->shape();
  CheckSparseShape(indptr_shp.size(), kSizeOne, "Indptr");

  auto indices_shp = indices->shape()->shape();
  CheckSparseShape(indices_shp.size(), kSizeOne, "Indices");

  auto values_shp = values->shape()->shape();
  if (indices_shp[kIndexZero] != values_shp[kIndexZero]) {
    MS_EXCEPTION(ValueError) << "Indices and values must have same size, but got: values length: "
                             << values_shp[kIndexZero] << ", indices length " << indices_shp[kIndexZero];
  }

  auto shape_value = shape->BuildValue()->cast<ValueTuplePtr>();
  MS_EXCEPTION_IF_NULL(shape_value);
  auto shp = shape_value->value();
  ShapeVector shape_vec;
  (void)std::transform(std::begin(shp), std::end(shp), std::back_inserter(shape_vec), [](const ValuePtr &e) -> int64_t {
    auto elem = GetValue<int64_t>(e);
    return elem;
  });
  if (values_shp.size() + 1 != shape_vec.size()) {
    MS_EXCEPTION(ValueError) << "Values' dimension should equal to csr_tensor's dimension - 1, but got"
                             << "Values' dimension: " << values_shp.size()
                             << ", csr_tensor's dimension: " << shape_vec.size() << ".";
  }
  if (shape_vec[kIndexZero] + 1 != indptr_shp[kIndexZero]) {
    MS_EXCEPTION(ValueError) << "Indptr must have length (1 + shape[0]), but got: " << indptr_shp[kIndexZero];
  }
  size_t shape_size = 1;
  auto shape_types = shape->ElementsType();
  for (size_t i = 0; i < shape_vec.size(); ++i) {
    if (shape_vec[i] <= 0) {
      MS_EXCEPTION(ValueError) << "The element of shape must be positive, but got " << shape_value->ToString();
    }
    if ((i > 1) && (shape_vec[i] != values_shp[i - 1])) {
      MS_EXCEPTION(ValueError) << "csr_tensor's shape should match with values' shape.";
    }
    if (!shape_types[i]->isa<Int>()) {
      MS_EXCEPTION(TypeError) << "The element type of shape must be Int, but got " << shape_types[i]->ToString();
    }
    shape_size *= LongToSize(shape_vec[i]);
  }
  if (static_cast<int64_t>(shape_size) < values_shp[kIndexZero]) {
    MS_EXCEPTION(ValueError) << "Shape total size: " << shape_size << " is too small to hold " << values_shp[kIndexZero]
                             << " non-zero values.";
  }
  AbstractBasePtrList element_list{indptr, indices, values, shape};
  return std::make_shared<abstract::AbstractCSRTensor>(element_list);
}

template <typename T>
std::shared_ptr<T> InferSparseAttr(const PrimitivePtr &primitive, const AbstractBasePtrList &args_spec_list) {
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  return CheckArg<T>(op_name, args_spec_list, 0);
}

AbstractBasePtr InferImplCSRTensorGetValues(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                            const AbstractBasePtrList &args_spec_list) {
  auto csr_tensor = InferSparseAttr<AbstractCSRTensor>(primitive, args_spec_list);
  MS_EXCEPTION_IF_NULL(csr_tensor->values());
  return csr_tensor->values();
}

AbstractBasePtr InferImplCSRTensorGetIndptr(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                            const AbstractBasePtrList &args_spec_list) {
  auto csr_tensor = InferSparseAttr<AbstractCSRTensor>(primitive, args_spec_list);
  MS_EXCEPTION_IF_NULL(csr_tensor->indptr());
  return csr_tensor->indptr();
}

AbstractBasePtr InferImplCSRTensorGetIndices(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                             const AbstractBasePtrList &args_spec_list) {
  auto csr_tensor = InferSparseAttr<AbstractCSRTensor>(primitive, args_spec_list);
  MS_EXCEPTION_IF_NULL(csr_tensor->indices());
  return csr_tensor->indices();
}

AbstractBasePtr InferImplCSRTensorGetDenseShape(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                const AbstractBasePtrList &args_spec_list) {
  auto csr_tensor = InferSparseAttr<AbstractCSRTensor>(primitive, args_spec_list);
  MS_EXCEPTION_IF_NULL(csr_tensor->shape());
  return csr_tensor->shape();
}

AbstractBasePtr InferImplAllSwap(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const AbstractBasePtrList &args_spec_list) {
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 3);
  auto tensor_in = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(tensor_in);
  MS_EXCEPTION_IF_NULL(tensor_in->shape());
  auto tensor_in_shape = tensor_in->shape()->shape();

  auto send_size = CheckArg<AbstractTensor>(op_name, args_spec_list, 1);
  MS_EXCEPTION_IF_NULL(send_size);
  auto recv_size = CheckArg<AbstractTensor>(op_name, args_spec_list, 2);
  MS_EXCEPTION_IF_NULL(recv_size);

  // Get the content of the recv size
  auto recv_size_value_ptr = recv_size->BuildValue();
  MS_EXCEPTION_IF_NULL(recv_size_value_ptr);
  auto recv_size_tensor = recv_size_value_ptr->cast<tensor::TensorPtr>();
  MS_EXCEPTION_IF_NULL(recv_size_tensor);
  auto data_pos = static_cast<int64_t *>(recv_size_tensor->data_c());
  MS_EXCEPTION_IF_NULL(data_pos);
  int64_t infer_max_size = 0;
  for (size_t i = 0; i < recv_size_tensor->DataSize(); ++i) {
    infer_max_size += *(data_pos + i);
  }

  ShapeVector tensor_out_shape = {Shape::SHP_ANY, tensor_in_shape[1]};
  ShapeVector min_shape = {1, tensor_in_shape[1]};

  ShapeVector max_shape = {infer_max_size / tensor_in_shape[1], tensor_in_shape[1]};

  auto tensor_out = std::make_shared<AbstractTensor>(tensor_in->element(),
                                                     std::make_shared<Shape>(tensor_out_shape, min_shape, max_shape));

  AbstractTensorPtr ret = std::make_shared<AbstractTensor>(
    tensor_out->element(), std::make_shared<Shape>(tensor_out_shape, min_shape, max_shape));
  return ret;
}

AbstractBasePtr InferImplAllReduce(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                   const AbstractBasePtrList &args_spec_list) {
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto x = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(x);
  MS_EXCEPTION_IF_NULL(x->shape());
  return std::make_shared<AbstractTensor>(x->element(), std::make_shared<Shape>(x->shape()->shape()));
}

AbstractBasePtr InferImplBroadcast(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                   const AbstractBasePtrList &args_spec_list) {
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto x = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(x);
  MS_EXCEPTION_IF_NULL(x->shape());
  return std::make_shared<AbstractTensor>(x->element(), std::make_shared<Shape>(x->shape()->shape()));
}

AbstractBasePtr InferImplAllGather(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                   const AbstractBasePtrList &args_spec_list) {
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto x = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(x);
  MS_EXCEPTION_IF_NULL(x->shape());
  auto tmp_shape = x->shape()->shape();
  if (!primitive->HasAttr(kRankSize)) {
    MS_LOG(EXCEPTION) << "Primitive don't have rank_size attr";
  }
  auto rank_size = GetValue<int>(primitive->GetAttr(kRankSize));
  if (rank_size == 0) {
    MS_LOG(EXCEPTION) << "rank_size is 0";
  }
  if (tmp_shape.empty()) {
    MS_LOG(EXCEPTION) << "shape size is 0";
  }
  if (tmp_shape[0] > 0) {
    tmp_shape[0] = tmp_shape[0] * rank_size;
  }
  return std::make_shared<AbstractTensor>(x->element(), std::make_shared<Shape>(tmp_shape));
}

AbstractBasePtr InferImplReduceScatter(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                       const AbstractBasePtrList &args_spec_list) {
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto x = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(x);
  MS_EXCEPTION_IF_NULL(x->shape());
  auto tmp_shape = x->shape()->shape();
  if (!primitive->HasAttr(kRankSize)) {
    MS_LOG(EXCEPTION) << "Primitive don't have rank_size attr";
  }
  auto rank_size = GetValue<int>(primitive->GetAttr(kRankSize));
  if (tmp_shape.empty()) {
    MS_LOG(EXCEPTION) << "shape size is 0";
  }
  tmp_shape[0] = LongMulWithOverflowCheck(tmp_shape[0], rank_size);
  return std::make_shared<AbstractTensor>(x->element(), std::make_shared<Shape>(tmp_shape));
}

AbstractBasePtr InferImplMemCpyAsync(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                     const AbstractBasePtrList &args_spec_list) {
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  auto x = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(x);
  MS_EXCEPTION_IF_NULL(x->shape());
  return std::make_shared<AbstractTensor>(x->element(), std::make_shared<Shape>(x->shape()->shape()));
}

AbstractBasePtr InferImplCast(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                              const AbstractBasePtrList &args_spec_list) {
  const std::string op_name = primitive->name();
  // GPU has 2 inputs while tbe has 1 only. Skip CheckArgsSize.
  auto input_x = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  MS_EXCEPTION_IF_NULL(input_x);
  auto attr = primitive->GetAttr("dst_type");
  if (attr == nullptr) {
    auto type_abs = CheckArg<AbstractType>(op_name, args_spec_list, 1);
    attr = type_abs->BuildValue();
    MS_EXCEPTION_IF_NULL(attr);
    primitive->set_attr("dst_type", attr);
  }
  auto input_type = attr->cast<TypePtr>();
  auto ret = std::make_shared<AbstractTensor>(input_type, input_x->shape());
  return ret;
}

AbstractBasePtr InferImplGpuConvertToDynamicShape(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                  const AbstractBasePtrList &args_spec_list) {
  const std::string &op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, 1);
  AbstractTensorPtr input = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);

  ShapeVector input_shape = input->shape()->shape();
  int32_t input_rank = SizeToInt(input_shape.size());
  ShapeVector inferred_shape(input_rank, Shape::SHP_ANY);
  ShapeVector min_shape(input_rank, 1);
  ShapeVector max_shape = input_shape;

  ShapePtr shape = std::make_shared<Shape>(inferred_shape, min_shape, max_shape);
  return std::make_shared<AbstractTensor>(input->element(), shape);
}

AbstractBasePtr InferImplLoad(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                              const AbstractBasePtrList &args_spec_list) {
  // Inputs: Ref/Tensor, universal
  CheckArgsSize(primitive->name(), args_spec_list, 2);
  auto ref_abs = dyn_cast<abstract::AbstractRefTensor>(args_spec_list[0]);
  if (ref_abs != nullptr) {
    // Return tensor value if input is Ref.
    return ref_abs->CloneAsTensor();
  }
  return args_spec_list[0]->Broaden();
}

AbstractBasePtr InferImplTransData(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                   const AbstractBasePtrList &args_spec_list) {
  // An object of a subclass of AbstractBase
  CheckArgsSize(primitive->name(), args_spec_list, 1);
  auto output = args_spec_list[0];
  MS_EXCEPTION_IF_NULL(output);
  return output;
}
AbstractBasePtr InferImplAdamApplyOne(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                      const AbstractBasePtrList &args_spec_list) {
  // An object of a subclass of AbstractBase
  CheckArgsSize(primitive->name(), args_spec_list, 10);
  auto input0 = args_spec_list[0];
  auto input1 = args_spec_list[1];
  auto input2 = args_spec_list[2];
  auto input3 = args_spec_list[3];
  auto input4 = args_spec_list[4];
  auto mul0_x = args_spec_list[5];
  auto mul1_x = args_spec_list[6];
  auto mul2_x = args_spec_list[7];
  auto mul3_x = args_spec_list[8];
  auto add2_y = args_spec_list[9];

  auto square0 = ops::SquareInfer(nullptr, primitive, {input0});
  auto mul1 = ops::MulInfer(nullptr, primitive, {mul1_x, input0});
  auto mul0 = ops::MulInfer(nullptr, primitive, {mul0_x, input2});
  auto mul2 = ops::MulInfer(nullptr, primitive, {mul2_x, input1});
  auto mul3 = ops::MulInfer(nullptr, primitive, {mul3_x, square0});
  auto add0 = ops::AddInfer(nullptr, primitive, {mul0, mul1});
  auto add1 = ops::AddInfer(nullptr, primitive, {mul2, mul3});
  auto sqrt0 = InferImplSqrt(nullptr, primitive, {add1});
  auto add2 = ops::AddInfer(nullptr, primitive, {add2_y, sqrt0});
  auto true_div0 = ops::RealDivInfer(nullptr, primitive, {add0, add2});
  auto mul4 = ops::MulInfer(nullptr, primitive, {input4, true_div0});
  auto sub0 = ops::SubInfer(nullptr, primitive, {input3, mul4});

  AbstractBasePtrList rets = {add1, add0, sub0};
  return std::make_shared<AbstractTuple>(rets);
}
AbstractBasePtr InferImplTensorMove(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const AbstractBasePtrList &args_spec_list) {
  // An object of a subclass of AbstractBase
  CheckArgsSize(primitive->name(), args_spec_list, 1);
  auto output = args_spec_list[0];
  MS_EXCEPTION_IF_NULL(output);
  return output;
}
AbstractBasePtr InferImplAdamApplyOneWithDecay(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                               const AbstractBasePtrList &args_spec_list) {
  // An object of a subclass of AbstractBase
  CheckArgsSize(primitive->name(), args_spec_list, 11);
  auto input0 = args_spec_list[0];
  auto input1 = args_spec_list[1];
  auto input2 = args_spec_list[2];
  auto input3 = args_spec_list[3];
  auto input4 = args_spec_list[4];
  auto mul0_x = args_spec_list[5];
  auto mul1_x = args_spec_list[6];
  auto mul2_x = args_spec_list[7];
  auto mul3_x = args_spec_list[8];
  auto mul4_x = args_spec_list[9];
  auto add2_y = args_spec_list[10];

  auto mul0 = ops::MulInfer(nullptr, primitive, {mul0_x, input2});
  auto mul1 = ops::MulInfer(nullptr, primitive, {mul1_x, input0});
  auto square0 = ops::SquareInfer(nullptr, primitive, {input0});
  auto add0 = ops::AddInfer(nullptr, primitive, {mul0, mul1});
  auto mul2 = ops::MulInfer(nullptr, primitive, {mul2_x, input1});
  auto mul3 = ops::MulInfer(nullptr, primitive, {mul3_x, square0});
  auto add1 = ops::AddInfer(nullptr, primitive, {mul2, mul3});
  auto sqrt0 = InferImplSqrt(nullptr, primitive, {add1});
  auto add2 = ops::AddInfer(nullptr, primitive, {add2_y, sqrt0});
  auto mul4 = ops::MulInfer(nullptr, primitive, {mul4_x, input3});
  auto real_div0 = ops::RealDivInfer(nullptr, primitive, {add0, add2});
  auto add3 = ops::AddInfer(nullptr, primitive, {mul4, real_div0});
  auto mul5 = ops::MulInfer(nullptr, primitive, {input4, add3});
  auto sub0 = ops::SubInfer(nullptr, primitive, {input3, mul5});
  AbstractBasePtrList rets = {add1, add0, sub0};
  return std::make_shared<AbstractTuple>(rets);
}
AbstractBasePtr InferImplCSRMM(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                               const AbstractBasePtrList &args_spec_list) {
  // Inputs: a sparse tensor and a dense tensor.
  constexpr auto kCSRMMInputsNum = 5;
  constexpr auto kCSRMMShapeSize = 2;
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, args_spec_list, kCSRMMInputsNum);
  auto indptr = CheckArg<AbstractTensor>(op_name, args_spec_list, 0);
  auto indices = CheckArg<AbstractTensor>(op_name, args_spec_list, 1);
  auto values = CheckArg<AbstractTensor>(op_name, args_spec_list, 2);
  auto shape = CheckArg<AbstractTuple>(op_name, args_spec_list, 3);
  auto dense = CheckArg<AbstractTensor>(op_name, args_spec_list, 4);
  MS_EXCEPTION_IF_NULL(indptr);
  MS_EXCEPTION_IF_NULL(indices);
  MS_EXCEPTION_IF_NULL(values);
  MS_EXCEPTION_IF_NULL(shape);
  MS_EXCEPTION_IF_NULL(dense);

  CheckSparseIndicesDtypeInt32(indptr->element()->BuildType(), "Indptr");
  CheckSparseIndicesDtypeInt32(indices->element()->BuildType(), "Indices");

  ShapeVector sparse_shape = ConvertToShapeVector(shape);
  auto dense_shape = dense->shape()->shape();
  if (sparse_shape.size() != kCSRMMShapeSize || dense_shape.size() != kCSRMMShapeSize) {
    MS_EXCEPTION(ValueError) << "Currently, only support " << kCSRMMShapeSize << "-D inputs! "
                             << "But csr tensor has " << sparse_shape.size() << " dimensions, "
                             << "and dense tensor has " << dense_shape.size() << " dimensions, ";
  }
  if (dense_shape[kIndexZero] != sparse_shape[kIndexOne]) {
    MS_EXCEPTION(ValueError) << "The dense's shape[0] should be equal to csr tensor's shape[1]"
                             << ", but dense's shape[0] is: " << dense_shape[kIndexZero]
                             << " and csr tensor's shape[1] is " << sparse_shape[kIndexOne];
  }

  ShapeVector out_shape = {sparse_shape[kIndexZero], dense_shape[kIndexOne]};
  auto ret = std::make_shared<AbstractTensor>(values->element()->BuildType(), out_shape);
  // SetAttr
  auto nnz_vec = indices->shape()->shape();
  auto csr_avg_rows = nnz_vec[kIndexZero] / dense_shape[kIndexZero];
  primitive->set_attr(kCSRAvgRows, MakeValue(csr_avg_rows));
  primitive->set_attr(kIsCSR, MakeValue(true));
  return ret;
}
}  // namespace abstract
}  // namespace mindspore
