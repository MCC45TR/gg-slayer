#!/usr/bin/python3
"""Read-only backend fixture: never imports or calls hardware code."""
import os
import sys
import time
mode=os.environ.get('SLAYER_BACKEND_FIXTURE_MODE','good')
if mode=='hang':time.sleep(10)
if mode=='error':print('fixture refresh failed',file=sys.stderr);raise SystemExit(4)
print('value\tmodel\tFixture laptop')
print('value\thardware_writes\tallowed')
print('cap\tmodel-bios\tready\tFixture model\tTest only')

print('key-rgb\t3\t0\t0\t255')
print('key\tW\tW\t2\t3\t1\t2\t1')
print('key\tA\tA\t3\t2\t1\t3\t1')
print('key-rgb\t2\t255\t0\t0')
