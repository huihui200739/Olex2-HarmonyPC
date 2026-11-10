"""Preserve every original UI/window check and add six independent 1D proofs."""
import copy,importlib.util,json
from pathlib import Path
from original_renderer_integration_validation import ROUNDTRIPS,summarize_application
from texture_1d_window_validation import validate as validate_texture
ROOT=Path(__file__).resolve().parents[1]
ADDED={'shared_texture_1d_'+label for label in ROUNDTRIPS}
def validate_application(app,root,identity):
 root=Path(root).resolve()
 if identity.get('versionName')!='0.22.11' or identity.get('versionCode')!=221100:raise ValueError('Only the exact current-window 1D candidate is accepted')
 checks=app['checks'];names=[c['name'] for c in checks]
 if len(names)!=200 or len(set(names))!=200 or not ADDED<=set(names) or not all(c['passed'] is True for c in checks):raise ValueError('Expected exactly the original 194 checks plus six successful 1D rounds')
 original=copy.deepcopy(app);original['checks']=[c for c in checks if c['name'] not in ADDED]
 summary=summarize_application(original,root,identity) # Original 187 order + 7 original window gates, unchanged.
 path=root/'scripts/original_renderer_validation.py';baseline=(ROOT/'scripts/original_renderer_validation.py').read_bytes()
 if baseline.count(b'len(adapted)==2')!=1 or path.read_bytes()!=baseline.replace(b'len(adapted)==2',b'len(adapted)==6'):
  raise ValueError('Candidate original-window verifier must differ only by its precise three shader pairs')
 spec=importlib.util.spec_from_file_location('texture_1d_original_window_verifier',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 report=(root/app['report_directory']).resolve()
 if not report.is_relative_to(root/'reports/app'):raise ValueError('UI report escaped its exact candidate')
 by_name={c['name']:c for c in checks};rounds=[]
 for label in ROUNDTRIPS:
  prefix='shared-'+label;folder=report/'runtime'/(prefix+'-original')
  actual=module.validate(folder,report/'runtime'/(prefix+'-before'),report/'runtime'/(prefix+'-after'),identity,create_previews=False)
  actual=json.loads(json.dumps(actual,ensure_ascii=False,allow_nan=False))
  if not actual['passed'] or len(actual['checks'])!=126 or actual!=by_name['shared_original_'+label]['details']:raise ValueError('Original 126-check evidence changed or failed: '+label)
  texture=validate_texture(folder)
  if not texture['passed'] or len(texture['checks'])!=64 or len({c['name'] for c in texture['checks']})!=64 or texture!=by_name['shared_texture_1d_'+label]['details']:raise ValueError('Actual 64-check texture evidence changed or failed: '+label)
  rounds.append(dict(label=label,pid=actual['pid'],run=actual['run'],checks=64))
 summary.update(original_194_ui_checks_preserved=True,total_ui_checks=200,total_independent_texture_checks=64*6,texture_roundtrips=rounds,actual_context_migration_verified=False,scope='Original 194 diagnostic UI gates including six times 126 original-window checks, plus six times 64 real current-window synthetic 1D checks; not persistent GX owner/context or full UI/renderer qualification')
 return summary
