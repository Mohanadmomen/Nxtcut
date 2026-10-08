#include <nxtcut/model/validation.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "test_fixtures.hpp"

namespace nxtcut::model {
namespace {

constexpr std::int64_t T = core::kTicksPerSecond;

TEST(ValidationTest, ValidFixtureProducesZeroIssues) {
    const Project project = test::build_valid_project(42ULL);
    const auto issues = collect_issues(project);
    EXPECT_TRUE(issues.empty()) << "Expected 0 issues, got " << issues.size();

    const auto status = validate(project);
    EXPECT_TRUE(status.has_value());
}

// 1. Two clips on one track at 0 to 5 s and 4 to 9 s: ClipOverlap, path of the second
TEST(ValidationTest, MutationClipOverlap) {
    Project p = test::build_valid_project();
    auto& track = p.sequences[p.main_sequence].tracks[0];
    // clip 0 is at [0, 5s)
    // clip 1 is at [5s, 10s), change it to [4s, 9s)
    track.clips[1].start = TimelineTime::from_ticks(4 * T);
    track.clips[1].duration = core::Duration::from_ticks(5 * T);

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::ClipOverlap);
    const std::string expected_path =
        "sequences/" + p.main_sequence.to_string() + "/tracks/0/clips/1";
    EXPECT_EQ(issues[0].path, expected_path);
}

// 2. Touching clips 0 to 5 s and 5 to 10 s: NO issue
TEST(ValidationTest, TouchingClipsNoIssue) {
    const Project p = test::build_valid_project();
    // In fixture, clip 0 is [0, 5s) and clip 1 is [5s, 10s), which touch
    const auto issues = collect_issues(p);
    EXPECT_TRUE(issues.empty());
}

// 3. Duration zero: InvalidDuration
TEST(ValidationTest, MutationDurationZero) {
    Project p = test::build_valid_project();
    p.sequences[p.main_sequence].tracks[0].clips[0].duration = core::Duration::zero();

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::InvalidDuration);
}

// 4. Negative duration: InvalidDuration
TEST(ValidationTest, MutationNegativeDuration) {
    Project p = test::build_valid_project();
    p.sequences[p.main_sequence].tracks[0].clips[0].duration = core::Duration::from_ticks(-100);

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::InvalidDuration);
}

// 5. Negative start: NegativeStart
TEST(ValidationTest, MutationNegativeStart) {
    Project p = test::build_valid_project();
    p.sequences[p.main_sequence].tracks[0].clips[0].start = TimelineTime::from_ticks(-50);

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::NegativeStart);
}

// 6. Video clip on audio track: ClipKindMismatch
TEST(ValidationTest, MutationClipKindMismatch) {
    Project p = test::build_valid_project();
    // In track 1 (Audio track), replace audio content with video content
    auto& audio_track = p.sequences[p.main_sequence].tracks[1];
    audio_track.clips[0].link_id = std::nullopt;
    p.sequences[p.main_sequence].tracks[0].clips[0].link_id = std::nullopt;

    const MediaId vid_id =
        std::get<VideoContent>(p.sequences[p.main_sequence].tracks[0].clips[0].content).media;
    audio_track.clips[0].content = VideoContent{vid_id};

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::ClipKindMismatch);
}

// 7. Unknown media id: MissingMedia
TEST(ValidationTest, MutationMissingMedia) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(999ULL);
    const MediaId unknown_id = generate_id<MediaId>(gen);
    p.sequences[p.main_sequence].tracks[0].clips[0].content = VideoContent{unknown_id};

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::MissingMedia);
}

// 8. Video clip pointing at an audio-only asset: MediaKindMismatch
TEST(ValidationTest, MutationMediaKindMismatch) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(888ULL);
    const MediaId audio_asset_id = generate_id<MediaId>(gen);

    MediaAsset audio_only;
    audio_only.id = audio_asset_id;
    audio_only.name = "audio.wav";
    audio_only.path = "/media/audio.wav";
    audio_only.kind = MediaKind::Audio;
    audio_only.duration = core::Duration::from_seconds(60.0);
    audio_only.audio = AudioStreamInfo{core::sample_rates::k48000, 2};
    audio_only.video = std::nullopt;

    p.media[audio_asset_id] = audio_only;
    p.sequences[p.main_sequence].tracks[0].clips[0].content = VideoContent{audio_asset_id};

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::MediaKindMismatch);
}

