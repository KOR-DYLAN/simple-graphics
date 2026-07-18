#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

set -eu

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <trace-root> <program> [arguments ...]" >&2
    exit 2
fi

trace_root=$1
shift
program=$1
shift

lttng_command=${SGL_LTTNG_COMMAND:-lttng}
lttng_sessiond_command=${SGL_LTTNG_SESSIOND_COMMAND:-lttng-sessiond}
kernel_trace=${SGL_LTTNG_KERNEL:-0}
lttng_library_dir=${SGL_LTTNG_LIBRARY_DIR:-}
lttng_report=${SGL_LTTNG_REPORT:-}
timestamp=$(date +%Y%m%d-%H%M%S)
session="sgl-${timestamp}-$$"
trace_dir="${trace_root}/${session}"
session_created=0
recording_started=0
use_managed_sessiond=0

if [ -n "${SGL_LTTNG_HOME:-}" ]; then
    lttng_home=$SGL_LTTNG_HOME
elif [ "$kernel_trace" = "1" ]; then
    lttng_home=$HOME
else
    # LTTng stores Unix sockets below LTTNG_HOME. Keep the default short
    # enough for Linux's 108-byte sockaddr_un path limit.
    lttng_home="${TMPDIR:-/tmp}/sgl-lttng-$(id -u)"
fi

run_lttng()
{
    if [ "$use_managed_sessiond" -eq 1 ]; then
        "$lttng_command" --no-sessiond "$@"
    else
        "$lttng_command" "$@"
    fi
}

cleanup()
{
    status=$?
    trap - EXIT HUP INT TERM

    if [ "$recording_started" -eq 1 ]; then
        run_lttng stop "$session" >/dev/null 2>&1 || true
    fi
    if [ "$session_created" -eq 1 ]; then
        run_lttng destroy "$session" >/dev/null 2>&1 || true
    fi

    exit "$status"
}

trap cleanup EXIT HUP INT TERM

if ! command -v "$lttng_command" >/dev/null 2>&1; then
    echo "error: lttng command not found: $lttng_command" >&2
    echo "Ubuntu: sudo apt install lttng-tools liblttng-ust-dev babeltrace2" >&2
    exit 1
fi

if [ "$kernel_trace" != "1" ] &&
   ! command -v "$lttng_sessiond_command" >/dev/null 2>&1; then
    echo "error: lttng-sessiond command not found: $lttng_sessiond_command" >&2
    echo "Ubuntu: sudo apt install lttng-tools" >&2
    exit 1
fi

if [ ! -x "$program" ]; then
    echo "error: profiled program is not executable: $program" >&2
    exit 1
fi

mkdir -p "$trace_root" "$lttng_home"
export LTTNG_HOME="$lttng_home"
if [ -n "$lttng_library_dir" ]; then
    LD_LIBRARY_PATH="${lttng_library_dir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    export LD_LIBRARY_PATH
fi

if [ "$kernel_trace" != "1" ]; then
    if ! "$lttng_command" --no-sessiond list >/dev/null 2>&1; then
        "$lttng_sessiond_command" --background --no-kernel
    fi
    use_managed_sessiond=1
fi

run_lttng create "$session" --output="$trace_dir"
session_created=1

run_lttng enable-event --userspace 'sgl_core:*'
run_lttng add-context --userspace --type=vpid
run_lttng add-context --userspace --type=vtid
run_lttng add-context --userspace --type=pthread_id
run_lttng add-context --userspace --type=procname

if [ "$kernel_trace" = "1" ]; then
    run_lttng enable-event --kernel \
        sched_switch,sched_wakeup,sched_waking
    run_lttng enable-event --kernel --syscall futex
fi

run_lttng start "$session"
recording_started=1

set +e
"$program" "$@"
program_status=$?
set -e

run_lttng stop "$session"
recording_started=0
run_lttng destroy "$session"
session_created=0

trap - EXIT HUP INT TERM

echo "LTTng trace: $trace_dir"
if [ -n "$lttng_report" ]; then
    if [ ! -x "$lttng_report" ]; then
        echo "error: LTTng report tool is not executable: $lttng_report" >&2
        exit 1
    fi
    "$lttng_report" "$trace_dir"
fi
exit "$program_status"
