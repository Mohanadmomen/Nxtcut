#include <nxtcut/core/color.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/keyframes/animatable.hpp>
#include <nxtcut/keyframes/interpolation.hpp>
#include <nxtcut/keyframes/keyframe.hpp>
#include <nxtcut/keyframes/keyframe_track.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/effect.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/property.hpp>
#include <nxtcut/model/time_coords.hpp>
#include <nxtcut/model/transform.hpp>
#include <nxtcut/model/validation.hpp>

#include <gtest/gtest.h>

#include <bit>
#include <concepts>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::model {
namespace {

using keyframes::Interpolation;
using keyframes::Keyframe;
using keyframes::KeyframeTrack;

template <keyframes::Animatable T>
KeyframeTrack<T> make_linear_track(std::vector<std::pair<std::int64_t, T>> points) {
    std::vector<Keyframe<T>> keys;
    keys.reserve(points.size());
    for (const auto& [t, v] : points) {
        keys.push_back(Keyframe<T>{
            .time = core::TimePoint::from_ticks(t),
            .value = v,
            .interpolation = Interpolation::linear(),
        });
    }
    auto result = KeyframeTrack<T>::create(std::move(keys));
    if (!result.has_value()) {
        return KeyframeTrack<T>{};
    }
    return std::move(result).value();
}

template <class P>
concept HasKeyframes = requires(P p) { p.keyframes(); };

// 1. Default Property<double>: not animated, keyframes()==nullptr, value_at(any time)==constant.
TEST(PropertyKeyframesTest, DefaultPropertyNotAnimated) {
    const Property<double> prop;
    EXPECT_FALSE(prop.is_animated());
    EXPECT_EQ(prop.keyframes(), nullptr);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(0)), 0.0);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(100)), 0.0);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(-50)), 0.0);
    EXPECT_DOUBLE_EQ(prop.constant_value(), 0.0);
}

// 2. Constant 99.0 plus linear track {0:0.0, 100:10.0}: is_animated; value_at(50)==5.0 exactly;
//    value_at(-10)==0.0; value_at(100)==10.0; value_at(500)==10.0; constant_value()==99.0.
TEST(PropertyKeyframesTest, ConstantPlusLinearTrackEvaluation) {
    Property<double> prop(99.0);
    const auto track = make_linear_track<double>({{0, 0.0}, {100, 10.0}});
    prop.set_keyframes(track);

    EXPECT_TRUE(prop.is_animated());
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(50)), 5.0);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(-10)), 0.0);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(100)), 10.0);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(500)), 10.0);
    EXPECT_DOUBLE_EQ(prop.constant_value(), 99.0);
}

// 3. set_keyframes(empty track) clears; clear_keyframes() restores constant behavior;
//    calling clear on a non-animated property is a no-op.
TEST(PropertyKeyframesTest, SetEmptyTrackAndClearKeyframes) {
    Property<double> prop(99.0);
    const auto track = make_linear_track<double>({{0, 0.0}, {100, 10.0}});
    prop.set_keyframes(track);
    EXPECT_TRUE(prop.is_animated());

    // set_keyframes with empty track clears animation
    prop.set_keyframes(KeyframeTrack<double>{});
    EXPECT_FALSE(prop.is_animated());
    EXPECT_EQ(prop.keyframes(), nullptr);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(50)), 99.0);

    // Set track again, then clear_keyframes restores constant behavior
    prop.set_keyframes(track);
    EXPECT_TRUE(prop.is_animated());
    prop.clear_keyframes();
    EXPECT_FALSE(prop.is_animated());
    EXPECT_EQ(prop.keyframes(), nullptr);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(50)), 99.0);

    // Calling clear on a non-animated property is a no-op
    prop.clear_keyframes();
    EXPECT_FALSE(prop.is_animated());
    EXPECT_EQ(prop.keyframes(), nullptr);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(50)), 99.0);
}

