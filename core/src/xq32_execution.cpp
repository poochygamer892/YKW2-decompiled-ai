#include "ykw2/core/xq32_execution.hpp"

#include <algorithm>

namespace ykw2::core {

bool Xq32ExecutionCursor::start(const Xq32ProgramInfo& program,
                                std::string_view entry_symbol) {
    program_ = &program;
    state_ = Xq32ExecutionState::Idle;
    entry_symbol_ = std::string(entry_symbol);
    instruction_index_ = 0;
    instruction_end_ = 0;

    const Xq32FunctionInfo* entry = nullptr;
    for (const auto& function : program.functions) {
        if (function.symbol == entry_symbol_) {
            entry = &function;
            break;
        }
    }
    if (entry != nullptr) {
        if (!entry->instruction_index_valid) {
            state_ = Xq32ExecutionState::InvalidEntry;
            return false;
        }
        instruction_index_ = entry->instruction_index;
    } else {
        if (!program.functions.empty() || program.instructions.empty()) {
            state_ = Xq32ExecutionState::InvalidEntry;
            return false;
        }
    }

    if (instruction_index_ >= program.instructions.size()) {
        state_ = Xq32ExecutionState::InvalidEntry;
        return false;
    }
    instruction_end_ = program.instructions.size();
    for (const auto& function : program.functions) {
        if (function.instruction_index_valid &&
            function.instruction_index > instruction_index_) {
            instruction_end_ = std::min<std::size_t>(instruction_end_,
                                                      function.instruction_index);
        }
    }
    if (instruction_end_ <= instruction_index_) {
        state_ = Xq32ExecutionState::InvalidEntry;
        return false;
    }
    state_ = Xq32ExecutionState::Running;
    return true;
}

bool Xq32ExecutionCursor::advance() {
    if (!running() || program_ == nullptr) {
        return false;
    }
    if (instruction_index_ + 1 >= instruction_end_) {
        state_ = Xq32ExecutionState::Completed;
        return false;
    }
    ++instruction_index_;
    return true;
}

Xq32ExecutionSnapshot Xq32ExecutionCursor::snapshot() const {
    Xq32ExecutionSnapshot result;
    result.state = state_;
    result.entry_symbol = entry_symbol_;
    result.instruction_index = instruction_index_;
    result.instruction_end = instruction_end_;
    if (program_ != nullptr && running() && instruction_index_ < program_->instructions.size()) {
        result.has_instruction = true;
        result.instruction = program_->instructions[instruction_index_];
    }
    return result;
}

const char* xq32_execution_state_name(Xq32ExecutionState state) {
    switch (state) {
        case Xq32ExecutionState::Idle:
            return "idle";
        case Xq32ExecutionState::Running:
            return "running";
        case Xq32ExecutionState::Completed:
            return "completed";
        case Xq32ExecutionState::InvalidEntry:
            return "invalid_entry";
    }
    return "idle";
}

}  // namespace ykw2::core
