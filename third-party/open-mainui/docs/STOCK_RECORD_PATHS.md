# Stock saved-record path compatibility

The cache row is the saved identity. ROM Favorites and Recents copy its path
and imgpath strings unchanged. Browsing, validation and artwork resolution use
separate normalized host paths. Exact identity preserves stock Favorites and
Recents membership, including records copied by GameSwitcher.

Scanned ROM rows use the console directory plus raw config rompath plus the
ROM-root-relative filename (including subfolders). Images use raw config imgpath
plus the basename without its extension and .png, whether or not the PNG exists.
Folder path and imgpath are identical, with empty pinyin/cpinyin. Parent keys are
. at the root and root-relative folder names without ./ below it.

XML ROM rows preserve the XML path text, including ./, under the raw ROM prefix.
Their image comes from XML image under the same ROM prefix, or is empty when
absent. XML names supply disp. Config imgpath and filename-derived images do not
override XML images. Both import paths retain normalized host validation, bounds,
root containment and existence checks. Absolute config prefixes and absolute XML
paths are used directly rather than being prefixed with a console directory.

Deeper scan nesting and XML subfolder parent/folder rules are inferred from the
verified single-level scan convention; these cases are not yet verified against
stock device output. Generated catalog tests cover scan, XML and absolute paths.

Only a cacheless directory scan computes a record from raw config values.
Search and Apps retain their existing record behavior. Recents preserves the
supplied record, and favorite markers, context actions and Details compare that
record's exact rompath.