// 4. set_constant(7.0) while animated: constant_value()==7.0, value_at still from the track.
TEST(PropertyKeyframesTest, SetConstantWhileAnimated) {
    Property<double> prop(99.0);
    const auto track = make_linear_track<double>({{0, 0.0}, {100, 10.0}});
    prop.set_keyframes(track);

    prop.set_constant(7.0);
    EXPECT_DOUBLE_EQ(prop.constant_value(), 7.0);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(50)), 5.0);
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(-10)), 0.0);

    // Clearing restores the updated constant
    prop.clear_keyframes();
    EXPECT_DOUBLE_EQ(prop.value_at(ClipTime::from_ticks(50)), 7.0);
}

// 5. Cursor overload: for 2000 deterministic times (fixed-seed LCG, forward, backward, jumps)
//    result bit-identical (std::bit_cast<uint64_t>) to value_at(time);
//    on a non-animated property a cursor with segment 7 stays at 7 and the constant is returned.
TEST(PropertyKeyframesTest, CursorEvaluationOverload) {
    Property<double> prop(42.0);
    const auto track =
        make_linear_track<double>({{0, 0.0}, {100, 10.0}, {200, 5.0}, {300, 25.0}, {400, 15.0}});
    prop.set_keyframes(track);

    KeyframeTrack<double>::Cursor cursor;
    std::uint64_t lcg_state = 0x123456789ABCDEF0ULL;
    for (int i = 0; i < 2000; ++i) {
        lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
        // Times ranging in [-50, 450] ticks to test extrapolation, segments, and random jumps
        const std::int64_t ticks = static_cast<std::int64_t>((lcg_state >> 32) % 500) - 50;
        const auto t = ClipTime::from_ticks(ticks);
        const double expected = prop.value_at(t);
        const double actual = prop.value_at(t, cursor);
        EXPECT_EQ(std::bit_cast<std::uint64_t>(expected), std::bit_cast<std::uint64_t>(actual));
    }

    // On a non-animated property, cursor with segment 7 stays at 7 and constant is returned
    const Property<double> non_animated(123.456);
    KeyframeTrack<double>::Cursor cursor_const;
    cursor_const.segment = 7;
    const double val = non_animated.value_at(ClipTime::from_ticks(50), cursor_const);
    EXPECT_DOUBLE_EQ(val, 123.456);
    EXPECT_EQ(cursor_const.segment, 7U);
}

// 6. Copy semantics: Property q = p copies; q.keyframes()==p.keyframes() (same pointer);
//    after q.set_keyframes(different track), p still evaluates with the old track and q with the
//    new one.
TEST(PropertyKeyframesTest, CopySemanticsSharesImmutableTrack) {
    Property<double> p(10.0);
    const auto track1 = make_linear_track<double>({{0, 0.0}, {100, 10.0}});
    p.set_keyframes(track1);

    Property<double> q = p;
    EXPECT_EQ(q.keyframes(), p.keyframes());

    const auto track2 = make_linear_track<double>({{0, 50.0}, {100, 150.0}});
    q.set_keyframes(track2);
    EXPECT_NE(q.keyframes(), p.keyframes());

    EXPECT_DOUBLE_EQ(p.value_at(ClipTime::from_ticks(50)), 5.0);
    EXPECT_DOUBLE_EQ(q.value_at(ClipTime::from_ticks(50)), 100.0);
}

// 7. Property<core::Color>: animated linear midpoint per channel; constant stays.
TEST(PropertyKeyframesTest, ColorPropertyAnimation) {
    const core::Color const_col{0.1f, 0.2f, 0.3f, 1.0f};
    Property<core::Color> prop(const_col);

    const auto track = make_linear_track<core::Color>({
        {0, core::Color{0.0f, 0.0f, 0.0f, 0.0f}},
        {100, core::Color{1.0f, 0.8f, 0.6f, 0.4f}},
    });
    prop.set_keyframes(track);
    EXPECT_TRUE(prop.is_animated());

    const core::Color mid = prop.value_at(ClipTime::from_ticks(50));
    EXPECT_FLOAT_EQ(mid.r, 0.5f);
    EXPECT_FLOAT_EQ(mid.g, 0.4f);
    EXPECT_FLOAT_EQ(mid.b, 0.3f);
    EXPECT_FLOAT_EQ(mid.a, 0.2f);

    EXPECT_TRUE(prop.constant_value().approx_equal(const_col));
}

