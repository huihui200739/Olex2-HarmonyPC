"""Isolated synthetic DPI/profile tests, not application or screen observations."""
import ast,copy,hashlib,json,math
from pathlib import Path
import sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from font_display_reference import validate_metrics,expected_parameters,validate_case,validate_snapshot,float32
from font_display_source_validation import FILES27,validate_source27
import font_display_window_validation as profile
import font_family_window_validation as old_profile

class FontDisplayProfileTests(unittest.TestCase):
 def metrics(self):return dict(ready=True,display_id=7,revision=3,dpi_x=144,dpi_y=192,density_pixels=2,source='actual-main-window-display-xDPI-yDPI',fixed_dpi_fallback=False)
 def case(self):return dict(point_size=18,display_metrics=self.metrics(),raster_pixel_size=48.0,raster_scale_x=.75)
 def test_float32_actual_dpi_reference_and_density_not_used_as_dpi(self):
  value=self.metrics();self.assertEqual(expected_parameters(18,value),dict(pixel_size=48,scale_x=.75));value['density_pixels']=3
  self.assertEqual(expected_parameters(18,value),dict(pixel_size=48,scale_x=.75))
 def test_case_binds_current_actual_display(self):self.assertEqual(validate_case(self.case(),self.metrics()),dict(pixel_size=48,scale_x=.75))
 def test_wrong_metrics_or_default96_parameters_rejected(self):
  for patch in [dict(raster_pixel_size=24),dict(raster_scale_x=1),dict(display_metrics={**self.metrics(),'revision':4})]:
   with self.assertRaises(ValueError):validate_case({**self.case(),**patch},self.metrics())
 def test_missing_nonfinite_or_false_source_metrics_rejected(self):
  for patch in [dict(ready=False),dict(display_id=-1),dict(display_id=.5),dict(revision=0),dict(dpi_y=0),dict(dpi_x=math.nan),dict(dpi_y=math.inf),dict(density_pixels=0),dict(fixed_dpi_fallback=True),dict(source='default-display'),dict(dpi_x=1,dpi_y=8192)]:
   with self.assertRaises(ValueError):validate_metrics({**self.metrics(),**patch})
 def test_boolean_ids_or_dpi_are_not_real_numbers(self):
  for key in ('display_id','revision','dpi_x','dpi_y','density_pixels'):
   with self.assertRaises(ValueError):validate_metrics({**self.metrics(),key:True})
 def test_float32_serialized_decimal_roundtrip_is_exact(self):
  value=self.metrics();value['dpi_y']=112;value['dpi_x']=117;size=expected_parameters(27,value)
  case=dict(point_size=27,display_metrics=value,raster_pixel_size=float(format(size['pixel_size'],'.9g')),raster_scale_x=float(format(size['scale_x'],'.9g')))
  self.assertEqual(validate_case(case,value),size)
 def test_next_float32_parameter_is_not_tolerated(self):
  case=self.case();case['raster_pixel_size']=float32(48.00001)
  with self.assertRaises(ValueError):validate_case(case,self.metrics())
 def test_snapshot_before_after_bind_every_parameter(self):
  native=dict(ready=True,displayId=7,revision=3,dpiX=144,dpiY=192,densityPixels=2,fixedDpiFallback=False);snapshot=dict(graphics=dict(fontDisplay=native))
  self.assertEqual(validate_snapshot(self.metrics(),snapshot,copy.deepcopy(snapshot)),native)
  after=copy.deepcopy(snapshot);after['graphics']['fontDisplay']['displayId']=8
  with self.assertRaises(ValueError):validate_snapshot(self.metrics(),snapshot,after)
 def test_unobserved_or_missing_ui_snapshot_rejected(self):
  with self.assertRaises(ValueError):validate_snapshot(self.metrics(),{},dict(graphics={}))
 def test_exact28_does_not_mutate26_identity(self):
  self.assertEqual(profile.IDENTITY['versionName'],'0.22.28');self.assertEqual(old_profile.IDENTITY['versionName'],'0.22.26')
  self.assertEqual(old_profile._resource['SOURCE_MANIFESTS'][-1],'font-family-inputs.json')
  self.assertEqual(profile._namespace['_resource']['SOURCE_MANIFESTS'][-2:],('font-display-inputs.json','font-display-telemetry-inputs.json'))
 def test_old_twenty_request_profile_is_exactly_preserved(self):
  self.assertEqual(profile._namespace['_native_execution_source'],old_profile._native_execution_source.replace("versionName='0.22.26',versionCode=222600","versionName='0.22.28',versionCode=222800"))
 def test_new_runner_compiles_and_retains_strict_install_guard(self):
  path=ROOT/'scripts/run-font-display-window-smoke.py';text=path.read_text();ast.parse(text)
  self.assertIn("validate_install_output(text,run.returncode)",text)
  self.assertIn("validate_snapshot(checked['display_metrics'],before_state,after_state)",text)
  self.assertIn('range(1,4)',text)
 def test_source_model_matches_exact_frozen27_header_and_controller(self):
  pairs=[('experiments/gx-font-display/native/display_font_metrics.h','native/display_font_metrics.h'),('experiments/gx-font-display/model/FontDisplayMetrics.ts','app/entry/src/main/ets/model/FontDisplayMetrics.ts'),('experiments/gx-font-display/model/MainWindowFontMetrics.ets','app/entry/src/main/ets/model/MainWindowFontMetrics.ets')]
  for name,rel in pairs:self.assertEqual((ROOT/name).read_bytes(),(ROOT/'out/gx-application-evaluation/source27'/rel).read_bytes())
 def test_synthetic_overlay_integrity_and_file_set_are_strict(self):
  with tempfile.TemporaryDirectory(prefix='isolated-synthetic-dpi-source-') as directory:
   root=Path(directory);underlay=b'{"scope":"isolated synthetic fixture"}';(root/'font-family-inputs.json').write_bytes(underlay)
   for name in FILES27:
    p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('isolated synthetic overlay')
   app=root/'app/AppScope/app.json5';app.write_text(json.dumps(dict(app=dict(bundleName='com.example.olex2harmonypc',versionName='0.22.27',versionCode=222700))))
   files={name:dict(bytes=(root/name).stat().st_size,sha256=hashlib.sha256((root/name).read_bytes()).hexdigest()) for name in FILES27}
   meta=dict(version='0.22.27',base='0.22.26',original_font_requests_per_action=20,base_manifest_sha256=hashlib.sha256(underlay).hexdigest(),files=files)
   p=root/'font-display-inputs.json';p.write_text(json.dumps(meta));self.assertEqual(validate_source27(root),root.resolve())
   changed=root/'native/display_font_metrics.h';changed.write_text('changed synthetic header')
   with self.assertRaises(ValueError):validate_source27(root)
   changed.write_text('isolated synthetic overlay');meta['files'].pop('native/display_font_metrics.h');p.write_text(json.dumps(meta))
   with self.assertRaises(ValueError):validate_source27(root)

if __name__=='__main__':unittest.main()
