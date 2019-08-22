#!/usr/bin/env python

with open('testserv.c') as f:
    lines = f.readlines()

test_prefix = 'cttest_'
cmd_prefix = 'mustsend(fd, "'

test = ''
for line in lines:
    line = line.strip()
    if test_prefix in line:
        test = line[len(test_prefix):line.index('(')]
        continue
    if cmd_prefix in line and r'\r\n' in line:
        cmd = line[line.index(cmd_prefix)+len(cmd_prefix):line.index(r'\r\n')]
        with open('testcases/'+test+'.txt', 'a') as fo:
            fo.write(cmd + '\r\n')
