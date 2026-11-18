#include "docx_fonts.h"

#include <qtzip/QtZipReader>
#include <qtzip/QtZipWriter>

#include <QBuffer>
#include <QPair>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringView>
#include <QVector>


namespace BusinessLayer {

int replaceDocxXmlFonts(QByteArray& _xml, const QString& _fontFamily)
{
    static const QRegularExpression kRunFonts(QStringLiteral("<w:rFonts\\b[^>]*>"));
    static const QRegularExpression kFontAttribute(
        QStringLiteral("\\bw:(ascii|hAnsi|cs|eastAsia)=\"[^\"]*\""));

    const QString value = _fontFamily.toHtmlEscaped();
    const QString source = QString::fromUtf8(_xml);
    QString result;
    result.reserve(source.size());
    int replaced = 0;
    qsizetype last = 0;
    auto elements = kRunFonts.globalMatch(source);
    while (elements.hasNext()) {
        const auto element = elements.next();
        result += QStringView(source).mid(last, element.capturedStart() - last);

        const QString tag = element.captured();
        qsizetype tagLast = 0;
        auto attributes = kFontAttribute.globalMatch(tag);
        while (attributes.hasNext()) {
            const auto attribute = attributes.next();
            result += QStringView(tag).mid(tagLast, attribute.capturedStart() - tagLast);
            result += QStringLiteral("w:%1=\"%2\"").arg(attribute.captured(1), value);
            tagLast = attribute.capturedEnd();
            ++replaced;
        }
        result += QStringView(tag).mid(tagLast);
        last = element.capturedEnd();
    }
    if (replaced == 0) {
        return 0;
    }

    result += QStringView(source).mid(last);
    _xml = result.toUtf8();
    return replaced;
}

bool replaceDocxFonts(const QString& _filePath, const QString& _fontFamily)
{
    QVector<QPair<QString, QByteArray>> entries;
    {
        QtZipReader reader(_filePath);
        if (!reader.isReadable() || reader.status() != QtZipReader::NoError) {
            return false;
        }

        const auto files = reader.fileInfoList();
        for (const auto& info : files) {
            if (!info.isFile) {
                continue;
            }

            auto data = reader.fileData(info.filePath);
            if (data.size() != info.size) {
                return false;
            }
            if (info.filePath.startsWith(QLatin1String("word/"))
                && info.filePath.endsWith(QLatin1String(".xml"))) {
                replaceDocxXmlFonts(data, _fontFamily);
            }
            entries.append({ info.filePath, data });
        }
    }
    if (entries.isEmpty()) {
        return false;
    }

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    {
        QtZipWriter writer(&buffer);
        for (const auto& entry : std::as_const(entries)) {
            writer.addFile(entry.first, entry.second);
        }
        writer.close();
        if (writer.status() != QtZipWriter::NoError) {
            return false;
        }
    }

    QSaveFile file(_filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(buffer.data()) != buffer.data().size()) {
        return false;
    }
    return file.commit();
}

} // namespace BusinessLayer
