#include "ykw2/core/xq32_execution.hpp"

#include <cassert>

namespace {

ykw2::core::Xq32ProgramInfo program() {
    ykw2::core::Xq32ProgramInfo result;
    result.instructions = {
        {0, 1, 100, 100, 0, 0, true},
        {1, 1, 200, 200, 0, 0, true},
        {2, 0, 300, 300, 0, 0, true},
    };
    result.functions = {
        {0, 0, "Main", true, true},
        {2, 0, "Other", true, true},
    };
    return result;
}

void test_cursor_stays_inside_selected_function() {
    const auto fixture = program();
    ykw2::core::Xq32ExecutionCursor cursor;
    assert(cursor.start(fixture, "Main"));
    auto snapshot = cursor.snapshot();
    assert(snapshot.state == ykw2::core::Xq32ExecutionState::Running);
    assert(snapshot.instruction_index == 0);
    assert(snapshot.instruction_end == 2);
    assert(snapshot.instruction.operation_token == 100);
    assert(cursor.jump_to(1));
    assert(cursor.snapshot().instruction_index == 1);
    assert(!cursor.jump_to(2));
    assert(cursor.snapshot().instruction_index == 1);
    assert(cursor.jump_to(0));
    assert(cursor.advance());
    assert(cursor.snapshot().instruction.operation_token == 200);
    assert(!cursor.advance());
    assert(cursor.snapshot().state == ykw2::core::Xq32ExecutionState::Completed);
}

void test_cursor_reports_missing_entry() {
    const auto fixture = program();
    ykw2::core::Xq32ExecutionCursor cursor;
    assert(!cursor.start(fixture, "Missing"));
    assert(cursor.snapshot().state == ykw2::core::Xq32ExecutionState::InvalidEntry);
}

}  // namespace

int main() {
    test_cursor_stays_inside_selected_function();
    test_cursor_reports_missing_entry();
    return 0;
}
