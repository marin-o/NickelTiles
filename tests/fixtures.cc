#include "fixtures.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QStringList>

namespace {

// QStringLiteral needs a literal, so the root is a macro rather than a constant.
#define NT_TEST_ROOT "/tmp/nt-tests"

// Zips a folder tree with deflate, as real EPUBs are, so the reader's inflate
// path runs. Member names use '/' on every platform.
const char kZipScript[] = "import os, sys, zipfile\n"
                          "src, out = sys.argv[1], sys.argv[2]\n"
                          "with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:\n"
                          "    for root, dirs, files in os.walk(src):\n"
                          "        for name in sorted(files):\n"
                          "            path = os.path.join(root, name)\n"
                          "            z.write(path, os.path.relpath(path, src).replace(os.sep, '/'))\n";

}   // namespace

void removeFixtures() {
    QDir(QStringLiteral(NT_TEST_ROOT)).removeRecursively();
}

bool writeFile(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(data) == data.size();
}

QByteArray jpegBytes(int width, int height) {
    QImage img(width, height, QImage::Format_RGB32);
    img.fill(Qt::gray);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG");
    buf.close();
    return bytes;
}

bool writeEpub(const QString &epubPath, const QMap<QString, QByteArray> &files) {
    // Stage the members in a folder named after the book, so two books never share one.
    const QString stage = QStringLiteral(NT_TEST_ROOT) + QStringLiteral("/stage/") + QFileInfo(epubPath).fileName();
    QDir(stage).removeRecursively();
    for (auto it = files.constBegin(); it != files.constEnd(); ++it)
        if (!writeFile(stage + QLatin1Char('/') + it.key(), it.value())) return false;

    QDir().mkpath(QFileInfo(epubPath).absolutePath());
    QStringList args;
    args << QStringLiteral("-c") << QLatin1String(kZipScript) << stage << epubPath;
    return QProcess::execute(QStringLiteral("python3"), args) == 0;
}
