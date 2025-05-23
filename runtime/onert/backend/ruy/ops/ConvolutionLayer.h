/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ONERT_BACKEND_RUY_OPS_CONVOLUTIONLAYER_H__
#define __ONERT_BACKEND_RUY_OPS_CONVOLUTIONLAYER_H__

#include <backend/IPortableTensor.h>
#include "../ExternalContext.h"
#include "OperationUtils.h"

#include <ruy/operation/Conv.h>
#include <exec/IFunction.h>
#include <functional>
#include <memory>

namespace onert::backend::ruy::ops
{

class ConvolutionLayer : public ::onert::exec::IFunction
{
public:
  ConvolutionLayer();
  ~ConvolutionLayer();

public:
  void convFloat32();

  void configure(const IPortableTensor *input, const IPortableTensor *kernel,
                 const IPortableTensor *bias, ir::PaddingType _paddingType,
                 const uint32_t paddingLeft, const uint32_t paddingRight, const uint32_t paddingTop,
                 const uint32_t paddingBottom, const uint32_t strideWidth,
                 const uint32_t strideHeight, const uint32_t dilationWidthFactor,
                 const uint32_t dilationHeightFactor, const ir::Activation activation,
                 IPortableTensor *output, const std::shared_ptr<ExternalContext> &external_context);

  void run() override;

  void prepare() override;

private:
  const IPortableTensor *_input;
  const IPortableTensor *_kernel;
  const IPortableTensor *_bias;
  IPortableTensor *_output;

  ir::PaddingType _paddingType;
  uint32_t _paddingLeft;
  uint32_t _paddingTop;
  uint32_t _paddingRight;
  uint32_t _paddingBottom;

  uint32_t _strideWidth;
  uint32_t _strideHeight;
  uint32_t _dilationWidthFactor;
  uint32_t _dilationHeightFactor;

  ir::Activation _activation;

  std::unique_ptr<nnfw::ruy::Conv> _conv_kernel;

  bool _prepare;

  std::shared_ptr<ExternalContext> _external_context;
};

} // namespace onert::backend::ruy::ops

#endif // __ONERT_BACKEND_RUY_OPS_CONVOLUTIONLAYER_H__
