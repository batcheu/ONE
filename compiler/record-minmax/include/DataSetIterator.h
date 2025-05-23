/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __RECORD_MINMAX_DATASET_ITERATOR_H__
#define __RECORD_MINMAX_DATASET_ITERATOR_H__

#include "DataBuffer.h"

#include <vector>

namespace record_minmax
{

// Base class for dataset iterator
class DataSetIterator
{
public:
  virtual bool hasNext() const = 0;

  virtual std::vector<DataBuffer> next() = 0;

  // Revisit this interface later
  virtual bool check_type_shape() const = 0;

  virtual ~DataSetIterator() = default;
};

} // namespace record_minmax

#endif // __RECORD_MINMAX_DATASET_ITERATOR_H__
