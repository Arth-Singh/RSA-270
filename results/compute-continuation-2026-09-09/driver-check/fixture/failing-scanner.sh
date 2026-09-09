#!/usr/bin/env bash
if [ "$1" = self-test ]; then echo "self-test PASSED"; exit 0; fi
echo "intentional scanner argument failure" >&2
exit 2
