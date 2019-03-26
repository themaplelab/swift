import shlex
import subprocess
import sys

if len(sys.argv) < 2:
    print("Too few args to " + sys.argv[0])
    sys.exit(0)

try:
    subprocess.check_call(shlex.split(' '.join(sys.argv[1:])))
    sys.exit(1)
except subprocess.CalledProcessError as e:
    sys.exit(0)
