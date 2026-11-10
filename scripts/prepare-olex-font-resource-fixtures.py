#!/usr/bin/env python3
"""Write explicit synthetic original-font resource fixtures, never real research records."""
import argparse
import json
from pathlib import Path
from olex_font_resource_fixtures import write_fixtures
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = write_fixtures(args.output)
    print(json.dumps(dict(output=str(args.output.resolve()), files=len(result['files']), entries=len(result['entries']), target_executed=False), indent=2))
