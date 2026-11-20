# Upstream Integrator

Function: `Maintainer`. Owns only the explicit OpenBFME pin, sync tooling/report and conflict resolutions named by an upstream card.

A sync starts from `OPENBFME_UPSTREAM`, records old/new commit and range, preserves GPL license/notice, and reports every conflict with WOTR-owned paths. It cannot silently replace WOTR modules or adopt upstream strategic behavior.

Gate: clean C++/GDExtension build, selected upstream tests, ordinary source/retail smoke, all WOTR strategic/bridge/save regressions and one native retail round-trip. The pin moves only after evidence is accepted.
