#include <nxtcut/commands/clip_commands.hpp>
#include <nxtcut/commands/editor.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/blend_mode.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/property.hpp>
#include <nxtcut/model/track.hpp>
#include <nxtcut/model/transform.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <string>

#include "test_helper.hpp"
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

TEST(ClipCommandsTest, SetClipPropertiesRenameDisableBlendModeTransform) {
    core::UuidGenerator gen(501ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto clip_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // 1. Rename
    SetClipProperties rename_cmd{main_seq_id,  clip_id,      "RenamedClip",
                                 std::nullopt, std::nullopt, std::nullopt};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, rename_cmd)));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].name, "RenamedClip");

    // 2. Set enabled false
    SetClipProperties disable_cmd{main_seq_id, clip_id,      std::nullopt,
                                  false,       std::nullopt, std::nullopt};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, disable_cmd)));
    EXPECT_FALSE(editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].enabled);

    // 3. Set blend mode
    SetClipProperties blend_cmd{
        main_seq_id, clip_id, std::nullopt, std::nullopt, model::BlendMode::Multiply, std::nullopt};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, blend_cmd)));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].blend_mode,
              model::BlendMode::Multiply);

    // 4. Transform with opacity 0.5 OK
    model::TransformProps t_valid;
    t_valid.opacity.set_constant(0.5);
    SetClipProperties transform_cmd{main_seq_id,  clip_id,      std::nullopt,
                                    std::nullopt, std::nullopt, t_valid};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, transform_cmd)));
    EXPECT_EQ(editor.snapshot()
                  ->sequences.at(main_seq_id)
                  .tracks[0]
                  .clips[0]
                  .transform.opacity.constant_value(),
              0.5);
}

TEST(ClipCommandsTest, SetClipPropertiesTransformValidationFailures) {
    core::UuidGenerator gen(502ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto clip_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    // Opacity 1.5 gives InvalidArgument
    {
        model::TransformProps t;
        t.opacity.set_constant(1.5);
        SetClipProperties cmd{main_seq_id, clip_id, std::nullopt, std::nullopt, std::nullopt, t};
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_EQ(editor.snapshot(), snap_before);
        EXPECT_EQ(editor.can_undo(), can_undo_before);
    }

    // Opacity -0.1 gives InvalidArgument
    {
        model::TransformProps t;
        t.opacity.set_constant(-0.1);
        SetClipProperties cmd{main_seq_id, clip_id, std::nullopt, std::nullopt, std::nullopt, t};
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_EQ(editor.snapshot(), snap_before);
        EXPECT_EQ(editor.can_undo(), can_undo_before);
    }

    // crop_left 0.6 with crop_right 0.5 (sum 1.1 > 1.0) gives InvalidArgument
    {
        model::TransformProps t;
        t.crop_left.set_constant(0.6);
        t.crop_right.set_constant(0.5);
        SetClipProperties cmd{main_seq_id, clip_id, std::nullopt, std::nullopt, std::nullopt, t};
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_EQ(editor.snapshot(), snap_before);
        EXPECT_EQ(editor.can_undo(), can_undo_before);
    }

    // crop_left 0.5 with crop_right 0.5 (sum 1.0 <= 1.0) succeeds
    {
        model::TransformProps t;
        t.crop_left.set_constant(0.5);
        t.crop_right.set_constant(0.5);
        SetClipProperties cmd{main_seq_id, clip_id, std::nullopt, std::nullopt, std::nullopt, t};
        ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));
    }
}

TEST(ClipCommandsTest, SetClipPropertiesLockedTrackFails) {
    core::UuidGenerator gen(503ULL);
    model::Project p = model::test::build_valid_project(gen);
    p.sequences.at(p.main_sequence).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(p), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto clip_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetClipProperties cmd{main_seq_id,  clip_id,      "NewName",
                          std::nullopt, std::nullopt, std::nullopt};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string_view::npos);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(ClipCommandsTest, SetClipPropertiesUnchangedNoHistoryEntry) {
    core::UuidGenerator gen(504ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto& clip = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0];

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetClipProperties cmd{main_seq_id,  clip.id,         clip.name,
                          clip.enabled, clip.blend_mode, clip.transform};
    auto res = editor.execute(cmd);
    ASSERT_TRUE(test::is_ok(res));
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(ClipCommandsTest, SetClipPropertiesUnknownClipNotFound) {
    core::UuidGenerator gen(505ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto unknown_clip_id = model::generate_id<model::ClipId>(gen);

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetClipProperties cmd{main_seq_id,  unknown_clip_id, "NewName",
                          std::nullopt, std::nullopt,    std::nullopt};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(ClipCommandsTest, SetAudioClipPropertiesVolumeAndFades) {
    core::UuidGenerator gen(506ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    // Track 1 in fixture is Audio track with clip 0 having duration 5.0 s
    const auto audio_clip_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id;

    // Volume 0.5 OK
    SetAudioClipProperties vol_cmd{main_seq_id, audio_clip_id, 0.5, std::nullopt, std::nullopt};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, vol_cmd)));

    // Volume -1 gives InvalidArgument
    {
        const auto snap_before = editor.snapshot();
        const bool can_undo_before = editor.can_undo();
        SetAudioClipProperties bad_vol{main_seq_id, audio_clip_id, -1.0, std::nullopt,
                                       std::nullopt};
        auto res = editor.execute(bad_vol);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_EQ(editor.snapshot(), snap_before);
        EXPECT_EQ(editor.can_undo(), can_undo_before);
    }

    // Fades 1 s and 2 s on 5 s clip OK
    SetAudioClipProperties fade_cmd1{main_seq_id, audio_clip_id, std::nullopt,
                                     model::test::duration_of_seconds(1),
                                     model::test::duration_of_seconds(2)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, fade_cmd1)));

    // fade_in 3 s with fade_out 3 s on 5 s clip (sum 6 s > 5 s) gives InvalidArgument
    {
        const auto snap_before = editor.snapshot();
        const bool can_undo_before = editor.can_undo();
        SetAudioClipProperties bad_fades{main_seq_id, audio_clip_id, std::nullopt,
                                         model::test::duration_of_seconds(3),
                                         model::test::duration_of_seconds(3)};
        auto res = editor.execute(bad_fades);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_EQ(editor.snapshot(), snap_before);
        EXPECT_EQ(editor.can_undo(), can_undo_before);
    }

    // fade_in 3 s with fade_out 2 s (sum 5 s == clip duration 5 s) OK
    SetAudioClipProperties exact_fades{main_seq_id, audio_clip_id, std::nullopt,
                                       model::test::duration_of_seconds(3),
                                       model::test::duration_of_seconds(2)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, exact_fades)));
}

TEST(ClipCommandsTest, SetAudioClipPropertiesVideoClipGivesInvalidArgument) {
    core::UuidGenerator gen(507ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    // Track 0 clip 0 is a Video clip
    const auto video_clip_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetAudioClipProperties cmd{main_seq_id, video_clip_id, 0.5, std::nullopt, std::nullopt};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(ClipCommandsTest, SetAudioClipPropertiesOverflowFadesGivesInvalidArgument) {
    core::UuidGenerator gen(508ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto audio_clip_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    // fade of INT64_MAX ticks plus 1 tick
    SetAudioClipProperties cmd{main_seq_id, audio_clip_id, std::nullopt,
                               core::Duration::from_ticks(std::numeric_limits<std::int64_t>::max()),
                               core::Duration::from_ticks(1)};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

}  // namespace
}  // namespace nxtcut::commands
