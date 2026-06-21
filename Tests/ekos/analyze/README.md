<!--
SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Analyze tests

`TestAnalyzeMetrics` covers the live Ekos Analyze OpenMetrics model and HTTP endpoint without requiring a display or connected astronomy equipment.

The tests exercise session reset behavior, capture and focus totals, rolling guider RMS, mount/alignment/meridian-flip state, scheduler job labels, OpenMetrics escaping and filter cardinality limits. Endpoint coverage includes lifecycle, ephemeral and conflicting ports, `GET`/`HEAD`, health and error paths, fragmented requests, and the request-size limit.

`AnalyzeMetricsFixture` exposes deterministic samples on `0.0.0.0:9108`. The `integration` directory contains Prometheus and OpenTelemetry Collector configurations used for real ARM64 scrape checks.

Run the test from a configured build tree with:

```sh
ctest --test-dir build -R TestAnalyzeMetrics --output-on-failure
```

Analyze log parsing and timeline rendering remain separate coverage gaps because they require the full Analyze widget and representative `.analyze` fixtures.
