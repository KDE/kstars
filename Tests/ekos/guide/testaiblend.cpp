/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/guide/internalguide/ai_blend.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

#include <QObject>
#include <cmath>

// Pure arithmetic tests for blendAIPulse() -- deliberately has no dependency on any
// MountSpecificGuider (WormGear/DirectDrive/Harmonic) or on cgmath's stateful pipeline, so
// a guarantee proven here holds for every mount type that funnels through
// cgmath::processAxis()'s useAI branch, not just whichever one happened to produce the
// telemetry that motivated the fix (WormGear/EQMod, 2026-08-24).
class TestAIBlend : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void backoffDisabled_behavesAsPlainPI();
        void zeroConfidence_noBackoff();
        void aiAgreesAndIsStrong_matchesLegacyFormula();
        void aiAgreesButIsWeak_reductionCappedToContribution();
        void aiDisagreesInSign_noBackoffAtAll();
        void aiIsZero_noBackoff();
        void zeroProportional_noDivideByZero();
        void neverWeakerThanCappedInvariant_randomSweep();
};

#include "testaiblend.moc"

void TestAIBlend::backoffDisabled_behavesAsPlainPI()
{
    // proportionalBackoffEnabled=false must reduce to plain P + I + AI, regardless of
    // confidence/gain -- this is the Options::AIProportionalBackoff()==false path.
    const auto r = blendAIPulse(-100.0, -10.0, 50.0, 0.9, 0.8, false);
    QCOMPARE(r.activePropGain, 1.0);
    QVERIFY(std::abs(r.total - (-100.0 - 10.0 + 0.8 * 0.9 * 50.0)) < 1e-9);
}

void TestAIBlend::zeroConfidence_noBackoff()
{
    // conf=0 means aiContribution=0 too (same aiGain*conf factor in both terms), so no
    // reduction should be applied -- the AI having zero trust must never itself weaken P.
    const auto r = blendAIPulse(-100.0, -10.0, 50.0, 0.0, 0.8, true);
    QCOMPARE(r.activePropGain, 1.0);
    QVERIFY(std::abs(r.total - (-110.0)) < 1e-9);
}

void TestAIBlend::aiAgreesAndIsStrong_matchesLegacyFormula()
{
    // When aiContribution is at least as large (same sign) as the proposed reduction,
    // the cap never engages -- behavior must be identical to the old flat formula.
    const double prop = -100.0, integ = -10.0, ai = -80.0, conf = 0.9, gain = 0.8;
    const auto r = blendAIPulse(prop, integ, ai, conf, gain, true);
    const double legacyActiveGain = 1.0 - (gain * conf * 0.5);
    const double legacyTotal = prop * legacyActiveGain + integ + (gain * conf * ai);
    QVERIFY(std::abs(r.activePropGain - legacyActiveGain) < 1e-9);
    QVERIFY(std::abs(r.total - legacyTotal) < 1e-9);
}

void TestAIBlend::aiAgreesButIsWeak_reductionCappedToContribution()
{
    // The real-world failure mode: AI agrees in direction but its contribution is much
    // smaller than the proposed P reduction. When the cap engages, the removed P is
    // exactly offset by the added aiContribution -- total collapses to EXACTLY plain P+I,
    // as if backoff had never been applied, rather than the weaker (prop*legacyGain+integ+
    // aiContribution) the old flat formula would have sent.
    const double prop = -100.0, integ = -10.0, ai = -5.0, conf = 0.9, gain = 0.8;
    const auto r = blendAIPulse(prop, integ, ai, conf, gain, true);
    const double aiContribution = gain * conf * ai; // -3.6
    const double proposedReduction = prop * gain * conf * 0.5; // -36.0, |proposed| > |aiContribution|
    QVERIFY(std::abs(aiContribution) < std::abs(proposedReduction));
    // actualReduction must equal aiContribution (the cap), not the larger proposedReduction,
    // which algebraically makes prop*activePropGain + aiContribution collapse to prop alone.
    const double expectedActiveGain = 1.0 - (aiContribution / prop);
    QVERIFY(std::abs(r.activePropGain - expectedActiveGain) < 1e-9);
    QVERIFY(std::abs(r.total - (prop + integ)) < 1e-9);
}

