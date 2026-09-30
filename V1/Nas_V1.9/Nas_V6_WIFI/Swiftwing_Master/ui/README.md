# Swiftwing Master UI

The editable HTML and CSS pages live here. build_webui.py in the parent folder
inlines each page stylesheet and the shared theme.css, gzip-compresses the
pages, and embeds all four pages into Swiftwing_Master.ino for serving from flash.

After changing a page or stylesheet, run python .\build_webui.py from this
folder.

The gzip routes keep their existing Content-Encoding: gzip headers and API
contracts. The source pages do not load any external fonts, scripts, or assets.
