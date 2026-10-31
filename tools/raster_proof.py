"""Bounded native CPU-raster experiment; fast mode is opt-in, never a release default.

GUNBLADE_FAST_RASTER=1 requests the guarded skip. GUNBLADE_CPU_RASTER
(including a value of 0) or GUNBLADE_SCENE_DIR always retains software raster.
Evidence is written only to a fresh private output directory supplied by the user.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from run_diagnostic import command

REGIONS = {'workram': (0x500000, 0x100000), 'extraram': (0x200000, 0x40000),
           'backup': (0x1d00000, 0x4000)}

def compare(reference, fast, *, fallback=False):
    reference,fast=Path(reference),Path(fast)
    files=sorted(path.name for path in reference.glob('*.bin'))
    if not files:raise ValueError('No RAM checkpoint evidence')
    if files!=sorted(path.name for path in fast.glob('*.bin')):
        raise ValueError('RAM checkpoint sets differ')
    counts={region:0 for region in REGIONS};total=0;checkpoints={region:set() for region in REGIONS}
    for name in files:
        region,second=name.removesuffix('.bin').split('-')
        base,size=REGIONS[region]
        a=(reference/name).read_bytes();b=(fast/name).read_bytes()
        if a!=b:
            offset=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
            raise ValueError(f'{name}: byte mismatch at {base+offset:#x}')
        if len(a)!=size:raise ValueError(f'{name}: incomplete RAM checkpoint')
        counts[region]+=1;total+=len(a);checkpoints[region].add(int(second))
    seconds=checkpoints['workram']
    if any(found!=seconds for found in checkpoints.values()) or seconds!=set(range(max(seconds)+1)):
        raise ValueError('Incomplete region checkpoint coverage')
    if (reference/'native.tsv').read_bytes()!=(fast/'native.tsv').read_bytes():
        raise ValueError('Native time/state/register trace differs')
    decisions={}
    for name,folder in [('reference',reference),('fast',fast)]:
        counts_by_decision={}
        with (folder/'raster.tsv').open() as stream:
            for row in csv.DictReader(stream,delimiter='\t'):
                decision=row['decision'];counts_by_decision[decision]=counts_by_decision.get(decision,0)+1
                if decision=='direct_framebuffer' and int(row['polygons']):
                    raise ValueError('Direct framebuffer published retained geometry')
        decisions[name]=counts_by_decision
    if decisions['reference'].get('skip',0):raise ValueError('CPU reference skipped native raster')
    direct_seconds=[];seen=set()
    with (fast/'native.tsv').open() as stream:
        for row in csv.DictReader(stream,delimiter='\t'):
            second=int(float(row['time']))
            if second in seen:continue
            seen.add(second)
            if int(row['render_mode'])&1:
                name=f'frame-{second:03d}.png'
                if not (reference/name).is_file() or not (fast/name).is_file():
                    raise ValueError('Missing direct framebuffer capture')
                if (reference/name).read_bytes()!=(fast/name).read_bytes():
                    raise ValueError(f'Direct framebuffer capture differs: {name}')
                direct_seconds.append(second)
    if decisions['fast'].get('direct_framebuffer',0) and not direct_seconds:
        raise ValueError('Direct framebuffer observed without a timed capture')
    if fallback:
        if decisions['fast'].get('skip',0):raise ValueError('Fallback path skipped native raster')
        images=sorted(path.name for path in reference.glob('frame-*.png'))
        if not images or images!=sorted(path.name for path in fast.glob('frame-*.png')):
            raise ValueError('Fallback capture checkpoint sets differ')
        for name in images:
            if (reference/name).read_bytes()!=(fast/name).read_bytes():
                raise ValueError(f'Fallback capture differs: {name}')
    elif not decisions['fast'].get('skip',0):raise ValueError('Fast raster was never exercised')
    return dict(ram_checkpoints=counts,ram_bytes_compared=total,
                native_trace_identical=True,decisions=decisions,
                fallback_pngs_identical=len(images) if fallback else None,
                direct_framebuffer_png_seconds=direct_seconds,
                direct_framebuffer_observed=bool(decisions['fast'].get('direct_framebuffer',0)))

def run_pair(engine, roms, host, output, seconds=45, mode='gameplay'):
    output=Path(output).resolve()
    if output.exists():raise FileExistsError(output)
    output.mkdir(parents=True)
    frozen=output/'gunblade_xr.dll';shutil.copyfile(host,frozen)
    runs={}
    for name in ('reference','fast'):
        args,folder=command(engine,roms,output/name,seconds,Path(__file__).with_suffix('.lua'))
        env={key:value for key,value in os.environ.items() if not key.startswith('GUNBLADE_')}
        env.update(GUNBLADE_VR='1',GUNBLADE_XR_SIM='1',GUNBLADE_FAST_RASTER='1',
                   GUNBLADE_XR_LIBRARY=str(frozen),GUNBLADE_RESEARCH_DIR=str(folder),
                   GUNBLADE_DIAGNOSTIC_MODE=mode,GUNBLADE_RASTER_LOG=str(folder/'raster.tsv'),
                   GUNBLADE_XR_LOG=str(folder/'xr.log'))
        if name=='reference':env['GUNBLADE_CPU_RASTER']='0' # Presence forces CPU.
        env['TMP']=env['TEMP']=str(folder)
        env['PATH']='D:/msys64/mingw64/bin;'+env.get('PATH','')
        start=time.monotonic()
        with (folder/'engine.log').open('w') as log:
            child=subprocess.Popen(args,cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT)
            try:code=child.wait(timeout=max(120,seconds*4))
            except subprocess.TimeoutExpired:
                child.kill();child.wait();raise RuntimeError('Bounded native proof timed out')
        if code or '[LUA ERROR]' in (folder/'engine.log').read_text(errors='replace'):
            raise RuntimeError(f'{name} native diagnostic failed: {code}')
        runs[name]={'wall_seconds':time.monotonic()-start,'native_seconds':seconds}
    result=compare(output/'reference',output/'fast',fallback=mode!='gameplay')
    result.update(runs=runs,mode=mode,fast_default=False)
    (output/'proof.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('engine','roms','host','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--seconds',type=int,default=45)
    parser.add_argument('--mode',choices=('gameplay','service','attract'),default='gameplay')
    run_pair(**vars(parser.parse_args()))
