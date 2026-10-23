"""Sphinx configuration for the looper-rt documentation."""

from __future__ import annotations

import os

project = "looper-rt"
release = "0.1.0"

# No attribution in the footer: the build carries no author or copyright.
html_show_copyright = False

extensions = ["sphinx.ext.extlinks", "myst_parser"]

# Let cross document links point at a heading.
myst_heading_anchors = 3

# The tooling documents itself in a build of its own. This is the one place
# that says where that build is published; set LOOPER_CORE_DOCS to point
# somewhere else. The default assumes the two are deployed side by side.
CORE_DOCS = os.environ.get("LOOPER_CORE_DOCS", "../looper-core")
extlinks = {"core": (f"{CORE_DOCS}/%s", "%s")}
extlinks_detect_hardcoded_links = True

nitpicky = True

html_theme = "furo"
html_title = "looper-rt"
exclude_patterns = ["_build"]
