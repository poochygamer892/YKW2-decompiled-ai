#include "ykw2/core/xq32_dispatch.hpp"

#include <cassert>

namespace {

ykw2::core::Xq32ProgramInfo program() {
    ykw2::core::Xq32ProgramInfo result;
    result.arguments = {{1, 42}};
    result.instructions = {
        {0, 1, 0x001403e9u, 0x03e9u, 0x0014u, 0, true},
        {1, 0, 0x002103eau, 0x03eau, 0x0021u, 0, true},
    };
    result.functions = {{0, 0, "Main", true, true}};
    return result;
}

void test_unknown_operation_progresses_safely() {
    const auto fixture = program();
    ykw2::core::Xq32ExecutionCursor cursor;
    ykw2::core::Xq32OperationDispatcher dispatcher;
    assert(cursor.start(fixture));
    const auto result = dispatcher.dispatch(cursor, fixture);
    assert(result.status == ykw2::core::Xq32OperationStatus::UnknownOperation);
    assert(result.operation_token == 0x001403e9u);
    assert(result.progressed);
    assert(cursor.snapshot().instruction_index == 1);
}

void test_registered_handler_controls_cursor() {
    const auto fixture = program();
    ykw2::core::Xq32ExecutionCursor cursor;
    ykw2::core::Xq32OperationDispatcher dispatcher;
    dispatcher.register_handler(0x001403e9u,
                                [](auto& current, const auto&, const auto&) {
                                    current.jump_to(1);
                                });
    assert(cursor.start(fixture));
    const auto result = dispatcher.dispatch(cursor, fixture);
    assert(result.status == ykw2::core::Xq32OperationStatus::Handled);
    assert(result.progressed);
    assert(cursor.snapshot().instruction_index == 1);
}

}  // namespace

int main() {
    test_unknown_operation_progresses_safely();
    test_registered_handler_controls_cursor();
    return 0;
}
