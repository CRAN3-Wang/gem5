#!/usr/bin/env bash
# Supported-Python build helper; does not modify the parent project.
set -euo pipefail
gem5_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
python="${P1_PYTHON:-$gem5_root/build/p1-venv/bin/python}"
if [[ ! -x "$python" ]]; then
    echo "Set P1_PYTHON to a Python 3.10-3.12 environment with SCons installed." >&2
    exit 1
fi
"$python" -c 'import sys; assert (3,10) <= sys.version_info[:2] < (3,13), "Python 3.10-3.12 required"'
python_base="$("$python" -c 'import sys; print(sys.base_prefix)')"
export PYTHON_CONFIG="${PYTHON_CONFIG:-$python_base/bin/python3-config}"
export LD_LIBRARY_PATH="$python_base/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd "$gem5_root"
exec "$python" -m SCons build/X86/gem5.fast -j "${JOBS:-2}" "$@"