// 9. Media duration 10 s with clip source_in 2 s and duration 8 s at speed 1/1 (NO issue)
// and duration 8 s plus 1 tick (SourceRangeOutOfBounds)
TEST(ValidationTest, MutationSourceRangeSpeedOne) {
    Project p = test::build_valid_project();
    // Remove links to avoid audio clip secondary issues
    p.sequences[p.main_sequence].tracks[0].clips[0].link_id = std::nullopt;
    p.sequences[p.main_sequence].tracks[1].clips[0].link_id = std::nullopt;

    // Remove following clips on track 0 so stretching clip 0 does not overlap them
    p.sequences[p.main_sequence].tracks[0].clips.resize(1);

    const MediaId vid_id =
        std::get<VideoContent>(p.sequences[p.main_sequence].tracks[0].clips[0].content).media;
    p.media[vid_id].duration = core::Duration::from_ticks(10 * T);

    auto& clip = p.sequences[p.main_sequence].tracks[0].clips[0];
    clip.source_in = SourceTime::from_ticks(2 * T);
    clip.duration = core::Duration::from_ticks(8 * T);
    clip.speed = Speed::normal();

    // With duration 8s: source_in (2s) + source_span (8s) = 10s <= 10s: NO issue
    auto issues = collect_issues(p);
    EXPECT_TRUE(issues.empty());

    // With duration 8s + 1 tick: source_in (2s) + source_span (8s + 1 tick) = 10s + 1 tick > 10s
    clip.duration = core::Duration::from_ticks(8 * T + 1);
    issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::SourceRangeOutOfBounds);
}

// 10. Media 10 s, source_in 2 s, speed 2/1, duration 4 s (NO issue)
// and 4 s plus 1 tick (SourceRangeOutOfBounds)
TEST(ValidationTest, MutationSourceRangeSpeedTwo) {
    Project p = test::build_valid_project();
    p.sequences[p.main_sequence].tracks[0].clips[0].link_id = std::nullopt;
    p.sequences[p.main_sequence].tracks[1].clips[0].link_id = std::nullopt;

    const MediaId vid_id =
        std::get<VideoContent>(p.sequences[p.main_sequence].tracks[0].clips[0].content).media;
    p.media[vid_id].duration = core::Duration::from_ticks(10 * T);

    auto& clip = p.sequences[p.main_sequence].tracks[0].clips[0];
    clip.source_in = SourceTime::from_ticks(2 * T);
    clip.speed = Speed::create(2, 1).value();
    clip.duration = core::Duration::from_ticks(4 * T);

    // Duration 4s: ceil(4s * 2 / 1) = 8s; 2s + 8s = 10s <= 10s: NO issue
    auto issues = collect_issues(p);
    EXPECT_TRUE(issues.empty());

    // Duration 4s + 1 tick: ceil((4s + 1 tick) * 2 / 1) = 8s + 2 ticks; 2s + 8s + 2 ticks > 10s
    clip.duration = core::Duration::from_ticks(4 * T + 1);
    issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::SourceRangeOutOfBounds);
}

// 11. One clip carrying a LinkId alone: LinkIncomplete
TEST(ValidationTest, MutationLinkIncomplete) {
    Project p = test::build_valid_project();
    // Remove link_id from the audio clip, leaving only the video clip with link_id
    p.sequences[p.main_sequence].tracks[1].clips[0].link_id = std::nullopt;

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::LinkIncomplete);
}

