/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd. All Rights Reserved
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

#ifndef __ONERT_BACKEND_CL_COMMON_BACKEND_CONTEXT_H__
#define __ONERT_BACKEND_CL_COMMON_BACKEND_CONTEXT_H__

#include <backend/BackendContext.h>
#include <ir/Index.h>
#include <ir/OperandIndexMap.h>
#include <ir/OperandIndexSequence.h>
#include <util/logging.h>

namespace onert::backend::cl_common
{

// TODO Find better way to handle common code (reduce template)
template <typename T_TensorBuilder, typename T_ConstantInitializer, typename T_KernelGenerator>
class BackendContext : public onert::backend::BackendContext
{
public:
  BackendContext(const Backend *backend, ContextData &&data,
                 std::shared_ptr<ITensorRegistry> tensor_registry = nullptr,
                 std::shared_ptr<T_TensorBuilder> tensor_builder = nullptr,
                 std::shared_ptr<T_ConstantInitializer> constant_initializer = nullptr,
                 std::shared_ptr<T_KernelGenerator> kernel_gen = nullptr)
    : onert::backend::BackendContext(backend, std::move(data), tensor_registry),
      tensor_builder{tensor_builder}, constant_initializer{constant_initializer},
      kernel_gen{kernel_gen}
  {
  }

  FunctionMap genKernels() override
  {
    FunctionMap ret;

    // kernel_gen
    for (auto &&op_ind : _data.op_order)
    {
      auto fn_seq = kernel_gen->generate(op_ind);
      ret.emplace(op_ind, std::move(fn_seq));
    }

    tensor_builder->allocate();
    initConsts();

    // NOTE For memory optimization, we want to free some operand data
    const_cast<ir::Graph &>(*_data.graph)
      .operands()
      .iterate([&](const ir::OperandIndex &, ir::Operand &obj) { obj.releaseData(); });

    for (auto &&it : ret)
    {
      auto &fn_seq = it.second;
      fn_seq->iterate([&](exec::IFunction &ifunc) {
        ifunc.prepare();
        tensor_builder->postFunctionPrepare();
      });
    }

    return ret;
  }

protected:
  void initConsts()
  {
    _data.graph->operations().iterate([&](const ir::OperationIndex &, const ir::IOperation &op) {
      op.accept(*constant_initializer);
    });

    _data.graph->operands().iterate([&](const ir::OperandIndex &ind, const ir::Operand &operand) {
      if (_data.external_operands.contains(ind) || !operand.isConstant())
        return;
      const auto &obj = graph()->operands().at(ind);
      if (obj.isConstant() && !constant_initializer->exist(ind))
      {
        constant_initializer->registerDefaultInitializer(ind, obj);
      }
    });

    constant_initializer->run();
  }

  virtual void registerTensorInfo(const ir::OperandIndex &ind, const ir::OperandInfo &info) = 0;

  void planTensors()
  {
    ir::OperandIndexMap<uint32_t> uses_map;
    ir::OperandIndexMap<uint32_t> def_map;
    ir::OperandIndexSequence constants;

    // Prepare scanning
    _data.graph->operands().iterate([&](const ir::OperandIndex &ind, const ir::Operand &obj) {
      if (_data.external_operands.contains(ind))
        return;

      uses_map[ind] = obj.getUses().size();
      def_map[ind] = obj.getDef().valid() ? 1 : 0;

      if (obj.isConstant())
        constants.append(ind);

      if (!tensor_builder->isRegistered(ind))
      {
        // These tensors do not exist in any operation (No use and def)
        const auto &info = obj.info();
        registerTensorInfo(ind, info);
      }
    });

    // Start scanning to do notify{First|Last}Use for each tensor

    // If a tensor is a constant, increase the use of the tensor and allocate it first.
    // Increasing use count here makes the tensor never be deallocated, i.e it they will be
    // deallocated last.
    VERBOSE(planTensors) << "TENSORS as CONSTANT" << std::endl;
    for (const auto &ind : constants)
    {
      uses_map[ind]++;
      tensor_builder->notifyFirstUse(ind);
    }

    // At each operation,
    // 1. Scan DEF of outputs. If the DEF, allocate it
    // 2. Scan DEF of inputs. If variable tensor, allocate it
    // 3. Scan USE of inputs. Decrease the USE and deallocate if the USE is 0
    for (const auto &op_ind : _data.op_order)
    {
      const auto &op = graph()->operations().at(op_ind);
      auto op_inputs = op.getUsedInputSet();
      auto op_outputs = op.getUsedOutputSet();

      // Define outputs
      for (const auto &ind : op_outputs)
      {
        if (!tensor_builder->isRegistered(ind))
          continue;
        assert(def_map.find(ind) != def_map.end());
        if (def_map[ind])
        {
          def_map[ind] = 0;
          tensor_builder->notifyFirstUse(ind);
        }
      }

      // Scan variable tensors
      // This tensor has features like constant. But OperandInfo and LowerInfo treat them as
      // non-constant because of less memory usage by memory planning in here
      for (const auto &ind : op_inputs)
      {
        if (!tensor_builder->isRegistered(ind))
          continue;
        const auto &operand = graph()->operands().at(ind);
        if (operand.info().isVariable())
        {
          // The variable tensor with buffer is not supported yet
          assert(operand.data() == nullptr);
          assert(operand.getUses().size() == 1 && !operand.getDef().valid());
          assert(uses_map[ind] == 1 && def_map[ind] == 0);
          tensor_builder->notifyFirstUse(ind);
        }
      }

      for (const auto &ind : op_inputs)
      {
        if (!tensor_builder->isRegistered(ind))
          continue;
        assert(uses_map.find(ind) != uses_map.end());
        assert(uses_map[ind] > 0);
        uses_map[ind]--;
        if (uses_map[ind] == 0)
        {
          // plan for deallocation of static tensornode
          tensor_builder->notifyLastUse(ind);
        }
      }
    }

    _data.graph->operands().iterate([&](const ir::OperandIndex &ind, const ir::Operand &) {
      if (uses_map[ind] == 0)
      {
        tensor_builder->notifyLastUse(ind);
      }
    });

    // Dispose and validate
    for (const auto &ind : constants)
    {
      --uses_map[ind];
      if (uses_map[ind] == 0) // To prevent notifyLastUse from being called twice
      {
        tensor_builder->notifyLastUse(ind);
      }
    }

    assert(
      std::all_of(uses_map.begin(), uses_map.end(),
                  [](std::pair<const ir::OperandIndex, uint32_t> it) { return it.second == 0; }));

    assert(
      std::all_of(def_map.begin(), def_map.end(),
                  [](std::pair<const ir::OperandIndex, uint32_t> it) { return it.second == 0; }));
  }

public:
  // TODO Make it protected
  std::shared_ptr<T_TensorBuilder> tensor_builder;
  std::shared_ptr<T_ConstantInitializer> constant_initializer;
  std::shared_ptr<T_KernelGenerator> kernel_gen;
};

} // namespace onert::backend::cl_common

#endif // __ONERT_BACKEND_CL_COMMON_BACKEND_CONTEXT_H__
