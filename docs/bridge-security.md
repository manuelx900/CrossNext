# Security of optional inherited bridge services

CrossNext retains source code for upstream read-later and study bridge tools.
These services are separate from the X4 Pro firmware and are not required for
the current local reading, Flashcards or Words workflows. This repository does
not provide a CrossNext-hosted bridge deployment.

For code and configuration details, see `server/read-bridge/README.md` and
`server/study-bridge/README.md`. Deployments must supply their own credentials,
hostnames, access policy and backups. Do not copy another operator's private
network layout, device inventory or account configuration.

Keep secrets in environment configuration outside Git. Examples and test
fixtures are not production credentials. Remove tokens and personal data from
diagnostics; review authentication, rate limits, external requests and backup
handling before exposing a service. Report vulnerabilities through the
[project security policy](../SECURITY.md).
