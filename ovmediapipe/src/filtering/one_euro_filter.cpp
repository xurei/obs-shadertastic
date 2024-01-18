// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include <iostream>
#include "one_euro_filter.h"

#include <cmath>

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif

namespace ovmediapipe {

    static const double kEpsilon = 0.000001;

    OneEuroFilter::OneEuroFilter(double frequency, double min_cutoff, double beta,
        double derivate_cutoff) {
        SetFrequency(frequency);
        SetMinCutoff(min_cutoff);
        SetBeta(beta);
        SetDerivateCutoff(derivate_cutoff);
        x_ = std::make_unique<LowPassFilter>(GetAlpha(min_cutoff));
        dx_ = std::make_unique<LowPassFilter>(GetAlpha(derivate_cutoff));
    }

    double OneEuroFilter::Apply(double value_scale, double value) {
        // estimate the current variation per second
        double dvalue = x_->HasLastRawValue()
            ? (value - x_->LastRawValue()) * value_scale * frequency_
            : 0.0;  // FIXME: 0.0 or value?
        double edvalue = dx_->ApplyWithAlpha((float)dvalue, (float)GetAlpha(derivate_cutoff_));
        // use it to update the cutoff frequency
        double cutoff = min_cutoff_ + beta_ * std::fabs(edvalue);

        // filter the given value
        return x_->ApplyWithAlpha((float)value, (float)GetAlpha(cutoff));
    }

    double OneEuroFilter::GetAlpha(double cutoff) {
        double te = 1.0 / frequency_;
        double tau = 1.0 / (2 * M_PI * cutoff);
        return 1.0 / (1.0 + tau / te);
    }

    void OneEuroFilter::SetFrequency(double frequency) {
        if (frequency <= kEpsilon) {
            std::cout << "frequency should be > 0" << std::endl;
            return;
        }
        frequency_ = frequency;
    }

    void OneEuroFilter::SetMinCutoff(double min_cutoff) {
        if (min_cutoff <= kEpsilon) {
            std::cout << "min_cutoff should be > 0" << std::endl;
            return;
        }
        min_cutoff_ = min_cutoff;
    }

    void OneEuroFilter::SetBeta(double beta) { beta_ = beta; }

    void OneEuroFilter::SetDerivateCutoff(double derivate_cutoff) {
        if (derivate_cutoff <= kEpsilon) {
            std::cout << "derivate_cutoff should be > 0" << std::endl;
            return;
        }
        derivate_cutoff_ = derivate_cutoff;
    }

}  // namespace mediapipe
