#!/usr/bin/env python3
"""Write separate8/18/27/72 synthetic resources; retain22/23 fixture bytes."""
import argparse,json
from pathlib import Path
from olex_font_resample_fixtures import write_fixtures
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=write_fixtures(a.output);print(json.dumps(dict(files=len(d['files']),entries=len(d['entries']),output=str(a.output.resolve()),target_executed=False),indent=2))
