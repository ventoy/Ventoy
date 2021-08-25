#!/bin/bash

VTOY_PATH=$PWD/../

echo "checking languages.json ..."
sh $VTOY_PATH/LANGUAGES/check.sh $VTOY_PATH || exit 1

echo "generating languages.json ..."

echo "var vtoy_language_data = " > languages.js
cat $VTOY_PATH/LANGUAGES/languages.json  >> languages.js
echo ";" >> languages.js

dos2unix languages.js

rm -f WebUI/static/js/languages.js
mv languages.js WebUI/static/js/

echo "====== SUCCESS =========="
