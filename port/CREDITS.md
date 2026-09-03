# Third-party credits — port/

Vendored (a copy stored in the repo instead of downloaded at build time),
test-harness only; nothing here is linked into the sim core or shipped.

| Library | Version | License | Source | Used for |
|---|---|---|---|---|
| nlohmann/json | v3.12.0 | MIT | https://github.com/nlohmann/json (release asset `json.hpp`) | fixture JSON loading in core-tests |
| Catch2 (amalgamated) | v3.16.0 | BSL-1.0 | https://github.com/catchorg/Catch2 (`extras/catch_amalgamated.hpp/.cpp` at tag v3.16.0) | unit/parity test framework in core-tests |

## port/unreal — vendored plugins

| Library | Version | License | Source | Used for |
|---|---|---|---|---|
| RealtimeMeshComponent (Core edition) | master commit `b8669a0891b728d8fa8a31f43f76d3853c10f743` (2026-08-23, plugin VersionName 5.4) | MIT | https://github.com/TriAxis-Games/RealtimeMeshComponent | runtime terrain/road/ocean meshes in the UE port |

RMC pin note: the newest git tag (v5.3.2, 2025-12-02) predates the repo's UE 5.5+
support statement; only master's README declares "Supported engine versions: UE
5.5 – 5.8", so master was pinned at the commit above instead of a tag (the
repo's release channel is Fab, not GitHub tags). Vendored with `.git` removed;
re-vendor by cloning that commit into port/unreal/Plugins/RealtimeMeshComponent.
