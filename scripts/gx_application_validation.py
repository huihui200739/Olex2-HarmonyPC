"""Independently check limited empty-GX and system-image observations.

These checks do not certify a complete application, renderer, transparent PNG
backend, font implementation, or physical device. Synthetic image references
are generated separately from the target decoder's output.
"""

def require(condition, message):
    if not condition:
        raise ValueError(message)

def integer(value, name, minimum=0):
    require(type(value) is int and value >= minimum, 'Invalid integer: '+name)
    return value

def validate_lifecycle(report):
    require(report.get('execution_completed') is True, 'GX lifecycle did not complete')
    cycles=report.get('cycles')
    require(isinstance(cycles,list) and len(cycles)==3, 'Expected three genuine GX cycles')
    for number,row in enumerate(cycles,1):
        require(type(row.get('cycle')) is int and row['cycle']==number, 'GX cycle order differs')
        expected={key:1 for key in ('basic_identity','core_identity','graphics_identity',
            'scene_owned_by_renderer','scene_disabled','graphics_file_type','no_loaded_file',
            'mouse_library','state_registry_identity','structure_visible','singleton_cleared')}
        expected.update(file_count=1,frame_visible=0,font_count=0,font_requests=0,
                        scene_destroyed=1,make_current_attempts=1,macro_count=193,function_count=46)
        for key,wanted in expected.items():
            require(type(row.get(key)) is int and row[key]==wanted, 'GX '+str(number)+' differs: '+key)
    return dict(cycles=3,scene_destructions=3,singleton_releases=3,
                macro_count=193,function_count=46,gpu_context_created=False,font_backend_tested=False)

def _reference(fixture):
    w=integer(fixture.get('width'),'fixture width',1);h=integer(fixture.get('height'),'fixture height',1)
    c=integer(fixture.get('channels'),'fixture channels',3)
    require(c in (3,4),'Unsupported fixture color layout')
    pixels=fixture.get('pixels')
    require(isinstance(pixels,list) and len(pixels)==w*h*c and all(type(v) is int and 0<=v<=255 for v in pixels),'Invalid synthetic reference pixels')
    rgba=[v for i in range(w*h) for v in (pixels[i*c:i*c+3]+[pixels[i*c+3] if c==4 else 255])]
    return w,h,rgba

def _pixels(observation,w,h,channels,expected_format):
    for key,wanted in [('source_width',w),('source_height',h),('width',w),('height',h),('actual_format',expected_format),('requested_format',expected_format)]:
        require(type(observation.get(key)) is int and observation[key]==wanted,'Decoded metadata differs: '+key)
    stride=integer(observation.get('stride'),'stride',w*channels)
    read=integer(observation.get('read_size'),'read size',(h-1)*stride+w*channels)
    capacity=integer(observation.get('buffer_size'),'buffer size',read)
    require(capacity==stride*h and read<=capacity,'Native read/buffer geometry differs')
    pixels=observation.get('bytes')
    require(isinstance(pixels,list) and len(pixels)==read and all(type(v) is int and 0<=v<=255 for v in pixels),'Decoded byte payload differs')
    require(type(observation.get('alpha_type')) is int and observation['alpha_type'] in (0,1,2,3),'Invalid alpha metadata')
    return [v for y in range(h) for v in pixels[y*stride:y*stride+w*channels]]

