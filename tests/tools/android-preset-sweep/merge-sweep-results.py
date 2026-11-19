#
#  merge-sweep-results.py
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-11-07.
#

import argparse
import glob
import json
import os
import sys
import time

PLAYS = ("ok", "degraded")
METRIC_FIELDS = (
    "frames", "fps_mean", "fps_p5", "cost_p50_ms", "cost_p95_ms", "sim_steps_per_frame",
    "warp_shader", "comp_shader", "gl_errors", "last_error", "blank", "frozen", "report_display_fps",
)


def read_lines(path):
    records = []
    if not os.path.isfile(path):
        return records
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError:
                print("skipped a torn line in %s" % path, file=sys.stderr)
    return records


def event(args):
    record = {
        "index": args.index,
        "name": args.name,
        "source": args.source,
        "status": args.status,
        "error": args.error,
        "prepare_ms": None,
        "commit_ms": None,
        "thermal_status": args.thermal,
        "display_hz": args.display_hz,
        "set": args.set or None,
        "timestamp_ms": int(time.time() * 1000),
    }
    for field in METRIC_FIELDS:
        record[field] = None
    print(json.dumps(record, ensure_ascii=False))


def merge(args):
    by_index = {}
    for record in read_lines(args.device_results):
        by_index[record["index"]] = record
    events = {}
    for record in read_lines(args.events):
        events[record["index"]] = record

    retries = {}
    for path in glob.glob(os.path.join(args.retry_root, "*", "results.jsonl")):
        for record in read_lines(path):
            retries[record["index"]] = record
    retry_events = {}
    for path in glob.glob(os.path.join(args.retry_root, "*", "events.jsonl")):
        for record in read_lines(path):
            retry_events[record["index"]] = record

    merged = {}
    for index, record in by_index.items():
        merged[index] = record
    for index, record in events.items():
        if index in merged:
            continue
        first_failure = record["error"]
        final = record
        if record["status"] == "crash":
            retried = retries.get(index)
            if retried is not None:
                final = dict(retried)
                final["status"] = "crash_flaky" if retried["status"] in PLAYS else retried["status"]
                final["first_failure"] = first_failure
                final["retried"] = True
            elif index in retry_events:
                final = dict(record)
                final["retried"] = True
                final["retry_error"] = retry_events[index]["error"]
            else:
                final = dict(record)
                final["retried"] = False
        merged[index] = final

    ordered = [merged[index] for index in sorted(merged)]
    with open(args.out, "w", encoding="utf-8") as handle:
        for record in ordered:
            handle.write(json.dumps(record, ensure_ascii=False) + "\n")

    counts = {}
    for record in ordered:
        counts[record["status"]] = counts.get(record["status"], 0) + 1
    flags = {
        "blank": sum(1 for record in ordered if record.get("blank")),
        "frozen": sum(1 for record in ordered if record.get("frozen")),
    }
    print(json.dumps({"records": len(ordered), "status": counts, "flags": flags}, sort_keys=True))

    if args.expected is not None and len(ordered) != args.expected:
        print("expected %d records, merged %d" % (args.expected, len(ordered)), file=sys.stderr)
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    event_parser = sub.add_parser("event")
    event_parser.add_argument("--index", type=int, required=True)
    event_parser.add_argument("--name", required=True)
    event_parser.add_argument("--source", default="")
    event_parser.add_argument("--status", required=True)
    event_parser.add_argument("--error", default="")
    event_parser.add_argument("--thermal", type=int, default=None)
    event_parser.add_argument("--display-hz", type=float, default=None)
    event_parser.add_argument("--set", default="")

    merge_parser = sub.add_parser("merge")
    merge_parser.add_argument("--device-results", required=True)
    merge_parser.add_argument("--events", required=True)
    merge_parser.add_argument("--retry-root", required=True)
    merge_parser.add_argument("--out", required=True)
    merge_parser.add_argument("--expected", type=int, default=None)

    args = parser.parse_args()
    if args.command == "event":
        event(args)
        return 0
    return merge(args)


if __name__ == "__main__":
    sys.exit(main())
