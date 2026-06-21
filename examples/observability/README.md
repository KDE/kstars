<!--
SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Ekos observability

Ekos can expose live Analyze session metrics in the OpenMetrics 1.0 text format. Prometheus can scrape the endpoint directly, and an OpenTelemetry Collector can consume it with its Prometheus receiver.

## Enable the endpoint

1. Open **Settings → Configure KStars → Ekos → Telemetry**.
2. Select **Enable OpenMetrics endpoint**.
3. Keep `127.0.0.1:9108` when the scraper runs on the KStars host, then apply the settings.
4. Verify `curl http://127.0.0.1:9108/healthz` and `curl http://127.0.0.1:9108/metrics`.

The endpoint starts when the Ekos Manager and Analyze module are created. It is disabled by default and performs no network or periodic background work while disabled.

For remote scraping, bind a specific observatory-LAN address or `0.0.0.0`. The server has no authentication or TLS and exposes scheduler job names, filters, and session measurements. Restrict access with a host firewall and trusted VLAN/VPN, or retain the loopback default and use an SSH tunnel or authenticated TLS reverse proxy. Do not expose the port directly to the Internet.

## Prometheus

Copy [`prometheus.yml`](prometheus.yml), replace the target hostname when Prometheus is not on the KStars host, and add its scrape job to the production Prometheus configuration. A 5–15 second interval is appropriate; faster scraping does not improve the rate at which devices produce measurements.

After reloading Prometheus, confirm that `up{job="kstars-ekos"}` is `1` and query `kstars_ekos_session_elapsed_seconds`. If the target is down, verify the KStars status indicator, bind address, firewall, and that Ekos has been opened.

## OpenTelemetry Collector

The Collector does not need a native OTLP endpoint in KStars. Its Prometheus receiver understands OpenMetrics and converts samples into the OpenTelemetry metrics data model. Copy the `receivers.prometheus` section from [`otel-collector.yml`](otel-collector.yml), set the KStars target, and configure the OTLP exporter for the observability backend.

The example exports with OTLP/gRPC. Add the backend's authentication and TLS settings rather than setting `insecure: true` in production.

## Grafana

Import [`grafana-dashboard.json`](grafana-dashboard.json), select the Prometheus datasource, then choose the `job` and `instance` variables. The dashboard includes session, capture, guiding, focus, mount, alignment, meridian-flip, and scheduler panels. It defaults to a six-hour observation window and a five-second refresh.

## Metric behavior

All counters describe the current Analyze session and reset when Analyze restarts. Prometheus recognizes this as a normal counter reset. Session start and elapsed gauges identify the boundary. Metrics without a valid measurement are omitted; consumers must not interpret a missing series as zero. Loaded historical `.analyze` files never replace live exporter data.

String states are stable ASCII values and are not translated. Filter labels are limited to 32 distinct values per session; additional filters use `filter="other"`. Active scheduler job names are intentionally exposed. Filenames, target filenames, detailed focus failure information, and scheduler end-reason text are not exported.

| Area | Metrics | Type / unit |
|---|---|---|
| Build/session | `kstars_info`, `kstars_ekos_session_start_time_seconds`, `kstars_ekos_session_elapsed_seconds` | Gauge; Unix/elapsed seconds |
| Capture state | `kstars_ekos_capture_active`, `kstars_ekos_capture_filter_info`, `kstars_ekos_capture_configured_exposure_seconds`, `kstars_ekos_capture_elapsed_seconds`, `kstars_ekos_capture_last_duration_seconds` | Gauge |
| Capture totals | `kstars_ekos_capture_results_total{filter,result}`, `kstars_ekos_capture_exposure_seconds_total{filter}` | Counter; attempts/seconds |
| Capture quality | `kstars_ekos_capture_hfr`, `kstars_ekos_capture_eccentricity`, `kstars_ekos_capture_detected_stars`, `kstars_ekos_capture_median_adu`, `kstars_ekos_capture_target_distance_arcseconds` | Gauge |
| Focus | `kstars_ekos_autofocus_active`, `kstars_ekos_autofocus_last_duration_seconds`, `kstars_ekos_autofocus_runs_total{filter,result}`, `kstars_ekos_focus_position_steps`, `kstars_ekos_adaptive_focus_adjustment_steps`, `kstars_ekos_adaptive_focus_runs_total{filter,result}`, `kstars_ekos_focuser_temperature_celsius` | Gauges and counters |
| Guiding state | `kstars_ekos_guide_state{state}`, `kstars_ekos_guide_state_transitions_total{state}` | One-hot gauge and counter |
| Guiding quality | `kstars_ekos_guide_ra_error_arcseconds`, `kstars_ekos_guide_dec_error_arcseconds`, `kstars_ekos_guide_drift_arcseconds`, `kstars_ekos_guide_rms_arcseconds`, `kstars_ekos_guide_capture_rms_arcseconds` | Gauge; arcseconds |
| Guiding support | `kstars_ekos_guide_ra_pulse_milliseconds`, `kstars_ekos_guide_dec_pulse_milliseconds`, `kstars_ekos_guide_signal_to_noise_ratio`, `kstars_ekos_guide_detected_stars`, `kstars_ekos_guide_sky_background_adu` | Gauge |
| Mount | `kstars_ekos_mount_state{state}`, mount RA/DEC/hour-angle/azimuth/altitude `_degrees` gauges, `kstars_ekos_mount_pier_side{side}` | One-hot and coordinate gauges |
| Alignment/flip | `kstars_ekos_align_state{state}`, `kstars_ekos_align_results_total{result}`, `kstars_ekos_meridian_flip_state{state}`, `kstars_ekos_meridian_flip_results_total{result}` | One-hot gauges and counters |
| Scheduler | `kstars_ekos_scheduler_job_active`, `kstars_ekos_scheduler_job_info{job_name}`, `kstars_ekos_scheduler_job_elapsed_seconds`, `kstars_ekos_scheduler_job_last_duration_seconds`, `kstars_ekos_scheduler_jobs_started_total`, `kstars_ekos_scheduler_jobs_ended_total` | Gauges and counters |

Every family includes OpenMetrics `TYPE`, `HELP`, and applicable `UNIT` metadata. `/metrics` supports `GET` and `HEAD`; `/healthz` reports only exporter readiness.
