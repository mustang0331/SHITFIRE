# Third-party credits — port/

Vendored (a copy stored in the repo instead of downloaded at build time),
test-harness only; nothing here is linked into the sim core or shipped.

| Library | Version | License | Source | Used for |
|---|---|---|---|---|
| nlohmann/json | v3.12.0 | MIT | https://github.com/nlohmann/json (release asset `json.hpp`) | fixture JSON loading in core-tests |
| Catch2 (amalgamated) | v3.16.0 | BSL-1.0 | https://github.com/catchorg/Catch2 (`extras/catch_amalgamated.hpp/.cpp` at tag v3.16.0) | unit/parity test framework in core-tests |
