# coding: utf-8

# Copyright 2020-2021 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and

# limitations under the License.
# ============================================================================

"""Operators for sparse operators."""

from ..._checkparam import Validator as validator
from ...common import dtype as mstype
from ..primitive import PrimitiveWithInfer, prim_attr_register, Primitive


class SparseToDense(PrimitiveWithInfer):
    """
    Converts a sparse representation into a dense tensor.

    Inputs:
        - **indices** (Tensor) - A 2-D Tensor, represents the position of the element in the sparse tensor.
          Support int32, int64, each element value should be a non-negative int number. The shape is :math:`(n, 2)`.
        - **values** (Tensor) - A 1-D Tensor, represents the value corresponding to the position in the `indices`.
          The shape should be :math:`(n,)`.
        - **sparse_shape** (tuple(int)) - A positive int tuple which specifies the shape of sparse tensor,
          should have 2 elements, represent sparse tensor shape is :math:`(N, C)`.

    Returns:
        Tensor, converted from sparse tensor. The dtype is same as `values`, and the shape is `sparse_shape`.

    Raises:
        TypeError: If the dtype of `indices` is neither int32 nor int64.
        ValueError: If `sparse_shape`, shape of `indices` and shape of `values` don't meet the parameter description.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> indices = Tensor([[0, 1], [1, 2]])
        >>> values = Tensor([1, 2], dtype=mindspore.float32)
        >>> sparse_shape = (3, 4)
        >>> sparse_to_dense = ops.SparseToDense()
        >>> out = sparse_to_dense(indices, values, sparse_shape)
        >>> print(out)
        [[0. 1. 0. 0.]
         [0. 0. 2. 0.]
         [0. 0. 0. 0.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SparseToDense."""
        self.init_prim_io_names(inputs=['indices', 'values', 'dense_shape'], outputs=['output'])

    def __infer__(self, indices, values, sparse_shape):
        validator.check_tensor_dtype_valid('indices', indices['dtype'], [mstype.int32, mstype.int64], self.name)
        validator.check_tensor_dtype_valid('values', values['dtype'], mstype.number_type + (mstype.bool_,), self.name)
        indices_shape = indices['shape']
        if len(indices_shape) != 2:
            raise ValueError(f"For '{self.name}', the 'indices' must be a 2-D tensor, "
                             f"but got 'indices' shape: {indices_shape}.")
        values_shape = values['shape']
        if len(values_shape) != 1 or values_shape[0] != indices_shape[0]:
            raise ValueError(f"For '{self.name}', the 'values' must be a 1-D tensor and the first dimension length "
                             f"must be equal to the first dimension length of 'indices', "
                             f"but got 'indices' shape: {indices_shape}, 'values' shape: {values_shape}.")
        sparse_shape_v = sparse_shape['value']
        for i in sparse_shape_v:
            if isinstance(i, bool) or not isinstance(i, int) or i <= 0:
                raise ValueError(f"For '{self.name}', all elements in 'sparse_shape' must be "
                                 f"positive int number, but got 'sparse_shape': {sparse_shape_v}.")
        if len(sparse_shape_v) != indices_shape[1]:
            raise ValueError(f"For '{self.name}', the length of 'sparse_shape' must be equal to the second dimension "
                             f"length of 'indices', but got the second dimension length of 'indices': "
                             f"{indices_shape[1]}, length of 'sparse_shape': {len(sparse_shape_v)}.")
        out = {'shape': sparse_shape['value'],
               'dtype': values['dtype'],
               'value': None}
        return out


class SparseTensorDenseAdd(Primitive):
    """
    Add a sparse tensor and a dense tensor to get a dense tensor.

    Inputs:
        - **x1_indices** (Tensor) - A 2-D Tensor, represents the position of the element in the sparse tensor.
          Support int32, int64, each element value should be a non-negative int number. The shape is :math:`(n, 2)`.
        - **x1_values** (Tensor) - A 1-D Tensor, represents the value corresponding to the position in the `indices`.
          The shape should be :math:`(n,)`.
        - **x1_shape** (tuple(int)) - A positive int tuple which specifies the shape of sparse tensor,
          should have 2 elements, represent sparse tensor shape is :math:`(N, C)`.
        -**x2** (Tensor)- A dense Tensor, the dtype is same as `values`.

    Returns:
        Tensor, add result of sparse tensor and dense tensor. The dtype is same as `values`,
        and the shape is `x1_shape`.

    Raises:
        TypeError: If the dtype of `x1_indices` and 'x1_shape' is neither int32 nor int64.
        ValueError: If `x1_shape`, shape of `x1_indices`, shape of `x1_values` and shape
        of 'x2' don't meet the parameter description.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> from mindspore import Tensor
        >>> from mindspore.ops import operations as ops
        >>> from mindspore.common import dtype as mstype
        >>> x1_indices = Tensor([[0, 0], [0, 1]], dtype=mstype.int64)
        >>> x1_values = Tensor([1, 1], dtype=mstype.float32)
        >>> x1_shape = Tensor([3, 3], dtype=mstype.int64)
        >>> x2= Tensor([[1, 1, 1], [1, 1, 1], [1, 1, 1]], dtype=mstype.float32)
        >>> sparse_tensor_dense_add = ops.SparseTensorDenseAdd()
        >>> out = sparse_tensor_dense_add(x1_indices, x1_values, x1_shape, x2)
        >>> print(out)
        [[2. 2. 1.]
         [1. 1. 1.]
         [1. 1. 1.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SparseTensorDenseAdd."""
        self.init_prim_io_names(inputs=['x1_indices', 'x1_values', 'x1_shape', 'x2'], outputs=['y'])


class SparseTensorDenseMatmul(Primitive):
    """
    Multiplies sparse matrix `A` by dense matrix `B`.
    The rank of sparse matrix and dense matrix must be equal to `2`.

    Args:
        adjoint_st (bool): If true, sparse tensor is transposed before multiplication. Default: False.
        adjoint_dt (bool): If true, dense tensor is transposed before multiplication. Default: False.

    Inputs:
        - **indices** (Tensor) - A 2-D Tensor, represents the position of the element in the sparse tensor.
          Support int32, int64, each element value should be a non-negative int number. The shape is :math:`(n, 2)`.
        - **values** (Tensor) - A 1-D Tensor, represents the value corresponding to the position in the `indices`.
          Support float16, float32, float64, int32, int64, complex64, complex128. The shape should be :math:`(n,)`.
        - **sparse_shape** (tuple(int)) or (Tensor) - A positive int tuple or tensor which specifies the shape of
          sparse tensor, and only constant value is allowed when sparse_shape is a tensor, should have 2 elements,
          represent sparse tensor shape is :math:`(N, C)`.
        - **dense** (Tensor) - A 2-D Tensor, the dtype is same as `values`.
          If `adjoint_st` is False and `adjoint_dt` is False, the shape must be :math:`(C, M)`.
          If `adjoint_st` is False and `adjoint_dt` is True, the shape must be :math:`(M, C)`.
          If `adjoint_st` is True and `adjoint_dt` is False, the shape must be :math:`(N, M)`.
          If `adjoint_st` is True and `adjoint_dt` is True, the shape must be :math:`(M, N)`.

    Outputs:
        Tensor, the dtype is the same as `values`.
        If `adjoint_st` is False, the shape is :math:`(N, M)`.
        If `adjoint_st` is True, the shape is :math:`(C, M)`.

    Raises:
        TypeError: If the type of `adjoint_st` or `adjoint_dt` is not bool, or the dtype of `indices`,
            dtype of `values` and dtype of `dense` don't meet the parameter description.
        ValueError: If `sparse_shape`, shape of `indices`, shape of `values`,
            and shape of `dense` don't meet the parameter description.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> indices = Tensor([[0, 1], [1, 2]], dtype=mindspore.int32)
        >>> values = Tensor([1, 2], dtype=mindspore.float32)
        >>> sparse_shape = (3, 4)
        >>> dense = Tensor([[1, 1], [2, 2], [3, 3], [4, 4]], dtype=mindspore.float32)
        >>> sparse_dense_matmul = ops.SparseTensorDenseMatmul()
        >>> out = sparse_dense_matmul(indices, values, sparse_shape, dense)
        >>> print(out)
        [[2. 2.]
         [6. 6.]
         [0. 0.]]
    """

    @prim_attr_register
    def __init__(self, adjoint_st=False, adjoint_dt=False):
        """Initialize SparseTensorDenseMatmul"""
        self.adjoint_st = adjoint_st
        self.adjoint_dt = adjoint_dt
        self.init_prim_io_names(inputs=['indices', 'values', 'sparse_shape', 'dense'],
                                outputs=['output'])
        self.add_prim_attr('adjoint_a', self.adjoint_st)
        self.add_prim_attr('adjoint_b', self.adjoint_dt)
        validator.check_value_type("adjoint_st", adjoint_st, [bool], self.name)
        validator.check_value_type("adjoint_dt", adjoint_dt, [bool], self.name)
        self.set_const_input_indexes([2])

    def __infer__(self, indices, values, sparse_shape, dense):
        validator.check_tensor_dtype_valid('indices', indices['dtype'], [mstype.int32, mstype.int64], self.name)
        valid_types = (mstype.float16, mstype.float32, mstype.float64, mstype.int32, mstype.int64)
        args = {'values': values['dtype'], 'dense': dense['dtype']}
        validator.check_tensors_dtypes_same_and_valid(args, valid_types, self.name)
        indices_shape = indices['shape']
        if len(indices_shape) != 2 or indices_shape[1] != 2:
            raise ValueError(f"For '{self.name}', the 'indices' must be a 2-D tensor and "
                             f"the second dimension length must be 2, but got 'indices' shape: {indices_shape}.")
        values_shape = values['shape']
        if len(values_shape) != 1 or values_shape[0] != indices_shape[0]:
            raise ValueError(f"For '{self.name}', the 'values' must be a 1-D tensor and "
                             f"the first dimension length must be equal to the first dimension length of 'indices', "
                             f"but got 'indices' shape: {indices_shape}, 'values' shape: {values_shape}.")
        a_shape = sparse_shape['value'][::-1] if self.adjoint_st else sparse_shape['value']
        b_shape = dense['shape'][::-1] if self.adjoint_dt else dense['shape']
        for i in a_shape:
            if isinstance(i, bool) or not isinstance(i, int) or i <= 0:
                raise ValueError(f"For '{self.name}', all elements in 'sparse_shape' must be "
                                 f"positive int number, but got 'sparse_shape': {a_shape}.")
        if len(a_shape) != 2 or len(b_shape) != 2:
            raise ValueError(f"For '{self.name}', both the length of 'sparse_shape' and the tensor "
                             f"rank of 'dense' must be equal to 2, but got the length of "
                             f"'sparse_shape': {len(a_shape)}, "
                             f"the tensor rank of 'dense': {len(b_shape)}.")
        if a_shape[1] != b_shape[0]:
            raise ValueError(f"For '{self.name}', the second dimension length of 'sparse_shape' must be equal to the "
                             f"first dimension length of 'dense', but got "
                             f"the tensor shape of 'sparse': {a_shape} and the tensor shape of 'dense': {b_shape}. "
                             f"Don't meet the condition for matmul")
        out_shape = [a_shape[0], b_shape[1]]
        out = {'shape': tuple(out_shape),
               'dtype': values['dtype'],
               'value': None}
        return out


