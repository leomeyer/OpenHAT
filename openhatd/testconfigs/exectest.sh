#!/bin/sh

echo "Starting exectest..."

exitcode=$1

sleep 2

echo "Terminating exectest..."
echo "A message that goes to stderr..." >&2

exit $1
