// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/providers/hip/hip_common.h"

namespace onnxruntime {
namespace rocm {

template <typename input_t, typename output_t, typename acc_t, bool is_log_softmax>
void dispatch_softmax_backward(output_t* grad_input, const input_t* grad, const input_t* output, int softmax_elements, int softmax_elements_stride, int batch_count);

template <typename T>
class SoftmaxGrad final : public RocmKernel {
 public:
  SoftmaxGrad(const OpKernelInfo& info) : RocmKernel{info} {
    info.GetAttrOrDefault("axis", &axis_, static_cast<int64_t>(1));
    log_softmax_ = info.GetKernelDef().OpName() == "LogSoftmaxGrad";
  }

  Status ComputeInternal(OpKernelContext* context) const override;

 private:
  int64_t axis_;
  bool log_softmax_;
};

}  // namespace rocm
}  // namespace onnxruntime