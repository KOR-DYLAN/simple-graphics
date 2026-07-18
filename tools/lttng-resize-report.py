#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.
#

"""Summarize SGL resize and threadpool intervals from an LTTng CTF trace."""

import argparse
import statistics
from collections import defaultdict

import bt2


EVENT_PREFIX = "sgl_core:"


def parse_args():
    parser = argparse.ArgumentParser(
        description="Report resize/threadpool metrics from an SGL LTTng trace."
    )
    parser.add_argument("trace", help="LTTng CTF trace directory")
    parser.add_argument(
        "--top",
        type=int,
        default=12,
        help="number of slowest/problematic dispatches to print",
    )
    return parser.parse_args()


def percentile(values, percent):
    if not values:
        return 0.0

    ordered = sorted(values)
    index = round((len(ordered) - 1) * percent)
    return ordered[index]


def microseconds(nanoseconds):
    return nanoseconds / 1_000.0


def format_us(nanoseconds):
    return f"{microseconds(nanoseconds):9.2f}"


def field_value(value):
    try:
        return int(value)
    except (TypeError, ValueError):
        return str(value)


def field_dict(field):
    return {name: field_value(value) for name, value in field.items()}


def event_messages(trace_path):
    for message in bt2.TraceCollectionMessageIterator(trace_path):
        if isinstance(message, bt2._EventMessageConst):
            event = message.event
            if event.name.startswith(EVENT_PREFIX):
                yield (
                    message.default_clock_snapshot.ns_from_origin,
                    event.name[len(EVENT_PREFIX) :],
                    field_dict(event.common_context_field),
                    field_dict(event.payload_field),
                )


def new_dispatch(timestamp, payload, resize):
    return {
        "start": timestamp,
        "end": timestamp,
        "pool": payload["pool"],
        "generation": payload["generation"],
        "operation_count": payload["operation_count"],
        "worker_limit": payload["worker_limit"],
        "requested_threads": payload["requested_threads"],
        "resize": dict(resize) if resize is not None else {},
        "participants": [],
        "waits": [],
        "queue_waits": [],
    }


def parse_trace(trace_path):
    event_count = 0
    current_resize = {}
    resize_begin = {}
    resize_records = []
    dispatches = {}
    completed_dispatches = []
    participant_begin = {}
    completion_wait_begin = {}
    queue_wait_begin = {}
    active_dispatch_by_thread = {}

    for timestamp, name, context, payload in event_messages(trace_path):
        event_count += 1
        thread_id = context["vtid"]

        if name == "resize_begin":
            current_resize[thread_id] = dict(payload)
            resize_begin[thread_id] = timestamp
        elif name == "resize_end":
            begin = resize_begin.pop(thread_id, None)
            resize = current_resize.pop(thread_id, {})
            if begin is not None:
                resize["duration"] = timestamp - begin
                resize_records.append(resize)
        elif name == "threadpool_dispatch_begin":
            key = (payload["pool"], payload["generation"])
            dispatches[key] = new_dispatch(
                timestamp, payload, current_resize.get(thread_id)
            )
            active_dispatch_by_thread[thread_id] = key
        elif name == "threadpool_dispatch_end":
            key = (payload["pool"], payload["generation"])
            dispatch = dispatches.pop(key, None)
            if dispatch is not None:
                dispatch["end"] = timestamp
                completed_dispatches.append(dispatch)
            active_dispatch_by_thread.pop(thread_id, None)
        elif name == "threadpool_participant_begin":
            key = (payload["pool"], payload["generation"], thread_id)
            participant_begin[key] = (timestamp, payload["role"])
            active_dispatch_by_thread[thread_id] = key[:2]
        elif name == "threadpool_participant_end":
            dispatch_key = (payload["pool"], payload["generation"])
            key = (*dispatch_key, thread_id)
            begin = participant_begin.pop(key, None)
            dispatch = dispatches.get(dispatch_key)
            if begin is not None and dispatch is not None:
                dispatch["participants"].append(
                    {
                        "thread_id": thread_id,
                        "role": payload["role"],
                        "start": begin[0],
                        "duration": timestamp - begin[0],
                        "completed_operations": payload["completed_operations"],
                    }
                )
            if payload["role"] == "worker":
                active_dispatch_by_thread.pop(thread_id, None)
        elif name == "threadpool_completion_wait_begin":
            key = (payload["pool"], payload["generation"], thread_id)
            completion_wait_begin[key] = timestamp
        elif name == "threadpool_completion_wait_end":
            dispatch_key = (payload["pool"], payload["generation"])
            key = (*dispatch_key, thread_id)
            begin = completion_wait_begin.pop(key, None)
            dispatch = dispatches.get(dispatch_key)
            if begin is not None and dispatch is not None:
                dispatch["waits"].append(timestamp - begin)
        elif name == "queue_lock_contended":
            key = (thread_id, payload["queue"], str(payload["operation"]))
            dispatch_key = active_dispatch_by_thread.get(thread_id)
            if (dispatch_key is None) and (len(dispatches) == 1):
                # A worker reserves its first operation before its participant
                # event. Attribute that wait only when the active dispatch is
                # unambiguous; concurrent pools must not be guessed.
                dispatch_key = next(iter(dispatches))
            queue_wait_begin[key] = (
                timestamp,
                dispatch_key,
            )
        elif name == "queue_lock_acquired":
            key = (thread_id, payload["queue"], str(payload["operation"]))
            begin = queue_wait_begin.pop(key, None)
            if begin is not None:
                dispatch = dispatches.get(begin[1])
                if dispatch is not None:
                    dispatch["queue_waits"].append(timestamp - begin[0])

    return event_count, resize_records, completed_dispatches