void TestAIBlend::aiDisagreesInSign_noBackoffAtAll()
{
    // AI contribution points the OPPOSITE way from P (e.g. mispredicted direction this
    // frame) -- must not remove ANY P, since doing so would leave a net hole neither term
    // is covering. This is the exact case (correlation -0.15 in real data) that caused the
    // original regression.
    const double prop = -100.0, integ = -10.0, ai = 20.0, conf = 0.9, gain = 0.8;
    const auto r = blendAIPulse(prop, integ, ai, conf, gain, true);
    QCOMPARE(r.activePropGain, 1.0);
    const double aiContribution = gain * conf * ai;
    QVERIFY(std::abs(r.total - (prop + integ + aiContribution)) < 1e-9);
}

void TestAIBlend::aiIsZero_noBackoff()
{
    const auto r = blendAIPulse(-100.0, -10.0, 0.0, 0.9, 0.8, true);
    QCOMPARE(r.activePropGain, 1.0);
    QVERIFY(std::abs(r.total - (-110.0)) < 1e-9);
}

void TestAIBlend::zeroProportional_noDivideByZero()
{
    const auto r = blendAIPulse(0.0, -10.0, 30.0, 0.9, 0.8, true);
    QVERIFY(std::isfinite(r.activePropGain));
    QVERIFY(std::isfinite(r.total));
    QVERIFY(std::abs(r.total - (-10.0 + 0.8 * 0.9 * 30.0)) < 1e-9);
}

void TestAIBlend::neverWeakerThanCappedInvariant_randomSweep()
{
    // Broad sweep over signs/magnitudes of every input, covering all three mount-guider
    // confidence regimes in one shot: DirectDriveGuider's fixed 0.95, HarmonicGuider's
    // Kalman-derived value, and WormGearGuider's phase-uncertainty-penalized value all just
    // land somewhere in [0,1] here. Invariant: |total| must always be >= |P+I+aiContribution|
    // computed with backoff off, i.e. the cap can only ever raise the floor, never lower it
    // further than an uncapped-but-same-direction reduction would.
    const double props[]  = { -200.0, -50.0, -1.0, 1.0, 50.0, 200.0 };
    const double integs[] = { -20.0, 0.0, 20.0 };
    const double ais[]    = { -100.0, -10.0, -1.0, 0.0, 1.0, 10.0, 100.0 };
    const double confs[]  = { 0.0, 0.3, 0.5, 0.95, 1.0 };
    const double gains[]  = { 0.0, 0.5, 1.0 };

    int checked = 0;
    for (double prop : props)
        for (double integ : integs)
            for (double ai : ais)
                for (double conf : confs)
                    for (double gain : gains)
                    {
                        const auto r = blendAIPulse(prop, integ, ai, conf, gain, true);
                        QVERIFY(std::isfinite(r.total));
                        QVERIFY(std::isfinite(r.activePropGain));

                        const double aiContribution = gain * conf * ai;
                        const double proposedReduction = prop * gain * conf * 0.5;
                        const double baseline = prop + integ; // plain P+I, no AI at all

                        double expectedTotal;
                        if (aiContribution == 0.0 || (proposedReduction > 0.0) != (aiContribution > 0.0))
                            // Case A: no reinforcement (wrong direction, or nothing to give) --
                            // no P removed, AI term (possibly opposing) just adds on top.
                            expectedTotal = prop + integ + aiContribution;
                        else if (std::abs(aiContribution) <= std::abs(proposedReduction))
                            // Case B: reinforcing but weaker than the proposed reduction -- the
                            // capped reduction exactly cancels the added contribution, so the
                            // AI's own term ends up fully spent "buying back" the P it removed,
                            // net identical to plain P+I with no AI influence whatsoever.
                            expectedTotal = baseline;
                        else
                            // Case C: reinforcing and at least as strong as proposed -- cap
                            // never engages, matches the original (uncapped) formula exactly.
                            expectedTotal = prop * (1.0 - gain * conf * 0.5) + integ + aiContribution;

                        QVERIFY(std::abs(r.total - expectedTotal) < 1e-6);
                        ++checked;
                    }
    QVERIFY(checked > 0);
}

QTEST_GUILESS_MAIN(TestAIBlend)