class CSRSparseMatrixToSparseTensor(Primitive):
    """
    Converts a CSR sparse matrix(maybe batched) to its sparse tensor form.

    Inputs:
        - **x_dense_shape** (Tensor) - A 1-D Tensor. It represents the dense form shape of
          the input CSR sparse matrix, the shape of which should be :math:`(2,)` or :math:`(3,)`.
        - **x_batch_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix is of
          batch size `n`, it should have shape :math:`(n+1,)`, while the `i`-th element of which stores
          acummulated counts of non-zero values of the first `i - 1` batches.
        - **x_row_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix is of
          batch size `n` and row number `m`, it can be divided into `n` parts, each part of length
          `m + 1`. The `i`-th element of each :math:`(m+1,)` vector stores acummulated counts of
          non-zero values of the first `i - 1` rows in the corresponding batch.
        - **x_col_indices** (Tensor) - A 1-D Tensor. It represents column indices of the non-zero values
          in the input CSR sparse matrix.
        - **x_values** (Tensor) - A 1-D Tensor. It represents all the non-zero values in the
          input CSR sparse matrix.

    Outputs:
        - **indices** (Tensor) - A 2-D Tensor. It represents the position of the non-zero element
          in the sparse tensor.
        - **values** (Tensor) - A 1-D Tensor. It represents the value corresponding to the position
          in the `indices`, the shape of which should be :math:`(N,)`.
        - **dense_shape** (Tensor) - A 1-D Tensor. It represents the dense form shape of
          the sparse tensor. Its shape should be :math:`(2,)` or :math:`(3,)`.

    Raises:
        TypeError: If the dtype of `x_dense_shape` or `x_batch_pointers` or `x_row_pointers` or
                   `x_col_indices` is not int32 or int64.
        TypeError: If the dtype of `x_values` is not float32, float64, complex64 or complex128.
        ValueError: If `x_dense_shape` or `x_batch_pointers` or `x_row_pointers` or `x_values` or
                   `x_dense_shape` is not a 1-D tensor.
        ValueError: If rank of `x_dense_shape` is not 2 or 3.
        ValueError: If shape of `x_col_indices` is not corresponding to shape of `x_values`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore.ops.operations.sparse_ops import CSRSparseMatrixToSparseTensor
        >>> x_dense_shape = Tensor(np.array([2, 2, 4]).astype(np.int64))
        >>> x_batch_pointers = Tensor(np.array([0, 3, 6]).astype(np.int64))
        >>> x_row_pointers = Tensor(np.array([0, 1, 3, 0, 1, 3]).astype(np.int64))
        >>> x_col_indices = Tensor(np.array([1, 2, 3, 1, 2, 3]).astype(np.int64))
        >>> x_values = Tensor(np.array([1, 4, 3, 1, 4, 3]).astype(np.float32))
        >>> csr_sparse_matrix_to_sparse_tensor = ops.CSRSparseMatrixToSparseTensor()
        >>> out = csr_sparse_matrix_to_sparse_tensor(x_dense_shape, x_batch_pointers, x_row_pointers,
        ...                                          x_col_indices, x_values)
        >>> print(out[0])
        [[0 0 1]
         [0 1 2]
         [0 1 3]
         [1 0 1]
         [1 1 2]
         [1 1 3]]
        >>> print(out[1])
        [1. 4. 3. 1. 4. 3.]
        >>> print(out[2])
        [2 2 4]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize CSRSparseMatrixToSparseTensor."""
        self.init_prim_io_names(inputs=['x_dense_shape', 'x_batch_pointers', 'x_row_pointers',
                                        'x_col_indices', 'x_values'],
                                outputs=['indices', 'values', 'dense_shape'])


class DenseToCSRSparseMatrix(Primitive):
    """
    Converts a dense matrix(maybe batched) to its CSR sparse form.

    .. warning::
        This is an experimental prototype that is subject to change and/or deletion.

    Inputs:
        - **dense_input** (Tensor) - A 2-D or 3-D Tensor. It represents the input dense matrix.
        - **indices** (Tensor) - A 2-D Tensor. It represents indices of all the nonzero elements.

    Outputs:
        - **y_dense_shape** (Tensor) - A 1-D Tensor. It represents the dense form shape of
          the output CSR sparse matrix, the shape of which should be :math:`(2,)` or :math:`(3,)`.
        - **y_batch_pointers** (Tensor) - A 1-D Tensor. Supposing the output CSR sparse matrix is of
          batch size `n`, it should have shape :math:`(n+1,)`, while the `i`-th element of which stores
          acummulated counts of nonzero values of the first `i - 1` batches.
        - **y_row_pointers** (Tensor) - A 1-D Tensor. Supposing the output CSR sparse matrix is of
          batch size `n` and row number `m`, it can be divided into `n` parts, each part of length
          `m + 1`. The `i`-th element of each :math:`(m+1,)` vector stores acummulated counts of
          nonzero values of the first `i - 1` rows in the corresponding batch.
        - **y_col_indices** (Tensor) - A 1-D Tensor. It represents column indices of the nonzero values
          in the output CSR sparse matrix.
        - **y_values** (Tensor) - A 1-D Tensor. It represents all the nonzero values in the
          output CSR sparse matrix.

    Raises:
        TypeError: If the dtype of `indices` is not int32 or int64.
        TypeError: If the dtype of `dense_input` is not float32, float64, complex64 or complex128.
        ValueError: If either of the inputs is not a tensor.
        ValueError: If rank of `dense_input` is not 2 or 3.
        ValueError: If rank of `indices` is not 2.
        ValueError: If shape[1] of `indices` and rank of `dense_input` is not the same.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> x = Tensor([[[1., 0.], [0., 2.]]], dtype=mindspore.float32)
        >>> indices = Tensor([[0, 0, 0], [0, 1, 1]], dtype=mindspore.int32)
        >>> dense_to_csr = ops.DenseToCSRSparseMatrix()
        >>> out = dense_to_csr(x, indices)
        >>> print(out[0])
        [1 2 2]
        >>> print(out[1])
        [0 2]
        >>> print(out[2])
        [0 1 2]
        >>> print(out[3])
        [0 1]
        >>> print(out[4])
        [1. 2.]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize DenseToCSRSparseMatrix"""
        self.init_prim_io_names(
            inputs=['dense_input', 'indices'],
            outputs=['y_dense_shape', 'y_batch_pointers', 'y_row_pointers', 'y_col_indices', 'y_values'])


class DenseToDenseSetOperation(Primitive):
    """
    Applies set operation along last dimension of 2 `Tensor` inputs.
    Iterate over groups in set x1 and set x2, applying `ApplySetOperation` to each,
    and outputting the result `SparseTensor`. A "group" is a collection of values
    with the same first n-1 dimensions in x1 and x2.

    Args:
        set_operation (str): The type of set operation, case insensitive. Default:"a-b".
            "a-b": Get the difference set of x1 to x2.
            "b-a": Get the difference set of x2 to x1.
            "intersection": Get the intersection set of x2 to x1.
            "union": Get the union set of x2 to x1.
        validate_indices (bool): Optional attributes for DenseToDenseSetOperation.  Default: True.

    Inputs:
        - **x1** (Tensor) - The input tensor `x1` with rank `n`. 1st `n-1` dimensions must be the same as `x2`.
          Dimension `n` contains values in a set, duplicates are allowed but ignored.
        - **x2** (Tensor) - The input tensor `x2` with rank `n`. 1st `n-1` dimensions must be the same as `x1`.
          Dimension `n` contains values in a set, duplicates are allowed but ignored.

    Outputs:
        - **y_indices** (Tensor) - A 2-D Tensor of type int64, represents the position of the element
          in the sparse tensor.
        - **y_values** (Tensor) - A 1-D Tensor, represents the value corresponding to the position
          in the `y_indices`. The dtype is same as input.
        - **y_shape** (Tensor) - A 1-D Tensor of type int64, represents the shape of sparse tensor.
          `y_shape[0...n-1]` is the same as the 1st `n-1` dimensions of `x1` and `x2`,
          `y_shape[n]` is the max result set size across all `0...n-1` dimensions.

    Raises:
        TypeError: If input `x1` or `x2` is not Tensor.
        TypeError: If the type of `x1` is not the same as `x2`.
        ValueError: If the group shape of `x1` or `x2` mismatch with each other.
        ValueError: If the rank of `x1` or `x2` is less than 2.
        ValueError: If the value of attr set_operation is not a valid value.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> x1 = Tensor([[2, 2, 0], [2, 2, 1], [0, 2, 2]], dtype=mstype.int32)
        >>> x2 = Tensor([[2, 2, 1], [0, 2, 0], [0, 1, 1]], dtype=mstype.int32)
        >>> dtod=P.DenseToDenseSetOperation(set_operation="a-b",validate_indices=True)
        >>> res=dtod(x1,x2)
        >>> print(res[0])
        [[0 0]
         [1 0]
         [2 0]]
        >>> print(res[1])
        [0 1 2]
        >>> print(res[2])
        [3 1]
    """

    @prim_attr_register
    def __init__(self, set_operation="a-b", validate_indices=True):
        """Initialize DenseToDenseSetOperation."""
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['y_indices', 'y_values', 'y_shape'])
        validator.check_value_type("set_operation", set_operation, [str], self.name)
        validator.check_value_type("validate_indices", validate_indices, [bool], self.name)


