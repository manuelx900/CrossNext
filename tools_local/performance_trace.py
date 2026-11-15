"""Convert one boot's TRACE log to Perfetto JSON and summarize completed operations."""
import argparse
import json
import math
from pathlib import Path
import re
import statistics


def convert(text):
    operations, spans, storage, counters = {}, [], {}, {}
    for line in text.splitlines():
        match = re.search(r"\b(span|operation|storage|counter) id=(\d+) (.*)", line)
        if not match:
            continue
        kind, identifier, fields = match.groups()
        identifier = int(identifier)
        values = dict(re.findall(r"(\w+)=([\w.]+)", fields))
        if kind == "operation":
            if identifier in operations:
                raise ValueError("Repeated operation ID: split captures by boot before converting")
            operations[identifier] = values
        elif kind == "span":
            spans.append((identifier, values))
        elif kind == "storage":
            storage[identifier] = {k: int(v) for k, v in values.items()}
        else:
            counters.setdefault(identifier, {})[values["name"]] = int(values["value"])
    events = []
    groups = {}
    for identifier, op in operations.items():
        start, duration = int(op["ts"]), int(op["dur"])
        groups.setdefault(op["kind"], []).append(duration / 1000)
        events.append({"name": op["kind"], "cat": "operation", "ph": "X", "pid": 1,
                       "tid": 0, "ts": start, "dur": duration,
                       "args": {"id": identifier, **storage.get(identifier, {}), **counters.get(identifier, {})}})
    for identifier, fields in spans:
        if identifier in operations:
            events.append({"name": fields["stage"], "cat": "stage", "ph": "X", "pid": 1,
                           "tid": int(fields["task"]), "ts": int(fields["ts"]), "dur": int(fields["dur"]),
                           "args": {"operation": identifier}})
    summary = {}
    for name, samples in groups.items():
        ordered = sorted(samples)
        summary[name] = {"samples": len(samples), "p50_ms": statistics.median(samples),
                         "p95_ms": ordered[math.ceil(len(ordered) * .95) - 1], "max_ms": ordered[-1]}
    return {"traceEvents": events, "displayTimeUnit": "ms"}, summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    trace, summary = convert(args.log.read_text(encoding="utf-8", errors="replace"))
    if not summary:
        parser.error("No completed TRACE operations; check the development build and capture")
    args.output.write_text(json.dumps(trace), encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
