// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/common/common.h"
#include "core/framework/op_kernel.h"
#include "core/providers/hip/hip_common.h"
#include "core/providers/hip/tensor/gather_nd.h"

namespace onnxruntime {
namespace rocm {

template <typename Tind>
class GatherNDGrad final : public GatherNDBase {
 public:
  GatherNDGrad(const OpKernelInfo& info) : GatherNDBase(info) {}
  Status ComputeInternal(OpKernelContext* context) const override;
};

}  // namespace rocm
}  // namespace onnxruntime