class Sspaddmm(Primitive):
    r"""
    Matrix multiplies a sparse tensor `x2` with a dense tensor `x3`, then adds the sparse tensor `x1`.
    If `x1_shape` is :math:`(s0, s1)`, `x2_shpae` should be :math:`(s0, s2)`, the `x3_shape` should be :math:`(s2, s1)`.

    .. warning::
        This is an experimental prototype that is subject to change and/or deletion.

    .. math::
        out =\beta * x1  + \alpha * (x2 @ x3),

    Inputs:
        - **x1_indices** (Tensor) - A 2-D Tensor, represents the position of the element in the sparse tensor.
          Support int32, int64. The shape is :math:`(2, n)`.  If `x1_shape` is :math:`(s0, s1)`, the row index
          value of `x1_indices` should be a non-negative and less than `s0` int number, the col index value of
          `x1_indices` should be a non-negative and less than `s1` int number.
        - **x1_values** (Tensor) - A 1-D Tensor, represents the value corresponding to the position in
          the `x1_indices`. Support float32, float64, int8, int16, int32, int64, uint8. The dtype should be the same as
          `x2_values` and `x3_dense`. The shape should be :math:`(n,)`.
        - **x1_shape** (Tensor) - A 1-D Tensor, specifies the shape of sparse tensor. Support int32, int64,
          have 2 positive int elements, shape is :math:`(2,)`. The dtype should be the same as `x1_indices`.
        - **x2_indices** (Tensor) - A 2-D Tensor, represents the position of the element in the sparse tensor.
          Support int32, int64. The shape is :math:`(2, n)`. If `x2_shape` is :math:`(s0, s2)`, the row index
          value of `x2_indices` should be a non-negative and less than `s0` int number, the col index value of
          `x2_indices` should be a non-negative and less than `s2` int number.
        - **x2_values** (Tensor) - A 1-D Tensor, represents the value corresponding to the position in the `x2_indices`.
          Support float32, float64, int8, int16, int32, int64, uint8. The dtype should be the same as `x1_values`
          and `x3_dense`. The shape should be :math:`(n,)`.
        - **x2_shape** (Tensor) - A 1-D Tensor, specifies the shape of sparse tensor. Support int32,int64,
          have 2 positive int elements, shape is :math:`(2,)`. The dtype is same as `x2_indices`.
        - **x3_dense** (Tensor) - A 2-D Tensor, the dtype should be the same as `x2_values` and `x3_dense`.
        - **alpha** (Tensor) - A 0-D or 1-D Tensor, the weight of x1. If alpha is 1-D tensor,
          the shape should be :math:`()` otherwise the shape is :math:`(1,)`. Support uint8, uint16, uint32, uint64,
          int8, int16, int32, int64, float16, float32, float64. If the dtype of alpha is not the same with expected
          output dtype, alpha value should be convert without overflow.
        - **beta** (Tensor) - A 0-D or 1-D, the weight of x2@x3. If alpha is 1-D tensor,
          the shape should be :math:`()` otherwise the shape is :math:`(1,)`. Support uint8, uint16, uint32, uint64,
          int8, int16, int32, int64, float16, float32, float64. If the `x1_values` dtype is byte, char, short, int,
          long, the dtype of beta doesn't support float16, float32, float64.

    Outputs:
        - **y_indices** (Tensor) - A 2-D Tensor, represents the position of the element in the sparse tensor.
          The dtype is int64, each element value should be a non-negative int number. The shape is :math:`(2, n)`.
        - **y_values** (Tensor) - A 1-D Tensor, represents the value corresponding to the position in the `y_indices`.
          The dtype is the same as `x1_values` . The shape should be :math:`(n,)`.
        - **y_shape** (Tensor) - A 1-D Tensor, A positive int tuple which specifies the shape of sparse tensor.
          The dtype is int64, the values is the same as `x1_shape`.

    Raises:
        TypeError: If dtype of `x1_indices`, `x1_shape` is not the same and neither int32 nor int64.
        TypeError: If dtype of `x2_indices`, `x2_shape` is not the same and not int32 or int64.
        TypeError: If type of `x1_values`, `x2_values`, `x3_dense` is not the same.
        TypeError: If dtype of `x1_values`, `x2_values`, `x3_dense` is not uint8, int8, int16, int32, int64, float32,
                   float64.
        ValueError: If shape of `x1_indices`, `x2_indices` is not (2, n).
        ValueError: If shape of `x1_values`, `x2_values` is not (n,).
        ValueError: If dim0 size of `x1_values` is not the same with dim1 size of `x1_indices`.
        ValueError: If dim0 size of `x2_values` is not the same with dim1 size of `x2_indices`.
        ValueError: If shape of `x1_shape` or shape of `x2_shape` is not (2,).
        ValueError: If dim of `x3_dense` is not 2D.
        ValueError: If dtype of `alpha` is not the same with `x2_values` dtype, and alpha value convert to the
                    `x2_values` dtype overflow.
        TypeError: If dtype of `alpha`, `beta` is not uint8, uint16, uint32, uint64, int8, int16, int32, int64,
                   float16, float32, float64.
        TypeError: If the `x1_values` dtype is byte, char, short, int, long, while the dtype of beta is float16,
                   float32 or float64.
        ValueError: If the shape of `alpha`, `beta` is not () or (1,).

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> x1_indices = Tensor(np.array([[0, 1], [0, 1]]), mstype.int64)
        >>> x1_values = Tensor(np.array([1, 2]), mstype.int32)
        >>> x1_shape = Tensor(np.array([3, 3]), mstype.int64)
        >>> x2_indices = Tensor(np.array([[0, 1], [2, 2]]), mstype.int64)
        >>> x2_values = Tensor(np.array([3, 4]), mstype.int32)
        >>> x2_shape = Tensor(np.array([3, 3]), mstype.int64)
        >>> x3_dense = Tensor(np.array([[1, 2, 3], [1, 3, 2], [3, 2, 1]]), mstype.int32)
        >>> alpha = Tensor(np.array(1), mstype.int32)
        >>> beta = Tensor(np.array(1), mstype.int32)
        >>> sspaddmm = ops.Sspaddmm()
        >>> out_indices, out_values, out_shapes = sspaddmm(x1_indices, x1_values, x1_shape,
        ... x2_indices, x2_values, x2_shape, x3_dense, alpha, beta)
        >>> print(out_indices)
        [[0 1 0 0 0 1 1 1]
         [0 1 0 1 2 0 1 2]]
        >>> print(out_values)
        [ 1  2  9  6  3 12  8  4]
        >>> print(out_shapes)
        [3 3]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Sspaddmm."""
        self.init_prim_io_names(inputs=['x1_indices', 'x1_values', 'x1_shape', 'x2_indices', 'x2_values', 'x2_shape',
                                        'x3_dense', 'alpha', 'beta'], outputs=['y_indices', 'y_values', 'y_shape'])


class SparseConcat(Primitive):
    """
    concatenates the input SparseTensor(COO format) along the specified dimension. demo API now

    Args:
        concat_dim(Scalar) - A Scalar, decide the dimension to concatenation along.
        The value must be in range [-rank, rank), where rank is the number of dimensions in each input
        SparseTensor. Support int32, int64.

    Inputs:

        - **sp_input_indices** (Tensor) - the list of Tensor which means COOTensor indices, and Need to
            concatenates.
        - **sp_input_values** (Tensor) - the list of Tensor which means COOTensor values, and
            need to concatenates.
        - **sp_input_shape** (Tensor) - the list of Tensor which means COOTensor shape, and
            need to concatenates.

    Outputs:
        - **output_indices** (Tensor) - the result of concatenates the input SparseTensor along the
            specified dimension. This is the indices of output COOTensor
        - **output_values** (Tensor) - the result of concatenates the input SparseTensor along the
            specified dimension. This is the values of output COOTensor
        - **output_shape** (Tensor) - the result of concatenates the input SparseTensor along the
            specified dimension. This is the shape of output COOTensor

    Raises:
        ValueError: If only one sparse tensor input.
        Error: If input axis value is not in range [-rank, rank).

    Supported Platforms:
        ``CPU``

    Examples:
        >>> indics0 = Tensor([[0, 1], [1, 2]], dtype=mstype.int32)
        >>> values0 = Tensor([1, 2], dtype=mstype.int32)
        >>> shape0 = Tensor([3, 4], dtype=mstype.int64)
        >>> indics1 = Tensor([[0, 0], [1, 1]], dtype=mstype.int32)
        >>> values1 = Tensor([3, 4], dtype=mstype.int32)
        >>> shape1 = Tensor([3, 4], dtype=mstype.int64)
        >>> sparse_concat = ops.SparseConcat(0)
        >>> out = sparse_concat((indices0, indices1), (values0, values1), (shape0, shape1))
        >>> print(out)
        shape = [3 4]
        [0 1]: "1"
        [0 4]: "3"
        [1 2]: "4"
        [1 5]: "2"
    """
    @prim_attr_register
    def __init__(self, concat_dim=0):
        """Initialize SparseConcat."""
        self.init_prim_io_names(inputs=['sp_input_indices', 'sp_input_values', 'sp_input_shapes'],
                                outputs=['output_indices', 'output_values', 'output_shape'])
        validator.check_value_type("concat_dim", concat_dim, [int], self.name)


