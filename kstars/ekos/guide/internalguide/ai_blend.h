#pragma once
/*
 * ai_blend.h — AI/PID pulse blending arithmetic, shared by every MountSpecificGuider
 *
 * Deliberately a free function with no dependency on cgmath, Options, or any
 * mount-specific guider (WormGear/DirectDrive/Harmonic all funnel through the same
 * blend in cgmath::processAxis()) so it can be unit-tested in isolation and so a fix
 * verified against one mount's data is provable to hold for all of them.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <algorithm>
#include <cmath>

struct AIBlendResult
{
    double total          { 0.0 }; ///< Final pulse (ms) to send, before the min-pulse/max-pulse clamp
    double activePropGain { 1.0 }; ///< Effective P multiplier actually applied, for logging/debug
};

/**
 * @brief Blend a standard P+I response with an AI feed-forward correction.
 *
 * Root cause fixed here (found 2026-08-24 replaying real WormGear guiding data): the old
 * formula backed proportionalResponse off by up to 50% purely as a function of confidence,
 * regardless of whether aiResponse was actually large enough -- or even pointed the right
 * way -- to make up for what was removed. Confidence measures how well the AI's model fits
 * its own target (e.g. periodic error); it says nothing about whether that correction is
 * large enough to substitute for the P-term's job of reacting to the *current* instantaneous
 * error, which also includes non-periodic noise the model was never trying to predict. On
 * real data this backed off ~64ms average P+I responses to ~53ms pre-deadband, but because
 * the minimum-pulse cutoff is a hard threshold, that pushed the fraction of frames that
 * actually cleared it from 77% down to 30% -- a much larger effective loss of authority than
 * the raw average suggests, and the AI term was doing nothing to compensate (correlation
 * with the removed P was -0.15, i.e. essentially not compensating at all).
 *
 * Fix: only let the backoff remove as much P-authority as aiResponse is actually
 * contributing in the SAME direction. If the AI's own contribution doesn't reinforce the
 * correction (opposite sign, or zero), no P is removed -- this frame behaves like backoff
 * was off. If it does reinforce and is at least as large as the proposed reduction, behavior
 * is unchanged from before (min() picks the original, smaller reduction). The blended
 * response can therefore never be weaker than what P+I alone would have sent for a
 * same-signed, adequately-sized AI contribution, and never gets artificially starved by a
 * confident-but-small or wrong-direction prediction -- true for any MountSpecificGuider
 * regardless of what physics/ML model produced aiResponse or how it derives confidence.
 *
 * @param proportionalResponse Standard P-term response (ms), signed.
 * @param integralResponse     Standard I-term response (ms), signed. Never backed off.
 * @param aiResponse           AI model's raw predicted correction (ms), signed, pre-gain.
 * @param confidence           Per-axis model confidence in [0,1].
 * @param aiGain                Global AI feed-forward gain in [0,1] (Options::AIPredictionGain()).
 * @param proportionalBackoffEnabled Whether to attempt any P backoff at all
 *        (Options::AIProportionalBackoff()); when false, behaves as plain P+I+AI with no cap needed.
 */
inline AIBlendResult blendAIPulse(double proportionalResponse, double integralResponse,
                                  double aiResponse, double confidence, double aiGain,
                                  bool proportionalBackoffEnabled)
{
    const double aiContribution = aiGain * confidence * aiResponse;

    double activePropGain = 1.0;
    if (proportionalBackoffEnabled && std::abs(proportionalResponse) > 1e-9)
    {
        const double proposedReduction = proportionalResponse * aiGain * confidence * 0.5;
        double actualReduction = 0.0;
        // Only back off P when the AI term reinforces the same direction; cap the amount
        // removed at whatever the AI term itself is actually contributing.
        if (aiContribution != 0.0 && (proposedReduction > 0.0) == (aiContribution > 0.0))
        {
            actualReduction = std::copysign(
                                  std::min(std::abs(proposedReduction), std::abs(aiContribution)), proposedReduction);
        }
        activePropGain = 1.0 - (actualReduction / proportionalResponse);
    }

    AIBlendResult result;
    result.activePropGain = activePropGain;
    result.total = proportionalResponse * activePropGain + integralResponse + aiContribution;
    return result;
}
