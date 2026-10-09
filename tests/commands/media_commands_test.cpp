#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/media_commands.hpp>
#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/geometry.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/media.hpp>
#include <nxtcut/model/project.hpp>

#include <gtest/gtest.h>

#include <variant>

#include "test_helper.hpp"
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

TEST(MediaCommandsTest, AddMediaNilIdGetsGeneratedAndRecordedInReceipt) {
    core::UuidGenerator gen(401ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    model::MediaAsset asset;
    asset.id = model::MediaId::nil();
    asset.name = "new_image.png";
    asset.path = "/media/new_image.png";
    asset.kind = model::MediaKind::Image;
    asset.duration = core::Duration::zero();

    AddMedia cmd{asset};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));

    ASSERT_EQ(receipt_res->created_media.size(), 1U);
    const auto gen_id = receipt_res->created_media[0];
    EXPECT_FALSE(gen_id.is_nil());

    const auto* found = model::find_media(*editor.snapshot(), gen_id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->path, "/media/new_image.png");
}

TEST(MediaCommandsTest, AddMediaExplicitIdKept) {
    core::UuidGenerator gen(402ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto explicit_id = model::generate_id<model::MediaId>(gen);
    model::MediaAsset asset;
    asset.id = explicit_id;
    asset.name = "explicit.png";
    asset.path = "/media/explicit.png";
    asset.kind = model::MediaKind::Image;
    asset.duration = core::Duration::zero();

    AddMedia cmd{asset};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));

    ASSERT_EQ(receipt_res->created_media.size(), 1U);
    EXPECT_EQ(receipt_res->created_media[0], explicit_id);

    const auto* found = model::find_media(*editor.snapshot(), explicit_id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, explicit_id);
}

TEST(MediaCommandsTest, AddMediaDuplicateIdGivesAlreadyExists) {
    core::UuidGenerator gen(403ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Use existing video media asset from fixture
    model::MediaId existing_id{};
    for (const auto& [mid, m] : editor.snapshot()->media) {
        if (m.kind == model::MediaKind::Video) {
            existing_id = mid;
            break;
        }
    }

    model::MediaAsset asset;
    asset.id = existing_id;
    asset.name = "duplicate.png";
    asset.path = "/media/duplicate.png";
    asset.kind = model::MediaKind::Image;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    AddMedia cmd{asset};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::AlreadyExists);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(MediaCommandsTest, AddMediaValidationFailures) {
    core::UuidGenerator gen(404ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    // 1. Empty path gives InvalidArgument
    {
        model::MediaAsset asset;
        asset.name = "empty_path.png";
        asset.path = "";
        asset.kind = model::MediaKind::Image;

        AddMedia cmd{asset};
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_EQ(editor.snapshot(), snap_before);
        EXPECT_EQ(editor.can_undo(), can_undo_before);
    }

    // 2. Video without video stream info gives InvalidArgument
    {
        model::MediaAsset asset;
        asset.name = "novideo.mp4";
        asset.path = "/media/novideo.mp4";
        asset.kind = model::MediaKind::Video;
        asset.duration = model::test::duration_of_seconds(10);
        asset.video = std::nullopt;

        AddMedia cmd{asset};
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_EQ(editor.snapshot(), snap_before);
        EXPECT_EQ(editor.can_undo(), can_undo_before);
    }

    // 3. Audio with duration 0 gives InvalidArgument
    {
        model::MediaAsset asset;
        asset.name = "zero_audio.wav";
        asset.path = "/media/zero_audio.wav";
        asset.kind = model::MediaKind::Audio;
        asset.duration = core::Duration::zero();
        asset.audio = model::AudioStreamInfo{core::sample_rates::k48000, 2};

        AddMedia cmd{asset};
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_EQ(editor.snapshot(), snap_before);
        EXPECT_EQ(editor.can_undo(), can_undo_before);
    }
}

TEST(MediaCommandsTest, AddMediaImageAssetWithDurationZeroSucceeds) {
    core::UuidGenerator gen(405ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    model::MediaAsset asset;
    asset.name = "zero_image.png";
    asset.path = "/media/zero_image.png";
    asset.kind = model::MediaKind::Image;
    asset.duration = core::Duration::zero();

    AddMedia cmd{asset};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));
}

TEST(MediaCommandsTest, RemoveMediaUnusedSucceeds) {
    core::UuidGenerator gen(406ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Add an unused asset first
    model::MediaAsset asset;
    asset.name = "unused.png";
    asset.path = "/media/unused.png";
    asset.kind = model::MediaKind::Image;

    AddMedia add_cmd{asset};
    auto add_receipt = test::round_trip(editor, add_cmd);
    ASSERT_TRUE(test::is_ok(add_receipt));
    const auto unused_id = add_receipt->created_media[0];

    // Remove it
    RemoveMedia rm_cmd{unused_id};
    auto rm_receipt = test::round_trip(editor, rm_cmd);
    ASSERT_TRUE(test::is_ok(rm_receipt));
    EXPECT_EQ(model::find_media(*editor.snapshot(), unused_id), nullptr);
}

TEST(MediaCommandsTest, RemoveMediaInUseGivesInvalidArgument) {
    core::UuidGenerator gen(407ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Fixture video media asset is referenced by clips
    model::MediaId in_use_id{};
    for (const auto& [mid, m] : editor.snapshot()->media) {
        if (m.kind == model::MediaKind::Video) {
            in_use_id = mid;
            break;
        }
    }

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RemoveMedia cmd{in_use_id};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("in use"), std::string_view::npos);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(MediaCommandsTest, RemoveMediaUnknownNotFound) {
    core::UuidGenerator gen(408ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto unknown_id = model::generate_id<model::MediaId>(gen);
    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RemoveMedia cmd{unknown_id};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(MediaCommandsTest, RelinkMediaChangesPath) {
    core::UuidGenerator gen(409ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    model::MediaId target_id{};
    for (const auto& [mid, m] : editor.snapshot()->media) {
        if (m.kind == model::MediaKind::Video) {
            target_id = mid;
            break;
        }
    }

    RelinkMedia cmd{target_id, "/new_location/video.mp4"};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));

    const auto* found = model::find_media(*editor.snapshot(), target_id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->path, "/new_location/video.mp4");
}

TEST(MediaCommandsTest, RelinkMediaSamePathNoHistoryEntry) {
    core::UuidGenerator gen(410ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    model::MediaId target_id{};
    std::string current_path;
    for (const auto& [mid, m] : editor.snapshot()->media) {
        if (m.kind == model::MediaKind::Video) {
            target_id = mid;
            current_path = m.path;
            break;
        }
    }

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RelinkMedia cmd{target_id, current_path};
    auto res = editor.execute(cmd);
    ASSERT_TRUE(test::is_ok(res));
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(MediaCommandsTest, RelinkMediaEmptyPathInvalidArgument) {
    core::UuidGenerator gen(411ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto project_ptr = editor.snapshot();
    const auto& video_clip =
        project_ptr->sequences.at(project_ptr->main_sequence).tracks[0].clips[0];
    const model::MediaId target_id = std::get<model::VideoContent>(video_clip.content).media;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RelinkMedia cmd{target_id, ""};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

}  // namespace
}  // namespace nxtcut::commands
