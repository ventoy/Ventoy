#!/bin/bash

echo "generating languages.js ..."

iconv -f utf-16 -t utf-8 ../LANGUAGES/languages.ini  | egrep -v '=STR|^;' | egrep  'Language-|STR_' > languages.js

dos2unix languages.js

sed 's/\(STR_.*\)=/"\1":/g' -i languages.js

sed "s/: *'/:\"/g" -i languages.js

sed "s/'\s*$/\",/g" -i languages.js

sed 's/\[Language-\(.*\)\].*/"STR_XXX":""},{"name":"\1",/g' -i languages.js

sed "1s/.*\},/var vtoy_language_data = \[/" -i languages.js

sed 's/\("STR_WEB_COMMUNICATION_ERR"[^,]*\)/\1,/g' -i languages.js
sed 's/,,/,/g' -i languages.js

CNT=$(grep -v -c ',$' languages.js)

if [ $CNT -gt 0 ]; then
    echo "====== FAILED ========="
    grep -v -n ',$' languages.js
    exit 1
fi


echo '"STR_XXX":""}' >> languages.js
echo '];' >> languages.js

rm -f WebUI/static/js/languages.js
mv languages.js WebUI/static/js/

echo "====== SUCCESS =========="