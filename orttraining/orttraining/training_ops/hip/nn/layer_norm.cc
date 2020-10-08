// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "orttraining/training_ops/hip/nn/layer_norm.h"
#include "orttraining/training_ops/hip/nn/layer_norm_impl.h"

#include "core/providers/common.h"

namespace onnxruntime {
namespace rocm {

#define REGISTER_GRADIENT_KERNEL_TYPED(T, U)                      \
  ONNX_OPERATOR_TYPED_KERNEL_EX(                                  \
      LayerNormalizationGrad,                                     \
      kMSDomain,                                                  \
      1,                                                          \
      T##_##U,                                                    \
      kRocmExecutionProvider,                                     \
      KernelDefBuilder()                                          \
          .TypeConstraint("T", DataTypeImpl::GetTensorType<T>())  \
          .TypeConstraint("U", DataTypeImpl::GetTensorType<U>()), \
      LayerNormGrad<T, U>);                                       \
  ONNX_OPERATOR_TYPED_KERNEL_EX(                                  \
      InvertibleLayerNormalizationGrad,                           \
      kMSDomain,                                                  \
      1,                                                          \
      T##_##U,                                                    \
      kRocmExecutionProvider,                                     \
      KernelDefBuilder()                                          \
          .TypeConstraint("T", DataTypeImpl::GetTensorType<T>())  \
          .TypeConstraint("U", DataTypeImpl::GetTensorType<U>()), \
      InvertibleLayerNormGrad<T, U>);
REGISTER_GRADIENT_KERNEL_TYPED(float, float)
REGISTER_GRADIENT_KERNEL_TYPED(double, double)
REGISTER_GRADIENT_KERNEL_TYPED(MLFloat16, float)

template <typename T, typename U>
LayerNormGrad<T, U>::LayerNormGrad(const OpKernelInfo& op_kernel_info) : RocmKernel(op_kernel_info) {
  ORT_ENFORCE(op_kernel_info.GetAttr("axis", &axis_).IsOK());
}

template <typename T, typename U>
Status LayerNormGrad<T, U>::ComputeInternal(OpKernelContext* p_op_kernel_context) const {
  typedef typename ToHipType<T>::MappedType HipT;
  typedef typename ToHipType<U>::MappedType HipU;
  // Inputs
  const Tensor* Y_grad = p_op_kernel_context->Input<Tensor>(0);
  const Tensor* X = p_op_kernel_context->Input<Tensor>(1);
  const Tensor* scale = p_op_kernel_context->Input<Tensor>(2);
  const Tensor* mean = p_op_kernel_context->Input<Tensor>(3);
  const Tensor* inv_std_var = p_op_kernel_context->Input<Tensor>(4);

  auto Y_grad_data = reinterpret_cast<const HipT*>(Y_grad->template Data<T>());
  auto X_data = reinterpret_cast<const HipT*>(X->template Data<T>());
  auto scale_data = reinterpret_cast<const HipT*>(scale->template Data<T>());
  auto mean_data = reinterpret_cast<const HipU*>(mean->template Data<U>());
  auto inv_std_var_data = reinterpret_cast<const HipU*>(inv_std_var->template Data<U>());

  const TensorShape& x_shape = X->Shape();
  const int64_t axis = HandleNegativeAxis(axis_, x_shape.NumDimensions());
  auto n1 = x_shape.SizeToDimension(axis);
  auto n2 = x_shape.SizeFromDimension(axis);
  ORT_ENFORCE(n2 != 1, "n2 should not be 1");

  // Outputs
  Tensor* X_grad = p_op_kernel_context->Output(0, x_shape);
  auto X_grad_data = reinterpret_cast<HipT*>(X_grad->template MutableData<T>());

  Tensor* scale_grad = p_op_kernel_context->Output(1, scale->Shape());
  Tensor* bias_grad = p_op_kernel_context->Output(2, scale->Shape());
  auto scale_grad_data = reinterpret_cast<HipT*>(scale_grad->template MutableData<T>());
  auto bias_grad_data = reinterpret_cast<HipT*>(bias_grad->template MutableData<T>());

  const int part_size = 16;
  auto part_grad_gamma = GetScratchBuffer<HipU>(part_size * n2);
  auto part_grad_beta = GetScratchBuffer<HipU>(part_size * n2);

  HostLayerNormGradient(GetDeviceProp(), Y_grad_data, X_data, reinterpret_cast<const HipT*>(NULL),
                        scale_data, reinterpret_cast<const HipT*>(NULL), mean_data, inv_std_var_data, n1, n2,
                        X_grad_data, scale_grad_data, bias_grad_data,
                        part_grad_gamma.get(), part_grad_beta.get(), part_size);
  return Status::OK();
}

template <typename T, typename U>
InvertibleLayerNormGrad<T, U>::InvertibleLayerNormGrad(const OpKernelInfo& op_kernel_info) : RocmKernel(op_kernel_info) {
  ORT_ENFORCE(op_kernel_info.GetAttr("axis", &axis_).IsOK());
}

template <typename T, typename U>
Status InvertibleLayerNormGrad<T, U>::ComputeInternal(OpKernelContext* p_op_kernel_context) const {
  typedef typename ToHipType<T>::MappedType HipT;
  typedef typename ToHipType<U>::MappedType HipU;
  // Inputs
  const Tensor* Y_grad = p_op_kernel_context->Input<Tensor>(0);
  const Tensor* Y = p_op_kernel_context->Input<Tensor>(1);
  const Tensor* scale = p_op_kernel_context->Input<Tensor>(2);
  const Tensor* bias = p_op_kernel_context->Input<Tensor>(3);
  const Tensor* inv_std_var = p_op_kernel_context->Input<Tensor>(4);

  auto Y_grad_data = reinterpret_cast<const HipT*>(Y_grad->template Data<T>());
  auto Y_data = reinterpret_cast<const HipT*>(Y->template Data<T>());
  auto scale_data = reinterpret_cast<const HipT*>(scale->template Data<T>());
  auto bias_data = reinterpret_cast<const HipT*>(bias->template Data<T>());
  auto inv_std_var_data = reinterpret_cast<const HipU*>(inv_std_var->template Data<U>());

  const TensorShape& y_shape = Y->Shape();
  const TensorShape& x_shape = y_shape;
  const int64_t axis = HandleNegativeAxis(axis_, x_shape.NumDimensions());
  auto n1 = x_shape.SizeToDimension(axis);
  auto n2 = x_shape.SizeFromDimension(axis);
  ORT_ENFORCE(n2 != 1, "n2 should not be 1");

  // Outputs
  Tensor* X_grad = p_op_kernel_context->Output(0, x_shape);
  auto X_grad_data = reinterpret_cast<HipT*>(X_grad->template MutableData<T>());

  Tensor* scale_grad = p_op_kernel_context->Output(1, scale->Shape());
  Tensor* bias_grad = p_op_kernel_context->Output(2, scale->Shape());
  auto scale_grad_data = reinterpret_cast<HipT*>(scale_grad->template MutableData<T>());
  auto bias_grad_data = reinterpret_cast<HipT*>(bias_grad->template MutableData<T>());

  const int part_size = 16;
  auto part_grad_gamma = GetScratchBuffer<HipU>(part_size * n2);
  auto part_grad_beta = GetScratchBuffer<HipU>(part_size * n2);

  HostLayerNormGradient(GetDeviceProp(), Y_grad_data, reinterpret_cast<const HipT*>(NULL), Y_data,
                        scale_data, bias_data, reinterpret_cast<const HipU*>(NULL), inv_std_var_data, n1, n2,
                        X_grad_data, scale_grad_data, bias_grad_data,
                        part_grad_gamma.get(), part_grad_beta.get(), part_size);
  return Status::OK();
}

}  //namespace rocm
}  // namespace onnxruntime