def resize_label(resize):
    if not resize:
        return "unknown"

    lut = "lut" if resize.get("has_prebuilt_lut") else "temp"
    return (
        f"{resize['backend']}/{resize['method']} "
        f"{resize['destination_width']}x{resize['destination_height']} "
        f"{resize['bytes_per_pixel']}ch {resize['requested_threads']}t {lut}"
    )


def finalize_dispatch(dispatch):
    duration = dispatch["end"] - dispatch["start"]
    workers = [
        participant
        for participant in dispatch["participants"]
        if participant["role"] == "worker"
    ]
    all_start_delays = [
        participant["start"] - dispatch["start"]
        for participant in dispatch["participants"]
    ]
    worker_start_delays = [
        participant["start"] - dispatch["start"] for participant in workers
    ]
    operation_counts = [
        participant["completed_operations"]
        for participant in dispatch["participants"]
    ]
    dispatch["duration"] = duration
    dispatch["worker_count"] = len(workers)
    dispatch["max_start_delay"] = max(all_start_delays, default=0)
    dispatch["max_worker_start_delay"] = max(worker_start_delays, default=0)
    dispatch["completion_wait"] = sum(dispatch["waits"])
    dispatch["queue_wait"] = sum(dispatch["queue_waits"])
    dispatch["idle_participants"] = sum(count == 0 for count in operation_counts)
    if operation_counts:
        dispatch["operation_spread"] = max(operation_counts) - min(operation_counts)
    else:
        dispatch["operation_spread"] = 0


def print_thread_summary(dispatches):
    by_thread_count = defaultdict(list)

    for dispatch in dispatches:
        by_thread_count[dispatch["requested_threads"]].append(dispatch)

    print("\nThreadpool summary (microseconds)")
    print(
        "threads dispatches  dispatch-avg  wake-p95  completion-avg  "
        "queue-total  contention"
    )
    for thread_count in sorted(by_thread_count):
        rows = by_thread_count[thread_count]
        durations = [row["duration"] for row in rows]
        wake_delays = [row["max_worker_start_delay"] for row in rows]
        completion_waits = [row["completion_wait"] for row in rows]
        queue_waits = [row["queue_wait"] for row in rows]
        contention_count = sum(len(row["queue_waits"]) for row in rows)
        print(
            f"{thread_count:7d} {len(rows):10d} "
            f"{format_us(statistics.mean(durations))} "
            f"{format_us(percentile(wake_delays, 0.95))} "
            f"{format_us(statistics.mean(completion_waits))} "
            f"{format_us(sum(queue_waits))} {contention_count:11d}"
        )


def print_problem_dispatches(dispatches, top_count):
    ranked = sorted(
        dispatches,
        key=lambda row: (
            row["max_worker_start_delay"] / max(row["duration"], 1),
            row["completion_wait"] / max(row["duration"], 1),
        ),
        reverse=True,
    )

    print("\nLargest worker-start ratios")
    print(
        " wake%  wait% duration-us workers idle spread contended  resize"
    )
    for dispatch in ranked[:top_count]:
        duration = max(dispatch["duration"], 1)
        print(
            f"{dispatch['max_worker_start_delay'] * 100.0 / duration:6.1f} "
            f"{dispatch['completion_wait'] * 100.0 / duration:6.1f} "
            f"{format_us(duration)} "
            f"{dispatch['worker_count']:2d}/{dispatch['worker_limit']:<2d} "
            f"{dispatch['idle_participants']:4d} "
            f"{dispatch['operation_spread']:6d} "
            f"{len(dispatch['queue_waits']):9d}  "
            f"{resize_label(dispatch['resize'])}"
        )


def print_findings(dispatches):
    late_workers = 0
    unused_worker_slots = 0
    idle_participants = 0
    long_completion_waits = 0

    for dispatch in dispatches:
        duration = max(dispatch["duration"], 1)
        if dispatch["max_worker_start_delay"] * 4 > duration:
            late_workers += 1
        if dispatch["worker_count"] < dispatch["worker_limit"]:
            unused_worker_slots += 1
        if dispatch["idle_participants"] > 0:
            idle_participants += 1
        if dispatch["completion_wait"] * 4 > duration:
            long_completion_waits += 1

    print("\nScheduling and work-distribution indicators")
    print(f"worker start exceeds 25% of dispatch: {late_workers}")
    print(f"worker slots unused (queue drained):   {unused_worker_slots}")
    print(f"dispatches with idle participants:     {idle_participants}")
    print(f"completion wait exceeds 25%:           {long_completion_waits}")


def main():
    args = parse_args()
    event_count, resize_records, dispatches = parse_trace(args.trace)

    for dispatch in dispatches:
        finalize_dispatch(dispatch)

    print(f"trace events: {event_count}")
    print(f"resize intervals: {len(resize_records)}")
    print(f"threadpool dispatches: {len(dispatches)}")
    print_thread_summary(dispatches)
    print_problem_dispatches(dispatches, args.top)
    print_findings(dispatches)


if __name__ == "__main__":
    main()
