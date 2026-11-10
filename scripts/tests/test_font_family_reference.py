"""Isolated synthetic family name/style/format tests, not system font observations."""
import copy
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from font_family_reference import description,parse_description,validate_catalog,probe_selection

class FontFamilyReferenceTests(unittest.TestCase):
    def style(self,index=0,label='Synthetic Regular',weight=3):
        return dict(index=index,name=label,weight_code=weight,width_code=5,slant_code=0)
    def catalog(self):
        return dict(source='OH_Drawing_FontMgr',closest_match_used=False,weight_codes_are_SDK_raw_values=True,
            families=[dict(index=0,name='Synthetic Sans',styles=[self.style(),self.style(1,'Synthetic Bold',6)]),
                      dict(index=1,name='Synthetic Serif',styles=[self.style()])])
    def test_roundtrip_utf8_quotes_backslash_colon_and_empty_style(self):
        for family in ('Synthetic Sans','Synthetic " : \\ name','合成字体','𝄞 Family'):
            for label in ('','Synthetic Regular','合成粗体'):
                for points in (1,18,128):
                    for fixed in (False,True):
                        style=self.style(label=label);text=description(family,style,points,fixed);parsed=parse_description(text)
                        self.assertEqual(parsed,dict(family=family,style={k:style[k] for k in ('name','weight_code','width_code','slant_code')},points=points,fixed=fixed))
    def test_raw_sdk_weight_codes_are_not_guessed_css_values(self):
        for code in (0,3,6,8,400,700,1000):
            self.assertEqual(parse_description(description('Synthetic Sans',self.style(weight=code)))['style']['weight_code'],code)
    def test_invalid_utf8_control_empty_or_oversized_name_rejected(self):
        for family in ('','a\0b','a\nb','\ud800','a'*513):
            with self.assertRaises(ValueError):description(family,self.style())
    def test_noncanonical_numbers_hex_and_versions_rejected(self):
        for value in ('ohos-family:v1:018:3:5:0:41:','ohos-family:v1:18:3:5:0:4A:',
                      'ohos-family:v1:18:3:5:0:4:','ohos-family:v2:18:3:5:0:41:',
                      'ohos-family:v1:18:3:5:0:00:','ohos-family:v1:18:3:5:0:eda080:'):
            with self.assertRaises(ValueError):parse_description(value)
    def test_out_of_range_or_boolean_codes_rejected(self):
        for key,value in (('weight_code',1001),('weight_code',True),('width_code',0),('width_code',10),('slant_code',3)):
            style=self.style();style[key]=value
            with self.assertRaises(ValueError):description('Synthetic Sans',style)
    def test_synthetic_catalog_keeps_all_actual_style_labels_and_order(self):
        cat=self.catalog();self.assertEqual(validate_catalog(cat),cat['families'])
        selected=probe_selection(cat)
        self.assertEqual([f['index'] for f,s in selected],[0,0,0,1,0])
        self.assertEqual([s['index'] for f,s in selected],[0,0,0,0,1])
    def test_misreported_or_noninteger_index_rejected(self):
        for kind in ('family','style'):
            cat=self.catalog()
            (cat['families'][0] if kind=='family' else cat['families'][0]['styles'][0])['index']=True
            with self.assertRaises(ValueError):validate_catalog(cat)
    def test_fallback_claim_is_rejected(self):
        cat=self.catalog();cat['closest_match_used']=True
        with self.assertRaises(ValueError):validate_catalog(cat)
    def test_insufficient_real_families_or_styles_not_replaced_by_defaults(self):
        cat=self.catalog();cat['families'].pop()
        with self.assertRaises(ValueError):probe_selection(cat)
        cat=self.catalog();cat['families'][0]['styles'].pop()
        with self.assertRaises(ValueError):probe_selection(cat)
    def test_catalog_is_not_mutated_by_planning(self):
        cat=self.catalog();before=copy.deepcopy(cat);probe_selection(cat);self.assertEqual(cat,before)

class FontFamilyExactProfileTests(unittest.TestCase):
    def test_new_identity_does_not_mutate_frozen25(self):
        import font_raster_flush_window_validation as old
        import font_family_window_validation as new
        self.assertEqual(old.IDENTITY['versionName'],'0.22.25');self.assertEqual(new.IDENTITY['versionName'],'0.22.26')
        self.assertNotIn('font-family-inputs.json',old._namespace['_namespace']['SOURCE_MANIFESTS'])
    def test_only_native_request_gate_changes_from_fifteen_to_twenty(self):
        import font_family_window_validation as new
        old=new._vector['text'];before="legacy['font_requests']==run*15";after="legacy['font_requests']==run*20"
        self.assertEqual(old.count(before),1)
        self.assertEqual(new._native['IDENTITY'],new.IDENTITY)
        self.assertEqual(new._native_execution_source,old.replace(before,after,1))
        import dis
        self.assertTrue(any(i.argval==20 and i.opname in ('LOAD_CONST','LOAD_SMALL_INT')
            for i in dis.get_instructions(new._native['validate'])))
    def test_native_pixel_reference_remains_exact_original_float32_fmla(self):
        import font_family_window_validation as new
        from native_font_raster_window_validation import expected_frame_with_raster
        self.assertIs(new._native['expected_frame'],expected_frame_with_raster)
    def test_new_source_overlay_is_last_without_changing_underlay(self):
        import font_family_window_validation as new
        self.assertEqual(new._resource['SOURCE_MANIFESTS'][-2:],('font-raster-flush-inputs.json','font-family-inputs.json'))
