#pragma once

#include <nxtcut/core/color.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/effect.hpp>
#include <nxtcut/model/media.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/property.hpp>
#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/track.hpp>
#include <nxtcut/model/transform.hpp>

#include <string>

namespace nxtcut::model {

/**
 * @brief Exact bit-level equality for straight colors.
 *
 * Compares RGBA float components bit-for-bit via std::bit_cast<uint32_t>.
 * Under this definition, 0.0f differs from -0.0f, and two identical NaN
 * encodings evaluate as identical.
 */
[[nodiscard]] bool identical(const core::Color& a, const core::Color& b) noexcept;

/**
 * @brief Exact bit-level equality for Property<double>.
 *
 * Compares payload values bit-for-bit via std::bit_cast<uint64_t>.
 * 0.0 differs from -0.0; identical NaNs are identical.
 */
[[nodiscard]] bool identical(const Property<double>& a, const Property<double>& b) noexcept;

/**
 * @brief Value equality for Property<bool>.
 */
[[nodiscard]] bool identical(const Property<bool>& a, const Property<bool>& b) noexcept;

/**
 * @brief Value equality for Property<std::string>.
 */
[[nodiscard]] bool identical(const Property<std::string>& a,
                             const Property<std::string>& b) noexcept;

/**
 * @brief Exact bit-level equality for Property<core::Color>.
 */
[[nodiscard]] bool identical(const Property<core::Color>& a,
                             const Property<core::Color>& b) noexcept;

/**
 * @brief Exact field-by-field equality for VideoStreamInfo.
 */
[[nodiscard]] bool identical(const VideoStreamInfo& a, const VideoStreamInfo& b) noexcept;

/**
 * @brief Exact field-by-field equality for AudioStreamInfo.
 */
[[nodiscard]] bool identical(const AudioStreamInfo& a, const AudioStreamInfo& b) noexcept;

/**
 * @brief Exact field-by-field equality for TransformProps.
 */
[[nodiscard]] bool identical(const TransformProps& a, const TransformProps& b) noexcept;

/**
 * @brief Exact variant equality for EffectParam.
 */
[[nodiscard]] bool identical(const EffectParam& a, const EffectParam& b) noexcept;

/**
 * @brief Exact field-by-field equality for EffectInstance.
 */
[[nodiscard]] bool identical(const EffectInstance& a, const EffectInstance& b) noexcept;

/**
 * @brief Exact field-by-field equality for MediaAsset.
 */
[[nodiscard]] bool identical(const MediaAsset& a, const MediaAsset& b) noexcept;

/**
 * @brief Exact variant equality for ClipContent.
 */
[[nodiscard]] bool identical(const ClipContent& a, const ClipContent& b) noexcept;

/**
 * @brief Exact field-by-field equality for Clip.
 */
[[nodiscard]] bool identical(const Clip& a, const Clip& b) noexcept;

/**
 * @brief Exact field-by-field equality for Marker.
 */
[[nodiscard]] bool identical(const Marker& a, const Marker& b) noexcept;

/**
 * @brief Exact field-by-field equality for Track.
 */
[[nodiscard]] bool identical(const Track& a, const Track& b) noexcept;

/**
 * @brief Exact field-by-field equality for Sequence.
 */
[[nodiscard]] bool identical(const Sequence& a, const Sequence& b) noexcept;

/**
 * @brief Exact field-by-field equality for Project.
 */
[[nodiscard]] bool identical(const Project& a, const Project& b) noexcept;

}  // namespace nxtcut::model
