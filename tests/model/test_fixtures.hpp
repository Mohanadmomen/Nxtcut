#pragma once

#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/media.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>
#include <nxtcut/model/track.hpp>

#include <cstdint>

namespace nxtcut::model::test {

/**
 * @brief Helper that constructs a completely valid sample project with no validation issues.
 *
 * Structure:
 * - 2 sequences (MainSequence and SubSequence)
 * - 1 video asset with audio (video.mp4, duration 60s) and 1 image asset (image.png)
 * - In MainSequence:
 *   - Video Track:
 *     - Clip 0: VideoClip (0s..5s), linked with audio clip
 *     - Clip 1: ImageClip (5s..10s)
 *     - Clip 2: CompoundClip (10s..15s), pointing to SubSequence
 *   - Audio Track:
 *     - Clip 0: AudioClip (0s..5s), linked with video clip
 * - In SubSequence:
 *   - Video Track with 1 ImageClip (0s..10s, giving SubSequence a duration of 10s)
 *
 * @param seed Seed for deterministic UuidGenerator reproducibility.
 * @return Fully formed, valid Project.
 */
inline Project build_valid_project(std::uint64_t seed = 42ULL) {
    core::UuidGenerator gen(seed);

    const MediaId video_media_id = generate_id<MediaId>(gen);
    const MediaId image_media_id = generate_id<MediaId>(gen);
    const SequenceId seq1_id = generate_id<SequenceId>(gen);
    const SequenceId seq2_id = generate_id<SequenceId>(gen);
    const TrackId sub_track_id = generate_id<TrackId>(gen);
    const TrackId main_vtrack_id = generate_id<TrackId>(gen);
    const TrackId main_atrack_id = generate_id<TrackId>(gen);
    const ClipId sub_clip_id = generate_id<ClipId>(gen);
    const ClipId video_clip_id = generate_id<ClipId>(gen);
    const ClipId audio_clip_id = generate_id<ClipId>(gen);
    const ClipId image_clip_id = generate_id<ClipId>(gen);
    const ClipId compound_clip_id = generate_id<ClipId>(gen);
    const LinkId link_id = generate_id<LinkId>(gen);
    const ProjectId project_id = generate_id<ProjectId>(gen);

    MediaAsset video_asset;
    video_asset.id = video_media_id;
    video_asset.name = "video.mp4";
    video_asset.path = "/media/video.mp4";
    video_asset.kind = MediaKind::Video;
    video_asset.duration = core::Duration::from_seconds(60.0);
    video_asset.video =
        VideoStreamInfo{core::Size<std::int32_t>{1920, 1080}, core::frame_rates::k30};
    video_asset.audio = AudioStreamInfo{core::sample_rates::k48000, 2};

    MediaAsset image_asset;
    image_asset.id = image_media_id;
    image_asset.name = "image.png";
    image_asset.path = "/media/image.png";
    image_asset.kind = MediaKind::Image;
    image_asset.duration = core::Duration::zero();

    // SubSequence (Sequence 2)
    Clip sub_image_clip;
    sub_image_clip.id = sub_clip_id;
    sub_image_clip.name = "SubImage";
    sub_image_clip.start = TimelineTime::zero();
    sub_image_clip.duration = core::Duration::from_seconds(10.0);
    sub_image_clip.source_in = SourceTime::zero();
    sub_image_clip.speed = Speed::normal();
    sub_image_clip.content = ImageContent{image_media_id};

    Track sub_track;
    sub_track.id = sub_track_id;
    sub_track.name = "SubV1";
    sub_track.kind = TrackKind::Video;
    sub_track.clips.push_back(sub_image_clip);

    Sequence seq2;
    seq2.id = seq2_id;
    seq2.name = "SubSequence";
    seq2.canvas = core::Size<std::int32_t>{1920, 1080};
    seq2.tracks.push_back(sub_track);

    // MainSequence (Sequence 1)
    Clip v_clip;
    v_clip.id = video_clip_id;
    v_clip.name = "MainVideo";
    v_clip.start = TimelineTime::zero();
    v_clip.duration = core::Duration::from_seconds(5.0);
    v_clip.source_in = SourceTime::zero();
    v_clip.speed = Speed::normal();
    v_clip.link_id = link_id;
    v_clip.content = VideoContent{video_media_id};

    Clip img_clip;
    img_clip.id = image_clip_id;
    img_clip.name = "MainImage";
    img_clip.start = TimelineTime::from_ticks(core::Duration::from_seconds(5.0).ticks());
    img_clip.duration = core::Duration::from_seconds(5.0);
    img_clip.source_in = SourceTime::zero();
    img_clip.speed = Speed::normal();
    img_clip.content = ImageContent{image_media_id};

    Clip comp_clip;
    comp_clip.id = compound_clip_id;
    comp_clip.name = "MainCompound";
    comp_clip.start = TimelineTime::from_ticks(core::Duration::from_seconds(10.0).ticks());
    comp_clip.duration = core::Duration::from_seconds(5.0);
    comp_clip.source_in = SourceTime::zero();
    comp_clip.speed = Speed::normal();
    comp_clip.content = CompoundContent{seq2_id};

    Track main_vtrack;
    main_vtrack.id = main_vtrack_id;
    main_vtrack.name = "V1";
    main_vtrack.kind = TrackKind::Video;
    main_vtrack.clips.push_back(v_clip);
    main_vtrack.clips.push_back(img_clip);
    main_vtrack.clips.push_back(comp_clip);

    Clip a_clip;
    a_clip.id = audio_clip_id;
    a_clip.name = "MainAudio";
    a_clip.start = TimelineTime::zero();
    a_clip.duration = core::Duration::from_seconds(5.0);
    a_clip.source_in = SourceTime::zero();
    a_clip.speed = Speed::normal();
    a_clip.link_id = link_id;
    a_clip.content = AudioContent{video_media_id};

    Track main_atrack;
    main_atrack.id = main_atrack_id;
    main_atrack.name = "A1";
    main_atrack.kind = TrackKind::Audio;
    main_atrack.clips.push_back(a_clip);

    Sequence seq1;
    seq1.id = seq1_id;
    seq1.name = "MainSequence";
    seq1.canvas = core::Size<std::int32_t>{1920, 1080};
    seq1.tracks.push_back(main_vtrack);
    seq1.tracks.push_back(main_atrack);

    Project project;
    project.id = project_id;
    project.name = "SampleProject";
    project.media[video_media_id] = video_asset;
    project.media[image_media_id] = image_asset;
    project.sequences[seq1_id] = seq1;
    project.sequences[seq2_id] = seq2;
    project.main_sequence = seq1_id;

    return project;
}

/**
 * @brief Constructs a sequence with one video track holding 5-second compound clips.
 *
 * Laid out back to back from 0 s (clip i starts at i times 5 s), with source_in 0,
 * speed 1/1 and unique ids from gen.
 *
 * @param gen UuidGenerator for generating unique track and clip IDs.
 * @param self_id ID to assign to the created sequence.
 * @param targets Sequence IDs referenced by each compound clip.
 * @return Fully formed Sequence.
 */
inline Sequence make_compound_sequence(core::UuidGenerator& gen, SequenceId self_id,
                                       const std::vector<SequenceId>& targets) {
    Sequence s;
    s.id = self_id;
    s.canvas = core::Size<std::int32_t>{1920, 1080};
    Track t;
    t.id = generate_id<TrackId>(gen);
    t.kind = TrackKind::Video;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        Clip c;
        c.id = generate_id<ClipId>(gen);
        c.start = TimelineTime::from_ticks(static_cast<std::int64_t>(i) *
                                           core::Duration::from_seconds(5.0).ticks());
        c.duration = core::Duration::from_seconds(5.0);
        c.source_in = SourceTime::zero();
        c.speed = Speed::normal();
        c.content = CompoundContent{targets[i]};
        t.clips.push_back(c);
    }
    s.tracks.push_back(t);
    return s;
}

}  // namespace nxtcut::model::test
