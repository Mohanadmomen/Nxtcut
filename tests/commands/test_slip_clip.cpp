#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "test_helper.hpp"
#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

constexpr std::int64_t kF = 29'400'000;

inline model::TimelineTime time_f(std::int64_t frames) {
    return model::TimelineTime::from_ticks(frames * kF);
}

inline core::Duration dur_f(std::int64_t frames) {
    return core::Duration::from_ticks(frames * kF);
}

inline model::SourceTime src_f(std::int64_t frames) {
    return model::SourceTime::from_ticks(frames * kF);
}

struct SlipFixture {
    model::Project project;
    model::SequenceId seq_id;
    model::ClipId clip_c_id;
    model::MediaId video_media_id;
};

SlipFixture create_slip_fixture(core::UuidGenerator& gen, std::int64_t media_dur_f = 1000) {
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    seq.frame_rate = core::frame_rates::k24;

    const auto video_media_id = std::get<model::VideoContent>(seq.tracks[0].clips[0].content).media;
    project.media[video_media_id].duration = dur_f(media_dur_f);
    project.media[video_media_id].video->frame_rate = core::frame_rates::k24;

    const auto clip_c_id = model::generate_id<model::ClipId>(gen);

    model::Clip c;
    c.id = clip_c_id;
    c.name = "ClipC";
    c.start = time_f(50);
    c.duration = dur_f(100);
    c.source_in = src_f(300);
    c.speed = model::Speed::normal();
    c.content = model::VideoContent{video_media_id};

    seq.tracks[0].clips.clear();
    seq.tracks[0].clips.push_back(c);

    seq.tracks[1].clips.clear();

    return SlipFixture{std::move(project), seq_id, clip_c_id, video_media_id};
}

TEST(SlipClipTest, S1_DeltaPlus25FAdvancesSourceIn) {
    core::UuidGenerator gen(9001ULL);
    auto fix = create_slip_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SlipClip cmd{fix.seq_id, fix.clip_c_id, dur_f(25)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));

    const auto& c = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(c.start, time_f(50));
    EXPECT_EQ(c.duration, dur_f(100));
    EXPECT_EQ(c.source_in, src_f(325));
}

TEST(SlipClipTest, S2_S3_DeltaNegativeSourceBoundary) {
    core::UuidGenerator gen(9002ULL);
    auto fix = create_slip_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // S2 delta -300F: src_in 0 (ok)
    SlipClip cmd_s2{fix.seq_id, fix.clip_c_id, dur_f(-300)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_s2)));

    const auto& c = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(c.source_in, src_f(0));

    // S3 delta -301F: InvalidArgument (from original fixture src_in 300)
    core::UuidGenerator gen3(9003ULL);
    auto fix3 = create_slip_fixture(gen3);
    auto ed_res3 = Editor::create(std::move(fix3.project), gen3);
    ASSERT_TRUE(test::is_ok(ed_res3));
    Editor& ed3 = *ed_res3.value();

    SlipClip cmd_s3{fix3.seq_id, fix3.clip_c_id, dur_f(-301)};
    auto res_s3 = ed3.execute(cmd_s3);
    ASSERT_FALSE(res_s3.has_value());
    EXPECT_EQ(res_s3.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(SlipClipTest, S4_DeltaPositiveMediaDurationBoundary) {
    core::UuidGenerator gen(9004ULL);
    auto fix = create_slip_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // S4 delta +600F: src_in 900 (ok, 900+100 = 1000)
    SlipClip cmd_s4{fix.seq_id, fix.clip_c_id, dur_f(600)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_s4)));

    const auto& c = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(c.source_in, src_f(900));

    // +601F: InvalidArgument (media duration exceeded)
    core::UuidGenerator gen5(9005ULL);
    auto fix5 = create_slip_fixture(gen5);
    auto ed_res5 = Editor::create(std::move(fix5.project), gen5);
    ASSERT_TRUE(test::is_ok(ed_res5));
    Editor& ed5 = *ed_res5.value();

    SlipClip cmd_exceed{fix5.seq_id, fix5.clip_c_id, dur_f(601)};
    auto res_exceed = ed5.execute(cmd_exceed);
    ASSERT_FALSE(res_exceed.has_value());
    EXPECT_EQ(res_exceed.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(SlipClipTest, S5_SpeedTwoDoubleSourceSpan) {
    core::UuidGenerator gen(9006ULL);
    auto fix = create_slip_fixture(gen);
    fix.project.sequences.at(fix.seq_id).tracks[0].clips[0].speed =
        model::Speed::create(2, 1).value();

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // delta +10F gives src_in 320; source span is 200F
    SlipClip cmd{fix.seq_id, fix.clip_c_id, dur_f(10)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));

    const auto& c = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(c.source_in, src_f(320));
}