// 12. Two clips sharing a LinkId but in different sequences: LinkAcrossSequences (and NOT
// LinkIncomplete)
TEST(ValidationTest, MutationLinkAcrossSequences) {
    Project p = test::build_valid_project();
    const LinkId link_id = *p.sequences[p.main_sequence].tracks[0].clips[0].link_id;

    // Remove link from audio clip in Sequence 1
    p.sequences[p.main_sequence].tracks[1].clips[0].link_id = std::nullopt;

    // Find Sequence 2 (the non-main sequence) and give its clip the link_id
    for (auto& [sid, seq] : p.sequences) {
        if (sid != p.main_sequence) {
            seq.tracks[0].clips[0].link_id = link_id;
            break;
        }
    }

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::LinkAcrossSequences);
}

// 13. Same ClipId on two clips: DuplicateId
TEST(ValidationTest, MutationDuplicateClipId) {
    Project p = test::build_valid_project();
    auto& track = p.sequences[p.main_sequence].tracks[0];
    track.clips[1].id = track.clips[0].id;

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::DuplicateId);
}

// 14. Compound clip pointing at an unknown sequence: MissingSequence
TEST(ValidationTest, MutationMissingSequence) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(777ULL);
    const SequenceId unknown_seq = generate_id<SequenceId>(gen);
    p.sequences[p.main_sequence].tracks[0].clips[2].content = CompoundContent{unknown_seq};

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::MissingSequence);
}

// 15. Compound cycle A to B to A: CompoundCycle (exactly one issue)
TEST(ValidationTest, MutationCompoundCycle) {
    Project p = test::build_valid_project();
    // Sequence 1 already has compound clip 2 pointing at Sequence 2.
    // In Sequence 2, add a valid compound clip pointing at Sequence 1.
    for (auto& [sid, seq] : p.sequences) {
        if (sid != p.main_sequence) {
            core::UuidGenerator gen(555ULL);
            Clip back_comp;
            back_comp.id = generate_id<ClipId>(gen);
            back_comp.name = "BackCompound";
            back_comp.start = TimelineTime::from_ticks(10 * T);
            back_comp.duration = core::Duration::from_ticks(5 * T);
            back_comp.source_in = SourceTime::zero();
            back_comp.speed = Speed::normal();
            back_comp.content = CompoundContent{p.main_sequence};
            seq.tracks[0].clips.push_back(back_comp);
            break;
        }
    }

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::CompoundCycle);
}

// 16. Missing main sequence: MissingMainSequence
TEST(ValidationTest, MutationMissingMainSequence) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(333ULL);
    p.main_sequence = generate_id<SequenceId>(gen);

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::MissingMainSequence);
}

// 17. Canvas width zero: InvalidCanvas
TEST(ValidationTest, MutationInvalidCanvas) {
    Project p = test::build_valid_project();
    p.sequences[p.main_sequence].canvas.width = 0;

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::InvalidCanvas);
}

// 18. Sequences map key different from sequence.id: IdMismatch
TEST(ValidationTest, MutationIdMismatch) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(222ULL);
    const SequenceId wrong_key = generate_id<SequenceId>(gen);

    // Find Sequence 2, remove and reinsert under wrong key
    SequenceId orig_s2{};
    for (const auto& [sid, seq] : p.sequences) {
        if (sid != p.main_sequence) {
            orig_s2 = sid;
            break;
        }
    }

    Sequence s2 = p.sequences[orig_s2];
    p.sequences.erase(orig_s2);
    p.sequences[wrong_key] = s2;
    // Update compound clip in MainSequence to point to wrong_key so it doesn't fail MissingSequence
    p.sequences[p.main_sequence].tracks[0].clips[2].content = CompoundContent{wrong_key};

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::IdMismatch);
}

// 19. Clip with start INT64_MAX minus 5 ticks and duration 10 ticks: ClipRangeOverflow
TEST(ValidationTest, MutationClipRangeOverflow) {
    Project p = test::build_valid_project();
    auto& clip = p.sequences[p.main_sequence].tracks[0].clips[0];
    clip.start = TimelineTime::from_ticks(std::numeric_limits<std::int64_t>::max() - 5);
    clip.duration = core::Duration::from_ticks(10);

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::ClipRangeOverflow);
}

