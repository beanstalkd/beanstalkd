#!/bin/sh

printf 'const char version[] = "'
mk/vers.sh
printf '";\n'
