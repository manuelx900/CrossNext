# Deployment checklist for an optional read-later bridge

This is an inherited service, separate from CrossNext firmware. The repository
contains no operator credentials or CrossNext-hosted deployment. Configure your
own infrastructure before using it.

1. Review `../compose.yaml`, `deploy.sh`, `firewall.sh` and `isolation_test.sh`.
   The scripts retain upstream host aliases, filesystem paths, container users
   and network ranges. Adapt them to your own deployment; do not run them
   against a host you have not configured.
2. Create the service's untracked `.env` with the variables listed in
   [the service README](../README.md). Set file permissions for the intended
   service account, and arrange encrypted backups outside the repository.
3. Verify that your Instapaper application permits the accounts you intend to
   support. Keep `READ_ALLOWLIST` restricted until that is confirmed. Do not
   infer permission from another operator's approval or a successful health check.
4. Run the local protocol and attack suites before deployment. Configure your
   own hostname, TLS, tunnel and access policy; the source does not create them.
5. Install the host firewall rules and run the isolation checks from both the
   application and tunnel networks. Verify blocked access to your private
   networks and the host, as well as required outbound API connectivity.
6. Test sign-in, pairing, synchronization and unpairing with a disposable
   account and synthetic article. If access is intended for other accounts,
   verify that behavior with a separate account before opening the allowlist.
7. Inspect logs without sharing credentials, article contents, device IDs or
   account identifiers. Keep a way to stop external ingress and restore the
   previous service image and data backup.

Repeat isolation and account-access checks after relevant deployment changes.
The [security notes](../../../docs/bridge-security.md) describe the service's
scope; no deployment state or account approval is guaranteed by this checklist.