TEST(ValidationTest, CompoundCyclesTwoDisjointCyclesGiveTwoIssues) {
    core::UuidGenerator gen(301ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);
    const SequenceId seq_c = generate_id<SequenceId>(gen);
    const SequenceId seq_d = generate_id<SequenceId>(gen);

    Project p;
    p.main_sequence = seq_a;

    p.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b});
    p.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_a});
    p.sequences[seq_c] = test::make_compound_sequence(gen, seq_c, {seq_d});
    p.sequences[seq_d] = test::make_compound_sequence(gen, seq_d, {seq_c});

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 2U);
    EXPECT_EQ(issues[0].code, ValidationCode::CompoundCycle);
    EXPECT_EQ(issues[1].code, ValidationCode::CompoundCycle);
}

TEST(ValidationTest, CompoundCyclesBranchingCyclesGiveTwoIssues) {
    // A -> B -> A and A -> C -> A
    core::UuidGenerator gen(302ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);
    const SequenceId seq_c = generate_id<SequenceId>(gen);

    Project p;
    p.main_sequence = seq_a;

    p.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b, seq_c});
    p.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_a});
    p.sequences[seq_c] = test::make_compound_sequence(gen, seq_c, {seq_a});

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 2U);
    EXPECT_EQ(issues[0].code, ValidationCode::CompoundCycle);
    EXPECT_EQ(issues[1].code, ValidationCode::CompoundCycle);
}

TEST(ValidationTest, CompoundCyclesSingleCycleAtoBtoAGivesOneIssue) {
    core::UuidGenerator gen(303ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);

    Project p;
    p.main_sequence = seq_a;

    p.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b});
    p.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_a});

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::CompoundCycle);
}

TEST(ValidationTest, CompoundCyclesSelfLoopGivesOneIssue) {
    core::UuidGenerator gen(304ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);

    Project p;
    p.main_sequence = seq_a;
    p.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_a});

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::CompoundCycle);
}

TEST(ValidationTest, CompoundCyclesDiamondGivesZeroIssues) {
    // A -> B, A -> C, B -> D, C -> D
    core::UuidGenerator gen(305ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);
    const SequenceId seq_c = generate_id<SequenceId>(gen);
    const SequenceId seq_d = generate_id<SequenceId>(gen);

    Project p;
    p.main_sequence = seq_a;

    // Sequence D has a text clip so its duration is 5s
    Sequence s_d;
    s_d.id = seq_d;
    s_d.canvas = core::Size<std::int32_t>{1920, 1080};
    Track t_d;
    t_d.id = generate_id<TrackId>(gen);
    t_d.kind = TrackKind::Video;
    Clip c_d;
    c_d.id = generate_id<ClipId>(gen);
    c_d.start = TimelineTime::zero();
    c_d.duration = core::Duration::from_ticks(5 * T);
    c_d.source_in = SourceTime::zero();
    c_d.speed = Speed::normal();
    c_d.content = TextContent{"end"};
    t_d.clips.push_back(c_d);
    s_d.tracks.push_back(t_d);
    p.sequences[seq_d] = s_d;

    p.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_d});
    p.sequences[seq_c] = test::make_compound_sequence(gen, seq_c, {seq_d});
    p.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b, seq_c});

    const auto issues = collect_issues(p);
    EXPECT_TRUE(issues.empty());
}

TEST(ValidationTest, CompoundCyclesParallelEdgesGiveOneIssue) {
    // B holds TWO compound clips pointing to A, with A pointing to B
    core::UuidGenerator gen(306ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);

    Project p;
    p.main_sequence = seq_a;
    p.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b});
    p.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_a, seq_a});

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::CompoundCycle);
}

TEST(ValidationTest, MultipleProblemsReportsAll) {
    Project p = test::build_valid_project();

    const SequenceId main = p.main_sequence;
    p.sequences[main].canvas.height = 0;
    p.sequences[main].tracks[0].clips[0].start = TimelineTime::from_ticks(-100);

    core::UuidGenerator gen(101ULL);
    p.main_sequence = generate_id<SequenceId>(gen);

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 3U);
    EXPECT_EQ(issues[0].code, ValidationCode::MissingMainSequence);
    EXPECT_EQ(issues[1].code, ValidationCode::InvalidCanvas);
    EXPECT_EQ(issues[2].code, ValidationCode::NegativeStart);

    const auto status = validate(p);
    EXPECT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code(), core::ErrorCode::InvalidArgument);

    const std::string msg(status.error().message());
    EXPECT_NE(msg.find(std::to_string(issues.size()) + " validation issue(s)"), std::string::npos);
    EXPECT_NE(msg.find(std::string(to_string(issues[0].code))), std::string::npos);
}