def validate_image_resource_audit(audit,expected_created=None,expected_attempted=None,require_image_handles=True,proof="full-metadata"):
    require(proof in ("full-metadata","owned-memory"),"Unknown image resource proof")
    """Independent owned-resource proof, not a claim about all SDK internals."""
    require(isinstance(audit,dict),'Missing owned image resource audit')
    require(audit.get('overflow') is False,'Image resource audit overflowed')
    if proof=='full-metadata':require(audit.get('balanced') is True,'Full descriptor metadata audit did not pass')
    else:
        require(audit.get('owned_scope_balanced') is True,'Owned memory/image release scope failed')
        require(audit.get('input_kind')=='owned-memory-buffer' and audit.get('sdk_received_file_descriptor') is False,'Decoder did not use the owned memory route')
    f=audit.get('file');require(isinstance(f,dict),'Missing owned input descriptor audit')
    for key in ('open_attempted','opened','identity_known','close_attempted','scan_completed'):
        require(type(f.get(key)) is bool,'Invalid descriptor flag: '+key)
    handles=audit.get('handles');require(isinstance(handles,list),'Missing image handle audits')
    if expected_created is not None:
        require([h.get('name') for h in handles if h.get('created') is True]==expected_created,'Acquired image objects differ from executed path')
    if expected_attempted is not None:
        require([h.get('name') for h in handles if h.get('creation_observed') is True]==expected_attempted,'Image object creation attempts differ from executed path')
    if not f['open_attempted']:
        require(not handles and not f['opened'] and not f['close_attempted'],'Unopened input has resources')
        return dict(created=0,released=0,input_opened=False)
    if not f['opened']:
        require(integer(f.get('open_errno'),'open errno',1)>0 and f.get('descriptor')==-1,'Failed open lacks actual errno')
        require(not f['identity_known'] and not f['close_attempted'] and not f['scan_completed'] and not handles,'Failed open was reported as owned')
        for key in ('device','inode','mode','identity_errno','close_code','close_errno','remaining_input_descriptors'):
            require(type(f.get(key)) is int and f[key]==0,'Failed open contains stale resource metadata: '+key)
        return dict(created=0,released=0,input_opened=False)
    integer(f.get('descriptor'),'owned input descriptor');integer(f.get('device'),'owned input device')
    integer(f.get('inode'),'owned input inode',1)
    mode=integer(f.get('mode'),'owned input mode');require(mode&0o170000==0o100000,'Owned input is not a regular synthetic file')
    require(f['identity_known'] and f['close_attempted'],'Input identity/close incomplete')
    if proof=='full-metadata':require(f['scan_completed'],'Complete descriptor metadata scan unavailable')
    else:
        encoded=integer(audit.get('encoded_bytes'),'owned encoded byte count',1)
        require(encoded<=64*1024*1024 and audit.get('buffer_released_after_handles') is True,'Encoded bytes did not outlive the image objects')
        require(type(audit.get('balanced')) is bool and audit['balanced'] is f['scan_completed'],'Full metadata audit was misrepresented')
        scan_error=integer(f.get('scan_errno'),'descriptor scan errno')
        failures=integer(f.get('scan_stat_failures'),'descriptor scan stat failures')
        require((scan_error==0 and failures==0) if f['scan_completed'] else scan_error>0,'Incomplete scan lacks actual failure evidence')
    for key in ('open_errno','identity_errno','close_code','close_errno','remaining_input_descriptors'):
        require(type(f.get(key)) is int and f[key]==0,'Owned input resource differs: '+key)
    required_names=['source','source-info','options','pixelmap','pixel-info'] if require_image_handles else []
    require([h.get('name') for h in handles]==required_names,'Unexpected/missing image handle records')
    count=0
    for h in handles:
        for key in ('creation_observed','created','release_attempted'):
            require(type(h.get(key)) is bool,'Invalid image handle flag: '+key)
        creation=integer(h.get('creation_code'),'image creation code')
        release=integer(h.get('release_code'),'image release code')
        require(h['release_attempted'] is h['created'],'Image handle release does not match acquired object')
        require(release==0,'Image handle release returned an error')
        if h['created']:
            require(h['creation_observed'] and creation==0,'Unexpected object creation result')
            count+=1
        elif h['creation_observed']:
            require(creation!=0,'Successful creation returned no object')
        else:require(creation==0,'Unattempted image handle has a creation result')
    return dict(created=count,released=count,input_opened=True)

