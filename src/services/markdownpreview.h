#pragma once

#include <QString>

QString renderMarkdownPreview(const QString &markdown, bool lightTheme, const QString &path);
QString renderHtmlPreview(const QString &html, const QString &path);
