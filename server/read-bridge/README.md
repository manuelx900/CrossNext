# Read-later bridge

This inherited service connects the Instapaper API to the reader's read-later
queue. It is separate from the CrossNext firmware. CrossNext does not operate
a public instance; references to upstream hosting do not grant deployment access.

The service converts article HTML to text, pairs readers and caches prepared
articles. The device supplies its own index for incremental synchronization.
The protocol does not expose the Instapaper bookmark-deletion operation.

## Configuration

Use `compose.yaml` and `bridge/app.py` as the configuration reference.
Supply your own values through an untracked `.env` file:

- `READ_FERNET_KEY`: encryption key for stored credentials.
- `READ_ALLOWLIST`: permitted accounts. An unset value refuses sign-in;
  `*` allows accounts beyond a fixed list.
- `READ_CONSUMER_KEY` and `READ_CONSUMER_SECRET`: your Instapaper application credentials.
- `INSTAPARSER_API_KEY`: article-text API key, when required by the API.
- `CLOUDFLARE_TUNNEL_TOKEN`: connector token if using the supplied tunnel container.
- `SUPABASE_URL` and `SUPABASE_ANON_KEY`: optional event reporting. With either
  unset, that reporting is disabled. Never supply a service-role key here.

Keep `.env` and `data/` out of Git and source archives. Preserve the Fernet key
with any encrypted-store backup. A new operator must verify their own API
permissions; previous deployment approvals do not transfer with the source.

## Deployment

Read [the deployment checklist](scripts/DEPLOY-RUNBOOK.md) before adapting the
inherited scripts. They contain upstream defaults for host aliases, paths,
subnets and container users. Review those values for your environment.

The Compose configuration uses one application worker, a read-only root
filesystem, resource limits and a separate bridge network. Its firewall and
isolation scripts require host-side setup. A successful container health check
does not establish isolation from other services.

See the [bridge security notes](../../docs/bridge-security.md) for scope and
the [Instapaper guide](../../docs/apps/instapaper.md) for the device protocol.

## Local tests

Create a Python environment and install `requirements.txt` in this directory.
The `tests/test_*.py` programs use `tests/fake_instapaper.py` so protocol checks
can run without real credentials. `tests/attack_test.py` checks malformed and
unauthorized requests against an isolated test instance. `tests/sim_stack.sh`
also builds and drives the simulator.

The shared page styling is in `bridge/chrome.py`; its counterpart in
`server/study-bridge/` must remain compatible with the shared-page tests.