TEST(SlipClipTest, S6_SpeedOneEleventhExactRounding) {
    core::UuidGenerator gen(9007ULL);
    auto fix = create_slip_fixture(gen);
    fix.project.sequences.at(fix.seq_id).tracks[0].clips[0].speed =
        model::Speed::create(1, 11).value();

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // delta +1F raises source_in by 2,672,727 ticks
    SlipClip cmd_p1{fix.seq_id, fix.clip_c_id, dur_f(1)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_p1)));
    EXPECT_EQ(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0].source_in.ticks(),
              src_f(300).ticks() + 2'672'727);

    // delta -1F from new base lowers source_in by 2,672,727 ticks
    SlipClip cmd_m1{fix.seq_id, fix.clip_c_id, dur_f(-1)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_m1)));
    EXPECT_EQ(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0].source_in.ticks(),
              src_f(300).ticks());
}

TEST(SlipClipTest, S7_SubframeSnappingDelta) {
    core::UuidGenerator gen(9008ULL);
    auto fix = create_slip_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // delta 0 -> empty
    SlipClip cmd_0{fix.seq_id, fix.clip_c_id, core::Duration::zero()};
    auto cs_0 = cmd_0.build(*editor.snapshot(), gen);
    ASSERT_TRUE(test::is_ok(cs_0));
    EXPECT_TRUE(cs_0.value().empty());

    // delta 14,699,999 ticks -> empty
    SlipClip cmd_sub{fix.seq_id, fix.clip_c_id, core::Duration::from_ticks(14'699'999)};
    auto cs_sub = cmd_sub.build(*editor.snapshot(), gen);
    ASSERT_TRUE(test::is_ok(cs_sub));
    EXPECT_TRUE(cs_sub.value().empty());

    // delta +14,700,000 ticks acts as +1F
    SlipClip cmd_half_p{fix.seq_id, fix.clip_c_id, core::Duration::from_ticks(14'700'000)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_half_p)));
    EXPECT_EQ(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0].source_in, src_f(301));

    // delta -14,700,000 ticks acts as -1F
    SlipClip cmd_half_m{fix.seq_id, fix.clip_c_id, core::Duration::from_ticks(-14'700'000)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_half_m)));
    EXPECT_EQ(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0].source_in, src_f(300));
}

TEST(SlipClipTest, S8_ClipKindsValidation) {
    core::UuidGenerator gen(9009ULL);
    auto fix = create_slip_fixture(gen);

    // Image clip -> InvalidArgument "slip"
    model::MediaId img_asset_id;
    bool found_image_asset = false;
    for (const auto& [mid, asset] : fix.project.media) {
        if (asset.kind == model::MediaKind::Image) {
            img_asset_id = mid;
            found_image_asset = true;
            break;
        }
    }
    ASSERT_TRUE(found_image_asset) << "fixture project has no image media asset";

    model::Clip img_clip;
    img_clip.id = model::generate_id<model::ClipId>(gen);
    img_clip.name = "Image";
    img_clip.start = time_f(200);
    img_clip.duration = dur_f(50);
    img_clip.source_in = model::SourceTime::zero();
    img_clip.content = model::ImageContent{img_asset_id};
    fix.project.sequences.at(fix.seq_id).tracks[0].clips.push_back(img_clip);

    // Text clip -> InvalidArgument "slip"
    model::Clip txt_clip;
    txt_clip.id = model::generate_id<model::ClipId>(gen);
    txt_clip.name = "Text";
    txt_clip.start = time_f(300);
    txt_clip.duration = dur_f(50);
    txt_clip.source_in = model::SourceTime::zero();
    txt_clip.content = model::TextContent{"Hello"};
    fix.project.sequences.at(fix.seq_id).tracks[0].clips.push_back(txt_clip);

    // Compound clip referencing SubSequence (Sequence 2)
    // Find sub sequence id from build_valid_project
    model::SequenceId sub_seq_id;
    for (const auto& [sid, s] : fix.project.sequences) {
        if (sid != fix.seq_id) {
            sub_seq_id = sid;
            break;
        }
    }
    // Set SubSequence duration long enough (e.g. 1000F)
    fix.project.sequences.at(sub_seq_id).tracks[0].clips[0].duration = dur_f(1000);

    model::Clip comp_clip;
    comp_clip.id = model::generate_id<model::ClipId>(gen);
    comp_clip.name = "Compound";
    comp_clip.start = time_f(400);
    comp_clip.duration = dur_f(50);
    comp_clip.source_in = src_f(100);
    comp_clip.content = model::CompoundContent{sub_seq_id};
    fix.project.sequences.at(fix.seq_id).tracks[0].clips.push_back(comp_clip);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Slip Image -> InvalidArgument "slip"
    SlipClip cmd_img{fix.seq_id, img_clip.id, dur_f(10)};
    auto res_img = editor.execute(cmd_img);
    ASSERT_FALSE(res_img.has_value());
    EXPECT_EQ(res_img.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res_img.error().message().find("slip"), std::string::npos);

    // Slip Text -> InvalidArgument "slip"
    SlipClip cmd_txt{fix.seq_id, txt_clip.id, dur_f(10)};
    auto res_txt = editor.execute(cmd_txt);
    ASSERT_FALSE(res_txt.has_value());
    EXPECT_EQ(res_txt.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res_txt.error().message().find("slip"), std::string::npos);

    // Slip Compound within bounds -> Success
    SlipClip cmd_comp{fix.seq_id, comp_clip.id, dur_f(20)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_comp)));

    const auto& c_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    EXPECT_EQ(c_clips[3].source_in, src_f(120));
}

