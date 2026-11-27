#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace HyprFmDocuments {

enum class DocumentBackend {
    None,
    Pdf,
    MuPdf,
    DjVu,
};

// Shared reflowable layout parameters between metadata page counting
// (`mutool run`) and page rasterization (`mutool draw`) so page counts
// always match rendered pages.
inline constexpr int kEbookPageWidth = 450;
inline constexpr int kEbookPageHeight = 640;
inline constexpr int kEbookFontSize = 11;

inline QString documentFileName(const QString &path)
{
    if (path.contains(QLatin1String("://"))) {
        const QString name = QUrl(path).fileName();
        if (!name.isEmpty())
            return name;
    }
    return QFileInfo(path).fileName();
}

inline QString documentSuffixForPath(const QString &path)
{
    const QString name = documentFileName(path);
    const QString lower = name.toLower();
    if (lower.endsWith(QLatin1String(".fb2.zip")))
        return QStringLiteral("fb2.zip");
    return QFileInfo(name).suffix().toLower();
}

inline DocumentBackend documentBackendForPath(const QString &path, const QString &mimeType = {})
{
    const QString ext = documentSuffixForPath(path);
    if (ext == QLatin1String("pdf"))
        return DocumentBackend::Pdf;
    if (ext == QLatin1String("epub") || ext == QLatin1String("mobi")
        || ext == QLatin1String("prc") || ext == QLatin1String("azw")
        || ext == QLatin1String("azw3") || ext == QLatin1String("fb2")
        || ext == QLatin1String("fb2.zip") || ext == QLatin1String("cbz")
        || ext == QLatin1String("cbt") || ext == QLatin1String("xps")
        || ext == QLatin1String("oxps"))
        return DocumentBackend::MuPdf;
    if (ext == QLatin1String("djvu") || ext == QLatin1String("djv"))
        return DocumentBackend::DjVu;

    const QString mime = mimeType.toLower();
    if (mime == QLatin1String("application/pdf"))
        return DocumentBackend::Pdf;
    if (mime == QLatin1String("application/epub+zip")
        || mime == QLatin1String("application/x-mobipocket-ebook")
        || mime == QLatin1String("application/vnd.amazon.mobi8-ebook")
        || mime == QLatin1String("application/vnd.amazon.ebook")
        || mime == QLatin1String("application/x-fictionbook+xml")
        || mime == QLatin1String("application/x-fictionbook")
        || mime == QLatin1String("application/x-zip-compressed-fb2")
        || mime == QLatin1String("application/vnd.comicbook+zip")
        || mime == QLatin1String("application/x-cbz")
        || mime == QLatin1String("application/x-cbt")
        || mime == QLatin1String("application/oxps")
        || mime == QLatin1String("application/vnd.ms-xpsdocument"))
        return DocumentBackend::MuPdf;
    if (mime == QLatin1String("image/vnd.djvu")
        || mime == QLatin1String("image/vnd.djvu+multipage")
        || mime == QLatin1String("image/x-djvu"))
        return DocumentBackend::DjVu;

    return DocumentBackend::None;
}

inline bool isEbookBackend(DocumentBackend backend)
{
    return backend == DocumentBackend::MuPdf || backend == DocumentBackend::DjVu;
}

