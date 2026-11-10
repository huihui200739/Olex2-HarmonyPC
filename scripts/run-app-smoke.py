#!/usr/bin/env python3
"""Install/test only the Olex2 diagnostic app on an explicitly selected emulator."""
import argparse
import sys
from layout_validation import controls_unclipped
from app_report_log import parse_app_reports
from matrix_validation import original_matrices_match
from desktop_command_validation import command_sample_matches,command_recovery_matches
from application_runtime_validation import SPECS as APPLICATION_FUNCTION_SPECS, runtime_matches, function_sample_matches, recovery_matches as application_recovery_matches
from line_path_validation import path_probe_matches
from desktop_shell_validation import identity_matches,version_visible_matches,labels_match,enter_matches
from original_log_validation import parse_catalog_log,empty_audit_matches,catalog_sample_matches,visible_catalog_matches,cleared_sample_matches,restart_sample_matches
from line_validation import original_line_upload_matches
from polygon_validation import polygon_probe_matches,original_polygon_upload_matches
from build_provenance import verify as verify_build_provenance, EMBEDDED
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import zipfile
import subprocess
import time
import uuid
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
BUNDLE = 'com.example.olex2harmonypc'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--target', required=True)
p.add_argument('--hdc', default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc')
p.add_argument('--hap', type=Path, default=ROOT / 'out/app/entry-default-unsigned.hap')
a = p.parse_args()
now = datetime.now(ZoneInfo('Asia/Shanghai'))
run_id = now.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:6]
report_dir = ROOT / 'reports/app' / run_id
report_dir.mkdir(parents=True)
summary = {'started_at': now.isoformat(timespec='seconds'), 'target': a.target,
           'bundle': BUNDLE, 'scope': 'Utility code, actual upstream sphere/torus/disk/rim helpers, custom GLES3 adapter, injected desktop input and local view/color-state persistence and original color-value parsing; not complete original renderer',
           'full_port_complete': False, 'original_olex2_renderer_tested': False,
           'physical_pc_verified': False, 'checks': []}
remote = '/data/local/tmp/olex2-app-' + uuid.uuid4().hex
pid = ''
remote_created = False
log_index = 0

def hdc(*args, timeout=40):
    r = subprocess.run([a.hdc, '-t', a.target, *args], capture_output=True, text=True, timeout=timeout)
    if r.returncode:
        raise RuntimeError(r.stdout + r.stderr)
    return r.stdout + r.stderr

def shell(command, timeout=40):
    marker = 'OLEX2_APP_EXIT_' + uuid.uuid4().hex
    out = hdc('shell', f'( {command} ); result=$?; printf "\\n{marker}=%s\\n" "$result"', timeout=timeout)
    match = re.search(re.escape(marker) + r'=(\d+)', out)
    if not match or match[1] != '0':
        raise RuntimeError(out)
    return out[:match.start()].rstrip()

def save(name, content):
    (report_dir / name).write_text(content, encoding='utf-8')

def record(name, condition, details):
    summary['checks'].append({'name': name, 'passed': bool(condition), 'details': details})
    print(name + ': ' + ('PASS' if condition else 'FAIL'), flush=True)
    if not condition:
        raise RuntimeError(name + ' did not pass')

def logs():
    global log_index
    out = shell(f'hilog -x -P {pid} -T Olex2Native')
    log_index += 1
    save(f'runtime-{log_index:02d}.log', out)
    return out

def reports(out):
    return parse_app_reports(out)

def wait_report(predicate, seconds=12):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        current = reports(logs())
        if current and predicate(current[-1]):
            return current[-1]
        time.sleep(.6)
    raise RuntimeError('No matching actual application report within timeout')

PALETTE = [(0.20,0.80,0.59,1), (0.90,0.38,0.16,1), (0.22,0.52,0.95,1)]
COLOR_SOURCE = 'upstream/glib/gloption.cpp: TGlOption::FromString/GetRGB (unchanged implementation)'

def color_pixel_matches(g):
    preset = g.get('colorPreset', -1)
    if preset not in range(3):
        return False
    rgb = [v*255 for v in PALETTE[preset][:3]]
    pixel = [g.get('readback'+c, -1) for c in 'RGB']
    scale = sum(a*b for a,b in zip(rgb,pixel))/sum(v*v for v in rgb)
    return (g.get('colorVerified') is True and g.get('geometryColorSamples',0)>0 and
            g.get('readbackA') == 255 and .29 <= scale <= 1.02 and
            all(abs(p-c*scale)<=3 for p,c in zip(pixel,rgb)))

def color_value_matches(g):
    preset = g.get('colorPreset', -1)
    if preset not in range(3):
        return False
    return (g.get('colorSource') == COLOR_SOURCE and
            all(abs(g.get('color'+c,-1)-v)<.00001 for c,v in zip('RGBA',PALETTE[preset])) and
            isinstance(g.get('colorRevision'), int) and g['colorRevision']>0)

MATERIAL_SOURCE = 'upstream/glib/glmaterial.cpp: TGlMaterial value methods (legacy Init excluded)'
MATERIAL_COMMANDS_SOURCE = 'upstream/glib/materialcommands.h: original TGlMaterial::Init command flow'
MATERIAL_FACTORS = [(.3,.7,0),(.6,.4,0),(0,0,1),(.3,.4,0)]

def material_values_match(g):
    preset = g.get('materialPreset',-1)
    if preset not in range(4) or g.get('colorPreset',-1) not in range(3): return False
    return (g.get('materialSource') == MATERIAL_SOURCE and g.get('materialFlags') == (1023 if preset==3 else 783) and
            g.get('materialCommandCount') == 12 and g.get('materialCommandsSource') == MATERIAL_COMMANDS_SOURCE and
            g.get('materialRevision',0)>0 and bool(g.get('materialCanonical')) and g.get('shininess')==(32 if preset==3 else 0) and
            all(abs(g.get('specular'+c,-1)-base*(.3 if preset==3 else 0))<.00001 for c,base in zip('RGB',PALETTE[g['colorPreset']][:3])) and
            all(abs(g.get(name+c,-1)-base*factor)<.00001
                for name,factor in zip(['ambient','diffuse','emission'],MATERIAL_FACTORS[preset])
                for c,base in zip('RGB',PALETTE[g['colorPreset']][:3])))

def material_pixels_match(g):
    preset=g.get('materialPreset',-1)
    return (preset in range(4) and g.get('materialVerified') is True and color_pixel_matches(g) and
            [.29,.59,.99,.29][preset]<=g.get('rowMinScale',-1)<=g.get('rowMeanScale',-1)<=g.get('rowMaxScale',-1)<=1.02)

def glossy_pixels_match(g, require_highlight=False):
    if not (g.get('materialPreset')==3 and material_values_match(g) and material_pixels_match(g) and
            g.get('specularReferenceVerified') is True and g.get('specularComparedPixels',0)>=4 and
            0<=g.get('specularMaxError',999)<=3 and len(g.get('specularSamples',[]))==4): return False
    if require_highlight and not (g.get('specularChangedPixels',0)>=2 and g.get('specularMaxDelta',0)>2): return False
    prior=-1
    for sample in g['specularSamples']:
        if len(sample)!=9: return False
        x,diffuse,specular,*pixels=sample
        if not (isinstance(x,int) and prior<x<g['width'] and 0<=diffuse<=1.00001 and 0<=specular<=1.00001): return False
        prior=x
        actual,baseline=pixels[:3],pixels[3:]
        for c,p,b in zip('RGB',actual,baseline):
            expected_base=255*(g['ambient'+c]+g['diffuse'+c]*diffuse+g['emission'+c])
            expected=expected_base+255*g['specular'+c]*specular
            if not (isinstance(p,int) and isinstance(b,int) and 0<=b<=p+1<=256 and abs(p-expected)<=3.01 and abs(b-expected_base)<=3.01): return False
    return True

def lighting_response(samples):
    classic,soft,flat=samples
    same_view=['meshKind','meshGeneration','wireframe','meshVertices','meshIndices','meshRevision','colorPreset','width','height','yaw','pitch','distance']
    return (all(material_pixels_match(g) for g in samples) and
            all(all(g[k]==classic[k] for k in same_view) for g in samples) and
            all(g['geometryColorSamples']==classic['geometryColorSamples'] for g in samples) and
            abs(soft['rowMeanScale']-(classic['rowMeanScale']*4/7+3/7))<.01 and
            soft['rowMeanScale']>classic['rowMeanScale']+.01 and
            abs(flat['rowMeanScale']-1)<.01 and flat['rowMaxScale']-flat['rowMinScale']<.01)

def original_triangle_upload_matches(g):
    count=g.get('submittedVertices',0)
    return (g.get('originalTriangleVerified') is True and count>0 and count%3==0 and
            g.get('originalTriangleVertices')==g.get('originalTriangleNormalCommands')==count and
            g.get('originalTriangleColorCommands')==0 and
            g.get('originalTriangleSource')=='upstream/glib/trianglesubmit.h: original TGlPrimitive::Draw sgloTriangles branch')

