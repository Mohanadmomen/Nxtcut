#pragma once

#include <nxtcut/commands/editor.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/validation.hpp>

#include <gtest/gtest.h>

#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands::test {

using nxtcut::test::is_error;
using nxtcut::test::is_ok;

/**
 * @brief Round-trip execution helper verifying forward application, undo, and redo.
 *
 * Captures initial snapshot `before`, executes `command`, verifies validation passes on `after`,
 * verifies undo restores a snapshot identical to `before`, and verifies redo restores a snapshot
 * identical to `after`.
 *
 * @tparam C EditCommand type.
 * @param editor Editor instance to execute against.
 * @param command Command payload.
 * @return EditReceipt result returned by editor.execute(command).
 */
template <EditCommand C>
core::Result<EditReceipt> round_trip(Editor& editor, const C& command) {
    auto before = editor.snapshot();
    auto receipt_res = editor.execute(command);
    EXPECT_TRUE(test::is_ok(receipt_res));
    if (!receipt_res.has_value()) {
        return receipt_res;
    }
    auto after = editor.snapshot();
    EXPECT_TRUE(test::is_ok(model::validate(*after)));

    auto undo_status = editor.undo();
    EXPECT_TRUE(test::is_ok(undo_status));
    EXPECT_TRUE(model::identical(*editor.snapshot(), *before));

    auto redo_status = editor.redo();
    EXPECT_TRUE(test::is_ok(redo_status));
    EXPECT_TRUE(model::identical(*editor.snapshot(), *after));

    return receipt_res;
}

}  // namespace nxtcut::commands::test
