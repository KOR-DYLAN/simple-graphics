<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

Python Environment Setup
========================

[Main README](../README.md#prerequisites) | [Documentation index](README.md)

Requirements
------------
| Item | Requirement |
| --- | --- |
| Minimum Python | Python 3.10 |
| Recommended Python | Latest available Python 3.12 patch release |
| Required PyPI packages | None |
| Optional system binding | Babeltrace 2 `bt2` for LTTng report analysis |

The Kconfig menu, build dispatcher, and benchmark report generator use only
Python standard-library modules. A virtual environment is therefore optional,
but using one provides an isolated location for future development tools or
requirements. The LTTng `bt2` binding is handled separately because Linux
distributions normally package it with Babeltrace 2.

System Package Manager
----------------------
Use this path when the distribution provides Python 3.10 or newer. Install
`pip` and `venv` support even though the current project has no required PyPI
packages, so the environment is ready for future requirements.

### Ubuntu and WSL
```sh
sudo apt update
sudo apt install -y python3 python3-pip python3-venv
```

### Fedora
```sh
sudo dnf install -y python3 python3-pip
```

### Arch Linux
```sh
sudo pacman -S --needed python python-pip
```

### macOS with Homebrew
```sh
brew update
brew install python
```

Verify the interpreter, pip, and the curses module used by `make menu`:

```sh
python3 --version
python3 -m pip --version
python3 -c "import curses; print('curses: available')"
```

If `python3 --version` is older than 3.10, use pyenv instead of replacing the
operating system's Python installation.

Pyenv Build Dependencies
------------------------
Pyenv builds most CPython versions from source. Install the compiler and
development libraries before running `pyenv install`.

### Ubuntu and WSL
```sh
sudo apt update
sudo apt install -y \
    make build-essential libssl-dev zlib1g-dev libbz2-dev \
    libreadline-dev libsqlite3-dev curl git libncursesw5-dev \
    xz-utils tk-dev libxml2-dev libxmlsec1-dev libffi-dev \
    liblzma-dev libzstd-dev
```

### Fedora
```sh
sudo dnf install -y \
    make gcc patch zlib-devel bzip2 bzip2-devel readline-devel \
    sqlite sqlite-devel openssl-devel tk-devel libffi-devel \
    xz-devel libuuid-devel gdbm-libs libnsl2
```

Fedora 42 and newer can use `tk8-devel` instead of `tk-devel` when building a
Python release that is incompatible with Tcl/Tk 9.

### Arch Linux
```sh
sudo pacman -S --needed base-devel openssl zlib xz tk zstd curl git
```

### macOS with Homebrew
```sh
xcode-select --install
brew install pyenv openssl@3 readline sqlite3 xz tcl-tk@8 \
    libb2 zstd zlib pkgconfig
```

Install Pyenv
-------------
On Ubuntu, WSL, Fedora, and Arch Linux, use the official installer:

```sh
curl -fsSL https://pyenv.run | bash
~/.pyenv/bin/pyenv init --install
exec "$SHELL"
pyenv --version
```

Review the shell startup-file changes made by `pyenv init --install`. The
result must place `$PYENV_ROOT/bin` and pyenv shims before the system Python in
`PATH`.

On macOS, Homebrew installed pyenv in the previous section. Initialize it and
restart the shell:

```sh
pyenv init --install
exec "$SHELL"
pyenv --version
```

Install the Project Python
--------------------------
The following commands select the newest Python 3.12 patch release known by the
installed pyenv version. `pyenv local` writes the exact selected version to a
project-local `.python-version` file. This repository ignores that
developer-local selection file.

```sh
cd /path/to/simple-graphics
PYTHON_VERSION="$(pyenv latest -k 3.12)"
pyenv install -s "$PYTHON_VERSION"
pyenv local "$PYTHON_VERSION"
python --version
pyenv which python
```

Update pyenv first if `pyenv latest -k 3.12` cannot find a current release:

```sh
pyenv update
```

Homebrew installations use `brew upgrade pyenv` instead.

Virtual Environment and Pip
---------------------------
Create an isolated environment from the Python selected by pyenv:

```sh
python -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python --version
python -m pip --version
```

There is currently no `requirements.txt` because SGL has no mandatory PyPI
dependencies. If one is added later, install it inside the active environment:

```sh
python -m pip install -r requirements.txt
```

Do not use `sudo pip` or install project packages into the distribution's
system Python. Leave the environment with:

```sh
deactivate
```

Run SGL with the Selected Python
--------------------------------
With pyenv initialized, the Makefile's default `PYTHON=python3` resolves through
the pyenv shim. With `.venv` active, it resolves through the virtual environment.

```sh
make menu
make build
make run
```

Pass the interpreter explicitly when diagnosing path selection:

```sh
make PYTHON="$(command -v python)" menu
python -B test/menuconfig/test_menuconfig.py
```

Optional LTTng Python Binding
-----------------------------
`tools/lttng-resize-report.py` imports Babeltrace 2's `bt2` module. On Ubuntu
and WSL, install the distribution packages and verify them with the system
Python:

```sh
sudo apt install -y babeltrace2 python3-bt2
/usr/bin/python3 -c "import bt2; print(bt2.__file__)"
```

Distribution Python bindings are normally not visible inside a pyenv virtual
environment. Run profiling with pyenv's system interpreter selection:

```sh
PYENV_VERSION=system make profile-lttng
```

Do not install an unrelated package from PyPI solely because it is named
`bt2`; use the Babeltrace 2 binding supplied by the operating system or build
Babeltrace 2 against the selected Python explicitly.

References
----------
- [Pyenv installation and shell setup](https://github.com/pyenv/pyenv#installation)
- [Pyenv recommended build dependencies](https://github.com/pyenv/pyenv/wiki#suggested-build-environment)
- [Python virtual environments](https://docs.python.org/3/library/venv.html)
