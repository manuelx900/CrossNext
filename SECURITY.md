# Security

CrossNext is e-reader firmware for Xteink X4 Pro, based on CrossPlay,
CrossPoint Reader and the FreeInk SDK.

## Reporting a vulnerability

Use this repository's [Security page](https://github.com/manuelx900/CrossNext/security)
and private vulnerability reporting when that option is enabled. Do not post
credentials, private reading data or a working exploit in a public issue.
Non-sensitive crashes and reproducible defects can be reported through
[GitHub Issues](https://github.com/manuelx900/CrossNext/issues).

Include the firmware version, device model and reproduction steps. Remove
Wi-Fi details, tokens, personal documents and device identifiers from logs or
screenshots before sharing them. Use a small synthetic test file where possible.

## Scope and limitations

- EPUBs, microSD files, fonts, images, saved games and network responses are
  external inputs. Malformed data that causes memory corruption or exposes
  private information is relevant to a report.
- The local web server should be used only on a trusted network. Developer Mode
  and physical firmware installation require control of the device.
- This project does not claim verified boot, a security certification, a bug
  bounty or a guaranteed response time.
- Source code for optional inherited services may remain in the repository;
  its presence does not mean CrossNext operates a public service or stores
  credentials for that service.

If a defect also reproduces on an unmodified upstream project, include that
information so the fix can be coordinated with its maintainers. Original
copyright and licence notices remain in place.
