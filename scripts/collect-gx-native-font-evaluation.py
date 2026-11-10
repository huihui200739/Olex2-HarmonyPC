#!/usr/bin/env python3
"""Collect the exact candidate19 full UI PID set; preserve all resource controls."""
from native_font_tool_profiles import collection_source,execute_profile
if __name__=='__main__':
    execute_profile(collection_source(),'collect-gx-texture-context-evaluation.py')
