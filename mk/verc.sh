#!/bin/sh

echo -n 'const char version[] = "'
mk/vers.sh
echo '";'
