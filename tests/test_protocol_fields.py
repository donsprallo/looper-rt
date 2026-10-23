"""The schema and the runtime have to number the fields the same way.

A protobuf message carries field numbers, not field names, so the number
is the whole of the agreement. The Python side gets its numbers generated
from `proto/looper_control.proto`; the C side gets them from a header
written by hand, and the two meet only on the wire.

Nothing at run time notices when they drift apart. A number that moved is
not a parse error: it is a value read into the wrong field, which is why
this is checked as text rather than left to the integration test, where
only the fields that test happens to exercise would say anything.
"""

from __future__ import annotations

from protocol_schema import runtime_fields, schema_fields


def test_every_field_in_the_schema_is_numbered_the_same_in_the_runtime() -> None:
    """The one that matters: a moved number silently misreads a message."""
    schema, runtime = schema_fields(), runtime_fields()

    differing = {
        name: (number, runtime[name])
        for name, number in schema.items()
        if name in runtime and runtime[name] != number
    }

    assert differing == {}, f"schema and runtime disagree: {differing}"


def test_the_runtime_defines_every_field_the_schema_declares() -> None:
    """A field the runtime never learned about is one it cannot answer with."""
    missing = sorted(set(schema_fields()) - set(runtime_fields()))

    assert missing == [], f"the runtime defines no number for {missing}"


def test_the_runtime_defines_nothing_the_schema_does_not() -> None:
    """Otherwise a field outlives its removal and the next one reuses its number."""
    left_over = sorted(set(runtime_fields()) - set(schema_fields()))

    assert left_over == [], f"{left_over} are not in the schema"


def test_the_version_field_is_numbered_where_the_schema_puts_it() -> None:
    """It is the one field a client reads before it trusts any other."""
    assert runtime_fields()["RESPONSE_FIELD_PROTOCOL_VERSION"] == 31
