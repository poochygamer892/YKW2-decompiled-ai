#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ykw2/core/xq32_reader.hpp"

namespace ykw2::core {

enum class Xq32ExecutionState {
    Idle,
    Running,
    Completed,
    InvalidEntry,
};

struct Xq32ExecutionSnapshot {
    Xq32ExecutionState state = Xq32ExecutionState::Idle;
    std::string entry_symbol;
    std::size_t instruction_index = 0;
    std::size_t instruction_end = 0;
    bool has_instruction = false;
    Xq32InstructionInfo instruction;
};

// A bounded structural cursor for one XQ32 function. It does not assign
// meanings to operation tokens or mutate game state; it supplies the verified
// instruction/argument window that semantic handlers will consume later.
class Xq32ExecutionCursor {
public:
    bool start(const Xq32ProgramInfo& program, std::string_view entry_symbol = "Main");
    bool advance();
    // Performs only a bounds-checked intra-function jump. The caller must
    // supply the target after decoding the operation's verified semantics.
    bool jump_to(std::size_t instruction_index);
    bool running() const { return state_ == Xq32ExecutionState::Running; }
    Xq32ExecutionSnapshot snapshot() const;

private:
    const Xq32ProgramInfo* program_ = nullptr;
    Xq32ExecutionState state_ = Xq32ExecutionState::Idle;
    std::string entry_symbol_;
    std::size_t instruction_index_ = 0;
    std::size_t instruction_end_ = 0;
};

const char* xq32_execution_state_name(Xq32ExecutionState state);

}  // namespace ykw2::core
