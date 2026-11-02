"""Run the production native option transaction against isolated original game."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from run_diagnostic import command


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for arg in ('engine','roms','output','helper'):parser.add_argument('--'+arg,type=Path,required=True)
    parser.add_argument('--option',type=int,default=0)
    parser.add_argument('--value',type=int,default=0)
    parser.add_argument('--queue',action='store_true')
    parser.add_argument('--seconds',type=int,default=40)
    args=parser.parse_args()
    engine_sha256=hashlib.sha256(args.engine.read_bytes()).hexdigest()
    cmd,output=command(args.engine,args.roms,args.output,args.seconds,Path(__file__).with_name('research_game_options_bridge.lua'))
    env=os.environ.copy();env['GUNBLADE_RESEARCH_DIR']=str(output);env['TMP']=env['TEMP']=str(output)
    with (output/'helper.log').open('w') as helperlog,(output/'engine.log').open('w') as enginelog:
        helper=subprocess.Popen([str(args.helper.resolve()),str(output),'queue' if args.queue else str(args.option),str(args.value)],env=env,stdout=helperlog,stderr=subprocess.STDOUT)
        try:
            engine=subprocess.run(cmd,cwd=output,env=env,stdout=enginelog,stderr=subprocess.STDOUT,timeout=180)
            helpercode=helper.wait(timeout=15)
        finally:
            if helper.poll() is None:helper.terminate();helper.wait(timeout=10)
    script_errors='[LUA ERROR]' in (output/'engine.log').read_text(errors='replace')
    ok=engine.returncode==0 and helpercode==0 and not script_errors
    (output/'run.json').write_text(json.dumps(dict(exit_code=engine.returncode,helper_exit_code=helpercode,script_errors=script_errors,diagnostic_ok=ok,option=args.option,value=args.value,queue=args.queue,engine_sha256=engine_sha256),indent=2)+'\n')
    return int(not ok)


if __name__=='__main__':raise SystemExit(main())
