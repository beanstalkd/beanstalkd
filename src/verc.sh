#!/bin/sh

printf 'const char version[] = "'
./vers.sh
printf '";\n'
