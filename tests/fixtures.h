#ifndef NT_TESTS_FIXTURES_H
#define NT_TESTS_FIXTURES_H

// Files the host tests write. Everything lives under /tmp/nt-tests, which the
// Makefile points NT_CONFIG_DIR and NT_ONBOARD into, so a test never touches
// a real reader or the developer's files.

#include <QByteArray>
#include <QMap>
#include <QString>

// Removes /tmp/nt-tests. Each test calls it from init() and cleanup().
void removeFixtures();

// Writes `data` to `path`, creating the folders on the way. False on failure.
bool writeFile(const QString &path, const QByteArray &data);

// A real JPEG of the given size, so image decoding is exercised.
QByteArray jpegBytes(int width, int height);

// Writes an EPUB-like zip at `epubPath` holding `files` (member name -> bytes),
// deflated. It uses python3's zipfile module; a missing python3 makes this
// return false, so the test fails instead of skipping.
bool writeEpub(const QString &epubPath, const QMap<QString, QByteArray> &files);

#endif
