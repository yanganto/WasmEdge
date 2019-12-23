// SPDX-License-Identifier: Apache-2.0
#include "executor/executor.h"
#include "ast/module.h"
#include "ast/section.h"
#include "executor/instance/module.h"

#include <boost/algorithm/hex.hpp>
#include <cstdlib>
#include <iostream>

namespace SSVM {
namespace Executor {

/// Set and instantiate host function. See "include/executor/executor.h".
ErrCode Executor::setHostFunction(std::unique_ptr<HostFunction> &Func,
                                  const std::string &ModName,
                                  const std::string &FuncName) {
  ErrCode Status = ErrCode::Success;
  auto NewFuncInst = std::make_unique<Instance::FunctionInstance>(true);
  unsigned int NewHostFuncId = 0;
  unsigned int NewFuncInstId = 0;
  const Instance::ModuleInstance::FType *FuncType = Func->getFuncType();

  /// Set function instance data.
  if ((Status = NewFuncInst->setNames(ModName, FuncName)) != ErrCode::Success) {
    return Status;
  }
  if ((Status = NewFuncInst->setFuncType(FuncType)) != ErrCode::Success) {
    return Status;
  }

  /// Insert host function to host function manager.
  if ((Status = HostFuncMgr.insertHostFunction(Func, NewHostFuncId)) !=
      ErrCode::Success) {
    return Status;
  }
  if ((Status = NewFuncInst->setHostFuncAddr(NewHostFuncId)) !=
      ErrCode::Success) {
    return Status;
  }

  /// Insert function instance to store manager.
  if ((Status = StoreMgr.insertFunctionInst(NewFuncInst, NewFuncInstId)) !=
      ErrCode::Success) {
    return Status;
  }
  return Status;
}

/// Set start function name. See "include/executor/executor.h".
ErrCode Executor::setStartFuncName(const std::string &Name) {
  StartFunc = Name;
  return ErrCode::Success;
}

/// Set AST Module node to executor. See "include/executor/executor.h".
ErrCode Executor::setModule(std::unique_ptr<AST::Module> &Module) {
  /// Check is the correct state.
  if (Stat != State::Inited)
    return ErrCode::WrongExecutorFlow;

  /// Get ownership of module.
  Mod = std::move(Module);
  Stat = State::ModuleSet;
  return ErrCode::Success;
}

/// Instantiate module. See "include/executor/executor.h".
ErrCode Executor::instantiate() {
  /// Check is the correct state.
  if (Stat != State::ModuleSet)
    return ErrCode::WrongExecutorFlow;

  /// Instantiate module.
  ErrCode Result = instantiate(Mod.get());
  if (Result == ErrCode::Success)
    Stat = State::Instantiated;
  else
    std::cout << " Error: Wasm instantiation failed. Code: "
              << static_cast<uint32_t>(Result) << std::endl;
  return Result;
}

/// Set arguments. See "include/executor/executor.h".
ErrCode Executor::setArgs(std::vector<Value> &Args) {
  /// Check is the correct state.
  if (Stat != State::Instantiated)
    return ErrCode::WrongExecutorFlow;

  /// Push args to stack.
  for (auto It = Args.begin(); It != Args.end(); It++) {
    StackMgr.push(std::move(*It));
  }
  Args.clear();
  Stat = State::ArgsSet;
  return ErrCode::Success;
}

/// Resume from JSON. See "include/executor/executor.h"
ErrCode Executor::restore(const rapidjson::Value &Doc) {
  /// Find Global instances.
  rapidjson::Value::ConstMemberIterator ItGlob = Doc.FindMember("Global");
  if (ItGlob != Doc.MemberEnd()) {
    for (auto It = ItGlob->value.Begin(); It != ItGlob->value.End(); ++It) {
      /// Get global address and hex
      const uint32_t Idx = It->GetArray()[0].GetUint();
      Instance::GlobalInstance *GlobInst = nullptr;
      if (ErrCode Status = StoreMgr.getGlobal(Idx, GlobInst);
          Status != ErrCode::Success) {
        return Status;
      }
      AST::ValVariant Val = static_cast<uint64_t>(
          std::strtoull(It->GetArray()[1].GetString(), nullptr, 16));
      GlobInst->setValue(Val);
    }
  }

  /// Find Memory instances.
  rapidjson::Value::ConstMemberIterator ItMem = Doc.FindMember("Memory");
  if (ItMem != Doc.MemberEnd()) {
    for (auto It = ItMem->value.Begin(); It != ItMem->value.End(); ++It) {
      /// Get memory address and data
      const uint32_t Idx = It->GetArray()[0].GetUint();
      const std::string &Hex = It->GetArray()[1].GetString();
      Instance::MemoryInstance *MemInst = nullptr;
      if (ErrCode Status = StoreMgr.getMemory(Idx, MemInst);
          Status != ErrCode::Success) {
        return Status;
      }
      std::vector<unsigned char> MemVec;
      boost::algorithm::unhex(Hex.begin(), Hex.end(),
                              std::back_inserter(MemVec));
      if (ErrCode Status = MemInst->setBytes(MemVec, 0, 0, MemVec.size());
          Status != ErrCode::Success) {
        return Status;
      }
    }
  }
  return ErrCode::Success;
}

/// Invoke start function. See "include/executor/executor.h".
ErrCode Executor::run() {
  /// Check is the correct state.
  if (Stat != State::ArgsSet)
    return ErrCode::WrongExecutorFlow;

  /// Run start function.
  ErrCode Result = ErrCode::Success;
  if (auto StartAddr = ModInst->getStartAddr()) {
    Result = Engine.runStartFunction(*StartAddr);
  }
  Stat = State::Executed;
  return Result;
}

/// Get return values. See "include/executor/executor.h".
ErrCode Executor::getRets(std::vector<Value> &Rets) {
  /// Check is the correct state.
  if (Stat != State::Executed)
    return ErrCode::WrongExecutorFlow;

  /// Push args to stack.
  Rets.clear();
  Rets.resize(StackMgr.size());
  auto Iter = Rets.rbegin();
  while (StackMgr.size() > 0) {
    Value V;
    StackMgr.pop(V);
    *Iter = V;
    Iter++;
  }

  Stat = State::Finished;
  return ErrCode::Success;
}

/// Reset Executor. See "include/executor/executor.h".
ErrCode Executor::reset(bool Force) {
  if (!Force && (Stat != State::Finished && Stat != State::Executed)) {
    return ErrCode::WrongExecutorFlow;
  }
  Mod.reset();
  ModInst = nullptr;
  Engine.reset();
  StackMgr.reset();
  StoreMgr.reset();
  HostFuncMgr.reset();
  Stat = State::Inited;
  return ErrCode::Success;
}

} // namespace Executor
} // namespace SSVM