inline QString documentKindLabel(const QString &path, const QString &mimeType = {})
{
    const QString ext = documentSuffixForPath(path);
    if (ext == QLatin1String("pdf")) return QStringLiteral("PDF");
    if (ext == QLatin1String("epub")) return QStringLiteral("EPUB");
    if (ext == QLatin1String("mobi")) return QStringLiteral("MOBI");
    if (ext == QLatin1String("azw3")) return QStringLiteral("AZW3");
    if (ext == QLatin1String("azw")) return QStringLiteral("AZW");
    if (ext == QLatin1String("prc")) return QStringLiteral("PRC");
    if (ext == QLatin1String("fb2") || ext == QLatin1String("fb2.zip"))
        return QStringLiteral("FictionBook");
    if (ext == QLatin1String("cbz")) return QStringLiteral("CBZ");
    if (ext == QLatin1String("cbt")) return QStringLiteral("CBT");
    if (ext == QLatin1String("xps") || ext == QLatin1String("oxps"))
        return QStringLiteral("XPS");
    if (ext == QLatin1String("djvu") || ext == QLatin1String("djv"))
        return QStringLiteral("DjVu");

    const QString mime = mimeType.toLower();
    if (mime == QLatin1String("application/pdf")) return QStringLiteral("PDF");
    if (mime == QLatin1String("application/epub+zip")) return QStringLiteral("EPUB");
    if (mime == QLatin1String("application/x-mobipocket-ebook")) return QStringLiteral("MOBI");
    if (mime == QLatin1String("application/vnd.amazon.mobi8-ebook")) return QStringLiteral("AZW3");
    if (mime == QLatin1String("application/vnd.amazon.ebook")) return QStringLiteral("AZW");
    if (mime.contains(QLatin1String("fictionbook"))) return QStringLiteral("FictionBook");
    if (mime == QLatin1String("application/vnd.comicbook+zip") || mime == QLatin1String("application/x-cbz"))
        return QStringLiteral("CBZ");
    if (mime == QLatin1String("application/x-cbt")) return QStringLiteral("CBT");
    if (mime == QLatin1String("application/oxps") || mime == QLatin1String("application/vnd.ms-xpsdocument"))
        return QStringLiteral("XPS");
    if (mime.contains(QLatin1String("djvu"))) return QStringLiteral("DjVu");

    switch (documentBackendForPath(path, mimeType)) {
    case DocumentBackend::Pdf: return QStringLiteral("PDF");
    case DocumentBackend::DjVu: return QStringLiteral("DjVu");
    case DocumentBackend::MuPdf: return QStringLiteral("E-book");
    case DocumentBackend::None: return QStringLiteral("Document");
    }
    return QStringLiteral("Document");
}

inline bool looksLikeDocument(const QString &path, DocumentBackend backend)
{
    if (backend == DocumentBackend::None)
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = file.read(1024);
    if (head.isEmpty())
        return false;

    if (backend == DocumentBackend::Pdf)
        return head.contains("%PDF-");

    if (backend == DocumentBackend::DjVu)
        return head.startsWith("AT&TFORM");

    const QString ext = documentSuffixForPath(path);
    if (ext == QLatin1String("epub") || ext == QLatin1String("cbz")
        || ext == QLatin1String("fb2.zip") || ext == QLatin1String("xps")
        || ext == QLatin1String("oxps")) {
        return head.startsWith("PK\x03\x04") || head.startsWith("PK\x05\x06");
    }

    if (ext == QLatin1String("mobi") || ext == QLatin1String("prc")
        || ext == QLatin1String("azw") || ext == QLatin1String("azw3")) {
        if (head.size() < 68)
            return false;
        const QByteArray typeId = head.mid(60, 8);
        return typeId == "BOOKMOBI" || typeId == "TEXtREAd";
    }

    if (ext == QLatin1String("fb2"))
        return head.contains("<FictionBook") || head.contains("<fictionbook") || head.contains("<?xml");

    return true;
}

inline QString ensureMutoolInfoScript()
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/hyprfm");
    const QString scriptPath = dir + QStringLiteral("/mutool-info.js");
    const QByteArray script = QByteArrayLiteral(
        "var doc = Document.openDocument(scriptArgs[0]);\n"
        "if (doc.isReflowable()) doc.layout(")
        + QByteArray::number(kEbookPageWidth) + QByteArrayLiteral(", ")
        + QByteArray::number(kEbookPageHeight) + QByteArrayLiteral(", ")
        + QByteArray::number(kEbookFontSize) + QByteArrayLiteral(
        ");\n"
        "var n = doc.countPages();\n"
        "print(\"Pages: \" + n);\n"
        "print(\"Reflowable: \" + (doc.isReflowable() ? \"1\" : \"0\"));\n"
        "print(\"Format: \" + (doc.getMetaData(\"format\") || \"\"));\n"
        "print(\"Title: \" + (doc.getMetaData(\"info:Title\") || \"\"));\n"
        "print(\"Author: \" + (doc.getMetaData(\"info:Author\") || \"\"));\n"
        "print(\"Subject: \" + (doc.getMetaData(\"info:Subject\") || \"\"));\n"
        "print(\"Creator: \" + (doc.getMetaData(\"info:Creator\") || \"\"));\n"
        "print(\"Producer: \" + (doc.getMetaData(\"info:Producer\") || \"\"));\n"
        "if (n > 0) {\n"
        "    var b = doc.loadPage(0).getBounds();\n"
        "    print(\"Page size: \" + (b[2] - b[0]) + \" x \" + (b[3] - b[1]) + \" pts\");\n"
        "}\n");

    QFile existing(scriptPath);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == script)
        return scriptPath;

    if (!QDir().mkpath(dir))
        return {};
    QSaveFile saver(scriptPath);
    if (!saver.open(QIODevice::WriteOnly))
        return {};
    if (saver.write(script) != script.size() || !saver.commit())
        return {};
    return scriptPath;
}