class SparseSegmentSqrtN(Primitive):
    """
    Computes the sum along sparse segments of a tensor divided by the sqrt of N.
    N is the size of the segment being reduced.

    Inputs:
        - **x** (Tensor) - A tensor.
        - **indices** (Tensor) - Indices is a 1-D tensor. Must be one of the following types: int32, int64.
          Has same rank as segment_ids. The shape should be :math:`(N,)`.
        - **segment_ids** (Tensor) - Segment_ids is a 1-D tensor. Must be one of the following types: int32, int64.
          Values should be sorted and can be repeated. The shape should be :math:`(N,)`.

    Outputs:
        A Tensor. Has the same type as `x` .
        Has same shape as `x`, except for dimension 0 which is the number of segments.

    Raises:
        TypeError: If `x` or `indices` or `segment_ids` is not a tensor.
        TypeError: If the dtype of `x` is not any of the following data types: {float16, float32, float64}.
        TypeError: If the dtype of `indices` is not int32 or int64.
        TypeError: If the dtype of `segment_ids` is not int32 or int64.
        ValueError: If dimension size of `x` less than 1.
        ValueError: If any of `indices` and `segment_ids` is not a 1-D tensor.
        ValueError: If shape[0] of `indices` is not corresponding to shape[0] of `segment_ids`.
        ValueError: If indices in `segment_ids` are not contiguous or do not start from 0.
        ValueError: If `segment_ids` is not sorted.
        ValueError: If `indices` is out of range of x's first shape.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> x = Tensor(np.array([[1,2,3,4],[5,6,7,8],[9,10,11,12]]).astype(np.float32))
        >>> indices = Tensor(np.array([0,1,2]).astype(np.int32))
        >>> segment_ids = Tensor(np.array([0,1,2]).astype(np.int32))
        >>> sparse_segment_sqrt_n = SparseSegmentSqrtN()
        >>> output = sparse_segment_sqrt_n(x, indices, segment_ids)
        >>> print(output)
        [[ 1.  2.  3.  4.]
        [ 5.  6.  7.  8.]
        [ 9. 10. 11. 12.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SparseSegmentSqrtN"""
        self.init_prim_io_names(
            inputs=['x', 'indices', 'segment_ids'], outputs=['y'])


class SparseSegmentSqrtNWithNumSegments(Primitive):
    """
    Computes the sum along sparse segments of a tensor divided by the sqrt of N.
    N is the size of the segment being reduced.
    Like SparseSegmentSqrtN, but allows missing ids in segment_ids.
    If an id is missing, the output tensor at that position will be zeroed.

    Inputs:
        - **x** (Tensor) - A Tensor.
        - **indices** (Tensor) - 1-D Tensor. Must be one of the following types: int32, int64.
          Has same rank as segment_ids. The shape should be :math:`(N,)`.
        - **segment_ids** (Tensor) - Segment_ids: 1-D Tensor. Must be one of the following types: int32, int64.
          Values should be sorted and can be repeated. The shape should be :math:`(N,)`.
        - **num_segments** (Tensor) - Num_segments should equal the number of distinct segment_ids.

    Outputs:
        A Tensor. Has the same type as `x` .
        Has same shape as `x`, except for dimension 0 which is the value of `num_segments`.

    Raises:
        TypeError: If `x` or `indices` or `segment_ids` or `num_segments` is not a tensor.
        TypeError: If the dtype of `x` is not any of the following data types: {float16, float32, float64}.
        TypeError: If the dtype of `indices` and `segment_ids` and `num_segments` is not int32 or int64.
        TypeError: If dtype of `segment_ids` and `indices` mismatch.
        TypeError: If dtype of `num_segments` and `indices` mismatch.
        ValueError: If dimension size of `x` less than 1.
        ValueError: If any of `indices` and `segment_ids` is not a 1-D tensor.
        ValueError: If rank of `num_segments` is bigger than 1.
        ValueError: If numelements of `num_segments` is not 1.
        ValueError: If the first dimension of `indices` is not equal to the first dimension of `segment_ids`.
        ValueError: If `segment_ids` is not sorted.
        ValueError: If the last number of `segment_ids` is bigger than or equal to `num_segments`.
        ValueError: If `indices` is out of range of x's first shape.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> x = Tensor([[0, 1, 0, 0], [0, 1, 1, 0], [1, 0, 1, 0]], dtype=ms.float16)
        >>> indices = Tensor([0, 2, 1], dtype=ms.int32)
        >>> segment_ids = Tensor([0, 1, 2], dtype=ms.int32)
        >>> num_segments = Tensor([4], dtype=ms.int32)
        >>> sparse_segment_sqrt_n_with_num_segments = SparseSegmentSqrtNWithNumSegments()
        >>> output = sparse_segment_sqrt_n_with_num_segments(x, indices, segment_ids, num_segments)
        >>> print(output)
        [[0. 1. 0. 0.]
         [1. 0. 1. 0.]
         [0. 1. 1. 0.]
         [0. 0. 0. 0.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SparseSegmentSqrtNWithNumSegments"""
        self.init_prim_io_names(
            inputs=['x', 'indices', 'segment_ids', 'num_segemnts'], outputs=['y'])


class SparseMatrixNNZ(Primitive):
    r"""
    Count number of the non-zero elements in sparse matrix or sparse matrixs.
    If the sparse matrix input contains batch dimension, then output dimension will be same with the batch dimension.

    Note:
        It is assumed that all the inputs can form a legal CSR sparse matrix, otherwise this operator won't work.

    Inputs:
        - **x_dense_shape** (Tensor) -  A 1-D Tensor. It represents the dense form shape of
          the input CSR sparse matrix, the shape of which should be :math:`(2,)` or :math:`(3,)`.
        - **x_batch_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix is of
          batch size `n`, it should have shape :math:`(n+1,)`, while the `i`-th element of which stores
          acummulated counts of nonzero values of the first `i - 1` batches.
        - **x_row_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix is of
          batch size `n` and row number `m`, it can be divided into `n` parts, each part of length
          `m + 1`. The `i`-th element of each :math:`(m+1,)` vector stores acummulated counts of
          nonzero values of the first `i - 1` rows in the corresponding batch.
        - **x_col_indices** (Tensor) - A 1-D Tensor. It represents column indices of the nonzero values
          in the input CSR sparse matrix.
        - **x_values** (Tensor) - A 1-D Tensor. It represents all the nonzero values in the
          input CSR sparse matrix.

    Outputs:
        Tensor, the dtype is int32.
        If there are n batch within input sparse matrix, the shape is :math:`(n,)`.

    Raises:
        TypeError: If the dtype of `x_dense_shape`, `x_batch_pointers`, `x_row_pointers` or `x_col_indices`
            is not int32 or int64, or the dtypes of above inputs are not the same.
        TypeError: If the dtype of `x_values` is not supported.
        TypeError: If any of the inputs is not a tensor.
        ValueError: If any of the inputs is not 1-D.
        ValueError: If `x_values` and `x_col_indices` have different length.
        ValueError: If shape[0] of `x_dense_shape` is not 2 or 3.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> dense_shape = Tensor([2,3], dtype=mstype.int32)
        >>> batch_pointers = Tensor([0,1], dtype=mstype.int32)
        >>> row_pointers = Tensor([0,1,1], dtype=mstype.int32)
        >>> col_indices = Tensor([0], dtype=mstype.int32)
        >>> values = Tensor([99], dtype=mstype.float32)
        >>> sparse_matrix_nnz = ops.SparseMatrixNNZ()
        >>> out = sparse_matrix_nnz(dense_shape, batch_pointers, row_pointers, col_indices, values)
        >>> print(out)
        [1]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SparseMatrixNNZ"""
        self.init_prim_io_names(
            inputs=['x_dense_shape', 'x_batch_pointers', 'x_row_pointers', 'x_col_indices', 'x_values'], outputs=['y'])


class SparseAdd(Primitive):
    """
    Computes the sum of a COOTensor and another COOTensor.

    Inputs:
        - **x1_indices** (Tensor) - represents the first COOTensor's indices.
        - **x1_values** (Tensor) - represents the first COOTensor's values.
        - **x1_shape** (Tensor) - represents the first COOTensor's dense shape.
        - **x2_indices** (Tensor) - represents the second COOTensor's indices.
        - **x2_values** (Tensor) - represents the second COOTensor's values.
        - **x2_shape** (Tensor) - represents the second COOTensor's dense shape.
        - **thresh** (Tensor) - represents the magnitude threshold that determines if an output
          value/index pair take space.

    Outputs:
        - **sum_indices** (Tensor) - this is the indices of the sum.
        - **sum_values** (Tensor) - this is the values of the sum.
        - **sum_shape** (Tensor) - this is the shape of the sum.

    Raises:
        ValueError: If (x1_indices/x2_indices)'s dim is not equal to 2.
        ValueError: If (x1_values/x2_values)'s dim is not equal to 1.
        ValueError: If (x1_shape/x2_shape)'s dim is not equal to 1.
        ValueError: If thresh's dim is not equal to 0.
        TypeError: If (x1_indices/x2_indices)'s type is not equal to int64.
        TypeError: If (x1_shape/x2_shape)'s type is not equal to int64.
        ValueError: If (x1_indices/x2_indices)'s length is not equal to
            (x1_values/x2_values)'s length.
        TypeError: If (x1_values/x2_values)'s type is not equal to anf of
            (int8/int16/int32/int64/float32/float64/complex64/complex128).
        TypeError: If thresh's type is not equal to anf of
            (int8/int16/int32/int64/float32/float64).
        TypeError: If x1_indices's type is not equal to x2_indices's type.
        TypeError: If x1_values's type is not equal to x2_values's type.
        TypeError: If x1_shape's type is not equal to x2_shape's type.
        TypeError: If (x1_values/x2_values)'s type is not matched with thresh's type.

    Supported Platforms:
        ``CPU`` ``GPU``

    Examples:
        >>> from mindspore import Tensor
        >>> from mindspore import dtype as mstype
        >>> from mindspore.ops.operations.sparse_ops import SparseAdd
        >>> indics0 = Tensor([[0, 1], [1, 2]], dtype=mstype.int64)
        >>> values0 = Tensor([1, 2], dtype=mstype.int32)
        >>> shape0 = Tensor([3, 4], dtype=mstype.int64)
        >>> indics1 = Tensor([[0, 0], [1, 1]], dtype=mstype.int64)
        >>> values1 = Tensor([3, 4], dtype=mstype.int32)
        >>> shape1 = Tensor([3, 4], dtype=mstype.int64)
        >>> thres = Tensor(0, dtype=mstype.int32)
        >>> sparse_add = SparseAdd()
        >>> out = sparse_add(indics0, values0, shape0, indics1, values1, shape1, thres)
        >>> print(out)
        (Tensor(shape=[4, 2], dtype=Int64, value=[[0, 0], [0, 1], [1, 1], [1, 2]]),
        Tensor(shape=[4], dtype=Int32, value=[3, 1, 4, 2]),
        Tensor(shape=[2], dtype=Int64, value=[3, 4]))
    """
    @prim_attr_register
    def __init__(self):
        self.init_prim_io_names(
            inputs=["x1_indices", "x1_values", "x1_shape", "x2_indices", "x2_values", "x2_shape", "thresh"],
            outputs=["sum_indices", "sum_values", "sum_shape"])