TEST(ValidationTest, ValidationCodeToStringCoverage) {
    EXPECT_EQ(to_string(ValidationCode::IdMismatch), "IdMismatch");
    EXPECT_EQ(to_string(ValidationCode::DuplicateId), "DuplicateId");
    EXPECT_EQ(to_string(ValidationCode::MissingMainSequence), "MissingMainSequence");
    EXPECT_EQ(to_string(ValidationCode::InvalidCanvas), "InvalidCanvas");
    EXPECT_EQ(to_string(ValidationCode::InvalidDuration), "InvalidDuration");
    EXPECT_EQ(to_string(ValidationCode::NegativeStart), "NegativeStart");
    EXPECT_EQ(to_string(ValidationCode::ClipRangeOverflow), "ClipRangeOverflow");
    EXPECT_EQ(to_string(ValidationCode::ClipOverlap), "ClipOverlap");
    EXPECT_EQ(to_string(ValidationCode::ClipKindMismatch), "ClipKindMismatch");
    EXPECT_EQ(to_string(ValidationCode::MissingMedia), "MissingMedia");
    EXPECT_EQ(to_string(ValidationCode::MediaKindMismatch), "MediaKindMismatch");
    EXPECT_EQ(to_string(ValidationCode::SourceRangeOutOfBounds), "SourceRangeOutOfBounds");
    EXPECT_EQ(to_string(ValidationCode::LinkIncomplete), "LinkIncomplete");
    EXPECT_EQ(to_string(ValidationCode::LinkAcrossSequences), "LinkAcrossSequences");
    EXPECT_EQ(to_string(ValidationCode::MissingSequence), "MissingSequence");
    EXPECT_EQ(to_string(ValidationCode::CompoundCycle), "CompoundCycle");
}

TEST(ValidationTest, DuplicateTrackId) {
    Project p = test::build_valid_project();
    auto& tracks = p.sequences[p.main_sequence].tracks;
    ASSERT_GE(tracks.size(), 2U);
    tracks[1].id = tracks[0].id;

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::DuplicateId);
    EXPECT_TRUE(issues[0].path.ends_with("/tracks/1"));
}

TEST(ValidationTest, DuplicateEffectId) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(123ULL);
    const EffectId eff1 = generate_id<EffectId>(gen);
    const EffectId eff2 = generate_id<EffectId>(gen);

    auto& clip = p.sequences[p.main_sequence].tracks[0].clips[0];
    EffectInstance e1;
    e1.id = eff1;
    e1.effect_type = "effect1";
    EffectInstance e2;
    e2.id = eff2;
    e2.effect_type = "effect2";

    clip.effects = {e1, e2};

    // Control: with different ids, no issue
    const auto control_issues = collect_issues(p);
    EXPECT_TRUE(control_issues.empty());

    // Duplicate effect id on effect 1
    clip.effects[1].id = eff1;
    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::DuplicateId);
    EXPECT_TRUE(issues[0].path.ends_with("/effects/1"));
}

TEST(ValidationTest, DuplicateMarkerId) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(124ULL);
    const MarkerId mid = generate_id<MarkerId>(gen);

    Marker m1;
    m1.id = mid;
    m1.time = TimelineTime::zero();
    m1.label = "m1";

    Marker m2;
    m2.id = mid;
    m2.time = TimelineTime::from_ticks(1 * T);
    m2.label = "m2";

    p.sequences[p.main_sequence].markers = {m1, m2};

    const auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::DuplicateId);
    EXPECT_TRUE(issues[0].path.ends_with("/markers/1"));
}

