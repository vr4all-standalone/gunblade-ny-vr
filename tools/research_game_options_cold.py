"""Cold-start only from a previous isolated diagnostic's saved native files."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
from run_diagnostic import command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--previous', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--roms', type=Path, required=True)
    args = parser.parse_args()
    previous = args.previous.resolve()
    research = Path('D:/VR4ALL-Gunblade-Build/research/settings-menu-20261101').resolve()
    if not previous.is_relative_to(research) or not (previous/'run.json').is_file():
        raise ValueError('Seed must be a completed isolated options diagnostic')
    cmd, output = command(args.engine, args.roms, args.output, 21,
                          Path(__file__).with_name('research_game_options.lua'))
    shutil.copytree(previous/'nvram', output/'nvram', dirs_exist_ok=True)
    env = os.environ.copy()
    env['GUNBLADE_RESEARCH_DIR'] = str(output)
    env['GUNBLADE_OPTIONS_PROFILE'] = 'cold'
    env['TMP'] = env['TEMP'] = str(output)
    with (output/'engine.log').open('w') as log:
        result = subprocess.run(cmd, cwd=output, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=180)
    errors = '[LUA ERROR]' in (output/'engine.log').read_text(errors='replace')
    (output/'run.json').write_text(json.dumps(dict(exit_code=result.returncode,
        script_errors=errors, diagnostic_ok=result.returncode == 0 and not errors,
        original_state_source=str(previous)), indent=2)+'\n')
    return result.returncode or int(errors)


if __name__ == '__main__':
    raise SystemExit(main())
