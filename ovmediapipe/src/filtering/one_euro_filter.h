// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#pragma once

#include <memory>

#include "low_pass_filter.h"

namespace ovmediapipe {

    class OneEuroFilter {
    public:
        OneEuroFilter(double frequency, double min_cutoff, double beta,
            double derivate_cutoff);

        double Apply(double value_scale, double value);

    private:
        double GetAlpha(double cutoff);

        void SetFrequency(double frequency);

        void SetMinCutoff(double min_cutoff);

        void SetBeta(double beta);

        void SetDerivateCutoff(double derivate_cutoff);

        double frequency_;
        double min_cutoff_;
        double beta_;
        double derivate_cutoff_;
        std::unique_ptr<LowPassFilter> x_;
        std::unique_ptr<LowPassFilter> dx_;
    };

}  // namespace ovmediapipe

