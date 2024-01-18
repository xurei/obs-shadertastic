// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#pragma once

#include <memory>

namespace ovmediapipe {

    class LowPassFilter {
    public:
        explicit LowPassFilter(float alpha);

        float Apply(float value);

        float ApplyWithAlpha(float value, float alpha);

        bool HasLastRawValue();

        float LastRawValue();

        float LastValue();

    private:
        void SetAlpha(float alpha);

        float raw_value_;
        float alpha_;
        float stored_value_;
        bool initialized_;
    };

}  // namespace ovmediapipe

