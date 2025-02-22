/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_LITE_SRC_EXTENDRT_KERNEL_ASCEND_SRC_ACL_OPTIONS_PARSER_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_KERNEL_ASCEND_SRC_ACL_OPTIONS_PARSER_H_

#include <memory>
#include <string>
#include "include/api/context.h"
#include "include/errorcode.h"
#include "extendrt/kernel/ascend/options/acl_model_options.h"

namespace mindspore::kernel {
namespace acl {
using mindspore::lite::STATUS;

class AclOptionsParser {
 public:
  STATUS ParseAclOptions(const mindspore::Context *ctx, AclModelOptionsPtr *const acl_options);

 private:
  STATUS ParseOptions(const std::shared_ptr<DeviceInfoContext> &device_info, AclModelOptions *acl_options);
  STATUS CheckDeviceId(int32_t *device_id);
};
}  // namespace acl
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_EXTENDRT_KERNEL_ASCEND_SRC_ACL_OPTIONS_PARSER_H_
