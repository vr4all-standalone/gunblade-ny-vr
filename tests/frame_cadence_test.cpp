// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/frame_cadence.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <type_traits>

using gunblade::FrameCadence;
static bool near(double a,double b){return std::abs(a-b)<1e-8;}

static void native_snapshot_cadence_is_independent_of_xr_submissions() {
    FrameCadence cadence;
    assert(!cadence.summary().valid&&!cadence.ready());
    for(unsigned i=0;i<=360;++i)
        cadence.submitted(10.+double(i)/72,1./72,uint64_t(i*58./72),10.+double(i)/72-.005);
    const auto result=cadence.summary();
    assert(result.valid&&cadence.ready());
    assert(near(result.seconds,5)&&near(result.submitted_hz,72)&&near(result.target_hz,72));
    assert(near(result.native_updates_hz,58)&&near(result.native_advanced_hz,58));
    assert(result.native_updates==290&&result.submitted_intervals==360);
    assert(result.skipped_native==0&&result.late_intervals==0);
    assert(result.source_age_samples==360&&near(result.source_age_p95_ms,5));
}

static void skipped_snapshots_are_separate_from_observed_updates() {
    FrameCadence cadence;
    cadence.submitted(10.,.01,100,9.995);
    cadence.submitted(10.01,.01,103,10.005);
    cadence.submitted(10.02,.01,103,10.005);
    const auto result=cadence.summary();
    assert(near(result.submitted_hz,100)&&near(result.native_updates_hz,50));
    assert(near(result.native_advanced_hz,150)&&result.skipped_native==2);
    assert(result.source_age_samples==2&&near(result.source_age_p95_ms,15));
    assert(near(result.source_age_max_ms,15));
}

static void late_intervals_measure_successful_submission_spacing() {
    FrameCadence cadence;
    cadence.submitted(10.,.01,1);
    cadence.submitted(10.01,.01,1);
    cadence.submitted(10.06,.01,2);
    const auto result=cadence.summary();
    assert(result.late_intervals==1&&near(result.maximum_interval_ms,50));
    assert(near(result.p95_interval_ms,50)&&near(result.submitted_hz,100./3));
    assert(near(result.target_hz,100));
}

static void native_counter_restore_starts_a_new_window() {
    FrameCadence cadence;
    cadence.submitted(10.,.01,100);
    cadence.submitted(10.01,.01,101);
    assert(cadence.summary().valid);
    cadence.submitted(10.02,.01,2);
    assert(!cadence.summary().valid);
    cadence.submitted(10.03,.01,3);
    const auto result=cadence.summary();
    assert(result.valid&&result.native_updates==1&&result.skipped_native==0);
    assert(near(result.seconds,.01)&&near(result.native_advanced_hz,100));
}

static void invalid_or_ineligible_samples_do_not_span_reset_windows() {
    FrameCadence cadence;
    cadence.submitted(10.,.01,1);
    cadence.submitted(10.01,.01,2);
    cadence.submitted(20.,.01,3,0,false);
    assert(!cadence.summary().valid);
    cadence.submitted(30.,.01,4);
    assert(!cadence.summary().valid);
    cadence.submitted(30.01,.01,4);
    assert(near(cadence.summary().seconds,.01)&&cadence.summary().native_updates==0);
    cadence.submitted(29.,.01,5);
    assert(!cadence.summary().valid);
    cadence.submitted(29.,.01,6);
    assert(!cadence.summary().valid);
    for(double period:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        cadence.submitted(30.,period,7);assert(!cadence.summary().valid);
    }
    cadence.submitted(NAN,.01,7);assert(!cadence.summary().valid);
    cadence.submitted(31.,.01,8);cadence.submitted(31.01,.01,9);
    assert(cadence.summary().valid&&!cadence.ready(NAN));
    cadence.reset();assert(!cadence.summary().valid&&!cadence.ready(0));
}

static void absent_or_incompatible_source_timestamps_are_unavailable() {
    FrameCadence cadence;
    cadence.submitted(10.,.01,1);
    cadence.submitted(10.01,.01,2);
    cadence.submitted(10.02,.01,3,11.);
    cadence.submitted(10.03,.01,4,NAN);
    assert(cadence.summary().source_age_samples==0);
    assert(cadence.summary().source_age_p95_ms==0);
    cadence.submitted(10.04,.01,5,10.);
    assert(cadence.summary().source_age_samples==1);
    assert(near(cadence.summary().source_age_p95_ms,40));
}

static void fixed_storage_keeps_the_latest_percentile_samples() {
    FrameCadence cadence;
    cadence.submitted(10.,1.,0);
    for(unsigned i=1;i<=1000;++i)cadence.submitted(10.+i,1.,i,10.+i-i*.001);
    const auto result=cadence.summary();
    assert(result.submitted_intervals==1000&&result.source_age_samples==1000);
    assert(near(result.submitted_hz,1)&&near(result.p95_interval_ms,1000));
    // Latest 600 ages are401..1000ms; nearest-rank p95 is970ms.
    assert(near(result.source_age_p95_ms,970)&&near(result.source_age_max_ms,1000));
}

int main() {
    static_assert(std::is_trivially_copyable_v<FrameCadence>);
    static_assert(noexcept(FrameCadence{}.submitted(0,.01,0)));
    native_snapshot_cadence_is_independent_of_xr_submissions();
    skipped_snapshots_are_separate_from_observed_updates();
    late_intervals_measure_successful_submission_spacing();
    native_counter_restore_starts_a_new_window();
    invalid_or_ineligible_samples_do_not_span_reset_windows();
    absent_or_incompatible_source_timestamps_are_unavailable();
    fixed_storage_keeps_the_latest_percentile_samples();
}
