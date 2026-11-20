"""Compatibility alias for the expanded Test 4 all-in-template-price test."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name("test_template_full_total_host.py")), run_name="__main__")