TEST(SlipClipTest, S9_LinkedPartnersAndIgnoreLinks) {
    core::UuidGenerator gen(9010ULL);
    auto fix = create_slip_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    const auto link_c = model::generate_id<model::LinkId>(gen);
    const auto audio_c_id = model::generate_id<model::ClipId>(gen);

    seq.tracks[0].clips[0].link_id = link_c;

    model::Clip a_partner;
    a_partner.id = audio_c_id;
    a_partner.name = "AudioC";
    a_partner.start = time_f(50);
    a_partner.duration = dur_f(100);
    a_partner.source_in = src_f(300);
    a_partner.speed = model::Speed::normal();
    a_partner.link_id = link_c;
    a_partner.content = model::AudioContent{fix.video_media_id};
    seq.tracks[1].clips.push_back(a_partner);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Slip +20F slips both
    SlipClip cmd{fix.seq_id, fix.clip_c_id, dur_f(20)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;
    EXPECT_EQ(v_clips[0].source_in, src_f(320));
    EXPECT_EQ(a_clips[0].source_in, src_f(320));

    // ignore_links slips only named clip
    SlipClip cmd_ign{fix.seq_id, fix.clip_c_id, dur_f(10), true};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_ign)));

    const auto& v2 = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a2 = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;
    EXPECT_EQ(v2[0].source_in, src_f(330));
    EXPECT_EQ(a2[0].source_in, src_f(320));  // Audio unchanged

    // Partner violating handles fails entire command and leaves project unchanged
    const auto before = editor.snapshot();
    // Delta -325F: audio clip at src_in 320 would slip to -5F < 0
    SlipClip cmd_violating{fix.seq_id, fix.clip_c_id, dur_f(-325)};
    auto res_violating = editor.execute(cmd_violating);
    ASSERT_FALSE(res_violating.has_value());
    EXPECT_EQ(res_violating.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_TRUE(model::identical(*editor.snapshot(), *before));
}

TEST(SlipClipTest, S10_LockedTrackAndUnknownClip) {
    core::UuidGenerator gen(9011ULL);
    auto fix = create_slip_fixture(gen);
    fix.project.sequences.at(fix.seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SlipClip cmd{fix.seq_id, fix.clip_c_id, dur_f(10)};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);

    // Unknown clip -> NotFound
    const auto unknown_id = model::generate_id<model::ClipId>(gen);
    SlipClip cmd_unk{fix.seq_id, unknown_id, dur_f(10)};
    auto res_unk = editor.execute(cmd_unk);
    ASSERT_FALSE(res_unk.has_value());
    EXPECT_EQ(res_unk.error().code(), core::ErrorCode::NotFound);
}

TEST(SlipClipTest, S11_AudioFadesUntouchedAndEmptyCreatedClips) {
    core::UuidGenerator gen(9012ULL);
    auto fix = create_slip_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    const auto a_id = model::generate_id<model::ClipId>(gen);
    model::AudioContent ac{fix.video_media_id};
    ac.fade_in = dur_f(20);
    ac.fade_out = dur_f(30);

    model::Clip a;
    a.id = a_id;
    a.name = "Audio";
    a.start = time_f(0);
    a.duration = dur_f(100);
    a.source_in = src_f(200);
    a.speed = model::Speed::normal();
    a.content = ac;
    seq.tracks[1].clips.push_back(a);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SlipClip cmd{fix.seq_id, a_id, dur_f(10)};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));
    EXPECT_TRUE(receipt_res.value().created_clips.empty());

    const auto& a_clip = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips[0];
    EXPECT_EQ(a_clip.duration, dur_f(100));
    EXPECT_EQ(a_clip.source_in, src_f(210));
    const auto& a_content = std::get<model::AudioContent>(a_clip.content);
    EXPECT_EQ(a_content.fade_in, dur_f(20));
    EXPECT_EQ(a_content.fade_out, dur_f(30));
}

}  // namespace
}  // namespace nxtcut::commands
