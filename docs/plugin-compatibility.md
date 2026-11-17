# Plugin compatibility policy

The upstream Plugin Hub catalog is not a guarantee that this fork implements
every host extension. `apps_local/plugins/PluginCompatibility.h` blocks these
plugin IDs until their missing host contracts are implemented and verified:

| Plugin | Audited version | Missing contract |
| --- | --- | --- |
| Dropbox | 1.0.0 | Native refresh grant, folder navigation and cursor continuation |
| Google Drive | 1.0.0 | Native folder navigation and cursor continuation |
| LocalSend | 0.1.1 | `transfer.localsend.v1` native receiver |
| Reading Statistics | 0.1.8 | `stats.native.v1` / `ui.dashboard.v1` native dashboard |

The policy uses IDs rather than titles. Native catalogs filter them before
pagination, installed plugin discovery omits them, and bundle/fetch handlers
refuse their plugin-root destinations. Files on the user's card are retained.
The HTTP status response exposes the same list to the browser plugin host.
The host filters Hub relay responses and its cached catalog JSON before
executing the Hub script. Web-only plugins with supported browser APIs remain
available; lacking a native catalog does not make a browser plugin incompatible.

The deny list is intentionally independent of version: a new bundle version
cannot add a missing firmware feature. Reassess an ID when host support changes.
Declared firmware-family, API-minimum and capability checks still apply.

## Reproducible audit

`tools_local/plugins/audit_catalog.py --catalog <catalog.json> --output <directory>`
downloads the exact bundle files declared in a pinned catalog and validates JSON,
records hashes and reports unsupported manifest contracts. `--cached` rechecks
those fixtures without network access. Requests are read-only and do not sign
in, create accounts, send events or change an external service.

The 2026-11-16 audit used the upstream catalog generated on 2026-11-14, containing
38 plugins and 195 declared assets. All 195 assets downloaded successfully and
their JSON files parsed. The browser fixture passed 28 bundle initializations
and one blocked-bundle filtering case (LocalSend). These results establish
packaging and host integration;
they do not prove authenticated BookFusion/Readwise/etc. service workflows.
Account-based operations need separate device tests with the user's own account.