class SparseMatrixSoftmax(Primitive):
    """
    Calculates the softmax of a CSRTensorMatrix.

    Args:
        logits (CSRTensor): Sparse CSR Tensor.
        dtype (dtype): Data type.

    Returns:
        CSRTensor. a csr_tensor containing:
        indptr: indicates the start and end point for `values` in each row.
        indices: the column positions of all non-zero values of the input.
        values: the non-zero values of the dense tensor.
        shape: the shape of the csr_tensor.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> import mindspore.common.dtype as mstype
        >>> from mindspore import Tensor, CSRTensor
        >>> from mindspore.ops.functional import sparse_matrix_softmax
        >>> logits_indptr = Tensor([0, 4, 6], dtype=mstype.int32)
        >>> logits_indices = Tensor([0, 2, 3, 4, 3, 4], dtype=mstype.int32)
        >>> logits_values = Tensor([1, 2, 3, 4, 1, 2], dtype=mstype.float32)
        >>> shape = (2, 6)
        >>> logits = CSRTensor(logits_indptr, logits_indices, logits_values, shape)
        >>> out = logits.sparse_matrix_softmax(dtype)
        >>> print(out)
        CSRTensor(shape=[2,6], dtype=Float32,
                  indptr=Tensor(shape=[3], dtype=Int64, value = [0, 4, 6]),
                  indices=Tensor(shape=[2], dtype=Int64, value = [0, 2, 3, 4, 3, 4]),
                  values=Tensor(shape=[2], dtype=Float32,
                  value = [3.2058e-02, 8.7144e-02, 2.3688e-01, 6.4391e-01, 2.6894e-01, 7.310e-01]))
    """

    @prim_attr_register
    def __init__(self, dtype):
        '''Initialize for SparseMatrixSoftmax'''
        if not isinstance(dtype, (type(mstype.float32), type(mstype.single), type(mstype.float64),
                                  type(mstype.double))):
            raise TypeError("Only float32 and float64 type data are supported, but got {}".format(dtype))
        self.add_prim_attr("dtype", dtype)
        self.init_prim_io_names(inputs=['x_dense_shape', 'x_batch_pointers', 'x_row_pointers',
                                        'x_col_indices', 'x_values'],
                                outputs=['y_dense_shape', 'y_batch_pointers', 'y_row_pointers', 'y_col_indices',
                                         'y_values'])


class CSRSparseMatrixToDense(Primitive):
    """
    Converts a CSR sparse matrix(maybe batched) to its dense form.

    Note:
        It is assumed that all the inputs can form a legal CSR sparse matrix, otherwise this operator won't work.

    Inputs:
        - **x_dense_shape** (Tensor) - A 1-D Tensor. It represents the dense form shape of
          the input CSR sparse matrix, the shape of which should be :math:`(2,)` or :math:`(3,)`.
        - **x_batch_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix is of
          batch size `n`, it should have shape :math:`(n+1,)`, while the `i`-th element of which stores
          acummulated counts of nonzero values of the first `i - 1` batches.
        - **x_row_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix is of
          batch size `n` and row number `m`, it can be divided into `n` parts, each part of length
          `m + 1`. The `i`-th element of each :math:`(m+1,)` vector stores acummulated counts of
          nonzero values of the first `i - 1` rows in the corresponding batch.
        - **x_col_indices** (Tensor) - A 1-D Tensor. It represents column indices of the nonzero values
          in the input CSR sparse matrix.
        - **x_values** (Tensor) - A 1-D Tensor. It represents all the nonzero values in the
          input CSR sparse matrix.

    Outputs:
        Tensor, which is the dense form of the input CSR sparse matrix.
        Its dtype is the same as `x_values`.

    Raises:
        TypeError: If the dtype of `x_dense_shape`, `x_batch_pointers`, `x_row_pointers` or `x_col_indices`
            is not int32 or int64, or the dtypes of above inputs are not the same.
        TypeError: If the dtype of `x_values` is not float32, float64, complex64 or complex128.
        TypeError: If any of the inputs is not a tensor.
        ValueError: If any of the inputs is not 1-D.
        ValueError: If shape[0] of `x_dense_shape` is not 2 or 3.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> dense_shape = Tensor([2, 2], dtype=mindspore.int32)
        >>> batch_pointers = Tensor([0, 1], dtype=mindspore.int32)
        >>> row_pointers = Tensor([0, 1, 1], dtype=mindspore.int32)
        >>> col_indices = Tensor([1], dtype=mindspore.int32)
        >>> values = Tensor([1.], dtype=mindspore.float32)
        >>> csr_to_dense = ops.CSRSparseMatrixToDense()
        >>> out = csr_to_dense(dense_shape, batch_pointers, row_pointers, col_indices, values)
        >>> print(out)
        [[0. 1.]
         [0. 0.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize CSRSparseMatrixToDense"""
        self.init_prim_io_names(
            inputs=['x_dense_shape', 'x_batch_pointers', 'x_row_pointers', 'x_col_indices', 'x_values'],
            outputs=['y'])


class SparseMatrixTranspose(Primitive):
    r"""
    Return the transpose of sparse matrix or sparse matrixs.
    If the sparse matrix input contains batch dimension, then output dimension will be same with the batch dimension.
    The rank of sparse matrix input must be equal to `2` or `3`.

    Note:
        It is assumed that all the inputs can form a legal CSR sparse matrix, otherwise this operator is not defined.

    Args:
        conjugate (bool): If True, the output sparse tensor is conjugated . Default: False.

    Inputs:
        - **dense_shape** (Tensor) - A 1-D Tensor, represents the shape of input sparse matrix under dense status.
          Support int32, int64. The shape is :math:`(2,)` or :math:`(3,)`.
        - **batch_pointers** (Tensor) - A 1-D Tensor, represents the non-zero elements number in each batch.
          Support int32, int64, takes on values: :math:`(0, nnz[0], nnz[0] + nnz[1], ..., total\_nnz)`.
          If there are `n` batch within input sparse matrix, the shape is :math:`(n+1)`.
        - **row_pointers** (Tensor) - A 1-D Tensor, represents the non-zero elements of each row.
          Support int32, int64, takes on values:
          :math:`(0, num\_rows\{b\}[0], num\_rows\{b\}[0] + num\_rows\{b\}[1], ..., nnz[b])`,
          for :math:`b = 0, ..., n - 1`.
          If there are `n` batch within input sparse matrix and dense shape is :math:`(rows,cols)`,
          the shape is :math:`((rows + 1) * n)`.
          Note: num_rows{0}[0] means the non-zero elements number in the first row of first sparse matrix.
        - **col_indices** (Tensor) - A 1-D Tensor, represents the column values for the given row and column index.
          Support int32, int64. The shape is :math:`(M)`,
          where `M` is the number of non-zero elements in all input sparse matrix.
        - **values** (Tensor) - A 1-D Tensor, represents the actual values for the given row and column index.
          Support BasicType. The shape is :math:`(M)`, where `M` is the number of non-zero elements in all
          input sparse matrix.

    Outputs:
        - **dense_shape** (Tensor) - A 1-D Tensor, represents the shape of output sparse matrix under dense status.
          Support int32, int64. The shape is the same as the input sparse matrix.
        - **batch_pointers** (Tensor) - A 1-D Tensor, which is the same as the input sparse matrix's batch_pointers.
        - **row_pointers** (Tensor) - A 1-D Tensor, represents the non-zero elements of each row of output sparse
          matrix. Support int32, int64, takes on values:
          :math:`(0, num\_rows\{b\}[0], num\_rows\{b\}[0] + num\_rows\{b\}[1], ..., nnz[b])`,
          for :math:`b = 0, ..., n - 1`.
          If there are `n` batch within output sparse matrix and dense shape is :math:`(rows,cols)`,
          the shape is :math:`((rows + 1) * n)`.
          Note: num_rows{0}[0] means the non-zero elements number in the first row of first sparse matrix.
        - **col_indices** (Tensor) - A 1-D Tensor, represents the column values for the given row and column index.
          Support int32, int64. The shape is :math:`(M)`,
          where `M` is the number of non-zero elements in all input sparse matrix.
        - **values** (Tensor) - A 1-D Tensor, which is the same as the input sparse matrix's values.

    Raises:
        TypeError: If dtype of `values` doesn't meet the parameter description.
        TypeError: The data type of `dense_shape, batch_pointers, row_pointers, col_indices` is not int32 or int64.
        ValueError: If rank of `dense_shape` is not 2 or 3.
        TypeError: The input data should have the correct CSR form.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> from mindspore.ops import operations as ops
        >>> dense_shape = Tensor([2,3], dtype=ms.int32)
        >>> batch_pointers = Tensor([0,1], dtype=ms.int32)
        >>> row_pointers = Tensor([0,1,1], dtype=ms.int32)
        >>> col_indices = Tensor([0], dtype=ms.int32)
        >>> values = Tensor([99], dtype=ms.float32)
        >>> sparse_matrix_transpose = ops.SparseMatrixTranspose()
        >>> output = sparse_matrix_transpose(dense_shape, batch_pointers, row_pointers, col_indices, values)
        >>> print(output[0])
        [3 2]
        >>> print(output[1])
        [0 1]
        >>> print(output[2])
        [0 1 1 1]
        >>> print(output[3])
        [0]
        >>> print(output[4])
        [99.]
    """
    @prim_attr_register
    def __init__(self, conjugate=False):
        """Initialize SparseMatrixTranspose"""
        validator.check_value_type("conjugate", conjugate, [bool], self.name)
        self.add_prim_attr("max_length", 100000000)
        self.init_prim_io_names(inputs=['x_dense_shape', 'x_batch_pointers', 'x_row_pointers',
                                        'x_col_indices', 'x_values'],
                                outputs=['y_dense_shape', 'y_batch_pointers', 'y_row_pointers',
                                         'y_col_indices', 'y_values'])


