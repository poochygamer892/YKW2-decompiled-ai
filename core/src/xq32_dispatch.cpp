#include "ykw2/core/xq32_dispatch.hpp"

#include <utility>

namespace ykw2::core {

void Xq32OperationDispatcher::register_handler(std::uint32_t operation_token,
                                                Handler handler) {
    if (handler) {
        handlers_[operation_token] = std::move(handler);
    } else {
        handlers_.erase(operation_token);
    }
}

Xq32OperationResult Xq32OperationDispatcher::dispatch(
    Xq32ExecutionCursor& cursor, const Xq32ProgramInfo& program) const {
    const auto before = cursor.snapshot();
    if (!cursor.running() || !before.has_instruction) {
        return {Xq32OperationStatus::Completed, 0, false};
    }
    const auto& instruction = before.instruction;
    if (!instruction.argument_span_valid) {
        return {Xq32OperationStatus::InvalidArguments, instruction.operation_token, false};
    }
    const auto found = handlers_.find(instruction.operation_token);
    if (found == handlers_.end()) {
        const auto progressed = cursor.advance();
        return {Xq32OperationStatus::UnknownOperation, instruction.operation_token, progressed};
    }
    found->second(cursor, program, instruction);
    const auto after = cursor.snapshot();
    return {Xq32OperationStatus::Handled, instruction.operation_token,
            before.instruction_index != after.instruction_index ||
                before.state != after.state};
}

const char* xq32_operation_status_name(Xq32OperationStatus status) {
    switch (status) {
        case Xq32OperationStatus::Idle: return "idle";
        case Xq32OperationStatus::Handled: return "handled";
        case Xq32OperationStatus::UnknownOperation: return "unknown_operation";
        case Xq32OperationStatus::InvalidArguments: return "invalid_arguments";
        case Xq32OperationStatus::Completed: return "completed";
    }
    return "idle";
}

}  // namespace ykw2::core
