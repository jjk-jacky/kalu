#!/bin/sh

if ! which autoreconf >/dev/null 2>&1; then
  echo autoreconf is required 1>&2
  exit 1
fi

autoreconf -i
