"""Reading the two halves of the control protocol's field numbering.

The schema and the runtime each state the field number of every message
field, in their own syntax, and nothing at run time checks that they agree.
Both are read here as plain text so that the comparison needs no compiler
and no running simulation.
"""

from __future__ import annotations

import re
from pathlib import Path

PACKAGE = Path(__file__).resolve().parents[1] / "src" / "looper_rt"
SCHEMA = PACKAGE / "proto" / "looper_control.proto"
FIELDS_HEADER = PACKAGE / "runtime" / "src" / "looper_protocol_fields.h"
PROTOCOL_HEADER = PACKAGE / "runtime" / "include" / "looper_protocol.h"

MESSAGE = re.compile(r"\s*message\s+(\w+)\s*\{")
FIELD = re.compile(r"\s*(?:repeated\s+|optional\s+)?[\w.]+\s+(\w+)\s*=\s*(\d+)\s*;")
DEFINE = re.compile(r"^#define\s+(\w+)_FIELD_(\w+)\s+(\d+)u\s*$", re.MULTILINE)

#: Where the runtime spells a name differently from the schema. The C side
#: shortens what a reader of one file can infer -- a unit suffix, mostly --
#: and the numbers are what travel, not the names. Keeping the differences
#: in one list rather than in a rule means a new one is a decision: either
#: the runtime follows the schema, or the exception is written down here.
RUNTIME_SPELLING = {
    ("Bus", "bitrate_bps"): "BITRATE",
    ("Heap", "is_watched"): "WATCHED",
    ("Heap", "live_bytes"): "LIVE",
    ("Heap", "peak_bytes"): "PEAK",
    ("Heap", "untracked_blocks"): "UNTRACKED",
    ("Profile", "longest_nanoseconds"): "LONGEST",
    ("Profile", "self_nanoseconds"): "SELF",
    ("Profile", "total_nanoseconds"): "TOTAL",
    ("ProfilePath", "self_nanoseconds"): "SELF",
    ("ProfilePath", "total_nanoseconds"): "TOTAL",
    ("Response", "stack_deepest_bytes"): "STACK_BYTES",
    ("Response", "stack_deepest_depth"): "STACK_DEPTH",
    ("Response", "stack_path_complete"): "STACK_COMPLETE",
    ("Response", "watching_profile_paths"): "WATCHING_PATHS",
    ("Signal", "frequency_hz"): "FREQUENCY",
    ("Signal", "phase_degrees"): "PHASE",
    ("Timing", "answered_calls"): "ANSWERED",
    ("Timing", "longest_nanoseconds"): "LONGEST",
    ("Timing", "shortest_nanoseconds"): "SHORTEST",
    ("Timing", "total_nanoseconds"): "TOTAL",
}

#: The one message whose runtime prefix is not its name in upper snake case.
RUNTIME_PREFIX = {"JournalEntry": "JOURNAL"}


def upper_snake(camel: str) -> str:
    """Return a CamelCase name as UPPER_SNAKE_CASE."""
    return re.sub(r"(?<!^)(?=[A-Z])", "_", camel).upper()


def define_for(message: str, field: str) -> str:
    """Return the macro the runtime is expected to define for one field."""
    prefix = RUNTIME_PREFIX.get(message, upper_snake(message))
    suffix = RUNTIME_SPELLING.get((message, field), field.upper())
    return f"{prefix}_FIELD_{suffix}"


def schema_fields() -> dict[str, int]:
    """Return the field number of every message field in the schema.

    Returns:
        The expected macro name of each field, mapped to its number.
    """
    numbers: dict[str, int] = {}
    message: str | None = None
    for line in SCHEMA.read_text(encoding="utf-8").splitlines():
        opening = MESSAGE.match(line)
        if opening:
            message = opening.group(1)
        elif message is None:
            continue
        elif line.strip() == "}":
            message = None
        else:
            _collect(numbers, message, line)
    return numbers


def _collect(numbers: dict[str, int], message: str, line: str) -> None:
    """Record one field of a message, if the line declares one."""
    field = FIELD.match(line)
    if field is not None:
        numbers[define_for(message, field.group(1))] = int(field.group(2))


def runtime_fields() -> dict[str, int]:
    """Return every field number the runtime defines, by macro name."""
    header = FIELDS_HEADER.read_text(encoding="utf-8")
    return {
        f"{found.group(1)}_FIELD_{found.group(2)}": int(found.group(3))
        for found in DEFINE.finditer(header)
    }