inline QVariantMap parseMutoolInfoOutput(const QString &stdoutText, int *pageCountOut = nullptr)
{
    if (pageCountOut)
        *pageCountOut = 0;

    QHash<QString, QString> kv;
    for (const QString &line : stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;
        kv.insert(line.left(colon).trimmed(), line.mid(colon + 1).trimmed());
    }

    bool ok = false;
    const int pages = kv.value(QStringLiteral("Pages")).toInt(&ok);
    if (pageCountOut && ok && pages > 0)
        *pageCountOut = pages;

    QVariantMap meta;
    auto put = [&](const QString &label, const QString &key) {
        const QString v = kv.value(key);
        if (!v.isEmpty())
            meta[label] = v;
    };

    put(QStringLiteral("Title"), QStringLiteral("Title"));
    put(QStringLiteral("Author"), QStringLiteral("Author"));
    put(QStringLiteral("Subject"), QStringLiteral("Subject"));
    put(QStringLiteral("Creator"), QStringLiteral("Creator"));
    put(QStringLiteral("Producer"), QStringLiteral("Producer"));
    put(QStringLiteral("Format"), QStringLiteral("Format"));
    if (ok && pages > 0)
        meta[QStringLiteral("Pages")] = QString::number(pages);

    if (kv.value(QStringLiteral("Reflowable")) == QLatin1String("0")) {
        static const QRegularExpression sizeRe(
            QStringLiteral(R"(([0-9.]+)\s*x\s*([0-9.]+)\s*pts)"));
        const auto m = sizeRe.match(kv.value(QStringLiteral("Page size")));
        if (m.hasMatch()) {
            const double w = m.captured(1).toDouble() * 25.4 / 72.0;
            const double h = m.captured(2).toDouble() * 25.4 / 72.0;
            if (w > 0 && h > 0) {
                meta[QStringLiteral("Page size")] =
                    QStringLiteral("%1 x %2 mm").arg(w, 0, 'f', 0).arg(h, 0, 'f', 0);
            }
        }
    }

    return meta;
}

inline QVariantMap parseDjvuMetadataOutput(const QString &metaText, int pageCount)
{
    QVariantMap meta;
    static const QRegularExpression lineRe(
        QStringLiteral(R"re(^([A-Za-z0-9_-]+)\s+"((?:\\.|[^"\\])*)"\s*$)re"));

    for (const QString &line : metaText.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const auto m = lineRe.match(line.trimmed());
        if (!m.hasMatch())
            continue;
        const QString key = m.captured(1).toLower();
        QString val = m.captured(2);
        val = val.replace(QStringLiteral("\\\""), QStringLiteral("\""))
                 .replace(QStringLiteral("\\\\"), QStringLiteral("\\"))
                 .trimmed();
        if (val.isEmpty())
            continue;
        if (key == QLatin1String("title")) meta[QStringLiteral("Title")] = val;
        else if (key == QLatin1String("author")) meta[QStringLiteral("Author")] = val;
        else if (key == QLatin1String("subject")) meta[QStringLiteral("Subject")] = val;
        else if (key == QLatin1String("creator")) meta[QStringLiteral("Creator")] = val;
        else if (key == QLatin1String("producer")) meta[QStringLiteral("Producer")] = val;
        else if (key == QLatin1String("year")) meta[QStringLiteral("Year")] = val;
    }

    if (pageCount > 0)
        meta[QStringLiteral("Pages")] = QString::number(pageCount);
    return meta;
}

} // namespace HyprFmDocuments
