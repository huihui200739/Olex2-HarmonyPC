"""Isolated synthetic execution of the actual UI-injection function AST.

No hdc, UI, files, real application records, SDK or input-method settings are
modified. These are control-flow regressions, not actual keyboard/IME proof.
"""
import ast
from pathlib import Path
import shlex
import unittest

SOURCE=Path(__file__).resolve().parents[1]/'run-app-smoke.py'

class InputHarness:
    def __init__(self,expected,committed,fail_latin=False):
        self.expected=expected;self.committed=committed;self.fail_latin=fail_latin
        self.text='';self.events=[];self.logs={};self.summary={};self.shifts=0
        tree=ast.parse(SOURCE.read_text())
        function=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='input_literal')
        namespace=dict(click=self.click,layout=self.layout,bounds=lambda n:(0,0,100,20),find=lambda nodes,**kw:nodes[0],shell=self.shell,save=lambda name,text:self.logs.update({name:text}),summary=self.summary,shlex=shlex)
        exec(compile(ast.fix_missing_locations(ast.Module(body=[function],type_ignores=[])),str(SOURCE),'exec'),namespace)
        self.inject=namespace['input_literal']
    def click(self,nodes,**expected):
        self.events.append(('click',expected['id']))
        if expected['id']=='olex2-command-clear':self.text=''
    def layout(self,name):
        if name.endswith('-uinput-segmented-committed'):self.text=self.committed[3] if len(self.committed)>3 else self.committed[2]
        elif name.endswith('-uinput-committed'):self.text=self.committed[2]
        elif name.endswith('-committed-latin'):self.text=self.committed[1]
        elif name.endswith('-committed'):self.text=self.committed[0]
        return [dict(text=self.text,originalText=self.text)]
    def shell(self,command):
        self.events.append(('shell',command));parts=shlex.split(command)
        if command=='uitest uiInput keyEvent 2047':self.shifts+=1
        elif command=='uitest uiInput keyEvent 2050':self.text+=' '
        elif parts[:3]==['uitest','uiInput','text']:
            if self.fail_latin and self.shifts==1:raise RuntimeError('isolated synthetic Latin token failure')
            self.text+=parts[3]
        elif parts[:3]==['uinput','-K','-t']:self.text+=parts[3]
        return 'synthetic input event acknowledged'
    def run(self):return self.inject('synthetic',self.expected,'observations')
    def observation(self):return self.summary['observations']['synthetic']

class LiteralUiInjectionTests(unittest.TestCase):
    def test_primary_exact_ui_input(self):
        h=InputHarness('pc.detail 2',['pc.detail 2',None,None]);h.run()
        self.assertEqual(h.shifts,0);self.assertFalse(h.observation()['uinput_text_fallback_used']);self.assertEqual(h.observation()['actual_committed_text'],'pc.detail 2')
    def test_latin_retry_sends_restore_event(self):
        h=InputHarness('pc.detail 2',['pc.detail2','pc.detail 2',None]);h.run()
        self.assertEqual(h.shifts,2);self.assertTrue(h.observation()['typing_mode_restore_event_sent']);self.assertFalse(h.observation()['uinput_text_fallback_used'])
    def test_fullwidth_punctuation_uses_actual_text_route(self):
        expected='pc.reset >> pc.detail 9';h=InputHarness(expected,['pc.reset 》》 pc.detail 9','pc.reset 》》 pc.detail 9',expected]);h.run()
        self.assertEqual(h.shifts,2);self.assertTrue(h.observation()['uinput_text_fallback_used']);self.assertEqual(h.observation()['actual_committed_text'],expected)
        self.assertIn('synthetic-uinput-fallback.log',h.logs)
    def test_inexact_last_route_still_fails_before_execution(self):
        h=InputHarness('pc.reset >> pc.detail 9',['wrong','wrong','pc.reset 》》 pc.detail 9'])
        with self.assertRaisesRegex(RuntimeError,'exact literal command'):h.run()
        self.assertNotIn('observations',h.summary);self.assertEqual(h.shifts,2)
        self.assertNotEqual(h.summary['literal_input_attempts']['synthetic']['actual_committed_text'],h.expected)
    def test_latin_injection_exception_still_sends_restore(self):
        h=InputHarness('pc.detail 2',['pc.detail2',None,None],fail_latin=True)
        with self.assertRaisesRegex(RuntimeError,'isolated synthetic Latin'):h.run()
        self.assertEqual(h.shifts,2);self.assertIn('synthetic-typing-mode-restored.log',h.logs)
    def test_no_whitespace_normalization(self):
        h=InputHarness('pc.detail  2',['pc.detail 2','pc.detail 2','pc.detail 2'])
        with self.assertRaisesRegex(RuntimeError,'exact literal command'):h.run()
    def test_fallback_keeps_quotes_in_single_text_argument(self):
        expected='app.SetOption(ui.note,"two words")';h=InputHarness(expected,['wrong','wrong',expected]);h.run()
        commands=[c for kind,c in h.events if kind=='shell' and c.startswith('uinput ')]
        self.assertEqual(len(commands),1);self.assertEqual(shlex.split(commands[0]),['uinput','-K','-t',expected])
    def test_segmented_actual_keyboard_retry_still_requires_exact_final_text(self):
        expected='pc.reset >> pc.detail 9'
        h=InputHarness(expected,['wrong','wrong','pc.reset>>  pc.detail 9',expected]);h.run()
        self.assertTrue(h.observation()['segmented_uinput_fallback_used'])
        commands=[value for kind,value in h.events if kind=='shell' and value.startswith('uinput -K -t ')]
        self.assertEqual([shlex.split(c)[3] for c in commands[1:]],['pc.reset','>>','pc.detail','9'])
        self.assertEqual(h.observation()['actual_committed_text'],expected)
        self.assertIn('synthetic-uinput-segmented-fallback.log',h.logs)
    def test_segmented_retry_never_accepts_normalized_whitespace_or_punctuation(self):
        expected='pc.reset >> pc.detail 9'
        for wrong in ['pc.reset>>  pc.detail 9','pc.reset 》》 pc.detail 9','pc.reset >> pc.detail9']:
            with self.subTest(wrong=wrong):
                h=InputHarness(expected,['wrong','wrong','wrong',wrong])
                with self.assertRaisesRegex(RuntimeError,'exact literal command'):h.run()
                self.assertNotIn('observations',h.summary)
    def test_only_input_controls_no_native_execution_or_settings(self):
        expected='pc.reset >> pc.detail 9';h=InputHarness(expected,['wrong','wrong',expected]);h.run()
        self.assertTrue(all(value in ('olex2-command-clear','olex2-command-input') for kind,value in h.events if kind=='click'))
        self.assertTrue(all(value.startswith(('uitest uiInput keyEvent ','uitest uiInput text ','uinput -K -t ')) for kind,value in h.events if kind=='shell'))
        self.assertEqual(sum(value=='olex2-command-clear' for kind,value in h.events if kind=='click'),3)

if __name__=='__main__':unittest.main()