def good(r):
    g = r['graphics']
    generation = g.get('meshGeneration', -1)
    if generation not in range(6):
        return False
    kind = g.get('meshKind')
    t = 8 * (generation + 1)
    if kind == 'sphere':
        faces, vertices, batches, tails, degenerates = 8 * 4 ** generation, 8 * 4 ** generation - 2, 1, 0, 0
        source = 'upstream/sdl/esphere.h: OlxSphere<float, OctahedronFP<vec3f>>::Generate'
        submission = 'upstream/glib/spheresubmit.h: SphereTriangleSubmission::Render (shared with GlSphereEx)'
    elif kind in ['torus', 'disks', 'rims']:
        faces = t*t if kind == 'torus' else 6*(t-2) if kind == 'disks' else 6*t+2
        vertices = t*(t+1) if kind == 'torus' else 6*(t+1) if kind == 'disks' else 12*t+8
        batches, tails, degenerates = (t//2, 0, 0) if kind == 'torus' else (6, 0, 6) if kind == 'disks' else (3, 4, 0)
        method = {'torus': 'GlTorus::Render extracted loop', 'disks': 'GlSphereEx::RenderDisks extracted loops', 'rims': 'GlSphereEx::RenderRims extracted loops'}[kind]
        source = 'upstream/glib/primitivesubmit.h: ' + method
        submission = source
    else:
        return False
    wire_indices = faces*6 if kind == 'sphere' else 12*t if kind == 'disks' else faces*4
    mesh_ok = (g.get('meshValidated', False) and g.get('coverageVerified', False)
               and g.get('meshVertices') == vertices and g.get('meshTriangles') == faces
               and g.get('meshIndices') == (wire_indices if g.get('wireframe') else faces*3)
               and g.get('meshSource') == source and g.get('submissionSource') == submission
               and g.get('sourceBatches') == batches and g.get('discardedTailVertices') == tails
               and g.get('degenerateTrianglesSkipped') == degenerates
               and (g.get('wireframe') or g.get('geometryColorSamples', 0) > 0)
               and r.get('upstreamGeometryTested', False)
               and g.get('submissionValidated', False) and r.get('upstreamSubmissionTested', False)
               and g.get('submittedVertices') == faces*3)
    return (mesh_ok and original_line_upload_matches(g) and polygon_probe_matches(g) and original_polygon_upload_matches(g) and original_matrices_match(g) and original_triangle_upload_matches(g) and material_values_match(g) and (g.get('wireframe') or material_pixels_match(g)) and color_value_matches(g) and (g.get('wireframe') or color_pixel_matches(g)) and r['utilityPassed'] and r['checkCount'] == 10 and len(r.get('checks', [])) == 10
            and all(c['passed'] for c in r['checks']) and g['surfaceReady'] and g['initialized']
            and g['geometryVerified'] and g['pixelVerified'] and g['frames'] > 0
            and not g['error'] and not r['fullPortComplete'] and not r['originalOlex2RendererTested'])

def attributes(tree):
    if isinstance(tree, dict):
        if 'attributes' in tree:
            yield tree['attributes']
        for v in tree.values():
            yield from attributes(v)
    elif isinstance(tree, list):
        for v in tree:
            yield from attributes(v)

def layout(name, bundle=BUNDLE):
    selected = f'-b {bundle} ' if bundle else ''
    shell(f'uitest dumpLayout {selected}-p {remote}/layout.json')
    hdc('file', 'recv', remote + '/layout.json', str(report_dir / (name + '.json')))
    return list(attributes(json.loads((report_dir / (name + '.json')).read_text())))

def bounds(node):
    numbers = list(map(int, re.findall(r'-?\d+', node['bounds'])))
    if len(numbers) != 4:
        raise RuntimeError('Invalid observed node bounds')
    return numbers

def find(nodes, **expected):
    matches = [n for n in nodes if all(n.get(k) == v for k, v in expected.items()) and n.get('visible') == 'true']
    if len(matches) != 1:
        raise RuntimeError(f'Expected unique visible app control: {expected}, got {len(matches)}')
    return matches[0]

def click(nodes, **expected):
    x1, y1, x2, y2 = bounds(find(nodes, **expected))
    shell(f'uitest uiInput click {(x1+x2)//2} {(y1+y2)//2}')

def screenshot(name):
    shell(f'snapshot_display -f {remote}/screen.jpeg')
    hdc('file', 'recv', remote + '/screen.jpeg', str(report_dir / (name + '.jpeg')))

def mesh_level(target):
    current = wait_report(good)
    while current['graphics']['meshGeneration'] != target:
        generation = current['graphics']['meshGeneration']
        direction = 1 if generation < target else -1
        revision = current['graphics']['meshRevision']
        nodes = layout(f'layout-before-detail-{generation}-to-{target}')
        click(nodes, id='olex2-mesh-more' if direction > 0 else 'olex2-mesh-less')
        expected = generation + direction
        current = wait_report(lambda r: good(r) and r['graphics']['meshGeneration'] == expected and r['graphics']['meshRevision'] > revision)
    return current

def mesh_mode(wireframe):
    current = wait_report(good)
    if current['graphics']['wireframe'] == wireframe:
        return current
    revision = current['graphics']['meshRevision']
    nodes = layout('layout-before-mode-' + str(wireframe))
    click(nodes, id='olex2-mesh-mode')
    return wait_report(lambda r: good(r) and r['graphics']['wireframe'] == wireframe and r['graphics']['meshRevision'] > revision)

def primitive(kind):
    current = wait_report(good)
    if current['graphics']['meshKind'] == ['sphere', 'torus', 'disks', 'rims'][kind]:
        return current
    revision = current['graphics']['meshRevision']
    nodes = layout('layout-before-primitive-' + str(kind))
    click(nodes, id='olex2-primitive-' + str(kind))
    return wait_report(lambda r: good(r) and r['graphics']['meshKind'] == ['sphere', 'torus', 'disks', 'rims'][kind] and r['graphics']['meshRevision'] > revision)

def color(preset):
    current = wait_report(good)
    if current['graphics']['colorPreset'] == preset:
        return current
    revision = current['graphics']['colorRevision']
    click(layout('layout-before-color-' + str(preset)), id='olex2-color-' + str(preset))
    return wait_report(lambda r: good(r) and r['graphics']['colorPreset'] == preset and r['graphics']['colorRevision'] > revision)

def material(preset):
    current=wait_report(good)
    if current['graphics']['materialPreset']==preset: return current
    revision=current['graphics']['materialRevision']
    click(layout('layout-before-material-'+str(preset)),id='olex2-material-'+str(preset))
    return wait_report(lambda r: good(r) and r['graphics']['materialPreset']==preset and r['graphics']['materialRevision']>revision)

def keyboard(code, predicate):
    before = wait_report(good)
    nodes = layout('layout-before-key-' + str(code))
    click(nodes, id='olex2-viewport-input')
    shell('uitest uiInput keyEvent ' + str(code))
    return wait_report(lambda r: good(r) and r.get('keyEvents', 0) > before.get('keyEvents', 0) and predicate(r, before))

def input_literal(name,text,observation_key):
    def inject_literal(suffix):
        click(layout('layout-'+name+'-clear'+suffix),id='olex2-command-clear')
        nodes=layout('layout-'+name+'-input'+suffix);x1,y1,x2,y2=bounds(find(nodes,id='olex2-command-input'))
        # Keep spaces as separate real key events: the emulator's batched
        # text-to-key path can lose them during composition transitions.
        # Do not rewrite command text or infer acceptance from intended input.
        click(nodes,id='olex2-command-input')
        input_log=[]
        for index,part in enumerate(text.split(' ')):
            if index:
                # A space can commit an IME composition without inserting the
                # literal separator. Observe it before sending the next token.
                for space_attempt in range(3):
                    input_log.append(shell('uitest uiInput keyEvent 2050'))
                    observed=find(layout(f'layout-{name}-space-{index}-{space_attempt}'+suffix),id='olex2-command-input')
                    raw=observed.get('originalText',observed.get('text',''))
                    input_log.append('Observed literal separator: '+repr(raw))
                    if raw.endswith(' '):break
            if part:
                input_log.append(shell('uitest uiInput text '+shlex.quote(part)))
                layout(f'layout-{name}-part-{index}'+suffix)
        save(name+'-input'+suffix+'.log','\n'.join(input_log))
        node=find(layout('layout-'+name+'-committed'+suffix),id='olex2-command-input')
        return node.get('originalText',node.get('text',''))
    committed=inject_literal('');mode_toggled=False
    try:
        if committed!=text:
            # UI keyboard input can enter the active Chinese composition
            # buffer or convert punctuation. Cancel that composition and
            # temporarily toggle typing mode on the focused field. Never
            # change configured input methods, system preferences or SDK.
            shell('uitest uiInput keyEvent 2070');shell('uitest uiInput keyEvent 2047');mode_toggled=True
            committed=inject_literal('-latin')
    finally:
        if mode_toggled:
            save(name+'-typing-mode-restored.log',shell('uitest uiInput keyEvent 2047'))
    uinput_fallback=False
    if committed!=text:
        # Some IMEs still map ASCII punctuation to full-width forms after a
        # temporary Shift. Use the target's actual text-input injection route,
        # not a native command call or text/grammar rewrite, and read it back.
        uinput_fallback=True
        click(layout('layout-'+name+'-uinput-clear'),id='olex2-command-clear')
        click(layout('layout-'+name+'-uinput-focus'),id='olex2-command-input')
        output=shell('uinput -K -t '+shlex.quote(text))
        node=find(layout('layout-'+name+'-uinput-committed'),id='olex2-command-input')
        committed=node.get('originalText',node.get('text',''))
        save(name+'-uinput-fallback.log',output+'\nObserved actual UI text: '+repr(committed)+'\n')
    segmented_uinput_fallback=False
    if committed!=text:
        # Batched text can reorder a space around shifted punctuation. Send
        # bounded fragments through the same actual keyboard route, observe
        # each separator before the next fragment, then demand exact full text.
        segmented_uinput_fallback=True
        click(layout('layout-'+name+'-uinput-segmented-clear'),id='olex2-command-clear')
        click(layout('layout-'+name+'-uinput-segmented-focus'),id='olex2-command-input')
        fragments=[]
        for index,part in enumerate(text.split(' ')):
            if index:
                for space_attempt in range(3):
                    fragments.append(shell('uitest uiInput keyEvent 2050'))
                    n=find(layout(f'layout-{name}-uinput-segmented-space-{index}-{space_attempt}'),id='olex2-command-input')
                    raw=n.get('originalText',n.get('text',''))
                    fragments.append('Observed literal separator: '+repr(raw))
                    if raw.endswith(' '):break
            if part:
                fragments.append(shell('uinput -K -t '+shlex.quote(part)))
                layout(f'layout-{name}-uinput-segmented-part-{index}')
        node=find(layout('layout-'+name+'-uinput-segmented-committed'),id='olex2-command-input')
        committed=node.get('originalText',node.get('text',''))
        save(name+'-uinput-segmented-fallback.log','\n'.join(fragments)+'\nObserved actual UI text: '+repr(committed)+'\n')
    observed={'expected':text,'actual_committed_text':committed,'temporary_typing_mode_toggle':mode_toggled,'typing_mode_restore_event_sent':mode_toggled,'uinput_text_fallback_used':uinput_fallback,'segmented_uinput_fallback_used':segmented_uinput_fallback}
    summary.setdefault('literal_input_attempts',{})[name]=observed
    if committed!=text:raise RuntimeError(name+': UI input did not commit the exact literal command')
    summary.setdefault(observation_key,{})[name]=observed

def options_document_checks():
    global pid
    before=wait_report(lambda r:good(r) and r.get('applicationRuntime',{}).get('initialized'))
    summary['options_document_before']=before
    empty=dict(sequence=0,operation='',state='idle',selectedUri='',byteCount=0,sha256='',importedOptions=0,optionsChanged=False,destinationMayBePartial=False,error='')
    record('options_document_baseline',before.get('optionsDocument')==empty,before)
    click(layout('layout-options-documents-open'),text='运行记录')
    wait_report(lambda r:not r['graphics']['surfaceReady'])
    def stable(old,new):
        return all(old['applicationRuntime'][k]==new['applicationRuntime'][k] for k in ('optionsSha256','optionsCount','saveRevision','commandHints')) and old['applicationFunction']==new['applicationFunction'] and old['desktopCommand']==new['desktopCommand']
    def launch(operation,sequence):
        snapshot=wait_report(lambda r:r.get('optionsDocument',{}).get('sequence')==sequence-1)
        summary.setdefault('options_document_before_samples',{})[operation+'-'+str(sequence)]=snapshot
        click(layout('layout-options-'+operation+'-'+str(sequence)),id='olex2-options-'+operation)
        deadline=time.monotonic()+12
        while time.monotonic()<deadline:
            nodes=layout('layout-options-system-'+operation+'-'+str(sequence),bundle=None)
            if any(n.get('visible')=='true' and n.get('text')=='取消' and n.get('type')=='Button' for n in nodes):return snapshot,nodes
            time.sleep(.5)
        raise RuntimeError('System document picker did not appear')
    for operation,sequence in [('export',1),('import',2)]:
        prior,nodes=launch(operation,sequence);click(nodes,text='取消',type='Button')
        actual=wait_report(lambda r:r.get('optionsDocument',{}).get('sequence')==sequence and r['optionsDocument']['state']!='choosing')
        d=actual['optionsDocument'];record('options_document_'+operation+'_cancelled',d['state']=='cancelled' and d['operation']==operation and d['selectedUri']=='' and d['byteCount']==0 and not d['destinationMayBePartial'] and not d['optionsChanged'] and stable(prior,actual),actual)
    prior,nodes=launch('export',3)
    filename='olex2-ui-settings-'+uuid.uuid4().hex[:12]+'.options'
    def type_filename(suffix):
        nodes=layout('layout-options-filename-before'+suffix,bundle=None);click(nodes,id='pickerFileNameTextInput')
        shell('uitest uiInput keyEvent 2072 2017');save('options-filename-input'+suffix+'.log',shell('uitest uiInput text '+shlex.quote(filename)))
        nodes=layout('layout-options-filename-committed'+suffix,bundle=None);node=find(nodes,id='pickerFileNameTextInput');return node.get('originalText',node.get('text',''))
    committed=type_filename('');toggled=False
    try:
        if committed!=filename:
            shell('uitest uiInput keyEvent 2070');shell('uitest uiInput keyEvent 2047');toggled=True;committed=type_filename('-latin')
        if committed!=filename:raise RuntimeError('System picker filename did not match the exact unique test name')
    finally:
        if toggled:save('options-filename-mode-restored.log',shell('uitest uiInput keyEvent 2047'))
    summary['options_document_filename_observation']=dict(expected=filename,actual_committed_text=committed,temporary_typing_mode_toggle=toggled,typing_mode_restore_event_sent=toggled)
    click(layout('layout-options-save-confirm',bundle=None),text='保存',type='Button')
    exported=wait_report(lambda r:r.get('optionsDocument',{}).get('sequence')==3 and r['optionsDocument']['state']!='choosing',seconds=20)
    d=exported['optionsDocument'];summary['options_document_exported']=exported
    record('options_document_exported_snapshot',d['state']=='passed' and d['operation']=='export' and d['selectedUri'].startswith('file://') and d['selectedUri'].endswith('/'+filename) and 0<d['byteCount']<=1048576 and d['sha256']==prior['applicationRuntime']['optionsSha256'] and stable(prior,exported),exported)
    nodes=layout('layout-options-exported-visible');record('options_document_saved_ui',find(nodes,id='olex2-options-document-status')['text']=='设置已导出，文件字节已核对',dict(visible_status=find(nodes,id='olex2-options-document-status')['text']))
    text='app.SetOption(ui.commandHints,false)';function_before=exported
    input_literal('options-document-hide-hints',text,'options_document_function_input_observations');click(layout('layout-options-hide-hints-run'),id='olex2-application-function-run')
    hidden=wait_report(lambda r:r['applicationFunction']['sequence']==function_before['applicationFunction']['sequence']+1 and r['applicationFunction']['input']==text)
    nodes=layout('layout-options-hidden-hint');summary['options_document_before_import']=hidden
    record('options_document_prepare_import_hints_hidden',hidden['applicationFunction']['passed'] and not hidden['applicationRuntime']['commandHints'] and hidden['applicationRuntime']['optionsSha256']!=d['sha256'] and not any(n.get('id')=='olex2-command-hint' and n.get('visible')=='true' for n in nodes),hidden)
    prior,nodes=launch('import',4);uri=d['selectedUri']
    def selected_source_visible(nodes):
        return any(n.get('id')==uri and n.get('type')=='Text' and n.get('visible')=='true' for n in nodes)
    # Cancel/confirm buttons appear before the system picker finishes loading
    # its locations and file list. Observe readiness, never infer a missing
    # search input from that transient frame or bypass the system picker.
    ready_deadline=time.monotonic()+20;ready_attempt=0
    while not selected_source_visible(nodes) and not any(n.get('id')=='FileManager_SearchBarComp_Input' and n.get('visible')=='true' for n in nodes):
        if time.monotonic()>=ready_deadline:raise RuntimeError('System document picker did not finish loading the file list/search input')
        time.sleep(.5);ready_attempt+=1;nodes=layout('layout-options-import-ready-'+str(ready_attempt),bundle=None)
    if not selected_source_visible(nodes):
        click(nodes,id='FileManager_SearchBarComp_Input');shell('uitest uiInput keyEvent 2072 2017');shell('uitest uiInput text '+shlex.quote(filename))
        node=find(layout('layout-options-import-search-query',bundle=None),id='FileManager_SearchBarComp_Input');query=node.get('originalText',node.get('text',''))
        if query!=filename:
            shell('uitest uiInput keyEvent 2072 2017');save('options-import-search-uinput.log',shell('uinput -K -t '+shlex.quote(filename)))
            node=find(layout('layout-options-import-search-query-uinput',bundle=None),id='FileManager_SearchBarComp_Input');query=node.get('originalText',node.get('text',''))
        summary['options_document_search_observation']=dict(expected=filename,actual_committed_text=query)
        if query!=filename:raise RuntimeError('System picker search did not commit the exact unique filename')
        shell('uitest uiInput keyEvent 2054');search_deadline=time.monotonic()+20;search_attempt=0
        while True:
            nodes=layout('layout-options-import-search-'+str(search_attempt),bundle=None)
            if selected_source_visible(nodes):break
            if time.monotonic()>=search_deadline:raise RuntimeError('System picker search did not display the exact exported URI')
            time.sleep(.5);search_attempt+=1
    click(nodes,id=uri,type='Text');click(layout('layout-options-import-confirm',bundle=None),text='打开',type='Button')
    imported=wait_report(lambda r:r.get('optionsDocument',{}).get('sequence')==4 and r['optionsDocument']['state']!='choosing',seconds=20)
    summary['options_document_imported']=imported;i=imported['optionsDocument'];rt=imported['applicationRuntime']
    record('options_document_imported_snapshot',i['state']=='passed' and i['operation']=='import' and i['selectedUri']==uri and i['sha256']==d['sha256'] and i['byteCount']==d['byteCount'] and i['importedOptions']==exported['applicationRuntime']['optionsCount'] and i['optionsChanged'] and rt['commandHints'] and rt['saveRevision']==prior['applicationRuntime']['saveRevision']+1 and rt['optionsSha256']==d['sha256'] and imported['applicationFunction']==prior['applicationFunction'] and imported['desktopCommand']==prior['desktopCommand'],imported)
    nodes=layout('layout-options-imported-visible');visible=sum(n.get('id')=='olex2-command-hint' and n.get('visible')=='true' for n in nodes);status=find(nodes,id='olex2-options-document-status')['text']
    record('options_document_import_restores_visible_setting',visible==1 and status==f"已合并 {i['importedOptions']} 个设置",dict(visible_hint_nodes=visible,visible_status=status))
    summary['options_document_before_restart']=imported
    old=pid;shell(f'aa force-stop {BUNDLE}');deadline=time.monotonic()+8
    while time.monotonic()<deadline and shell('pidof '+BUNDLE+' || true').strip():time.sleep(.25)
    shell(f'aa start -a EntryAbility -b {BUNDLE}');deadline=time.monotonic()+12;pid=''
    while time.monotonic()<deadline:
        candidate=shell('pidof '+BUNDLE+' || true').strip()
        if candidate.isdigit() and candidate!=old:pid=candidate;break
        time.sleep(.3)
    record('options_document_cold_restart_new_process',pid.isdigit() and pid!=old,dict(prior_pid=old,new_pid=pid))
    restored=wait_report(good);summary['options_document_after_restart']=restored;rt=restored['applicationRuntime']
    record('options_document_import_restored_after_restart',rt['optionsRestored'] and rt['commandHints'] and rt['optionsSha256']==d['sha256'] and rt['optionsCount']==imported['applicationRuntime']['optionsCount'] and rt['saveRevision']==0,restored)
    record('options_document_fresh_transfer_state',restored['optionsDocument']==empty and restored['applicationFunction']['sequence']==0 and restored['applicationFunction']['input']=='' and restored['desktopCommand']['sequence']==0,restored)

def original_log_checks(baseline):
    reference=parse_catalog_log((ROOT/'reports/app/original-log-verified-host-original_log-tests.log').read_text())
    record('original_log_empty_at_start',empty_audit_matches(baseline['originalLog'],fresh=True),dict(log=baseline['originalLog'],catalog=baseline['coreCatalog']))
    click(layout('layout-original-log-start'),id='olex2-nav-2')
    wait_report(lambda r:not r['graphics']['surfaceReady'] and not r['graphics']['initialized'])
    click(layout('layout-original-log-platform-page'),id='olex2-original-logs')
    opened=wait_report(lambda r:r.get('originalLogView') is True and r['graphics']['surfaceReady'] is False)
    pause=dict(visible_text=find(layout('layout-records-drawing-paused'),id='olex2-graphics-readiness')['text'],active_panel='运行记录')
    record('drawing_paused_visible_in_records',pause==dict(visible_text='绘图已暂停',active_panel='运行记录'),pause)
    click(layout('layout-original-log-empty'),id='olex2-core-macros')
    macros=wait_report(lambda r:r.get('coreCatalog',{}).get('sequence')==opened['coreCatalog']['sequence']+1)
    m=dict(log=macros['originalLog'],catalog=macros['coreCatalog'])
    record('original_log_macros_from_actual_core',catalog_sample_matches(m,reference,'macros'),m)
    text=find(layout('layout-original-log-macros'),id='olex2-core-log-text')['text']
    visible=dict(m,visible_text=text)
    record('original_log_macros_visible_in_ui',visible_catalog_matches(visible,reference,'macros'),visible)
    screenshot('original-core-macro-catalog')
    click(layout('layout-original-log-before-clear'),id='olex2-core-log-clear')
    cleared=wait_report(lambda r:r['originalLog']['clearCount']==m['log']['clearCount']+1)
    clear=dict(before=m['log'],after=cleared['originalLog'])
    record('original_log_clear_preserves_sequence',cleared_sample_matches(clear),clear)
    empty_text=find(layout('layout-original-log-cleared'),id='olex2-core-log-text')['text']
    record('original_log_empty_is_visible',empty_text=='本次会话暂无原版日志' and empty_audit_matches(cleared['originalLog']),dict(visible_text=empty_text,log=cleared['originalLog']))
    click(layout('layout-original-log-before-functions'),id='olex2-core-functions')
    functions=wait_report(lambda r:r['coreCatalog']['sequence']==m['catalog']['sequence']+1)
    f=dict(log=functions['originalLog'],catalog=functions['coreCatalog'])
    record('original_log_functions_from_actual_core',catalog_sample_matches(f,reference,'functions'),f)
    visible=dict(f,visible_text=find(layout('layout-original-log-functions'),id='olex2-core-log-text')['text'])
    record('original_log_functions_visible_in_ui',visible_catalog_matches(visible,reference,'functions'),visible)
    click(layout('layout-original-log-return'),id='olex2-nav-0')
    restored=wait_report(good)
    record('original_log_survives_surface_recreation',restored['originalLog']==f['log'] and good(restored),dict(log=restored['originalLog'],graphics=restored['graphics']))

def desktop_shell_identity_checks():
    expected=json.loads((ROOT/'app/AppScope/app.json5').read_text())['app']
    current=wait_report(good)
    identity=current.get('installedPackage',{})
    record('installed_package_identity_from_runtime',identity_matches(identity,expected),dict(identity=identity))
    nodes=layout('layout-installed-package-version')
    visible=dict(identity=identity,visible_text=find(nodes,id='olex2-installed-version')['text'])
    record('installed_package_version_visible',version_visible_matches(visible,expected),visible)
    sample=dict(entry=bounds(find(nodes,id='olex2-command-entry')))
    for name,control in (('view','olex2-command-run'),('application','olex2-application-function-run'),('help','olex2-command-help')):
        n=find(nodes,id=control)
        sample[name]=dict(text=n['text'],visible=n['visible'],bounds=bounds(n))
    record('desktop_command_labels_visible',labels_match(sample),sample)

def desktop_shell_enter_checks():
    for route,text in (('application','app.ConfigDir()'),('view','pc.help')):
        name='desktop_enter_'+route+'_interpreter'
        before=wait_report(good)
        input_literal(name,text,'desktop_shell_input_observations')
        observed=find(layout('layout-'+name+'-observed'),id='olex2-command-input')['text']
        shell('uitest uiInput keyEvent 2054')
        field='applicationFunction' if route=='application' else 'desktopCommand'
        after=wait_report(lambda r:good(r) and r[field]['sequence']==before[field]['sequence']+1)
        keys=('installedPackage','applicationRuntime','applicationFunction','desktopCommand')
        sample=dict(observed_input=observed,injected_keycode=2054,before={k:before[k] for k in keys},after={k:after[k] for k in keys})
        record(name,enter_matches(sample,route),sample)
    screenshot('desktop-shell-actual-window')

def application_runtime_checks():
    global pid
    baseline=wait_report(lambda r:good(r) and r.get('applicationRuntime',{}).get('initialized'))
    record('application_runtime_baseline',runtime_matches(baseline['applicationRuntime']) and baseline['applicationFunction']['sequence']==0,baseline)
    record('original_core_runtime_empty_startup',runtime_matches(baseline['applicationRuntime']),baseline)
    record('original_line_path_gpu_probe',path_probe_matches(baseline['graphics']),baseline['graphics'])
    summary['application_runtime_baseline']=baseline
    original_log_checks(baseline)
    def run(name,text):
        before=wait_report(good);summary.setdefault('application_function_before_samples',{})[name]=before
        input_literal(name,text,'application_function_input_observations')
        click(layout('layout-'+name+'-run'),id='olex2-application-function-run')
        actual=wait_report(lambda r:good(r) and r.get('applicationFunction',{}).get('sequence')==before['applicationFunction']['sequence']+1 and r['applicationFunction']['input']==text)
        record(name,function_sample_matches(name,actual,before),actual);return actual
    for name,text,passed,result,hints in APPLICATION_FUNCTION_SPECS:
        sample=run(name,text)
        if name=='application_function_hide_hints':
            nodes=layout('layout-application-hints-hidden')
            record('application_runtime_hints_hidden',not any(n.get('id')=='olex2-command-hint' and n.get('visible')=='true' for n in nodes),{'commandHints':sample['applicationRuntime']['commandHints'],'visible_hint_nodes':sum(n.get('id')=='olex2-command-hint' and n.get('visible')=='true' for n in nodes)})
    saved=wait_report(good)
    record('application_runtime_options_saved',saved['applicationRuntime']['lastSaveCommitted'] is True and len(saved['applicationRuntime']['optionsSha256'])==64 and saved['applicationRuntime']['commandHints'] is False,saved)
    summary['application_runtime_before_restart']=saved;screenshot('application-runtime-original-options')
    prior=pid;shell(f'aa force-stop {BUNDLE}');deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        if not shell('pidof '+BUNDLE+' || true').strip():break
        time.sleep(.2)
    shell(f'aa start -a EntryAbility -b {BUNDLE}');deadline=time.monotonic()+12
    while time.monotonic()<deadline:
        candidate=shell('pidof '+BUNDLE+' || true').strip()
        if candidate.isdigit() and candidate!=prior:pid=candidate;break
        time.sleep(.25)
    record('application_runtime_cold_restart_new_process',pid.isdigit() and pid!=prior,{'prior_pid':prior,'new_pid':pid})
    restored=wait_report(lambda r:good(r) and r.get('applicationRuntime',{}).get('optionsRestored'))
    summary['application_runtime_after_restart']=restored
    log_restart=dict(log=restored['originalLog'],catalog=restored['coreCatalog'],prior_pid=prior,new_pid=pid)
    record('original_log_cold_restart_without_replay',restart_sample_matches(log_restart),log_restart)
    old=saved['applicationRuntime'];new=restored['applicationRuntime']
    keys=('baseDir','instanceDir','sharedDir','configDir','optionsCount','optionsSha256','licenseSha256')
    hidden_nodes=layout('layout-application-hints-hidden-after-restart')
    record('application_runtime_restored_after_restart',runtime_matches(new) and not new['commandHints'] and new['saveRevision']==0 and all(old[k]==new[k] for k in keys) and not any(n.get('id')=='olex2-command-hint' and n.get('visible')=='true' for n in hidden_nodes),restored)
    f=restored['applicationFunction']
    record('application_runtime_fresh_function_state',f=={'sequence':0,'input':'','passed':False,'optionsChanged':False,'text':'','error':'','source':''} and restored['desktopCommand']['sequence']==0,f)
    if not application_recovery_matches(summary):raise RuntimeError('Independent original application runtime recovery validation failed')
    run('application_function_restore_hints','app.SetOption(ui.commandHints,true)')
    nodes=layout('layout-application-hints-restored')
    record('application_runtime_hints_visible',sum(n.get('id')=='olex2-command-hint' and n.get('visible')=='true' for n in nodes)==1,{'visible_hint_nodes':sum(n.get('id')=='olex2-command-hint' and n.get('visible')=='true' for n in nodes)})

def command_checks():
    global pid
    primitive(0);material(0);color(0);mesh_level(3);mesh_mode(False)
    click(layout('layout-command-baseline-reset'),id='olex2-view-reset')
    baseline=wait_report(lambda r:good(r) and abs(r['graphics']['yaw']-32)<.1 and abs(r['graphics']['pitch']+22)<.1)
    record('desktop_command_baseline',baseline['graphics']['meshKind']=='sphere' and not baseline['graphics']['wireframe'],baseline)
    command_source='upstream/sdl/macrolib.cpp + library.cpp + function.cpp + exparse: original TEMacroLib parser and TLibrary macro dispatch -> explicit pc.* view adapters'
    def run(name,text,passed,predicate,enter=False):
        before=wait_report(good);sequence=before.get('desktopCommand',{}).get('sequence',0)
        input_literal(name,text,'desktop_command_input_observations')
        if enter:shell('uitest uiInput keyEvent 2054')
        else:click(layout('layout-'+name+'-run'),id='olex2-command-run')
        actual=wait_report(lambda r:good(r) and r.get('desktopCommand',{}).get('sequence')==sequence+1 and r['desktopCommand'].get('input')==text)
        command=actual['desktopCommand'];valid=command['passed'] is passed and command['source']==command_source and predicate(actual['graphics'])
        if passed:valid=valid and command['dispatched']>0 and not command['error'] and command['applied'] is command['changed']
        else:valid=valid and command['applied'] is False and command['changed'] is False and bool(command['error']) and actual['graphics']==before['graphics']
        record(name,valid and command_sample_matches(name,actual),actual);return actual
    run('desktop_command_help','pc.help',True,lambda g:True)
    run('desktop_command_mesh',"PC.MESH 'torus'",True,lambda g:g['meshKind']=='torus')
    run('desktop_command_detail','pc.detail 2',True,lambda g:g['meshGeneration']==2)
    run('desktop_command_wire','pc.wire on',True,lambda g:g['wireframe'] is True)
    run('desktop_command_color','pc.color amber',True,lambda g:g['colorPreset']==1)
    run('desktop_command_material','pc.material flat',True,lambda g:g['materialPreset']==2)
    run('desktop_command_view','pc.view 41 -18 4.8',True,lambda g:abs(g['yaw']-41)<.001 and abs(g['pitch']+18)<.001 and abs(g['distance']-4.8)<.001)
    run('desktop_command_chain','pc.mesh disks >> pc.detail 1 >> pc.wire off >> pc.color sky >> pc.material classic',True,lambda g:g['meshKind']=='disks' and g['meshGeneration']==1 and g['wireframe'] is False and g['colorPreset']==2 and g['materialPreset']==0)
    run('desktop_command_unknown_rejected','pc.missing',False,lambda g:True)
    run('desktop_command_bad_argument_rejected','pc.detail 9',False,lambda g:True)
    run('desktop_command_failed_chain_unchanged','pc.reset >> pc.detail 9',False,lambda g:abs(g['yaw']-41)<.001 and abs(g['pitch']+18)<.001)
    run('desktop_command_reset','pc.reset',True,lambda g:abs(g['yaw']-32)<.001 and abs(g['pitch']+22)<.001 and abs(g['distance']-4.4)<.001)
    before_surface=run('desktop_command_enter_submit','pc.wire toggle',True,lambda g:g['wireframe'] is True,enter=True)
    screenshot('desktop-command-actual-window')
    click(layout('layout-command-nav-out'),id='olex2-nav-2');click(layout('layout-command-nav-back'),id='olex2-nav-0')
    surface=wait_report(lambda r:good(r) and r['graphics']['meshKind']=='disks' and r['graphics']['wireframe'] is True)
    record('desktop_command_surface_recreated',surface['graphics']['frames']<before_surface['graphics']['frames'],surface)
    keys=('meshKind','meshGeneration','wireframe','colorPreset','materialPreset','materialCanonical','yaw','pitch','distance')
    record('desktop_command_survives_surface_recreation',all(surface['graphics'][k]==before_surface['graphics'][k] for k in keys),surface)
    saved=wait_report(lambda r:good(r) and r['workspaceState']['saveStatus']=='saved')
    record('desktop_command_state_saved',saved['workspaceState']['schemaVersion']==4,saved)
    summary['desktop_command_state_before_restart']=saved;prior_pid=pid
    shell(f'aa force-stop {BUNDLE}');deadline=time.monotonic()+8
    while time.monotonic()<deadline and shell('pidof '+BUNDLE+' || true').strip():time.sleep(.25)
    shell(f'aa start -a EntryAbility -b {BUNDLE}');deadline=time.monotonic()+12;pid=''
    while time.monotonic()<deadline:
        candidate=shell('pidof '+BUNDLE+' || true').strip()
        if candidate.isdigit() and candidate!=prior_pid:pid=candidate;break
        time.sleep(.25)
    record('desktop_command_cold_restart_new_process',pid.isdigit() and pid!=prior_pid,{'prior_pid':prior_pid,'new_pid':pid})
    restored=wait_report(lambda r:good(r) and r['workspaceState']['restored'] and r['graphics']['meshKind']=='disks')
    record('desktop_command_restored_after_restart',all(restored['graphics'][k]==saved['graphics'][k] for k in keys) and restored.get('desktopCommand',{}).get('sequence')==0,restored)
    summary['desktop_command_state_after_restart']=restored
    if not command_recovery_matches(summary):raise RuntimeError('Independent command-state recovery validation failed')


shared_sequence = 0
def shared_original_roundtrip(label):
    global shared_sequence
    shared_sequence += 1
    run = shared_sequence
    # Compare exact host state only after its already-existing deferred save.
    wait_report(lambda r: good(r) and r.get('workspaceState',{}).get('saveStatus') == 'saved')
    before_label,after_label,folder_label = ('shared-'+label+suffix for suffix in ('-before','-after','-original'))
    def invoke(script,*args):
        command=[sys.executable,str(ROOT/'scripts'/script),*map(str,args)]
        result=subprocess.run(command,capture_output=True,text=True,timeout=100)
        save('shared-'+label+'-'+script+'.log',result.stdout+result.stderr)
        if result.returncode:raise RuntimeError('Original drawing '+label+' failed in '+script)
    common=['--target',a.target,'--hdc',a.hdc,'--hap',retained_hap]
    invoke('original_renderer_capture.py',*common,'--report',report_dir,'--label',before_label)
    before,after=report_dir/'runtime'/before_label,report_dir/'runtime'/after_label
    invoke('original_renderer_click.py','--target',a.target,'--hdc',a.hdc,'--layout',before/'layout.json','--id','olex2-original-renderer-probe')
    invoke('original_renderer_receive.py',*common,'--report',report_dir,'--run',run,'--folder',folder_label)
    invoke('original_renderer_capture.py',*common,'--report',report_dir,'--label',after_label)
    folder=report_dir/'runtime'/folder_label
    invoke('original_renderer_validation.py',folder,'--before',before,'--after',after,'--hap',retained_hap)
    validation=json.loads((folder/'independent-shared-validation.json').read_text())
    record('shared_original_'+label,validation['passed'],validation)


try:
    device_info = shell('echo ARCH; uname -m; echo TYPE; param get const.product.devicetype; echo MODEL; param get const.product.model; echo API; param get const.ohos.apiversion; echo SYSTEM; param get const.ohos.fullname')
    summary['device_info'] = device_info
    device_lines = device_info.splitlines()
    def device_value(label):
        return device_lines[device_lines.index(label) + 1].strip()
    summary['device_architecture'] = device_value('ARCH')
    summary['device_type'] = device_value('TYPE')
    summary['device_model'] = device_value('MODEL')
    summary['api'] = device_value('API')
    if (summary['device_architecture'] != 'aarch64' or summary['device_type'] != '2in1'
            or summary['device_model'] != 'emulator'):
        raise RuntimeError('This unsigned-debug runner requires an explicitly selected ARM64 PC/2in1 emulator')
    hap = a.hap.resolve()
    summary['hap'] = {'path': str(hap), 'sha256': hashlib.sha256(hap.read_bytes()).hexdigest(), 'bytes': hap.stat().st_size}
    proof_path = hap.with_suffix('.provenance.json')
    # Keep exact build inputs and package bytes with each runtime result. Later
    # builds must not erase the package needed to inspect historical evidence.
    retained_hap = report_dir / hap.name
    shutil.copy2(hap, retained_hap)
    retained_proof = report_dir / 'build-provenance.json'
    shutil.copy2(proof_path, retained_proof)
    if json.loads(retained_proof.read_text()).get('schema_version') == 2:
        shutil.copytree(proof_path.parent / 'build-evidence', report_dir / 'build-evidence')
    proof = verify_build_provenance(retained_hap, retained_proof, ROOT)
    summary['hap'].update(sha256=proof['hap_sha256'], bytes=retained_hap.stat().st_size)
    with zipfile.ZipFile(retained_hap) as package:
        (report_dir / 'build-inputs.json').write_bytes(package.read(EMBEDDED))
    summary['build_provenance'] = dict(verification=proof,
        retained_hap=str(retained_hap.relative_to(ROOT)),
        retained_provenance=str((report_dir / 'build-provenance.json').relative_to(ROOT)),
        retained_inputs=str((report_dir / 'build-inputs.json').relative_to(ROOT)))
    # Install the retained immutable copy, not a path another build may replace.
    install = hdc('install', str(retained_hap))
    save('install.log', install)
    record('debug_hap_installed', 'install bundle successfully' in install, install.strip())
    # Only stop this diagnostic app; preserve the emulator and all other apps/data.
    save('stop-before-start.log', hdc('shell', f'aa force-stop {BUNDLE}'))
    start = shell(f'aa start -a EntryAbility -b {BUNDLE}')
    save('start.log', start)
    record('ability_started', 'start ability successfully' in start, start)
    # Ability startup can return before the new process is visible to pidof.
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        pid = shell('pidof ' + BUNDLE + ' || true').strip()
        if pid.isdigit():
            break
        time.sleep(.25)
    if not pid.isdigit():
        raise RuntimeError('Cannot identify diagnostic app process after startup timeout')
    summary['pid'] = int(pid)
    shell('mkdir ' + remote)
    remote_created = True
    summary['startup_report'] = wait_report(good)
    desktop_shell_identity_checks()
    shared_original_roundtrip("initial")
    application_runtime_checks()
    shared_sequence = 0 # existing application-runtime test starts a different PID
    shared_original_roundtrip("after-application-runtime-restart")
    options_document_checks()
    command_checks()
    shared_sequence = 0 # existing command test starts a different PID
    shared_original_roundtrip("after-view-command-restart")
    desktop_shell_enter_checks()
    # Normalize through actual app controls, without deleting saved preferences.
    primitive(0)
    material(0)
    color(0)
    mesh_level(3)
    mesh_mode(False)
    click(layout('layout-startup-reset'), text='重置视角')
    initial = wait_report(lambda r: good(r) and abs(r['graphics']['yaw']-32) < .1
                          and abs(r['graphics']['pitch']+22) < .1
                          and abs(r['graphics']['distance']-4.4) < .01)
    summary['initial_report'] = initial
    record('actual_native_report', good(initial), initial)
    nodes = layout('layout-initial')
    panel=bounds(find(nodes,id='olex2-viewport-panel'))
    reset_control=bounds(find(nodes,id='olex2-view-reset'))
    zoom_control=bounds(find(nodes,id='olex2-view-zoom'))
    record('small_window_view_controls_fully_visible',
           all(panel[0]<=b[0]<=b[2]<=panel[2] and panel[1]<=b[1]<=b[3]<=panel[3] for b in (reset_control,zoom_control)),
           {'panel':panel,'reset':reset_control,'zoom':zoom_control})
    layout_ok,layout_details=controls_unclipped({'children':[{'attributes':node} for node in nodes]})
    record('shared_named_controls_unclipped',layout_ok,layout_details)
    record('reactive_graphics_card', any(n.get('text') == '像素读回通过' for n in nodes), 'Observed current UI text')
    screenshot('workspace-initial')
    record('actual_upstream_mesh_source', good(initial), initial['graphics'])
    record('actual_shared_submission_stream', initial['upstreamSubmissionTested'] and initial['graphics']['submissionValidated'], initial['graphics'])
    record('original_triangle_branch_drives_actual_upload', original_triangle_upload_matches(initial['graphics']) and good(initial), initial['graphics'])
    record('original_basis_drives_actual_matrix_uniforms', original_matrices_match(initial['graphics']) and good(initial), initial['graphics'])
    key = keyboard(2015, lambda r, b: abs(r['graphics']['yaw'] - b['graphics']['yaw'] - 10) < .1)
    record('injected_keyboard_rotate', True, key)
    key = keyboard(2058, lambda r, b: r['graphics']['distance'] < b['graphics']['distance'])
    record('injected_keyboard_zoom', True, key)
    key = keyboard(2039, lambda r, b: r['graphics']['wireframe'] != b['graphics']['wireframe'])
    record('injected_keyboard_wireframe', True, key)
    key = keyboard(2002, lambda r, b: r['graphics']['meshKind'] == 'torus')
    record('injected_keyboard_primitive', True, key)
    key = keyboard(2034, lambda r, b: abs(r['graphics']['yaw']-32)<.1 and abs(r['graphics']['distance']-4.4)<.01)
    record('injected_keyboard_reset', True, key)
    nodes = layout('layout-before-wheel')
    x1, y1, x2, y2 = bounds(find(nodes, id='olex2-viewport-input'))
    cx, cy = (x1+x2)//2, (y1+y2)//2
    # uinput keeps its px/py inside one invocation. A separate -s call would
    # construct an axis event at (0,0), not at the previously moved cursor.
    # Move, focus-click and scroll at the observed viewport in one invocation.
    before_wheel = wait_report(good)
    save('mouse-wheel-injection.log', shell(f'uinput -M -m {cx} {cy} -c 0 -s 3'))
    wheel = wait_report(lambda r: good(r) and r.get('axisEvents', 0) > before_wheel.get('axisEvents', 0) and abs(r['graphics']['distance']-before_wheel['graphics']['distance'])>.01)
    record('injected_mouse_wheel_zoom', True, wheel)
    nodes = layout('layout-after-wheel')
    click(nodes, text='重置视角')
    primitive(0)
    mesh_mode(False)
    mesh_level(3)
    low = mesh_level(0)
    nodes = layout('layout-detail-zero')
    record('mesh_detail_minimum', find(nodes, id='olex2-mesh-less').get('enabled') == 'false', low['graphics'])
    screenshot('mesh-detail-zero')
    high = mesh_level(5)
    nodes = layout('layout-detail-five')
    record('mesh_detail_maximum', find(nodes, id='olex2-mesh-more').get('enabled') == 'false', high['graphics'])
    wire = mesh_mode(True)
    record('mesh_wireframe_mode', wire['graphics']['wireframe'] and wire['graphics']['coverageVerified'], wire['graphics'])
    record('original_line_branch_drives_actual_wire_upload',original_line_upload_matches(wire['graphics']),wire['graphics'])
    wire_low = mesh_level(2)
    record('mesh_detail_change_in_wireframe', wire_low['graphics']['wireframe'], wire_low['graphics'])
    screenshot('upstream-wireframe')
    solid = mesh_mode(False)
    record('mesh_solid_mode_restored', not solid['graphics']['wireframe'], solid['graphics'])
    default = mesh_level(3)
    nodes = layout('layout-default-restored')
    before = default['graphics']
    click(nodes, text='↷')
    rotated = wait_report(lambda r: good(r) and r['graphics']['frames'] > before['frames'] and abs(r['graphics']['yaw'] - 52) < .1)
    record('rotate_control', True, rotated['graphics'])
    nodes = layout('layout-rotated')
    x1, y1, x2, y2 = bounds(find(nodes, type='XComponent'))
    cx, cy = (x1+x2)//2, (y1+y2)//2
    shell(f'uitest uiInput swipe {cx} {cy} {cx+110} {cy+55} 500')
    dragged = wait_report(lambda r: good(r) and r['graphics']['frames'] > rotated['graphics']['frames'] and abs(r['graphics']['yaw']-rotated['graphics']['yaw']) > 2)
    record('viewport_pan_gesture', True, dragged['graphics'])
    nodes = layout('layout-dragged')
    slider = find(nodes, type='Slider')
    x1, y1, x2, y2 = bounds(slider)
    shell(f'uitest uiInput click {x1+int((x2-x1)*.75)} {(y1+y2)//2}')
    zoomed = wait_report(lambda r: good(r) and r['graphics']['frames'] > rotated['graphics']['frames'] and abs(r['graphics']['distance'] - 4.4) > .2)
    record('zoom_control', True, zoomed['graphics'])
    nodes = layout('layout-zoomed')
    click(nodes, text='重置视角')
    reset = wait_report(lambda r: good(r) and r['graphics']['frames'] > zoomed['graphics']['frames'] and abs(r['graphics']['yaw']-32) < .1 and abs(r['graphics']['distance']-4.4) < .01)
    record('reset_control', True, reset['graphics'])
    nodes = layout('layout-reset')
    # Exercise the actual desktop maximize/restore control observed in this app.
    click(nodes, id='EnhanceMaximizeBtn')
    resized = wait_report(lambda r: good(r) and (r['graphics']['width'],r['graphics']['height']) != (reset['graphics']['width'],reset['graphics']['height']))
    record('native_window_resize', True, resized['graphics'])
    nodes = layout('layout-maximized')
    screenshot('workspace-maximized')
    click(nodes, id='EnhanceMaximizeBtn')
    restored = wait_report(lambda r: good(r) and (r['graphics']['width'],r['graphics']['height']) == (reset['graphics']['width'],reset['graphics']['height']))
    record('native_window_restore', True, restored['graphics'])
    nodes = layout('layout-restored')
    # Each selected preset must appear in actual geometry pixels, not the green
    # diagnostic marker, UI swatch or a uniform/status value alone.
    for kind in range(4):
        primitive(kind)
        mesh_level(3)
        mesh_mode(False)
        for preset in range(3):
            tested = color(preset)
            record(f'color_primitive_{kind}_preset_{preset}', color_pixel_matches(tested['graphics']), tested['graphics'])
            if kind==2 and preset==0:
                record('original_polygon_branch_and_concave_gpu_tessellation',good(tested) and polygon_probe_matches(tested['graphics']) and original_polygon_upload_matches(tested['graphics']),tested['graphics'])
            if kind == 0:
                screenshot(f'upstream-color-{preset}')
    # Keep all previous restoration cases and add a real disk/polygon surface
    # and cold-process cycle. The synthetic U-contour FBO probe must run in each context.
    primitive(2);mesh_level(2);mesh_mode(False);color(1);material(0)
    before_polygon=wait_report(lambda r:good(r) and r['graphics']['meshKind']=='disks')
    polygon_context_count=logs().count('OLEX2_CONTEXT_READY')
    click(layout('layout-before-polygon-nav'),id='olex2-nav-2')
    click(layout('layout-polygon-runtime'),id='olex2-nav-0')
    recreated_polygon=wait_report(lambda r:good(r) and r['graphics']['meshKind']=='disks')
    record('polygon_surface_recreated',logs().count('OLEX2_CONTEXT_READY')>polygon_context_count,recreated_polygon)
    record('polygon_survives_surface_recreation',original_polygon_upload_matches(recreated_polygon['graphics']) and polygon_probe_matches(recreated_polygon['graphics']) and all(before_polygon['graphics'][k]==recreated_polygon['graphics'][k] for k in ['meshKind','meshGeneration','wireframe','colorPreset','materialPreset','yaw','pitch','distance']),recreated_polygon)
    saved_polygon=wait_report(lambda r:good(r) and r['graphics']['meshKind']=='disks' and r['workspaceState']['saveStatus']=='saved')
    record('polygon_state_saved',saved_polygon['workspaceState']['schemaVersion']==4,saved_polygon)
    summary['polygon_state_before_restart']=saved_polygon;prior_polygon_pid=pid
    save('polygon-cold-restart-stop.log',shell(f'aa force-stop {BUNDLE}'))
    deadline=time.monotonic()+8
    while time.monotonic()<deadline and shell('pidof '+BUNDLE+' || true').strip():time.sleep(.25)
    if shell('pidof '+BUNDLE+' || true').strip():raise RuntimeError('Polygon cold restart did not stop the diagnostic process')
    save('polygon-cold-restart-start.log',shell(f'aa start -a EntryAbility -b {BUNDLE}'))
    deadline=time.monotonic()+12;pid=''
    while time.monotonic()<deadline:
        candidate=shell('pidof '+BUNDLE+' || true').strip()
        if candidate.isdigit() and candidate!=prior_polygon_pid:pid=candidate;break
        time.sleep(.25)
    record('polygon_cold_restart_new_process',pid.isdigit() and pid!=prior_polygon_pid,{'prior_pid':prior_polygon_pid,'new_pid':pid})
    restored_polygon=wait_report(lambda r:good(r) and r['workspaceState']['restored'] and r['graphics']['meshKind']=='disks')
    record('polygon_restored_after_restart',all(saved_polygon['graphics'][k]==restored_polygon['graphics'][k] for k in ['meshKind','meshGeneration','wireframe','colorPreset','materialPreset','materialCanonical','yaw','pitch','distance']) and original_polygon_upload_matches(restored_polygon['graphics']) and polygon_probe_matches(restored_polygon['graphics']),restored_polygon)
    summary['polygon_state_after_restart']=restored_polygon
    screenshot('polygon-restored-after-restart')
    # Actual geometry, same view and color: changing upstream material fields
    # must produce the predicted row-light response, not merely UI/status changes.
    for kind in range(4):
        primitive(kind)
        mesh_level(3)
        mesh_mode(False)
        for color_preset in (range(3) if kind==0 else [0]):
            color(color_preset)
            samples=[]
            for material_preset in range(3):
                tested=material(material_preset)
                g=tested['graphics']; samples.append(g)
                record(f'material_primitive_{kind}_color_{color_preset}_preset_{material_preset}',material_pixels_match(g),g)
                if kind==0 and color_preset==0 and material_preset==0:
                    record('original_material_commands_drive_uniforms', material_values_match(g) and material_pixels_match(g), g)
                if kind==0 and color_preset==0:
                    screenshot(f'upstream-material-{material_preset}')
            record(f'material_primitive_{kind}_color_{color_preset}_lighting_response',lighting_response(samples),samples)
    # Actual glossy pixels are independently predicted from submitted triangle
    # normals, perspective interpolation, specular commands and exponent32.
    for kind in range(4):
        primitive(kind);mesh_level(3);mesh_mode(False)
        for preset in range(3):
            color(preset);tested=material(3);g=tested['graphics']
            record(f'glossy_primitive_{kind}_color_{preset}',glossy_pixels_match(g,kind==0),g)
            if kind==0 and preset==0:screenshot('upstream-glossy')
    material(0)
    color(2)
    for kind in [1, 2, 3]:
        primitive(kind)
        for detail in [0, 3, 5]:
            mesh_level(detail)
            for wireframe in [False, True]:
                tested = mesh_mode(wireframe)
                record(f'primitive_{kind}_detail_{detail}_{"wire" if wireframe else "solid"}', good(tested), tested['graphics'])
                if detail == 3:
                    screenshot(f'primitive-{kind}-{"wire" if wireframe else "solid"}')
    primitive(1)
    mesh_level(2)
    mesh_mode(True)
    nodes = layout('layout-before-mesh-navigation')
    material(1)
    context_count = logs().count('OLEX2_CONTEXT_READY')
    click(nodes, text='运行记录')
    wait_report(lambda r: not r['graphics']['surfaceReady'] and not r['graphics']['initialized'])
    nodes = layout('layout-records')
    record('runtime_page_visible', any(n.get('text') == '原生运行记录' for n in nodes), 'Observed application runtime page')
    click(nodes, text='适配工作台')
    rebuilt = wait_report(good)
    nodes = layout('layout-recreated')
    record('surface_recreated_after_navigation', any(n.get('type') == 'XComponent' for n in nodes) and good(rebuilt) and logs().count('OLEX2_CONTEXT_READY') > context_count, rebuilt['graphics'])
    record('material_survives_surface_recreation', rebuilt['graphics']['materialPreset']==1 and material_values_match(rebuilt['graphics']) and material_pixels_match(rebuilt['graphics']),rebuilt['graphics'])
    record('color_survives_surface_recreation', rebuilt['graphics']['colorPreset'] == 2 and color_value_matches(rebuilt['graphics']) and color_pixel_matches(rebuilt['graphics']), rebuilt['graphics'])
    record('mesh_selection_survives_surface_recreation', rebuilt['graphics']['wireframe'] and rebuilt['graphics']['meshGeneration'] == 2 and rebuilt['graphics']['meshKind'] == 'torus', rebuilt['graphics'])
    mesh_mode(False)
    mesh_level(3)
    nodes = layout('layout-after-mesh-restoration')
    click(nodes, id='olex2-nav-1')
    time.sleep(.8)
    nodes = layout('layout-checks-page')
    record('utility_page_visible', any(n.get('text') == '来自真实上游的基础实现' for n in nodes), 'Observed native utility results page')
    prior_log = logs().count('OLEX2_APP_CHECKS passed=1 count=10')
    click(nodes, text='重新执行检查')
    time.sleep(1)
    rerun_log = logs()
    record('utility_checks_rerun', rerun_log.count('OLEX2_APP_CHECKS passed=1 count=10') > prior_log, 'Actual native utility log event count increased')
    nodes = layout('layout-checks-rerun')
    click(nodes, id='olex2-nav-0')
    color(1)
    material(2)
    # A real new process must load a saved nondefault view; surface recreation alone is insufficient.
    primitive(1)
    mesh_level(2)
    mesh_mode(True)
    keyboard(2015, lambda r, b: abs(r['graphics']['yaw'] - b['graphics']['yaw'] - 10) < .1)
    keyboard(2058, lambda r, b: r['graphics']['distance'] < b['graphics']['distance'])
    persisted = wait_report(lambda r: good(r) and r.get('workspaceState', {}).get('saveStatus') == 'saved')
    record('workspace_state_saved', True, persisted)
    summary['state_before_restart'] = persisted
    prior_pid = pid
    save('cold-restart-stop.log', shell(f'aa force-stop {BUNDLE}'))
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline and shell('pidof ' + BUNDLE + ' || true').strip():
        time.sleep(.25)
    save('cold-restart-start.log', shell(f'aa start -a EntryAbility -b {BUNDLE}'))
    deadline = time.monotonic() + 12
    pid = ''
    while time.monotonic() < deadline:
        candidate = shell('pidof ' + BUNDLE + ' || true').strip()
        if candidate.isdigit() and candidate != prior_pid:
            pid = candidate
            break
        time.sleep(.25)
    record('workspace_cold_restart_new_process', pid.isdigit() and pid != prior_pid,
           {'prior_pid': prior_pid, 'new_pid': pid})
    summary['restart_pid'] = int(pid)
    restarted = wait_report(lambda r: good(r) and r.get('workspaceState', {}).get('restored') is True)
    wanted, actual = persisted['graphics'], restarted['graphics']
    record('workspace_selection_restored_after_restart',
           all(wanted[k] == actual[k] for k in ['meshKind', 'meshGeneration', 'wireframe']), restarted)
    record('workspace_view_restored_after_restart',
           all(abs(wanted[k] - actual[k]) < .0001 for k in ['yaw', 'pitch', 'distance']), restarted)
    record('workspace_color_restored_after_restart', wanted['colorPreset'] == actual['colorPreset'] == 1 and wanted['colorPacked'] == actual['colorPacked'] and color_value_matches(actual) and color_pixel_matches(actual), restarted)
    record('workspace_material_restored_after_restart',wanted['materialPreset']==actual['materialPreset']==2 and wanted['materialCanonical']==actual['materialCanonical'] and material_values_match(actual) and material_pixels_match(actual),restarted)
    summary['state_after_restart'] = restarted
    screenshot('workspace-restored-after-restart')
    # Preserve all previous flat-material restoration checks, then independently
    # exercise glossy surface recreation and a second, distinct cold process.
    primitive(0);mesh_mode(False);mesh_level(3);color(1);material(3)
    before_gloss=wait_report(lambda r: good(r) and glossy_pixels_match(r['graphics'],True))
    click(layout('layout-before-gloss-nav'),id='olex2-nav-2')
    click(layout('layout-gloss-runtime'),id='olex2-nav-0')
    after_gloss=wait_report(lambda r: good(r) and glossy_pixels_match(r['graphics'],True))
    record('glossy_surface_recreated',after_gloss['graphics']['frames']<before_gloss['graphics']['frames'],after_gloss)
    record('glossy_survives_surface_recreation',after_gloss['graphics']['materialCanonical']==before_gloss['graphics']['materialCanonical'] and glossy_pixels_match(after_gloss['graphics'],True),after_gloss)
    saved=wait_report(lambda r: good(r) and r['workspaceState']['saveStatus']=='saved' and glossy_pixels_match(r['graphics'],True))
    record('glossy_state_saved',saved['workspaceState']['schemaVersion']==4,saved)
    summary['glossy_state_before_restart']=saved;prior_gloss_pid=pid
    shell(f'aa force-stop {BUNDLE}')
    deadline=time.monotonic()+8
    while time.monotonic()<deadline and shell('pidof '+BUNDLE+' || true').strip():time.sleep(.25)
    shell(f'aa start -a EntryAbility -b {BUNDLE}')
    deadline=time.monotonic()+12;pid=''
    while time.monotonic()<deadline:
        candidate=shell('pidof '+BUNDLE+' || true').strip()
        if candidate.isdigit() and candidate!=prior_gloss_pid:pid=candidate;break
        time.sleep(.25)
    record('glossy_cold_restart_new_process',pid.isdigit() and pid!=prior_gloss_pid,{'prior_pid':prior_gloss_pid,'new_pid':pid})
    restored_gloss=wait_report(lambda r:good(r) and r['workspaceState']['restored'] and glossy_pixels_match(r['graphics'],True))
    wanted_gloss=saved['graphics'];actual_gloss=restored_gloss['graphics']
    record('glossy_restored_after_restart',all(wanted_gloss[k]==actual_gloss[k] for k in ['meshKind','meshGeneration','wireframe','colorPreset','materialPreset','materialCanonical','yaw','pitch','distance']) and glossy_pixels_match(actual_gloss,True),restored_gloss)
    summary['glossy_state_after_restart']=restored_gloss
    screenshot('glossy-restored-after-restart')
    cleared=material(0)['graphics']
    record('glossy_return_to_classic_clears_state',cleared['shininess']==0 and all(cleared['specular'+c]==0 for c in 'RGB') and cleared['specularSamples']==[] and material_values_match(cleared) and material_pixels_match(cleared),cleared)
    mesh_mode(False)
    mesh_level(3)
    color(0)
    material(0)
    click(layout('layout-final-view-reset'), text='重置视角')
    final = wait_report(lambda r: good(r) and r['workspaceState']['saveStatus'] == 'saved'
                        and abs(r['graphics']['yaw']-32) < .1 and abs(r['graphics']['distance']-4.4) < .01)
    save('native-verification.json', json.dumps(final, ensure_ascii=False, indent=2) + '\n')
    screenshot('workspace-final')
    shared_sequence = 0 # the existing final cold-start test has a different PID
    shared_original_roundtrip('final-after-surface-and-cold-restarts')
    shared_original_roundtrip('final-same-process-repeat-2')
    shared_original_roundtrip('final-same-process-repeat-3')
    summary['original_synthetic_window_path_verified'] = True
    summary['final_report'] = wait_report(good)
    summary['result'] = 'passed: diagnostic app only; complete Olex2 port remains unfinished'
except Exception as error:
    summary['result'] = 'failed'
    summary['error'] = f'{type(error).__name__}: {error}'
    print(summary['error'], flush=True)
finally:
    try:
        if remote_created:
            summary['cleanup'] = shell(f'rm -f {remote}/layout.json {remote}/screen.jpeg; rmdir {remote}')
        else:
            summary['cleanup'] = 'not needed: remote directory was not created'
    except Exception as error:
        summary['cleanup_error'] = str(error)
    summary['finished_at'] = datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds')
    summary['report_directory'] = str(report_dir.relative_to(ROOT))
    serialized = json.dumps(summary, ensure_ascii=False, indent=2) + '\n'
    save('result.json', serialized)
    (ROOT / 'reports/app/last-run.json').write_text(serialized, encoding='utf-8')
    print('Report: ' + str(report_dir), flush=True)
raise SystemExit(0 if summary['result'].startswith('passed:') else 1)