TEST(ValidationTest, TextClipRules) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(125ULL);

    Track new_track;
    new_track.id = generate_id<TrackId>(gen);
    new_track.kind = TrackKind::Video;

    Clip txt_clip;
    txt_clip.id = generate_id<ClipId>(gen);
    txt_clip.name = "TextClip";
    txt_clip.start = TimelineTime::from_ticks(15 * T);
    txt_clip.duration = core::Duration::from_ticks(5 * T);
    txt_clip.source_in = SourceTime::zero();
    txt_clip.speed = Speed::normal();
    txt_clip.content = TextContent{"SampleText"};
    new_track.clips.push_back(txt_clip);

    p.sequences[p.main_sequence].tracks.push_back(new_track);

    // Valid text clip (source_in 0, speed 1/1): 0 issues
    auto issues = collect_issues(p);
    EXPECT_TRUE(issues.empty());

    // source_in = 1 tick: exactly 1 SourceRangeOutOfBounds
    p.sequences[p.main_sequence].tracks.back().clips[0].source_in = SourceTime::from_ticks(1);
    issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::SourceRangeOutOfBounds);

    // speed 2/1 (source_in 0): exactly 1 SourceRangeOutOfBounds
    p.sequences[p.main_sequence].tracks.back().clips[0].source_in = SourceTime::zero();
    p.sequences[p.main_sequence].tracks.back().clips[0].speed = Speed::create(2, 1).value();
    issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::SourceRangeOutOfBounds);
}

TEST(ValidationTest, ImageClipRules) {
    Project p = test::build_valid_project();

    Clip* img_clip = nullptr;
    for (auto& track : p.sequences[p.main_sequence].tracks) {
        for (auto& clip : track.clips) {
            if (std::holds_alternative<ImageContent>(clip.content)) {
                img_clip = &clip;
                break;
            }
        }
        if (img_clip != nullptr) {
            break;
        }
    }
    ASSERT_NE(img_clip, nullptr);

    // source_in = 1 tick: exactly 1 SourceRangeOutOfBounds
    img_clip->source_in = SourceTime::from_ticks(1);
    auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::SourceRangeOutOfBounds);

    // speed 2/1: exactly 1 SourceRangeOutOfBounds
    img_clip->source_in = SourceTime::zero();
    img_clip->speed = Speed::create(2, 1).value();
    issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::SourceRangeOutOfBounds);
}

TEST(ValidationTest, AudioClipOnNonAudioAsset) {
    Project p = test::build_valid_project();

    Clip* audio_clip = nullptr;
    for (auto& track : p.sequences[p.main_sequence].tracks) {
        for (auto& clip : track.clips) {
            if (std::holds_alternative<AudioContent>(clip.content)) {
                audio_clip = &clip;
                break;
            }
        }
        if (audio_clip != nullptr) {
            break;
        }
    }
    ASSERT_NE(audio_clip, nullptr);

    MediaId image_media_id{};
    bool found_image = false;
    for (const auto& [mid, asset] : p.media) {
        if (asset.kind == MediaKind::Image) {
            image_media_id = mid;
            found_image = true;
            break;
        }
    }
    ASSERT_TRUE(found_image);

    // AudioContent pointing at the fixture's IMAGE asset: exactly 1 MediaKindMismatch
    audio_clip->content = AudioContent{image_media_id};
    auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::MediaKindMismatch);

    // AudioContent pointing at Video asset whose audio is std::nullopt (using new asset): exactly 1
    // MediaKindMismatch
    Project p2 = test::build_valid_project();
    Clip* audio_clip2 = nullptr;
    for (auto& track : p2.sequences[p2.main_sequence].tracks) {
        for (auto& clip : track.clips) {
            if (std::holds_alternative<AudioContent>(clip.content)) {
                audio_clip2 = &clip;
                break;
            }
        }
        if (audio_clip2 != nullptr) {
            break;
        }
    }
    ASSERT_NE(audio_clip2, nullptr);

    core::UuidGenerator gen(666ULL);
    const MediaId video_no_audio_id = generate_id<MediaId>(gen);
    MediaAsset video_no_audio;
    video_no_audio.id = video_no_audio_id;
    video_no_audio.name = "no_audio.mp4";
    video_no_audio.path = "/media/no_audio.mp4";
    video_no_audio.kind = MediaKind::Video;
    video_no_audio.duration = core::Duration::from_seconds(60.0);
    video_no_audio.video =
        VideoStreamInfo{core::Size<std::int32_t>{1920, 1080}, core::frame_rates::k30};
    video_no_audio.audio = std::nullopt;

    p2.media[video_no_audio_id] = video_no_audio;
    audio_clip2->content = AudioContent{video_no_audio_id};

    issues = collect_issues(p2);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::MediaKindMismatch);
}

