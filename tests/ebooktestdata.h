#pragma once

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryFile>
#include <QtEndian>

namespace EbookTestData {

inline quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (unsigned char b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc ^ 0xFFFFFFFFu;
}

inline bool writeStoredZip(const QString &path, const QList<QPair<QByteArray, QByteArray>> &entries)
{
    QByteArray archive;
    QByteArray centralDir;
    auto put16 = [](QByteArray &out, quint16 v) {
        char buf[2];
        qToLittleEndian(v, buf);
        out.append(buf, 2);
    };
    auto put32 = [](QByteArray &out, quint32 v) {
        char buf[4];
        qToLittleEndian(v, buf);
        out.append(buf, 4);
    };

    for (const auto &entry : entries) {
        const QByteArray &name = entry.first;
        const QByteArray &data = entry.second;
        const quint32 offset = static_cast<quint32>(archive.size());
        const quint32 crc = crc32(data);
        const quint32 sz = static_cast<quint32>(data.size());

        // Local file header
        put32(archive, 0x04034B50u);
        put16(archive, 20);
        put16(archive, 0);
        put16(archive, 0); // STORED
        put16(archive, 0);
        put16(archive, 0);
        put32(archive, crc);
        put32(archive, sz);
        put32(archive, sz);
        put16(archive, static_cast<quint16>(name.size()));
        put16(archive, 0);
        archive.append(name);
        archive.append(data);

        // Central directory header
        put32(centralDir, 0x02014B50u);
        put16(centralDir, 20);
        put16(centralDir, 20);
        put16(centralDir, 0);
        put16(centralDir, 0);
        put16(centralDir, 0);
        put16(centralDir, 0);
        put32(centralDir, crc);
        put32(centralDir, sz);
        put32(centralDir, sz);
        put16(centralDir, static_cast<quint16>(name.size()));
        put16(centralDir, 0);
        put16(centralDir, 0);
        put16(centralDir, 0);
        put16(centralDir, 0);
        put32(centralDir, 0);
        put32(centralDir, offset);
        centralDir.append(name);
    }

    const quint32 cdOffset = static_cast<quint32>(archive.size());
    const quint32 cdSize = static_cast<quint32>(centralDir.size());
    archive.append(centralDir);

    // End of central directory
    put32(archive, 0x06054B50u);
    put16(archive, 0);
    put16(archive, 0);
    put16(archive, static_cast<quint16>(entries.size()));
    put16(archive, static_cast<quint16>(entries.size()));
    put32(archive, cdSize);
    put32(archive, cdOffset);
    put16(archive, 0);

    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(archive) == archive.size();
}

inline bool createMinimalEpub(const QString &path,
                              const QString &title = QStringLiteral("Test EPUB"),
                              const QString &author = QStringLiteral("Test Author"),
                              int chapters = 2)
{
    QList<QPair<QByteArray, QByteArray>> entries;
    entries.emplaceBack(QByteArrayLiteral("mimetype"), QByteArrayLiteral("application/epub+zip"));
    entries.emplaceBack(QByteArrayLiteral("META-INF/container.xml"),
        QByteArrayLiteral("<?xml version=\"1.0\"?>"
        "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
        "<rootfiles><rootfile full-path=\"content.opf\" "
        "media-type=\"application/oebps-package+xml\"/></rootfiles>"
        "</container>"));

    QByteArray manifest;
    QByteArray spine;
    QList<QPair<QByteArray, QByteArray>> chapterFiles;
    for (int i = 1; i <= chapters; ++i) {
        const QByteArray id = QByteArray("ch") + QByteArray::number(i);
        const QByteArray href = id + QByteArray(".xhtml");
        manifest += "<item id=\"" + id + "\" href=\"" + href + "\" media-type=\"application/xhtml+xml\"/>";
        spine += "<itemref idref=\"" + id + "\"/>";
        const QByteArray html =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
            "<h1>Chapter " + QByteArray::number(i) + "</h1>"
            "<p>HyprFM EPUB preview chapter " + QByteArray::number(i) + ".</p>"
            "</body></html>";
        chapterFiles.emplaceBack(href, html);
    }

    const QByteArray opf =
        "<?xml version=\"1.0\"?>"
        "<package xmlns=\"http://www.idpf.org/opf\" version=\"2.0\" unique-identifier=\"id\">"
        "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
        "<dc:title>" + title.toHtmlEscaped().toUtf8() + "</dc:title>"
        "<dc:creator>" + author.toHtmlEscaped().toUtf8() + "</dc:creator>"
        "</metadata>"
        "<manifest>" + manifest + "</manifest>"
        "<spine>" + spine + "</spine>"
        "</package>";

    entries.emplaceBack(QByteArrayLiteral("content.opf"), opf);
    for (const auto &ch : chapterFiles)
        entries.emplaceBack(ch.first, ch.second);
    return writeStoredZip(path, entries);
}

inline bool createMinimalMobi(const QString &path,
                              const QString &title = QStringLiteral("Test MOBI"))
{
    const QByteArray html = "<html><body><h1>Hello MOBI</h1><p>HyprFM MOBI preview.</p></body></html>";
    const QByteArray titleBytes = title.toUtf8();

    QByteArray palmdoc(16, '\0');
    qToBigEndian<quint16>(1, palmdoc.data() + 0); // uncompressed
    qToBigEndian<quint32>(static_cast<quint32>(html.size()), palmdoc.data() + 4);
    qToBigEndian<quint16>(1, palmdoc.data() + 8); // 1 text record
    qToBigEndian<quint16>(4096, palmdoc.data() + 10);

    constexpr int kMobiLen = 232;
    QByteArray mobi(kMobiLen, '\0');
    mobi.replace(0, 4, "MOBI");
    qToBigEndian<quint32>(kMobiLen, mobi.data() + 4);
    qToBigEndian<quint32>(2, mobi.data() + 8);
    qToBigEndian<quint32>(65001, mobi.data() + 12);
    qToBigEndian<quint32>(6, mobi.data() + 20);
    for (int off = 24; off <= 60; off += 4)
        qToBigEndian<quint32>(0xFFFFFFFFu, mobi.data() + off);
    qToBigEndian<quint32>(1, mobi.data() + 64);
    qToBigEndian<quint32>(16 + kMobiLen, mobi.data() + 68);
    qToBigEndian<quint32>(static_cast<quint32>(titleBytes.size()), mobi.data() + 72);
    qToBigEndian<quint32>(9, mobi.data() + 76);
    qToBigEndian<quint32>(6, mobi.data() + 88);
    qToBigEndian<quint32>(0xFFFFFFFFu, mobi.data() + 92);

    const QByteArray rec0 = palmdoc + mobi + titleBytes + QByteArray(2, '\0');
    QByteArray pdb(78, '\0');
    const QByteArray shortTitle = titleBytes.left(31);
    pdb.replace(0, shortTitle.size(), shortTitle);
    pdb.replace(60, 8, "BOOKMOBI");
    qToBigEndian<quint16>(2, pdb.data() + 76);

    const quint32 off0 = 78 + 2 * 8 + 2;
    const quint32 off1 = off0 + static_cast<quint32>(rec0.size());
    QByteArray recInfo(16, '\0');
    qToBigEndian<quint32>(off0, recInfo.data() + 0);
    qToBigEndian<quint32>(off1, recInfo.data() + 8);
    recInfo[15] = 1;

    const QByteArray out = pdb + recInfo + QByteArray(2, '\0') + rec0 + html;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(out) == out.size();
}

inline QByteArray minimalFb2Bytes(const QString &title = QStringLiteral("Test FB2"),
                                  const QString &author = QStringLiteral("Jane Doe"))
{
    const QStringList parts = author.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QString first = parts.value(0);
    const QString last = parts.size() > 1 ? parts.mid(1).join(QLatin1Char(' ')) : QString();
    return QByteArray(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\">"
        "<description><title-info>"
        "<author><first-name>" + first.toHtmlEscaped().toUtf8() + "</first-name>"
        "<last-name>" + last.toHtmlEscaped().toUtf8() + "</last-name></author>"
        "<book-title>" + title.toHtmlEscaped().toUtf8() + "</book-title>"
        "</title-info></description>"
        "<body><section><title><p>Chapter 1</p></title>"
        "<p>HyprFM FictionBook preview.</p></section></body>"
        "</FictionBook>");
}

inline bool createMinimalFb2(const QString &path,
                             const QString &title = QStringLiteral("Test FB2"),
                             const QString &author = QStringLiteral("Jane Doe"))
{
    const QByteArray bytes = minimalFb2Bytes(title, author);
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

inline bool createMinimalFb2Zip(const QString &path,
                                const QString &title = QStringLiteral("Zipped FB2"),
                                const QString &author = QStringLiteral("Jane Doe"))
{
    QList<QPair<QByteArray, QByteArray>> entries;
    entries.emplaceBack(QByteArrayLiteral("book.fb2"), minimalFb2Bytes(title, author));
    return writeStoredZip(path, entries);
}

inline bool createMinimalCbz(const QString &path, int pages = 2)
{
    QImage img(16, 24, QImage::Format_RGB32);
    img.fill(Qt::white);
    QByteArray png;
    QBuffer buf(&png);
    if (!buf.open(QIODevice::WriteOnly) || !img.save(&buf, "PNG"))
        return false;

    QList<QPair<QByteArray, QByteArray>> entries;
    for (int i = 1; i <= pages; ++i)
        entries.emplaceBack(QByteArray::number(i).rightJustified(3, '0') + ".png", png);
    return writeStoredZip(path, entries);
}

inline bool createMinimalDjvu(const QString &path)
{
    const QString c44 = QStandardPaths::findExecutable(QStringLiteral("c44"));
    if (c44.isEmpty())
        return false;
    QTemporaryFile ppmFile;
    if (!ppmFile.open())
        return false;
    QByteArray ppm = "P6\n100 140\n255\n";
    ppm.append(QByteArray(100 * 140 * 3, '\xF0'));
    if (ppmFile.write(ppm) != ppm.size())
        return false;
    ppmFile.close();

    QProcess proc;
    proc.start(c44, {ppmFile.fileName(), path});
    return proc.waitForFinished(10000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

} // namespace EbookTestData
