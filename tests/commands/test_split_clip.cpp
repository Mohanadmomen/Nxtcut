#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>
#include "test_helper.hpp"

namespace nxtcut::commands {
namespace {

using model::test::duration_of_seconds;
using model::test::timeline_at_seconds;

TEST(SplitClipTest, SplitV1at2sSplitsLinkedA1AndCoordinatesLinks) {
    core::UuidGenerator gen(6001ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto orig_link_id =
        editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].link_id;
    ASSERT_TRUE(orig_link_id.has_value());

    SplitClip cmd{main_seq_id, v1_id, timeline_at_seconds(2), false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    // V1 left: 0..2s, keeps orig id & link
    EXPECT_EQ(v_clips[0].id, v1_id);
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(2));
    EXPECT_EQ(v_clips[0].link_id, orig_link_id);

    // V1 right: 2..5s
    EXPECT_NE(v_clips[1].id, v1_id);
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(2));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(3));
    ASSERT_TRUE(v_clips[1].link_id.has_value());

    // A1 left: 0..2s, keeps orig link
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(2));
    EXPECT_EQ(a_clips[0].link_id, orig_link_id);

    // A1 right: 2..5s
    EXPECT_EQ(a_clips[1].start, timeline_at_seconds(2));
    EXPECT_EQ(a_clips[1].duration, duration_of_seconds(3));
    ASSERT_TRUE(a_clips[1].link_id.has_value());

    // Two right halves share one new LinkId
    EXPECT_EQ(v_clips[1].link_id, a_clips[1].link_id);
    EXPECT_NE(v_clips[1].link_id, orig_link_id);

    // Right source_in = orig source_in + 2s (speed 1)
    EXPECT_EQ(v_clips[1].source_in.ticks(), timeline_at_seconds(2).ticks());
}

TEST(SplitClipTest, SplitSpeed2ClipAdvancesSourceInBy4s) {
    core::UuidGenerator gen(6002ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Set V1 to speed 2/1, unlink to isolate
    project.sequences.at(main_seq_id).tracks[0].clips[0].speed =
        model::Speed::create(2, 1).value();
    project.sequences.at(main_seq_id).tracks[0].clips[0].link_id = std::nullopt;
    project.sequences.at(main_seq_id).tracks[1].clips[0].link_id = std::nullopt;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    SplitClip cmd{main_seq_id, v1_id, timeline_at_seconds(2), true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    // v_clips[1] is right half: source_in should be 0 + 2s * 2 = 4s
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(2));
    EXPECT_EQ(v_clips[1].source_in.ticks(), timeline_at_seconds(4).ticks());
}

TEST(SplitClipTest, SplitAtStartOrEndGivesInvalidArgument) {
    core::UuidGenerator gen(6003ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Split at start (0s)
    {
        SplitClip cmd{main_seq_id, v1_id, timeline_at_seconds(0), false};
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    }

    // Split at end (5s)
    {
        SplitClip cmd{main_seq_id, v1_id, timeline_at_seconds(5), false};
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    }
}

TEST(SplitClipTest, AtWithOneTickOfNoiseSnapsTo2s) {
    core::UuidGenerator gen(6004ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    const auto noisy_at =
        model::TimelineTime::from_ticks(timeline_at_seconds(2).ticks() + 1);

    SplitClip cmd{main_seq_id, v1_id, noisy_at, false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(2));
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(2));
}

TEST(SplitClipTest, LockedAudioTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(6005ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    project.sequences.at(main_seq_id).tracks[1].locked = true;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    SplitClip cmd{main_seq_id, v1_id, timeline_at_seconds(2), false};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(SplitClipTest, IgnoreLinksSplitsOnlyV1) {
    core::UuidGenerator gen(6006ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto orig_link_id =
        editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].link_id;
    ASSERT_TRUE(orig_link_id.has_value());

    SplitClip cmd{main_seq_id, v1_id, timeline_at_seconds(2), true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    // V1 left: keeps orig link
    EXPECT_EQ(v_clips[0].link_id, orig_link_id);
    // V1 right: has NO link
    EXPECT_FALSE(v_clips[1].link_id.has_value());
    // A1 not split, stays 5s, keeps link to left half
    ASSERT_EQ(a_clips.size(), 1U);
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(5));
    EXPECT_EQ(a_clips[0].link_id, orig_link_id);
}

TEST(SplitClipTest, AudioFadeSplitBehavior) {
    core::UuidGenerator gen(6007ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Set fades on audio clip A1
    auto& audio_content =
        std::get<model::AudioContent>(project.sequences.at(main_seq_id).tracks[1].clips[0].content);
    audio_content.fade_in = duration_of_seconds(1);
    audio_content.fade_out = duration_of_seconds(1);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto a1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id;

    SplitClip cmd{main_seq_id, a1_id, timeline_at_seconds(2), true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    const auto& left_audio = std::get<model::AudioContent>(a_clips[0].content);
    const auto& right_audio = std::get<model::AudioContent>(a_clips[1].content);

    // Left keeps fade_in, fade_out becomes 0
    EXPECT_EQ(left_audio.fade_in, duration_of_seconds(1));
    EXPECT_EQ(left_audio.fade_out, core::Duration::zero());

    // Right fade_in becomes 0, keeps fade_out
    EXPECT_EQ(right_audio.fade_in, core::Duration::zero());
    EXPECT_EQ(right_audio.fade_out, duration_of_seconds(1));
}

TEST(SplitClipTest, PartnerNotContainingSplitTimeIsLeftAlone) {
    core::UuidGenerator gen(6008ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Change A1's range to 0..1s, sharing link with V1 (0..5s)
    project.sequences.at(main_seq_id).tracks[1].clips[0].duration = duration_of_seconds(1);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Split at 2s: A1 (0..1s) does NOT contain 2s
    SplitClip cmd{main_seq_id, v1_id, timeline_at_seconds(2), false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    ASSERT_EQ(a_clips.size(), 1U);
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(1));
}

TEST(SplitClipTest, AudioFadeSplitSatisfiesValidate) {
    core::UuidGenerator gen(6011ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    auto& a1 = project.sequences.at(main_seq_id).tracks[1].clips[0];
    auto& a1_content = std::get<model::AudioContent>(a1.content);
    a1_content.fade_in = duration_of_seconds(1);
    a1_content.fade_out = duration_of_seconds(1);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto a1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id;

    SplitClip cmd{main_seq_id, a1_id, timeline_at_seconds(4), true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    ASSERT_EQ(a_clips.size(), 2U);
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(4));
    EXPECT_EQ(a_clips[1].start, timeline_at_seconds(4));
    EXPECT_EQ(a_clips[1].duration, duration_of_seconds(1));
}

}  // namespace
}  // namespace nxtcut::commands
