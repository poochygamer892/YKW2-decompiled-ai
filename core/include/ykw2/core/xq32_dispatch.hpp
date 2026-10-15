#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "ykw2/core/xq32_execution.hpp"

namespace ykw2::core {

enum class Xq32OperationStatus {
    Idle,
    Handled,
    UnknownOperation,
    InvalidArguments,
    Completed,
};

struct Xq32OperationResult {
    Xq32OperationStatus status = Xq32OperationStatus::Idle;
    std::uint32_t operation_token = 0;
    bool progressed = false;
};

// Dispatches one instruction without assigning meanings to unknown tokens.
// A registered handler owns the semantic decision and may call advance() or
// jump_to() on the bounded cursor. Unknown operations take one safe sequential
// step so the mobile shell remains responsive while ABI recovery continues.
class Xq32OperationDispatcher {
public:
    using Handler = std::function<void(Xq32ExecutionCursor&, const Xq32ProgramInfo&,
                                       const Xq32InstructionInfo&)>;

    void register_handler(std::uint32_t operation_token, Handler handler);
    Xq32OperationResult dispatch(Xq32ExecutionCursor& cursor,
                                 const Xq32ProgramInfo& program) const;

private:
    std::unordered_map<std::uint32_t, Handler> handlers_;
};

const char* xq32_operation_status_name(Xq32OperationStatus status);

}  // namespace ykw2::core
