#!/usr/bin/env python3
"""Collect the exact candidate15 full UI PID set without changing historical tools."""
from texture_load_tool_profiles import collection_source,execute_profile
if __name__=='__main__':
    execute_profile(collection_source(),'collect-gx-texture-context-evaluation.py')