def validate_image_observations(observations,fixtures,require_resources=False,resource_proof="full-metadata"):
    require(isinstance(observations,list) and isinstance(fixtures,list),'Expected image observations and synthetic fixtures')
    references={f['file']:f for f in fixtures}
    require(len(references)==len(fixtures) and references,'Duplicate or missing synthetic fixture')
    indexed={}
    for record in observations:
        key=(record.get('fixture'),record.get('requested'))
        require(key not in indexed and key[0] in references and key[1] in ('rgb','rgba'),'Duplicate/unexpected image observation')
        indexed[key]=record.get('observation')
    require(set(indexed)=={(name,mode) for name in references for mode in ('rgb','rgba')},'Missing native image observation')
    rgb_supported=[];transparent_preserved=[];opaque_count=0;details=[]
    for name,fixture in references.items():
        w,h,expected=_reference(fixture);transparent=any(expected[i]!=255 for i in range(3,len(expected),4))
        rgba=indexed[(name,'rgba')]
        require(isinstance(rgba,dict) and rgba.get('execution_completed') is True,'Native RGBA decoding did not complete: '+name)
        if require_resources or 'resource_audit' in rgba:
            validate_image_resource_audit(rgba.get('resource_audit'),['source','source-info','options','pixelmap','pixel-info'],['source','source-info','options','pixelmap','pixel-info'],proof=resource_proof)
        actual=_pixels(rgba,w,h,4,3)
        if transparent:
            premultiplied=[v for i in range(0,len(expected),4) for v in ([(expected[i+c]*expected[i+3]+127)//255 for c in range(3)]+[expected[i+3]])]
            require(actual==expected or actual==premultiplied,'Unrecognized native alpha/color conversion: '+name)
            preserved=actual==expected;transparent_preserved.append(preserved)
        else:
            require(actual==expected,'Opaque RGB/row orientation differs: '+name)
            opaque_count+=1;preserved=True
        rgb=indexed[(name,'rgb')]
        require(isinstance(rgb,dict),'Missing RGB capability result')
        if require_resources or 'resource_audit' in rgb:
            names=['source','source-info','options','pixelmap','pixel-info']
            validate_image_resource_audit(rgb.get('resource_audit'),names if rgb.get('execution_completed') is True else names[:3],names if rgb.get('execution_completed') is True else names[:4],proof=resource_proof)
        if rgb.get('execution_completed') is True:
            packed=_pixels(rgb,w,h,3,5)
            require(packed==[v for i,v in enumerate(expected) if i%4!=3],'Native RGB decoding changed source colors')
            rgb_supported.append(True)
        else:
            require(rgb.get('execution_completed') is False and rgb.get('error')=='decode error 7700301','Unclassified native RGB decode failure')
            rgb_supported.append(False)
        details.append(dict(fixture=name,opaque=not transparent,rgba_preserves_original_rgb=preserved,
                            alpha_metadata=rgba['alpha_type'],row_stride=rgba['stride']))
    require(opaque_count>0 and transparent_preserved,'Need both opaque and transparent observations')
    return dict(fixtures=len(fixtures),observations=len(observations),opaque_color_matches=opaque_count,
                native_rgb888_supported=all(rgb_supported),
                transparent_original_rgb_preserved=all(transparent_preserved),
                complete_texture_backend=False,details=details)

def validate_texture_hook(report,fixtures,require_resources=False,resource_proof="full-metadata"):
    """Verify actual native decoder results at the original virtual seam.

    Unsupported transparency is an explicit remaining limit, not success of a
    complete image backend. No original texture upload/GL pixels are inferred.
    """
    require(report.get('execution_completed') is True,'Platform texture seam did not complete')
    references={f['file']:f for f in fixtures}
    require('synthetic-rgb-2.png' in references,'Missing Unicode reference image')
    references['合成颜色-2.png']=references['synthetic-rgb-2.png']
    expected_names=['synthetic-rgb-2.png','synthetic-rgba-2.png','synthetic-rgb-1.png',
                    'synthetic-rgb-3.png','synthetic-opaque-rgba-4.png','合成颜色-2.png',
                    'synthetic-no-image-selected.png']
    cycles=report.get('cycles')
    require(isinstance(cycles,list) and len(cycles)==3,'Expected three platform texture application cycles')
    decoded=unsupported=0
    for number,cycle in enumerate(cycles,1):
        for key,wanted in [('cycle',number),('original_graphics_identity',1),('scene_destroyed',1),
                           ('font_requests',0),('make_current_attempts',1),('singleton_cleared',1)]:
            require(type(cycle.get(key)) is int and cycle[key]==wanted,'Texture lifecycle differs: '+key)
        before=integer(cycle.get('fd_before'),'own fd before',1)
        after=integer(cycle.get('fd_after'),'own fd after',1)
        require(before==after,'Platform image descriptors were not balanced')
        images=cycle.get('images')
        require(isinstance(images,list) and [row.get('fixture') for row in images]==expected_names,'Texture seam image order/content differs')
        for row in images:
            name=row['fixture']
            if name=='synthetic-no-image-selected.png':
                w=h=0;expected=[];status='file-error'
            else:
                w,h,rgba=_reference(references[name])
                if w!=h or w&(w-1):status='unsupported-dimensions'
                elif any(rgba[i]!=255 for i in range(3,len(rgba),4)):status='unsupported-transparency'
                else:status='decoded'
                expected=[v for i,v in enumerate(rgba) if i%4!=3] if status=='decoded' else []
            if require_resources or 'resource_audit' in row:
                names=[] if status=='file-error' else ['source','source-info'] if status=='unsupported-dimensions' else ['source','source-info','options','pixelmap','pixel-info']
                validate_image_resource_audit(row.get('resource_audit'),names,names,proof=resource_proof)
            require(row.get('status')==status and row.get('accepted') is (status=='decoded'),'Wrong platform image decision: '+name)
            for key,wanted in [('width',w),('height',h),('size',w if status=='decoded' else 0)]:
                require(type(row.get(key)) is int and row[key]==wanted,'Texture image metadata differs: '+name+' '+key)
            rgb=row.get('rgb')
            require(isinstance(rgb,list) and all(type(x) is int and 0<=x<=255 for x in rgb) and rgb==expected,'Original texture-seam RGB differs: '+name)
            require(isinstance(row.get('error'),str) and bool(row['error'])==(status!='decoded'),'Texture error/result disagree')
            if status in ('decoded','unsupported-transparency'):
                stride=integer(row.get('stride'),'texture row stride',w*4)
                integer(row.get('read_bytes'),'native texture read bytes',(h-1)*stride+w*4)
                require(type(row.get('format')) is int and row['format']==3,'Texture decoder did not return RGBA8888')
                require(type(row.get('alpha_metadata')) is int and row['alpha_metadata'] in (0,1,2,3),'Texture alpha metadata differs')
            else:
                for key in ('stride','read_bytes','format','alpha_metadata'):
                    require(type(row.get(key)) is int and row[key]==0,'Unsupported image reached native pixel decode: '+key)
            if status=='decoded':decoded+=1
            else:unsupported+=1
    return dict(cycles=3,opaque_decodes=decoded,explicit_unsupported_or_missing=unsupported,
                own_descriptor_counts_balanced=True,process_descriptor_counts_balanced=True,unicode_filename_tested=True,
                original_virtual_texture_seam=True,original_texture_gl_upload_tested=False,
                transparent_png_backend_complete=False,complete_application_backend=False)


def inspect_owned_resource_observations(report,proof="full-metadata",texture_backend="imagekit-opaque"):
    """Report only observed scopes, including a failed process-count cycle.

    This does not turn an incomplete texture lifecycle or fd mismatch into pass.
    """
    import json
    observations=report.get('images')
    require(isinstance(observations,list) and observations,'Missing native image probes')
    require(texture_backend in ('imagekit-opaque','libpng-unassociated-rgb'),'Unknown texture resource backend')
    rows=[(r.get('observation',{}),False) for r in observations]
    hook=report.get('texture_hook',{})
    complete=hook.get('execution_completed') is True
    if complete:cycles=hook.get('cycles')
    else:
        partial=hook.get('partial_trace')
        require(isinstance(partial,str),'Missing failed-cycle resource trace')
        cycles=json.loads(partial+']}').get('cycles')
    require(isinstance(cycles,list) and cycles,'No actual texture image cycle observed')
    for c in cycles:
        require(isinstance(c.get('images'),list) and len(c['images'])==7,'Incomplete failed-cycle image trace')
        rows.extend((row,texture_backend=='libpng-unassociated-rgb') for row in c['images'])
    created=released=opened=0
    for r,is_png in rows:
        if is_png:validate_png_decoder_resources(r.get('png_decoder'),opened=r.get('status')!='file-error')
        audit=validate_image_resource_audit(r.get('resource_audit'),require_image_handles=not is_png,proof=proof)
        created+=audit['created'];released+=audit['released'];opened+=int(audit['input_opened'])
    return dict(passed=True,proof=proof,complete_descriptor_metadata_scan_passed=all(r["resource_audit"]["balanced"] for r,is_png in rows),observed_image_scopes=len(rows),observed_hook_cycles=len(cycles),
                hook_completed=complete,owned_input_files_opened=opened,owned_input_files_closed=opened,
                acquired_public_image_handles=created,successful_public_handle_release_results=released,
                observed_surviving_input_inode_descriptors=0,unknown_descriptor_identities_remain=not all(r["resource_audit"]["balanced"] for r,is_png in rows),all_process_descriptors_proven_owned=False,
                all_sdk_internal_allocations_proven_released=False)


def validate_no_decoder_control(control,expected_stage=None):
    require(isinstance(control,dict) and control.get('execution_completed') is True,'No-decoder control did not complete')
    if expected_stage is not None:require(control.get('stage')==expected_stage,'Descriptor control stage differs')
    samples=control.get('samples');require(isinstance(samples,list) and len(samples)==41,'Expected fixed 41-sample no-decoder control')
    counts=[];identities=[];elapsed=[];inaccessible=0
    for number,row in enumerate(samples):
        require(type(row.get('sample')) is int and row['sample']==number,'No-decoder sample order differs')
        elapsed.append(integer(row.get('elapsed_ms'),'no-decoder elapsed milliseconds'))
        entries=row.get('identities');require(isinstance(entries,list),'Missing no-decoder descriptor identities')
        count=integer(row.get('count_excluding_scan_descriptor'),'no-decoder descriptor count',1)
        require(count==len(entries),'No-decoder count and identities differ')
        numbers=set()
        for entry in entries:
            require(isinstance(entry,str),'Invalid no-decoder identity')
            fields=entry.split(':');require(fields[0].isdigit() and fields[0] not in numbers,'Duplicate/invalid no-decoder descriptor')
            numbers.add(fields[0])
            if len(fields)==3 and fields[1]=='fstat-error':
                require(fields[2].isdigit() and int(fields[2])>0,'Missing descriptor inspection errno');inaccessible+=1
            else:require(len(fields)==4 and all(f.isdigit() for f in fields),'Unlabeled/invented descriptor identity')
        counts.append(count);identities.append(set(entries))
    require(elapsed==sorted(elapsed) and elapsed[-1]>=1000,'No-decoder control duration/order differs')
    return dict(samples=41,elapsed_ms=elapsed[-1],minimum_descriptor_count=min(counts),maximum_descriptor_count=max(counts),
                descriptor_count_changes=sum(a!=b for a,b in zip(counts,counts[1:])),
                descriptor_identity_changes=sum(a!=b for a,b in zip(identities,identities[1:])),
                inaccessible_descriptor_observations=inaccessible,descriptor_ownership_attributed=False)


def validate_png_decoder_resources(record,opened=True):
    """Only allocations observed by the fixed libpng allocation callbacks."""
    require(isinstance(record,dict) and record.get('version')=='1.6.59','Missing fixed PNG decoder identity')
    for key in ('allocations','releases','live_allocations','live_bytes','warnings'):
        integer(record.get(key),'PNG callback '+key)
    require(record['allocations']==record['releases'] and record['live_allocations']==record['live_bytes']==0,'PNG allocation callbacks did not balance')
    require(record['warnings']==0,'Ordinary PNG emitted decoder warnings')
    require(record['allocations']>0 if opened else record['allocations']==0,'PNG callback scope differs from actual input path')
    return dict(allocations=record['allocations'],releases=record['releases'],all_process_resources_proven_released=False)