// 8. Concepts/static_asserts: a local concept HasKeyframes<P> = requires(P p){ p.keyframes(); };
//    static_assert true for Property<double> and Property<core::Color>, false for Property<bool>
//    and Property<std::string>;
//    static_assert(noexcept(std::declval<const Property<double>&>().value_at(ClipTime{})));
//    Property<bool> and Property<std::string> still instantiable (declare objects, call
//    value_at/is_animated).
TEST(PropertyKeyframesTest, ConceptsAndStaticAsserts) {
    static_assert(HasKeyframes<Property<double>>);
    static_assert(HasKeyframes<Property<core::Color>>);
    static_assert(!HasKeyframes<Property<bool>>);
    static_assert(!HasKeyframes<Property<std::string>>);

    static_assert(noexcept(std::declval<const Property<double>&>().value_at(ClipTime{})));

    const Property<bool> pb(true);
    EXPECT_FALSE(pb.is_animated());
    EXPECT_TRUE(pb.value_at(ClipTime::from_ticks(10)));

    const Property<std::string> ps("test");
    EXPECT_FALSE(ps.is_animated());
    EXPECT_EQ(ps.value_at(ClipTime::from_ticks(10)), "test");
}

// 9. identical(): equal constants and no tracks -> true;
//    same constant, one animated -> false;
//    two separately created but equal tracks -> true;
//    different values/times/interpolation -> false;
//    same pointer (copy) -> true;
//    -0.0 vs 0.0 constants still differ;
//    Property<core::Color> likewise.
TEST(PropertyKeyframesTest, IdenticalComparisons) {
    const Property<double> c1(5.0);
    const Property<double> c2(5.0);
    EXPECT_TRUE(identical(c1, c2));

    const auto track1 = make_linear_track<double>({{0, 0.0}, {100, 10.0}});
    Property<double> anim1(5.0);
    anim1.set_keyframes(track1);
    EXPECT_FALSE(identical(c1, anim1));

    const auto track1_copy = make_linear_track<double>({{0, 0.0}, {100, 10.0}});
    Property<double> anim2(5.0);
    anim2.set_keyframes(track1_copy);
    EXPECT_TRUE(identical(anim1, anim2));

    const auto track_diff_val = make_linear_track<double>({{0, 0.0}, {100, 20.0}});
    Property<double> anim_diff_val(5.0);
    anim_diff_val.set_keyframes(track_diff_val);
    EXPECT_FALSE(identical(anim1, anim_diff_val));

    const auto track_diff_time = make_linear_track<double>({{0, 0.0}, {200, 10.0}});
    Property<double> anim_diff_time(5.0);
    anim_diff_time.set_keyframes(track_diff_time);
    EXPECT_FALSE(identical(anim1, anim_diff_time));

    // Different interpolation
    auto track_hold_res = KeyframeTrack<double>::create({
        Keyframe<double>{core::TimePoint::from_ticks(0), 0.0, Interpolation::hold()},
        Keyframe<double>{core::TimePoint::from_ticks(100), 10.0, Interpolation::hold()},
    });
    ASSERT_TRUE(track_hold_res.has_value());
    Property<double> anim_diff_interp(5.0);
    anim_diff_interp.set_keyframes(std::move(track_hold_res).value());
    EXPECT_FALSE(identical(anim1, anim_diff_interp));

    const Property<double> anim_copy = anim1;
    EXPECT_TRUE(identical(anim1, anim_copy));

    Property<double> pos_zero(0.0);
    Property<double> neg_zero(-0.0);
    EXPECT_FALSE(identical(pos_zero, neg_zero));
    pos_zero.set_keyframes(track1);
    neg_zero.set_keyframes(track1);
    EXPECT_FALSE(identical(pos_zero, neg_zero));

    // Property<core::Color> likewise
    const core::Color col{0.1f, 0.2f, 0.3f, 1.0f};
    const Property<core::Color> col_c1(col);
    const Property<core::Color> col_c2(col);
    EXPECT_TRUE(identical(col_c1, col_c2));

    const auto col_track1 = make_linear_track<core::Color>({
        {0, core::Color{0.0f, 0.0f, 0.0f, 1.0f}},
        {100, core::Color{1.0f, 1.0f, 1.0f, 1.0f}},
    });
    Property<core::Color> col_anim1(col);
    col_anim1.set_keyframes(col_track1);
    EXPECT_FALSE(identical(col_c1, col_anim1));

    const auto col_track1_copy = make_linear_track<core::Color>({
        {0, core::Color{0.0f, 0.0f, 0.0f, 1.0f}},
        {100, core::Color{1.0f, 1.0f, 1.0f, 1.0f}},
    });
    Property<core::Color> col_anim2(col);
    col_anim2.set_keyframes(col_track1_copy);
    EXPECT_TRUE(identical(col_anim1, col_anim2));

    const Property<core::Color> col_copy = col_anim1;
    EXPECT_TRUE(identical(col_anim1, col_copy));

    const auto col_track_diff = make_linear_track<core::Color>({
        {0, core::Color{0.0f, 0.0f, 0.0f, 1.0f}},
        {100, core::Color{0.5f, 1.0f, 1.0f, 1.0f}},
    });
    Property<core::Color> col_anim_diff(col);
    col_anim_diff.set_keyframes(col_track_diff);
    EXPECT_FALSE(identical(col_anim1, col_anim_diff));
}