TEST(ValidationTest, AudioClipSourceRange) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(777ULL);

    const MediaId audio_asset_id = generate_id<MediaId>(gen);
    MediaAsset audio_asset;
    audio_asset.id = audio_asset_id;
    audio_asset.name = "audio10s.wav";
    audio_asset.path = "/media/audio10s.wav";
    audio_asset.kind = MediaKind::Audio;
    audio_asset.duration = core::Duration::from_ticks(10 * T);
    audio_asset.audio = AudioStreamInfo{core::sample_rates::k48000, 2};
    p.media[audio_asset_id] = audio_asset;

    Track* audio_track = nullptr;
    Clip* audio_clip = nullptr;
    for (auto& track : p.sequences[p.main_sequence].tracks) {
        if (track.kind == TrackKind::Audio) {
            audio_track = &track;
            for (auto& clip : track.clips) {
                if (std::holds_alternative<AudioContent>(clip.content)) {
                    audio_clip = &clip;
                    break;
                }
            }
        }
    }
    ASSERT_NE(audio_track, nullptr);
    ASSERT_NE(audio_clip, nullptr);

    const auto link = audio_clip->link_id;
    if (link.has_value()) {
        for (auto& track : p.sequences[p.main_sequence].tracks) {
            for (auto& clip : track.clips) {
                if (clip.link_id == link) {
                    clip.link_id = std::nullopt;
                }
            }
        }
    }

    Clip isolated_audio = *audio_clip;
    isolated_audio.link_id = std::nullopt;
    isolated_audio.content = AudioContent{audio_asset_id};
    isolated_audio.source_in = SourceTime::from_ticks(2 * T);
    isolated_audio.duration = core::Duration::from_ticks(8 * T);
    isolated_audio.speed = Speed::normal();

    audio_track->clips = {isolated_audio};

    // source_in 2 s, duration 8 s, speed 1/1: 0 issues
    auto issues = collect_issues(p);
    EXPECT_TRUE(issues.empty());

    // Duration 8 s plus 1 tick: exactly 1 SourceRangeOutOfBounds
    audio_track->clips[0].duration = core::Duration::from_ticks(8 * T + 1);
    issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::SourceRangeOutOfBounds);
}

TEST(ValidationTest, CompoundSourceRange) {
    Project p = test::build_valid_project();

    Clip* comp_clip = nullptr;
    Track* comp_track = nullptr;
    std::size_t comp_idx = 0;
    for (auto& track : p.sequences[p.main_sequence].tracks) {
        for (std::size_t i = 0; i < track.clips.size(); ++i) {
            if (std::holds_alternative<CompoundContent>(track.clips[i].content)) {
                comp_clip = &track.clips[i];
                comp_track = &track;
                comp_idx = i;
                break;
            }
        }
        if (comp_clip != nullptr) {
            break;
        }
    }
    ASSERT_NE(comp_clip, nullptr);
    ASSERT_NE(comp_track, nullptr);

    // Ensure compound clip is the last clip of its track so extending it cannot overlap anything
    comp_track->clips.resize(comp_idx + 1);
    comp_clip = &comp_track->clips[comp_idx];

    const auto& comp_content = std::get<CompoundContent>(comp_clip->content);
    const auto* nested_seq = find_sequence(p, comp_content.sequence);
    ASSERT_NE(nested_seq, nullptr);
    const auto dur_res = sequence_duration(*nested_seq);
    ASSERT_TRUE(dur_res.has_value());
    const core::Duration n = dur_res.value();

    comp_clip->source_in = SourceTime::zero();
    comp_clip->speed = Speed::normal();
    comp_clip->duration = n;

    // source_in 0, speed 1/1 and clip duration exactly N: 0 issues
    auto issues = collect_issues(p);
    EXPECT_TRUE(issues.empty());

    // Duration N plus 1 tick: exactly 1 SourceRangeOutOfBounds
    comp_clip->duration = core::Duration::from_ticks(n.ticks() + 1);
    issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].code, ValidationCode::SourceRangeOutOfBounds);
}

