#!/bin/sh

MAJOR=63
MINOR=1

mv libicuuc.so.$MAJOR.$MINOR libicuuc.$MAJOR.$MINOR.so
mv libicudata.so.$MAJOR.$MINOR libicudata.$MAJOR.$MINOR.so
mv libicui18n.so.$MAJOR.$MINOR libicui18n.$MAJOR.$MINOR.so
