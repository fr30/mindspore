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

#include <iostream>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/rmsprop_impl.cuh"

template <typename T>
__global__ void RmsPropKernel(const size_t batch_size, const size_t input_elements, const T *learning_rate,
                              const T decay, const T momentum, const T epsilon, T *variable, T *mean_square, T *moment,
                              T *gradients, const size_t size) {
  auto all_elements = batch_size * input_elements;
  for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (all_elements); i += blockDim.x * gridDim.x) {
    auto batch = i / input_elements;
    mean_square[i] = decay * mean_square[i] + (1.0 - decay) * gradients[i] * gradients[i];
    moment[i] = momentum * moment[i] + learning_rate[batch] * rsqrt(mean_square[i] + epsilon) * gradients[i];
    variable[i] -= moment[i];
  }
}

template <typename T>
void RmsProp(const size_t batch_size, const size_t input_elements, const T *learning_rate, const T decay,
             const T momentum, const T epsilon, T *variable, T *mean_square, T *moment, T *gradients, const size_t size,
             cudaStream_t cuda_stream) {
  RmsPropKernel<<<GET_BLOCKS(input_elements), GET_THREADS, 0, cuda_stream>>>(batch_size, input_elements, learning_rate,
                                                                             decay, momentum, epsilon, variable,
                                                                             mean_square, moment, gradients, size);
}

template <typename T>
__global__ void RmsPropCenterKernel(const size_t batch_size, const size_t input_elements, const T *learning_rate,
                                    const T *decay, const T *momentum, const T *epsilon, T *variable, T *mean_gradients,
                                    T *mean_square, T *moment, T *gradients, const size_t size) {
  auto all_elements = batch_size * input_elements;
  for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (all_elements); i += blockDim.x * gridDim.x) {
    auto batch = i / input_elements;
    mean_gradients[i] = decay[batch] * mean_gradients[i] + (1.0 - decay[batch]) * gradients[i];
    mean_square[i] = decay[batch] * mean_square[i] + (1.0 - decay[batch]) * gradients[i] * gradients[i];
    moment[i] = momentum[batch] * moment[i] +
                learning_rate[batch] * rsqrt(mean_square[i] - mean_gradients[i] * mean_gradients[i] + epsilon[batch]) *
                  gradients[i];
    variable[i] -= moment[i];
  }
}

template <typename T>
void RmsPropCenter(const size_t batch_size, const size_t input_elements, const T *learning_rate, const T *decay,
                   const T *momentum, const T *epsilon, T *variable, T *mean_gradients, T *mean_square, T *moment,
                   T *gradients, const size_t size, cudaStream_t cuda_stream) {
  RmsPropCenterKernel<<<GET_BLOCKS(input_elements), GET_THREADS, 0, cuda_stream>>>(
    batch_size, input_elements, learning_rate, decay, momentum, epsilon, variable, mean_gradients, mean_square, moment,
    gradients, size);
}

template CUDA_LIB_EXPORT void RmsProp(const size_t batch_size, const size_t input_elements, const float *learning_rate,
                                      const float decay, const float momentum, const float epsilon, float *variable,
                                      float *mean_square, float *moment, float *gradients, const size_t size,
                                      cudaStream_t cuda_stream);

template CUDA_LIB_EXPORT void RmsPropCenter(const size_t batch_size, const size_t input_elements,
                                            const float *learning_rate, const float *decay, const float *momentum,
                                            const float *epsilon, float *variable, float *mean_gradients,
                                            float *mean_square, float *moment, float *gradients, const size_t size,
                                            cudaStream_t cuda_stream);
