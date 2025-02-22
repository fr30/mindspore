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

#include "frontend/optimizer/irpass/symbol_resolver.h"

#include <string>
#include <memory>

namespace mindspore {
namespace opt {
namespace irpass {
// {prim::kPrimGetAttr, object, attr}
// {prim::kPrimResolve, namespace, symbol}
AnfNodePtr Resolver::operator()(const OptimizerPtr &optimizer, const AnfNodePtr &node) {
  PatternNode<AnfNodePtr> object, attr, ns_node, sym_node;
  auto GetAttrLambda = [&node, &object, &attr, &optimizer]() -> AnfNodePtr {
    auto object_node = object.GetNode(node);
    auto attr_node = attr.GetNode(node);

    // {prim::kPrimGetAttr, {prim::kPrimResolve, namespace, symbol}, attr}
    if (IsPrimitiveCNode(object_node, prim::kPrimResolve)) {
      // node is get_attr node
      return parse::ResolveSymbolWithAttr(optimizer->manager(), object_node, attr_node, node);
    }
    // {prim::kPrimGetAttr, namespace, attr}
    if (IsValueNode<parse::NameSpace>(object_node)) {
      auto name_space = GetValueNode<parse::NameSpacePtr>(object_node);
      auto attr_str = GetValue<std::string>(GetValueNode(attr_node));
      parse::SymbolPtr symbol = std::make_shared<parse::Symbol>(attr_str);
      return parse::ResolveSymbol(optimizer->manager(), name_space, symbol, node);
    }
    // {prim::kPrimGetAttr, MsClassObject, attr}
    if (IsValueNode<parse::MsClassObject>(object_node)) {
      auto ms_class = GetValueNode<parse::MsClassObjectPtr>(object_node)->obj();
      auto attr_str = GetValue<std::string>(GetValueNode(attr_node));
      return parse::ResolveMsClassWithAttr(optimizer->manager(), ms_class, attr_str, node);
    }
    // {prim::kPrimGetAttr, bool, attr}
    if (IsValueNode<BoolImm>(object_node)) {
      return object_node;
    }
    return nullptr;
  };

  auto ResolveLambda = [&node, &ns_node, &sym_node, &optimizer]() -> AnfNodePtr {
    auto name_space = GetValueNode<parse::NameSpacePtr>(ns_node.GetNode(node));
    auto symbol = GetValueNode<parse::SymbolPtr>(sym_node.GetNode(node));
    auto manager = optimizer->manager();
    return parse::ResolveSymbol(manager, name_space, symbol, node);
  };

  // {prim::kPrimGetAttr, object, attr}
  MATCH_REPLACE_LAMBDA_IF(node, PPrimitive(prim::kPrimGetAttr, object, attr), GetAttrLambda,
                          attr.CheckFunc(IsValueNode<StringImm>, node));
  // {prim::kPrimResolve, namespace, symbol}
  MATCH_REPLACE_LAMBDA_IF(
    node, PPrimitive(prim::kPrimResolve, ns_node, sym_node), ResolveLambda,
    ns_node.CheckFunc(IsValueNode<parse::NameSpace>, node) && sym_node.CheckFunc(IsValueNode<parse::Symbol>, node));
  return nullptr;
}
}  // namespace irpass
}  // namespace opt
}  // namespace mindspore
