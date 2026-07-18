<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

Linux LTTng and Trace Compass Profiling
=======================================

[Documentation index](README.md) | [Main README](../README.md)

Purpose
-------
SGL has optional LTTng-UST tracepoints for resize and threadpool analysis.
Instrumentation is disabled by default, so normal benchmark binaries do not
link LTTng or execute tracepoint code.

The trace records these intervals:

```text
resize_begin
  threadpool_dispatch_begin
    threadpool_participant_begin [submitter]
    threadpool_participant_begin [worker] ...
      queue_lock_contended -> queue_lock_acquired
    threadpool_participant_end [submitter/worker] ...
    threadpool_completion_wait_begin -> threadpool_completion_wait_end
  threadpool_dispatch_end
resize_end
```

Installation
------------
Install the LTTng command-line tools, UST development package, and Babeltrace 2
on Ubuntu or WSL:

```sh
sudo apt update
sudo apt install lttng-tools liblttng-ust-dev babeltrace2
```

The project traces user-space code by default. `lttng-modules-dkms` is not
required for this mode. On WSL, a previously installed kernel tracer package
can fail while configuring Ubuntu's generic kernel headers even though all
three packages above installed successfully. Remove the unused kernel module
package and complete the interrupted package configuration:

```sh
sudo dpkg --remove lttng-modules-dkms
sudo dpkg --configure -a
sudo apt --fix-broken install
```

Install and repair `lttng-modules-dkms` only on a native Linux system whose
running kernel and development headers are supported when kernel tracing is
actually required.

Download Trace Compass from the
[official Trace Compass site](https://eclipse.dev/tracecompass/). Kernel event
capture additionally needs a compatible LTTng kernel tracer and permission to
trace the host kernel. This is optional and may not be available under WSL.

Capture
-------
Build an instrumented `RelWithDebInfo` binary and capture one measured resize
iteration without warm-up calls:

```sh
make profile-lttng BUILD_TYPE=RelWithDebInfo
```

The target creates a unique CTF trace below:

```text
build/profile-lttng/<toolchain>/profile/lttng/sgl-<timestamp>-<pid>/
```

The dedicated profile build tree prevents LTTng and short-repeat cache values
from changing the normal `build/<toolchain>` benchmark configuration. Override
it with `PROFILE_BUILD=/path/to/build` when required.

User-space capture starts a `--no-kernel` session daemon below the short
`/tmp/sgl-lttng-<uid>` runtime path. This avoids the Linux Unix-domain socket
path limit even when the build directory is deeply nested. Set
`SGL_LTTNG_HOME` only when a different short runtime path is required.

Change the short profiling workload when a longer sample is useful:

```sh
make profile-lttng PROFILE_REPEAT_COUNT=3 PROFILE_WARMUP_COUNT=1
```

Capture scheduler wake-up, context-switch, and futex events together with SGL
events when kernel tracing is available:

```sh
make profile-lttng PROFILE_KERNEL=1 BUILD_TYPE=RelWithDebInfo
```

Kernel capture uses the normal user home and session daemon selection instead
of the project-managed `--no-kernel` daemon. It therefore requires a working
LTTng kernel tracer and the normal root or `tracing` group permissions.

The equivalent direct CMake flow is:

```sh
cmake -S . -B build/lttng -DWITH_LTTNG=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSGL_TEST_RESIZE_REPEAT_COUNT=1 \
  -DSGL_TEST_RESIZE_WARMUP_COUNT=0
cmake --build build/lttng --target profile-lttng
```

Use `-DLTTNG_UST_ROOT=/path/to/prefix` for a non-system LTTng-UST install.
The local `profile-lttng` capture target is excluded from cross builds because
an LTTng session daemon must run on the machine that executes the instrumented
process. An instrumented cross build can still use LTTng-UST libraries from its
target sysroot and be captured on the target machine.

Inspect
-------
Check the CTF events on the command line first:

```sh
babeltrace2 build/profile-lttng/<toolchain>/profile/lttng/sgl-<timestamp>-<pid>
```

`profile-lttng` also runs the project report tool after capture. Re-run it on
any retained trace without recording the workload again:

```sh
tools/lttng-resize-report.py \
  build/profile-lttng/<toolchain>/profile/lttng/sgl-<timestamp>-<pid>
```

The report groups dispatch latency, worker-start delay, completion wait, queue
contention, and work distribution by requested thread count. A worker slot can
remain unused when other participants drain the queue first; this is distinct
from an idle participant, which joined a generation but completed no operation.

In Trace Compass, create a Tracing project, import the generated trace
directory, and open it as a CTF trace. When both user and kernel traces were
captured, add them to one Trace Compass experiment so timestamps can be
correlated.

Event interpretation:

| Measurement | Events and fields | Meaning |
| --- | --- | --- |
| End-to-end resize | `resize_begin` to `resize_end` | Backend, method, geometry, thread request, LUT use, and result. |
| Worker start latency | `threadpool_dispatch_begin` to each worker `threadpool_participant_begin` | Wake-up and scheduling delay before a worker joins a generation. |
| Per-thread work | participant begin/end and `completed_operations` | Work duration and operation-count balance across submitter and workers. |
| Completion tail | `threadpool_completion_wait_begin` to `_end` | Time the submitter waits for the final participant. |
| Queue contention | `queue_lock_contended` to matching `_acquired` | Time blocked on the shared operation or completion queue spinlock. |
| Scheduler cause | `sched_wakeup`, `sched_switch`, and futex events | Kernel evidence for delayed workers or oversubscription. |

Filter on the `pool` and `generation` fields when multiple resize operations
overlap in the view. Queue contention events contain the queue identity and
operation name (`enqueue`, `dequeue`, or `peek`). Common process and thread
contexts (`vpid`, `vtid`, `pthread_id`, and `procname`) are included.

Measurement Rules
-----------------
Tracepoint recording changes the timing of very short operations, especially
when queue contention emits extra events. Use the trace to locate structural
latency and scheduling behavior. Publish latency results from a separate
`WITH_LTTNG=OFF` Release build.