class SparseSparseMinimum(Primitive):
    r"""
    Returns the element-wise min of two SparseTensors.

    Inputs:
        - **x1_indices** (Tensor) - A 2-D Tensor. It represents the position of the non-zero element
          in the first sparse tensor.
        - **x1_values** (Tensor) - A 1-D Tensor. It represents the value corresponding to the position
          in the `x1_indices`, the shape of which should be :math:`(N,)`.
        - **x1_shape** (Tensor) - A 1-D Tensor. It represents the shape of the input sparse tensor,
          the shape of which should be :math:`(N,)`.
        - **x2_indices** (Tensor) - A 2-D Tensor. It represents the position of the non-zero element
          in the second sparse tensor.
        - **x2_values** (Tensor) - A 1-D Tensor. It represents the value corresponding to the position
          in the `x2_indices`, the shape of which should be :math:`(N,)`.
        - **x2_shape** (Tensor) - A 1-D Tensor. It represents the shape of the input sparse tensor,
          the shape of which should be :math:`(N,)`.

    Outputs:
        - **y_indices** (Tensor) - A 2-D Tensor. It represents the position of the element-wise min of
          two input tensors.
        - **y_values** (Tensor) - A 1-D Tensor. It represents the value corresponding to the position
          in the `y_indices`.

    Raises:
        TypeError: The dtype of `x1_indices`, `x1_shape`, `x2_indices` or `x2_shape` is wrong.
        TypeError: The dtype of `x1_values` or `x2_values` is wrong.
        TypeError: If `x1_indices`, `x1_values`, `x1_shape`, `x2_indices`, `x2_values`, `x2_shape`
                    is not a tensor.
        TypeError: If `x1_indices` is not a 2-D tensor.
        TypeError: If `x2_indices` is not a 2-D tensor.
        ValueError: If any of `x1_values` and `x1_shape` is not a 1-D tensor.
        ValueError: If shape[0] of `x1_indices` is not corresponding to shape[0] of `x1_values`.
        ValueError: If shape[1] of `x1_indices` is not corresponding to shape[0] of `x1_shape`.
        ValueError: If any of `x2_values` and `x2_shape` is not a 1-D tensor.
        ValueError: If shape[0] of `x2_indices` is not corresponding to shape[0] of `x2_values`.
        ValueError: If shape[1] of `x2_indices` is not corresponding to shape[0] of `x2_shape`.
        ValueError: If shape[0] of `x1_shape` is not corresponding to shape[0] of `x2_shape`.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> from mindspore.ops.operations.sparse_ops import SparseSparseMinimum
        >>> x1_indices = Tensor(np.array([[0, 0, 0], [0, 1, 0], [0, 1, 1]]).astype(np.int64))
        >>> x1_values = Tensor([1, 2, 3], dtype=mstype.float32)
        >>> x1_shape = Tensor(np.array([2, 2, 2]).astype(np.int64))
        >>> x2_indices = Tensor(np.array([[0, 0, 0], [0, 1, 0], [1, 0, 0]]).astype(np.int64))
        >>> x2_values = Tensor([2, 4, 5], dtype=mstype.float32)
        >>> x2_shape = Tensor(np.array([2, 2, 2]).astype(np.int64))
        >>> sparse_sparse_minimum = ops.SparseSparseMinimum()
        >>> out = sparse_sparse_minimum(x1_indices, x1_values, x1_shape, x2_indices, x2_values, x2_shape)
        >>> print(out[0])
        [[0 0 0]
         [0 1 0]
         [0 1 1]
         [1 0 0]]
        >>> print(out[1])
        [1. 2. 0. 0.]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SparseSparseMinimum."""
        self.init_prim_io_names(inputs=['x1_indices', 'x1_values', 'x1_shape', 'x2_indices', 'x2_values', 'x2_shape'],
                                outputs=['y_indices', 'y_values'])


class SparseTensorToCSRSparseMatrix(Primitive):
    """
    Converts a sparse tensor to its CSR sparse matrix(maybe batched) form.

    Inputs:
        - **x_indices** (Tensor) - A 2-D Tensor. It represents the position of the non-zero element
          in the sparse tensor. Support int32, int64.
        - **x_values** (Tensor) - A 1-D Tensor. It represents the value corresponding to the position
          in the `x_indices`, the shape of which should be :math:`(N,)`.
        - **x_dense_shape** (Tensor) - A 1-D Tensor. It represents the dense form shape of
          the input sparse tensor. Its shape should be :math:`(2,)` or :math:`(3,)`. Support int32, int64.
    Outputs:
        - **y_dense_shape** (Tensor) - A 1-D Tensor. It represents the dense form shape of
          the output CSR sparse matrix, the shape of which should be :math:`(2,)` or :math:`(3,)`.
        - **y_batch_pointers** (Tensor) - A 1-D Tensor. Supposing the output CSR sparse matrix is of
          batch size `n`, it should have shape :math:`(n+1,)`, while the `i`-th element of which stores
          acummulated counts of non-zero values of the first `i - 1` batches.
        - **y_row_pointers** (Tensor) - A 1-D Tensor. Supposing the output CSR sparse matrix is of
          batch size `n` and row number `m`, it can be divided into `n` parts, each part of length
          `m + 1`. The `i`-th element of each :math:`(m+1,)` vector stores acummulated counts of
          non-zero values of the first `i - 1` rows in the corresponding batch.
        - **y_col_indices** (Tensor) - A 1-D Tensor. It represents column indices of the non-zero values
          in the output CSR sparse matrix.
        - **y_values** (Tensor) - A 1-D Tensor. It represents all the non-zero values in the
          output CSR sparse matrix.

    Raises:
        TypeError: If the dtype of `x_indices` or `x_dense_shape` is not int32 or int64.
        TypeError: If the dtype of `x_values` is not one of: float32, float64, complex64 or complex128.
        ValueError: If `x_indices` or `x_values` or `x_dense_shape` is not a tensor.
        ValueError: If any of `x_values` and `x_dense_shape` is not a 1-D tensor.
        ValueError: If `x_indices` is not a 2-D tensor.
        ValueError: If shape[0] of `x_indices` is not corresponding to shape[0] of `x_values`.
        ValueError: If shape[1] of `x_indices` is not corresponding to shape[1] of `x_dense_shape`.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> from mindspore.ops.operations.sparse_ops import SparseTensorToCSRSparseMatrix
        >>> x_indices = Tensor(np.array([[0, 0, 1], [0, 1, 2], [0, 1, 3], [1, 0, 1], [1, 1, 2],\
                                         [1, 1, 3]]).astype(np.int64))
        >>> x_values = Tensor(np.array([1, 4, 3, 1, 4, 3]).astype(np.float32))
        >>> x_dense_shape = Tensor(np.array([2, 2, 4]).astype(np.int64))
        >>> sparse_tensor_to_csr_sparse_matrix = SparseTensorToCSRSparseMatrix()
        >>> out = sparse_tensor_to_csr_sparse_matrix(x_indices, x_values, x_dense_shape)
        >>> print(out[0])
        [2 2 4]
        >>> print(out[1])
        [0 3 6]
        >>> print(out[2])
        [0 1 3 0 1 3]
        >>> print(out[3])
        [1 2 3 1 2 3]
        >>> print(out[4])
        [1. 4. 3. 1. 4. 3.]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SparseTensorToCSRSparseMatrix."""
        self.init_prim_io_names(
            inputs=['x_indices', 'x_values', 'x_dense_shape'],
            outputs=['y_dense_shape', 'y_batch_pointers', 'y_row_pointers', 'y_col_indices', 'y_values'])


