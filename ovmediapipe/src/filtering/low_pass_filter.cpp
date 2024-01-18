// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include <iostream>
#include "low_pass_filter.h"

namespace ovmediapipe {

    LowPassFilter::LowPassFilter(float alpha) : initialized_{ false } {
        SetAlpha(alpha);
        raw_value_ = 0.f;
        stored_value_ = 0.f;
    }

    float LowPassFilter::Apply(float value) {
        float result;
        if (initialized_) {
            result = alpha_ * value + (1.0f - alpha_) * stored_value_;
        }
        else {
            result = value;
            initialized_ = true;
        }
        raw_value_ = value;
        stored_value_ = result;
        return result;
    }

    float LowPassFilter::ApplyWithAlpha(float value, float alpha) {
        SetAlpha(alpha);
        return Apply(value);
    }

    bool LowPassFilter::HasLastRawValue() { return initialized_; }

    float LowPassFilter::LastRawValue() { return raw_value_; }

    float LowPassFilter::LastValue() { return stored_value_; }

    void LowPassFilter::SetAlpha(float alpha) {
        if (alpha < 0.0f || alpha > 1.0f) {
            std::cout << "alpha: " << alpha << " should be in [0.0, 1.0] range" << std::endl;
            return;
        }
        alpha_ = alpha;
    }

}  // namespace mediapipe