// 10. Integration: TransformProps with animated opacity: identical(a, copy) true;
//     after changing the copy's track false.
//     EffectParam variant holding an animated Property<double>: identical true/false cases.
//     A Clip with animated opacity inside a valid Project: model::validate(project) succeeds,
//     and identical(project, copy) is true.
TEST(PropertyKeyframesTest, IntegrationTransformEffectAndProject) {
    const auto track = make_linear_track<double>({{0, 0.0}, {100, 1.0}});
    const auto diff_track = make_linear_track<double>({{0, 0.5}, {100, 0.8}});

    // TransformProps with animated opacity
    TransformProps t_a;
    t_a.opacity.set_keyframes(track);
    TransformProps t_copy = t_a;
    EXPECT_TRUE(identical(t_a, t_copy));

    t_copy.opacity.set_keyframes(diff_track);
    EXPECT_FALSE(identical(t_a, t_copy));

    // EffectParam variant holding an animated Property<double>
    EffectParam ep1 = Property<double>(5.0);
    std::get<Property<double>>(ep1).set_keyframes(track);
    EffectParam ep2 = ep1;
    EXPECT_TRUE(identical(ep1, ep2));

    std::get<Property<double>>(ep2).set_keyframes(diff_track);
    EXPECT_FALSE(identical(ep1, ep2));

    // Clip with animated opacity inside a valid Project
    Project project = test::build_valid_project(42ULL);
    auto& clip = project.sequences[project.main_sequence].tracks[0].clips[0];
    clip.transform.opacity.set_keyframes(track);

    const auto status = validate(project);
    EXPECT_TRUE(status.has_value());

    const Project project_copy = project;
    EXPECT_TRUE(identical(project, project_copy));

    Project mutated = project;
    mutated.sequences[mutated.main_sequence].tracks[0].clips[0].transform.opacity.set_keyframes(
        diff_track);
    EXPECT_FALSE(identical(project, mutated));
}

}  // namespace
}  // namespace nxtcut::model