class SparseMatrixSparseMatMul(Primitive):
    r"""
    Performs a matrix multiplication of a sparse matrix x1 with sparse matrix x2; return a sparse matrix x1*x2.
    Each matrix may be transposed or adjointed (conjugated and transposed),
    according to the Boolean parameters transpose_a,adjoint_a,transpose_b and adjoint_b.
    At most one of transpose_a or adjoint_a may be True. Similarly, at most one of transpose_b or adjoint_b may be True.

    Args:
        transpose_a (bool): If true, sparse tensor x1 is transposed before multiplication. Default: False.
        transpose_b (bool): If true, dense tensor x2 is transposed before multiplication. Default: False.
        adjoint_a (bool): If true, sparse tensor x1 is adjointed before multiplication. Default: False.
        adjoint_b (bool): If true, dense tensor x2 is adjointed before multiplication. Default: False.

    Inputs:
        - **x1_dense_shape** (Tensor) - A 1-D Tensor, represents the shape of input sparse matrix x1 under dense status.
          Support int32, int64. The shape is :math:`(2)` or :math:`(3)`.
        - **x1_batch_pointers** (Tensor) - A 1-D Tensor, represents the non-zero elements number in each batch.
          Support int32, int64, takes on values: :math:`(0, nnz[0], nnz[0] + nnz[1], ..., total\_nnz)`.
          If there are `n` batch within input sparse matrix x1, the shape is :math:`(n+1)`.
        - **x1_row_pointers** (Tensor) - A 1-D Tensor, represents the non-zero elements of each row.
          Support int32, int64, takes on values:
          :math:`(0, num\_rows\{b\}[0], num\_rows\{b\}[0] + num\_rows\{b\}[1], ..., nnz[b])`,
          for :math:`b = 0, ..., n - 1`.
          If there are `n` batch within input sparse matrix x1 and dense shape is :math:`(rows,cols)`,
          the shape is :math:`((rows + 1) * n)`.
          Note: num_rows{0}[0] means the non-zero elements number in the first row of first sparse matrix x1.
        - **x1_col_indices** (Tensor) - A 1-D Tensor, represents the column values for the given row and column index.
          Support int32, int64. The shape is :math:`(M)`,
          where `M` is the number of non-zero elements in  input sparse matrix x1.
        - **x1_values** (Tensor) - A 1-D Tensor, represents the actual values for the given row and column index.
          Support float32, double, complex64, complex128.
          The shape is :math:`(M)`, where `M` is the number of non-zero elements in input sparse matrix x1.

          **x2_dense_shape** (Tensor) - B 1-D Tensor, represents the shape of input sparse matrix x2 under dense status.
          Support int32, int64. The shape is :math:`(2)` or :math:`(3)`.
        - **x2_batch_pointers** (Tensor) - B 1-D Tensor, represents the non-zero elements number in each batch.
          Support int32, int64, takes on values: :math:`(0, nnz[0], nnz[0] + nnz[1], ..., total\_nnz)`.
          If there are `n` batch within input sparse matrix x2, the shape is :math:`(n+1)`.
        - **x2_row_pointers** (Tensor) - B 1-D Tensor, represents the non-zero elements of each row.
          Support int32, int64, takes on values:
          :math:`(0, num\_rows\{b\}[0], num\_rows\{b\}[0] + num\_rows\{b\}[1], ..., nnz[b])`,
          for :math:`b = 0, ..., n - 1`.
          If there are `n` batch within input sparse matrix x2 and dense shape is :math:`(rows,cols)`,
          the shape is :math:`((rows + 1) * n)`.
          Note: num_rows{0}[0] means the non-zero elements number in the first row of sparse matrix x2.
        - **x2_col_indices** (Tensor) - B 1-D Tensor, represents the column values for the given row and column index.
          Support int32, int64. The shape is :math:`(M)`,
          where `M` is the number of non-zero elements in  input sparse matrix x2.
        - **x2_values** (Tensor) - B 1-D Tensor, represents the actual values for the given row and column index.
          Support float32, double, complex64, complex128.
          The shape is :math:`(M)`, where `M` is the number of non-zero elements in input sparse matrix x2.

    Outputs:
        - **y_dense_shape** (Tensor) - B 1-D Tensor, represents the shape of output sparse matrix y under dense status.
          Support int32, int64. The shape is :math:`(2)` or :math:`(3)`.
        - **y_batch_pointers** (Tensor) - B 1-D Tensor, represents the non-zero elements number in each batch.
          Support int32, int64, takes on values: :math:`(0, nnz[0], nnz[0] + nnz[1], ..., total\_nnz)`.
          If there are `n` batch within output sparse matrix y, the shape is :math:`(n+1)`.
        - **y_row_pointers** (Tensor) - B 1-D Tensor, represents the non-zero elements of each row.
          Support int32, int64, takes on values:
          :math:`(0, num\_rows\{b\}[0], num\_rows\{b\}[0] + num\_rows\{b\}[1], ..., nnz[b])`,
          for :math:`b = 0, ..., n - 1`.
          If there are `n` batch within output sparse matrix y and dense shape is :math:`(rows,cols)`,
          the shape is :math:`((rows + 1) * n)`.
          Note: num_rows{0}[0] means the non-zero elements number in the first row of sparse matrix y.
        - **y_col_indices** (Tensor) - B 1-D Tensor, represents the column values for the given row and column index.
          Support int32, int64. The shape is :math:`(M)`,
          where `M` is the number of non-zero elements in  output sparse matrix y.
        - **y_values** (Tensor) - B 1-D Tensor, represents the actual values for the given row and column index.
          Support float32, double, complex64, complex128.
          The shape is :math:`(M)`, where `M` is the number of non-zero elements in output sparse matrix y.

    Raises:
        TypeError: If any dtype of `x1_dense_shape`, `x1_batch_pointers`, `x1_row_pointers`, `x1_col_indices`,
        `x1_values` or `x2_dense_shape`, `x2_batch_pointers`, `x2_row_pointers`, `x2_col_indices`,
        `x2_values` doesn't meet the parameter description.
        ValueError: If rank of `x1_dense_shape` or `x2_dense_shape' is not 2 or 3.

    Supported Platforms:


    Examples:
        >>> from mindspore.ops.operations.sparse_ops import SparseMatrixSparseMatMul
        >>> x1_dense_shape = Tensor([4, 5], dtype=mindspore.int32)
        >>> x1_batch_pointers = Tensor([0, 4], dtype=mindspore.int32)
        >>> x1_row_pointers = Tensor([0, 1, 1, 3, 4], dtype=mindspore.int32)
        >>> x1_col_indices = Tensor([0, 3, 4, 0], dtype=mindspore.int32)
        >>> x1_values = Tensor([1.0, 5.0, -1.0, -2.0], dtype=mindspore.float32)
        >>> x2_dense_shape = Tensor([5, 3], dtype=mindspore.int32)
        >>> x2_batch_pointers = Tensor([0, 3], dtype=mindspore.int32)
        >>> x2_row_pointers = Tensor([0, 1, 1, 3, 3, 3], dtype=mindspore.int32)
        >>> x2_col_indices = Tensor([0, 0, 1], dtype=mindspore.int32)
        >>> x2_values = Tensor([2.0, 7.0, 8.0], dtype=mindspore.float32)
        >>> sparse_matrix_sparse_mat_mul = SparseMatrixSparseMatMul()
        >>> out_dense_shape, out_batch_pointers, out_row_pointers, out_col_indices, out_values =
        ... sparse_matrix_sparse_mat_mul(x1_dense_shape, x1_batch_pointers, x1_row_pointers, x1_col_indices, x1_values,
        ...                              x2_dense_shape, x2_batch_pointers, x2_row_pointers, x2_col_indices, x2_values)
        >>> print(out_dense_shape)
        [4 3]
        >>> print(out_batch_pointers)
        [0 2]
        >>> print(out_row_pointers)
        [0 1 1 1 2]
        >>> print(out_col_indices)
        [0 0]
        >>> print(out_values)
        [ 2. -4.]
    """

    @prim_attr_register
    def __init__(self, transpose_a=False, transpose_b=False, adjoint_a=False, adjoint_b=False):
        """Initialize SparseMatrixSparseMatMul"""
        validator.check_value_type("transpose_a", transpose_a, [bool], self.name)
        validator.check_value_type("transpose_b", transpose_b, [bool], self.name)
        validator.check_value_type("adjoint_a", adjoint_b, [bool], self.name)
        validator.check_value_type("adjoint_b", adjoint_b, [bool], self.name)
        self.init_prim_io_names(
            inputs=['x1_dense_shape', 'x1_batch_pointers', 'x1_row_pointers', 'x1_col_indices', 'x1_values',
                    'x2_dense_shape', 'x2_batch_pointers', 'x2_row_pointers', 'x2_col_indices', 'x2_values'],
            outputs=['y_dense_shape', 'y_batch_pointers', 'y_row_pointers', 'y_col_indices', 'y_values'])


