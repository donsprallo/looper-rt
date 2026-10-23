"""The C runtime a looper simulation is built from.

This package holds no Python beyond this file. It exists so that the C
sources, the headers and the control protocol schema travel as an
installable distribution, which is what lets the Python tooling find them
the same way in a checkout and in an installed wheel.

A project that builds with CMake does not need this package at all and
brings the repository in with ``FetchContent`` instead.
"""

from __future__ import annotations

from pathlib import Path

#: Directory holding this package, which is where the C sources live.
ROOT = Path(__file__).parent

__version__ = "0.1.0"