TEST(ValidationTest, OverlapWithNonAdjacentClip) {
    Project p = test::build_valid_project();
    core::UuidGenerator gen(901ULL);

    Track new_track;
    new_track.id = generate_id<TrackId>(gen);
    new_track.kind = TrackKind::Video;

    auto make_text_clip = [&](TimelineTime start, core::Duration duration) {
        Clip c;
        c.id = generate_id<ClipId>(gen);
        c.start = start;
        c.duration = duration;
        c.source_in = SourceTime::zero();
        c.speed = Speed::normal();
        c.content = TextContent{"text"};
        return c;
    };

    // Vector order: [0 s, 10 s), [2 s, 4 s), [5 s, 7 s)
    new_track.clips.push_back(
        make_text_clip(TimelineTime::zero(), core::Duration::from_ticks(10 * T)));
    new_track.clips.push_back(
        make_text_clip(TimelineTime::from_ticks(2 * T), core::Duration::from_ticks(2 * T)));
    new_track.clips.push_back(
        make_text_clip(TimelineTime::from_ticks(5 * T), core::Duration::from_ticks(2 * T)));

    p.sequences[p.main_sequence].tracks.push_back(new_track);

    auto issues = collect_issues(p);
    ASSERT_EQ(issues.size(), 2U);
    EXPECT_EQ(issues[0].code, ValidationCode::ClipOverlap);
    EXPECT_TRUE(issues[0].path.ends_with("/clips/1"));
    EXPECT_EQ(issues[1].code, ValidationCode::ClipOverlap);
    EXPECT_TRUE(issues[1].path.ends_with("/clips/2"));

    // Variant: same clips stored in DIFFERENT vector order: [5 s, 7 s), [0 s, 10 s), [2 s, 4 s)
    Project p2 = test::build_valid_project();
    core::UuidGenerator gen2(902ULL);

    Track new_track2;
    new_track2.id = generate_id<TrackId>(gen2);
    new_track2.kind = TrackKind::Video;

    auto make_text_clip2 = [&](TimelineTime start, core::Duration duration) {
        Clip c;
        c.id = generate_id<ClipId>(gen2);
        c.start = start;
        c.duration = duration;
        c.source_in = SourceTime::zero();
        c.speed = Speed::normal();
        c.content = TextContent{"text"};
        return c;
    };

    // Vector order: [5 s, 7 s) (idx 0), [0 s, 10 s) (idx 1), [2 s, 4 s) (idx 2)
    new_track2.clips.push_back(
        make_text_clip2(TimelineTime::from_ticks(5 * T), core::Duration::from_ticks(2 * T)));
    new_track2.clips.push_back(
        make_text_clip2(TimelineTime::zero(), core::Duration::from_ticks(10 * T)));
    new_track2.clips.push_back(
        make_text_clip2(TimelineTime::from_ticks(2 * T), core::Duration::from_ticks(2 * T)));

    p2.sequences[p2.main_sequence].tracks.push_back(new_track2);

    issues = collect_issues(p2);
    ASSERT_EQ(issues.size(), 2U);
    EXPECT_EQ(issues[0].code, ValidationCode::ClipOverlap);
    EXPECT_TRUE(issues[0].path.ends_with("/clips/2"));
    EXPECT_EQ(issues[1].code, ValidationCode::ClipOverlap);
    EXPECT_TRUE(issues[1].path.ends_with("/clips/0"));
}

}  // namespace
}  // namespace nxtcut::model