class SparseMatrixMatMul(Primitive):
    r"""
    Performs a matrix multiplication of a sparse matrix x1 with dense matrix x2; return a dense matrix x1*x2.
    Each matrix may be transposed or adjointed (conjugated and transposed)
    according to the Boolean parameters transpose_x1,adjoint_x1,transpose_x2 and adjoint_x2.
    At most one of transpose_x1 or adjoint_x1 may be True.
    Similarly, at most one of transpose_x2 or adjoint_x2 may be True.

    Note:
        It is assumed that all the inputs can form a legal CSR sparse matrix, otherwise this operator is not defined.

    Args:
        transpose_x1 (bool): If true, sparse tensor x1 is transposed before multiplication. Default: False.
        transpose_x2 (bool): If true, dense tensor x2 is transposed before multiplication. Default: False.
        adjoint_x1 (bool): If true, sparse tensor x1 is adjointed before multiplication. Default: False.
        adjoint_x2 (bool): If true, dense tensor x2 is adjointed before multiplication. Default: False.
        transpose_output (bool): If true, output x1*x2 is tansposed. Default: False.
        conjugate_output (bool): If true, output x1*x2 is conjugated. Default: False.

    Inputs:
        - **x1_dense_shape** (Tensor) - A 1-D Tensor. It represents the dense form shape of
          the input CSR sparse matrix x1, the shape of which should be :math:`(2,)` or :math:`(3,)`.
        - **x1_batch_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix x1 is of
          batch size `n`, it should have shape :math:`(n+1,)`, while the `i`-th element of which stores
          acummulated counts of nonzero values of the first `i - 1` batches.
        - **x1_row_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix x1 is of
          batch size `n` and row number `m`, it can be divided into `n` parts, each part of length
          `m + 1`. The `i`-th element of each :math:`(m+1,)` vector stores acummulated counts of
          nonzero values of the first `i - 1` rows in the corresponding batch.
        - **x1_col_indices** (Tensor) - A 1-D Tensor. It represents column indices of the nonzero values
          in the input CSR sparse matrix x1.
        - **x1_values** (Tensor) - A 1-D Tensor. It represents all the nonzero values in the
          input CSR sparse matrix x1.

        - **x2_dense** (Tensor) - A 2-D or 3-D Tensor, represents the input dense matrix x2.
          Support float32, float64, complex64, complex128.

    Outputs:
        **y_dense** (Tensor) - A 2-D or 3-D Tensor, represents the output dense matrix y.
          Support float32, float64, complex64, complex128.

    Raises:
        TypeError: If any dtype of `x1_dense_shape`, `x1_batch_pointers`, `x1_row_pointers`, `x1_col_indices`,
        `x1_values` or `x2_dense` doesn't meet the parameter description.
        ValueError: If shape[0] of `x1_dense_shape` or the dimension of `x2_dense' is not 2 or 3.
        ValueError: If shape[0]-1 of x1_batch_pointers and shape[0] of x2_dense are not the same.

    Supported Platforms:


    Examples:
        >>> from mindspore.ops.operations.sparse_ops import SparseMatrixMatMul
        >>> x1_dense_shape = Tensor([4, 5], dtype=mindspore.int32)
        >>> x1_batch_pointers = Tensor([0, 4], dtype=mindspore.int32)
        >>> x1_row_pointers = Tensor([0, 1, 1, 3, 4], dtype=mindspore.int32)
        >>> x1_col_indices = Tensor([0, 3, 4, 0], dtype=mindspore.int32)
        >>> x1_values = Tensor([1.0, 5.0, -1.0, -2.0], dtype=mindspore.float32)
        >>> x2_dense = Tensor([[2.0, 0.8, 1.0],[ 2.9, 3.2, 0.0],[7.0, 4.6, 0.2],[3.5, 4.9, 1.4],[4.0, 3.7, 6.9]],
        ... dtype=mindspore.float32)
        >>> sparse_matrix_mat_mul = SparseMatrixMatMul()
        >>> out = sparse_matrix_mat_mul(x1_dense_shape, x1_batch_pointers, x1_row_pointers, x1_col_indices,
        ... x1_values, x2_dense)
        >>> print(out)
        [[ 2.         0.8        1.       ]
         [ 0.         0.         0.       ]
         [13.5       20.8        0.0999999]
         [-4.        -1.6       -2.       ]]
    """

    @prim_attr_register
    def __init__(self, transpose_x1=False, transpose_x2=False, adjoint_x1=False, adjoint_x2=False,
                 transpose_output=False, conjugate_output=False):
        """Initialize SparseMatrixMatMul"""
        validator.check_value_type("transpose_x1", transpose_x1, [bool], self.name)
        validator.check_value_type("transpose_x2", transpose_x2, [bool], self.name)
        validator.check_value_type("adjoint_x1", adjoint_x1, [bool], self.name)
        validator.check_value_type("adjoint_x2", adjoint_x2, [bool], self.name)
        validator.check_value_type("transpose_output", transpose_output, [bool], self.name)
        validator.check_value_type("conjugate_output", conjugate_output, [bool], self.name)
        self.init_prim_io_names(inputs=['x1_dense_shape', 'x1_batch_pointers', 'x1_row_pointers',
                                        'x1_col_indices', 'x1_values', 'x2_dense'], outputs=['y_dense'])


class SparseMatrixAdd(Primitive):
    """
    Addition of two CSR Tensors : C = alpha * A + beta * B

    Inputs:
        - **x1_dense_shape** (Tensor) - A 1-D Tensor represents the dense form shape of the input CSR sparse matrix.
        - **x1_batch_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix is of
          batch size `n`, it should have shape :math:`(n+1,)`, while the `i`-th element of which stores
          acummulated counts of non-zero values of the first `i - 1` batches.
        - **x1_row_pointers** (Tensor) - A 1-D Tensor. Supposing the input CSR sparse matrix is of
          batch size `n` and row number `m`, it can be divided into `n` parts, each part of length
          `m + 1`. The `i`-th element of each :math:`(m+1,)` vector stores acummulated counts of
          non-zero values of the first `i - 1` rows in the corresponding batch.
        - **x1_col_indices** (Tensor) - A 1-D Tensor. It represents column indices of the non-zero values
          in the input CSR sparse matrix.
        - **x1_values** (Tensor) - A 1-D Tensor. It represents all the non-zero values in the input CSR sparse matrix.
        - **x2_dense_shape** (Tensor) - A Tensor, same meaning as x1_dense_shape.
        - **x2_batch_pointers** (Tensor) - A Tensor, same meaning as x1_batch_pointers.
        - **x2_row_pointers** (Tensor) - A Tensor, same meaning as x1_row_pointers.
        - **x2_col_indices** (Tensor) - A Tensor, same meaning as x1_col_indices.
        - **x2_values** (Tensor) - A Tensor, same meaning as x1_values.
        - **alpha** (Tensor) - A Tensor.
        - **beta** (Tensor) - A Tensor.

    Outputs:
        - **y1_dense_shape** (Tensor) - A Tensor.
        - **y1_batch_pointers** (Tensor) - A Tensor.
        - **y1_row_pointers** (Tensor) - A Tensor.
        - **y1_col_indices** (Tensor) - A Tensor.
        - **y1_values** (Tensor) - A Tensor.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore.nn as nn
        >>> import mindspore.common.dtype as mstype
        >>> from mindspore import Tensor
        >>> from mindspore.ops.operations.sparse_ops import SparseMatrixAdd
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.op = SparseMatrixAdd()
        ...
        ...     def construct(self, a_shape, a_batch_pointer, a_indptr, a_indices, a_values,
        ...                   b_shape, b_batch_pointer, b_indptr, b_indices, b_values, alpha, beta):
        ...         return self.op(a_shape, a_batch_pointer, a_indptr, a_indices, a_values,
        ...                        b_shape, b_batch_pointer, b_indptr, b_indices, b_values, alpha, beta)
        >>> a_indptr = Tensor([0, 1, 2], dtype=mstype.int32)
        >>> a_indices = Tensor([0, 1], dtype=mstype.int32)
        >>> a_values = Tensor([1, 2], dtype=mstype.float32)
        >>> a_pointers = Tensor([0, a_values.shape[0]], dtype=mstype.int32)
        >>> shape = Tensor([2, 6], dtype=mstype.int32)
        >>> b_indptr = Tensor([0, 1, 2], dtype=mstype.int32)
        >>> b_indices = Tensor([0, 1], dtype=mstype.int32)
        >>> b_values = Tensor([1, 2], dtype=mstype.float32)
        >>> b_pointers = Tensor([0, b_values.shape[0]], dtype=mstype.int32)
        >>> alpha = Tensor(1, mstype.float32)
        >>> beta = Tensor(1, mstype.float32)
        >>> out = Net()(shape, a_pointers, a_indptr, a_indices, a_values,
        ...             shape, b_pointers, b_indptr, b_indices, b_values, alpha, beta)
        >>> print(out)
        (Tensor(shape=[2], dtype=Int32, value =[2, 6]),
         Tensor(shape[2], dtype=Int32, value = [0, 2]),
         Tensor(shape=[3], dtype=Int32, values = [0, 1, 2]),
         Tensor(shape=[2], dtype=Int32, values = [0, 1]),
         Tensor(shape=[2], dtype=Float32, values = [2.0, 4.0]))
    """
    @prim_attr_register
    def __init__(self):
        '''Initialize for SparseMatrixAdd'''
        self.init_prim_io_names(inputs=['x1_dense_shape', 'x1_batch_pointers', 'x1_row_pointers', 'x1_col_indices',
                                        'x1_values', 'x2_dense_shape', 'x2_batch_pointers', 'x2_row_pointers',
                                        'x2_col_indices', 'x2_values', 'alpha', 'beta'],
                                outputs=['y_dense_shape', 'y_batch_pointers', 'y_row_pointers', 'y_col_indices',
                                         'y_values'])


class SparseSplit(Primitive):
    """
    Split a `SparseTensor` into `num_split` tensors along one dimension.
    If the `shape[split_dim]` is not an integer multiple of `num_split`. Slices
    `[0 : shape[split_dim] % num_split]` gets one extra dimension.

    Args:
        num_split (int): An `int` that is `>= 1`. The number of ways to split. Default: 1.

    Inputs:
        - **split_dim** (Tensor) -A 0-D Tensor of type `int64`.
          The dimension along which to split.  Must be in the range `[0, rank(shape))`.
        - **indices** (Tensor) - A 2-D Tensor of type `int64`, represents the indices of the sparse tensor.
        - **values** (Tensor) - A 1-D Tensor, represents the values of the sparse tensor.
          Support float16, float32, float64, int32, int64, int8, int16, uint8, uint16, uint32,
          uint64, complex64, complex128, bool.
        - **shape** (Tensor) - A 1-D Tensor of type `int64`, represents the shape of the sparse tensor.

    Outputs:
          A tuple of `Tensor` objects (y_indices, y_values, y_shape).
        - **y_indices** (Tensor) - A 2-D Tensor of type `int64`.
        - **y_values** (Tensor) - A 1-D Tensor. The type is the same as input Tensor "values".
        - **y_shape** (Tensor) - A 1-D Tensor of type `int64`.

    Raises:
        TypeError: If the type of `split_dim` or `indices` or `shape` is not int64.
            If the type of `values` is not valid.
            If the type of `num_split` is not int.
        ValueError: If the num_element of `split_dim` is not 1.
            If the rank of `values` or `shape` is not 1.
            If the rank of `indices` is not 1.

    Supported Platforms:

    """
    @prim_attr_register
    def __init__(self, num_split=1):
        """Initialize SparseSplit."""
        self.init_prim_io_names(inputs=['split_dim', 'indices', 'values', 'shape'],
                                outputs=['y_indices', 'y_values', 'y_shape'])
        validator.check_value_type("num_split", num_split, [int], self.name)
