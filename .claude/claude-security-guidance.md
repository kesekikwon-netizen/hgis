# Strata (ka-hgis) security context

Strata is a Windows desktop GIS application (C++20, Qt 6 Widgets, QGIS/GDAL/PROJ libraries from OSGeo4W) used by field archaeologists. It has no web server, no browser-rendered UI, no multi-user accounts and no database server. Reviews should be precise: report only concrete problems in the changed code.

Not applicable here, do not report: XSS, CSRF, cookies or sessions, IDOR or auth bypass between users, server-side SSRF, SQL injection against a server database, HTTP response headers.

What does matter in this codebase:

- Secrets. VWorld API keys and NGII / heritage-intranet account passwords must never be hardcoded or committed (src/, tests/, data/, scripts/, docs/). Keys are read at runtime from %LOCALAPPDATA%\ka-hgis\ka-hgis-vworld.ini, HKCU\Software\ka-hgis, the VWORLD_API_KEY environment variable, or the gitignored config/secrets.ini. Flag any literal key-like value (GUID-shaped strings) and any code that logs, prints or shows a key or password in a message box or the session log (KaSessionLog).
- Stored credentials use Windows DPAPI (CryptProtectData). The portable build's -IncludeLocalCredentials option deliberately writes plain-text credentials into the portable's config folder at the user's explicit request; that path is known and accepted, but nothing from such a folder may be committed.
- User files. Surveys (GPKG), SHP, DXF, Excel/CSV, GeoTIFF and ZIP archives come from the user or from downloads. Watch for path traversal when extracting archives (zip-slip, also via GDAL /vsizip/), writing outside the folder the user chose, and overwriting or modifying the user's original survey data outside an explicit save or export.
- Strings built into OGR SQL, subset filters (setSubsetString), GDAL connection strings or provider URIs from user-entered text (layer names, attribute values, search text) must be quoted or escaped.
- QProcess or PowerShell launched with arguments that include user-controlled text.
- Network: VWorld, NGII and heritage-intranet requests must keep TLS verification on (no ignoreSslErrors). Parsers for downloaded XML/JSON/HTML must survive malformed or hostile input without crashing.
- PowerShell scripts under scripts/: no Invoke-Expression on downloaded text, no echoing of keys or passwords.
