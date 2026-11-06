#include <business_layer/export/screenplay/screenplay_dossier.h>
#include <business_layer/import/fountain_text_normalizer.h>
#include <business_layer/import/import_options.h>
#include <business_layer/import/screenplay/screenplay_fountain_importer.h>
#include <business_layer/import/screenplay/screenplay_google_doc_importer.h>
#include <business_layer/templates/text_template.h>

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QXmlStreamReader>
#include <QtTest>

using BusinessLayer::FountainTextNormalizer;

namespace {

using Paragraph = QPair<QString, QString>;

/**
 * @brief Extract (type, text) of screenplay paragraphs from the importer XML
 */
QVector<Paragraph> paragraphs(const QString& _xml)
{
    static const QSet<QString> kTypes = {
        "scene_heading", "action", "character", "parenthetical", "dialogue", "transition",
    };
    QVector<Paragraph> result;
    QString currentType;
    QXmlStreamReader reader(_xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }
        const auto name = reader.name().toString();
        if (kTypes.contains(name)) {
            currentType = name;
        } else if (name == "v" && !currentType.isEmpty()) {
            result.append({ currentType, reader.readElementText().trimmed() });
            currentType.clear();
        }
    }
    return result;
}

QVector<Paragraph> import(const QString& _fountain)
{
    BusinessLayer::ScreenplayFountainImporter importer;
    return paragraphs(importer.screenplayText("\n" + _fountain).text);
}

QStringList textsOf(const QVector<Paragraph>& _paragraphs, const QString& _type)
{
    QStringList texts;
    for (const auto& paragraph : _paragraphs) {
        if (paragraph.first == _type) {
            texts.append(paragraph.second);
        }
    }
    return texts;
}

} // namespace


/**
 * @brief Paragraph of a Google Doc: text and indent in pixels, a negative indent is right aligned
 */
using DocParagraph = QPair<QString, int>;

QVector<Paragraph> importGoogleDoc(const BusinessLayer::ScreenplayGoogleDocImporter& _importer,
                                   const QVector<DocParagraph>& _paragraphs)
{
    QTextDocument document;
    QTextCursor cursor(&document);
    for (int index = 0; index < _paragraphs.size(); ++index) {
        if (index > 0) {
            cursor.insertBlock();
        }
        QTextBlockFormat format;
        if (_paragraphs.at(index).second < 0) {
            format.setAlignment(Qt::AlignRight);
        } else {
            format.setLeftMargin(_paragraphs.at(index).second);
        }
        cursor.setBlockFormat(format);
        cursor.insertText(_paragraphs.at(index).first);
    }
    return paragraphs(_importer.parseDocument(BusinessLayer::ImportOptions(), document));
}

constexpr int kDialogue = 96;
constexpr int kParenthetical = 154;
constexpr int kCharacter = 211;

class FountainImportTest : public QObject
{
    Q_OBJECT

private slots:
    void dossierLeavesOutEmptySections()
    {
        using BusinessLayer::screenplayDossierHtml;
        QVERIFY(screenplayDossierHtml({}).isEmpty());
        QVERIFY(screenplayDossierHtml({ { "Logline", { { "  ", false } } } }).isEmpty());

        const auto html = QString::fromUtf8(screenplayDossierHtml(
            { { "Empty", {} },
              { "Logline", { { QString::fromUtf8("Cóc mẹ <kiện> cá trê & đàn con"), false } } },
              { "Treatment", { { "SCENE", true }, { QString(), false } } } }));
        QVERIFY(!html.contains("Empty"));
        QVERIFY(html.contains(QString::fromUtf8("Cóc mẹ &lt;kiện&gt; cá trê &amp; đàn con")));
        QVERIFY(html.contains("<b>SCENE</b>"));
        QVERIFY(html.contains("&nbsp;"));
        //
        // A new page for every section but the first one
        //
        QCOMPARE(html.count("page-break-before"), 1);
        QVERIFY(html.indexOf("Logline") < html.indexOf("page-break-before"));
    }

    void googleDocNewParagraphsAreDetected()
    {
        const BusinessLayer::ScreenplayGoogleDocImporter importer;
        const QVector<Paragraph> expected = {
            { "scene_heading", QString::fromUtf8("NGOẠI. RỪNG - ĐÊM") },
            { "action", QString::fromUtf8("Trời mưa.") },
            { "action", QString::fromUtf8("TIẾNG SÉT VANG LÊN") },
            { "character", QString::fromUtf8("CÁ TRÊ (V.O.)") },
            { "parenthetical", QString::fromUtf8("gắt") },
            { "dialogue", QString::fromUtf8("Xéo ngay!") },
            { "transition", QString::fromUtf8("CẮT SANG:") },
            { "scene_heading", QString::fromUtf8("CẢNH 2: BỜ AO") },
            { "transition", QString::fromUtf8("CHUYỂN CẢNH:") },
            { "scene_heading", QString::fromUtf8("BỜ AO - TRƯA") },
        };
        const auto result = importGoogleDoc(
            importer,
            { { QString::fromUtf8("NGOẠI. RỪNG - ĐÊM"), 0 },
              { QString(), 0 },
              { QString::fromUtf8("Trời mưa."), 0 },
              { QString(), 0 },
              { QString::fromUtf8("TIẾNG SÉT VANG LÊN"), 0 },
              { QString(), 0 },
              { QString::fromUtf8("CÁ TRÊ (V.O.)"), kCharacter },
              { QString::fromUtf8("(gắt)"), kParenthetical },
              { QString::fromUtf8("Xéo ngay!"), kDialogue },
              { QString(), 0 },
              { QString::fromUtf8("CẮT SANG:"), -1 },
              { QString(), 0 },
              { QString::fromUtf8("CẢNH 2: BỜ AO"), 0 },
              { QString(), 0 },
              { QString::fromUtf8("CHUYỂN CẢNH:"), 0 },
              { QString(), 0 },
              { QString::fromUtf8("BỜ AO - TRƯA"), 0 } });
        QCOMPARE(result, expected);
    }

    void googleDocSceneNumberIsNotPartOfTheHeading()
    {
        using BusinessLayer::TextParagraphType;
        BusinessLayer::ScreenplayGoogleDocImporter importer;
        importer.addKnownParagraph(QString::fromUtf8("Coppa chán nản."), TextParagraphType::Action);

        const QVector<Paragraph> expected = {
            { "scene_heading", QString::fromUtf8("EXT. THUNG LŨNG RẠP XIẾC - ĐÊM") },
            { "action", QString::fromUtf8("Coppa chán nản.") },
            { "scene_heading", QString::fromUtf8("CẢNH 7: BỜ AO") },
        };
        QCOMPARE(importGoogleDoc(importer,
                                 { { QString::fromUtf8("6. EXT. THUNG LŨNG RẠP XIẾC – ĐÊM"), 0 },
                                   { QString(), 0 },
                                   { QString::fromUtf8("Coppa chán nản."), 0 },
                                   { QString(), 0 },
                                   { QString::fromUtf8("7. CẢNH 7: BỜ AO"), 0 } }),
                 expected);
    }

    void googleDocKnownParagraphsKeepTheirType()
    {
        using BusinessLayer::TextParagraphType;
        BusinessLayer::ScreenplayGoogleDocImporter importer;
        importer.addKnownParagraph(QString::fromUtf8("Bờ ao yên bình"),
                                   TextParagraphType::SceneHeading);
        importer.addKnownParagraph(QString::fromUtf8("NỖI BUỒN"), TextParagraphType::Character);
        importer.addKnownParagraph(QString::fromUtf8("nói nhỏ"), TextParagraphType::Parenthetical);
        importer.addKnownParagraph(QString::fromUtf8("MẸ ƠI!"), TextParagraphType::Dialogue);

        //
        // Somebody cleared the formatting of the Doc: all indents are gone
        //
        const QVector<Paragraph> expected = {
            { "scene_heading", QString::fromUtf8("BỜ AO YÊN BÌNH") },
            { "character", QString::fromUtf8("NỖI BUỒN") },
            { "parenthetical", QString::fromUtf8("nói nhỏ") },
            { "dialogue", QString::fromUtf8("MẸ ƠI!") },
        };
        QCOMPARE(importGoogleDoc(importer,
                                 { { QString::fromUtf8("BỜ AO YÊN BÌNH"), 0 },
                                   { QString::fromUtf8("NỖI BUỒN"), 0 },
                                   { QString::fromUtf8("(nói nhỏ)"), 0 },
                                   { QString::fromUtf8("MẸ ƠI!"), 0 } }),
                 expected);
    }

    void googleDocTextWithTwoTypesIsDetected()
    {
        using BusinessLayer::TextParagraphType;
        BusinessLayer::ScreenplayGoogleDocImporter importer;
        importer.addKnownParagraph(QString::fromUtf8("CÓC MẸ"), TextParagraphType::Action);
        importer.addKnownParagraph(QString::fromUtf8("CÓC MẸ"), TextParagraphType::Character);

        const QVector<Paragraph> expected = {
            { "action", QString::fromUtf8("CÓC MẸ") },
            { "character", QString::fromUtf8("CÓC MẸ") },
            { "dialogue", QString::fromUtf8("Có.") },
        };
        QCOMPARE(importGoogleDoc(importer,
                                 { { QString::fromUtf8("CÓC MẸ"), 0 },
                                   { QString(), 0 },
                                   { QString::fromUtf8("CÓC MẸ"), kCharacter },
                                   { QString::fromUtf8("Có."), kDialogue } }),
                 expected);
    }

    void formatPlainTextDetectsEachLine()
    {
        const auto text = QString::fromUtf8("EXT. ĐƯỜNG ĐÈO – ĐÊM\n"
                                            "Rừng thông đen kịt.\n"
                                            "INT. XE QUANG – ĐÊM\n"
                                            "Anh rút ra một xấp giấy đóng ghim:\n"
                                            "HỢP ĐỒNG BẢO HIỂM NHÂN THỌ.\n"
                                            "Internet trên xe mất sóng.\n"
                                            "QUANG\n"
                                            "   (thì thầm)\n"
                                            "Xin lỗi em...\n"
                                            "Anh chớp mắt. Môi run.\n"
                                            "QUANG (V.O.)\n"
                                            "...xin lỗi con của ba.\n"
                                            "NỘI. QUÁN CÀ PHÊ - NGÀY\n"
                                            "Tối đen.");
        BusinessLayer::ScreenplayFountainImporter importer;
        const auto result = paragraphs(
            importer.screenplayText("\n" + FountainTextNormalizer::formatPlainText(text)).text);

        QCOMPARE(textsOf(result, "scene_heading"),
                 QStringList({ QString::fromUtf8("EXT. ĐƯỜNG ĐÈO – ĐÊM"),
                               QString::fromUtf8("INT. XE QUANG – ĐÊM"),
                               QString::fromUtf8("NỘI. QUÁN CÀ PHÊ - NGÀY") }));
        QCOMPARE(textsOf(result, "character").size(), 2);
        QCOMPARE(textsOf(result, "parenthetical").size(), 1);
        QCOMPARE(textsOf(result, "dialogue"),
                 QStringList({ QString::fromUtf8("Xin lỗi em..."),
                               QString::fromUtf8("...xin lỗi con của ba.") }));
        QVERIFY(textsOf(result, "action").contains(QString::fromUtf8("HỢP ĐỒNG BẢO HIỂM NHÂN THỌ.")));
        QVERIFY(textsOf(result, "action").contains(QString::fromUtf8("Internet trên xe mất sóng.")));
        QVERIFY(textsOf(result, "action").contains(QString::fromUtf8("Anh chớp mắt. Môi run.")));
        QCOMPARE(textsOf(result, "action").size(), 6);
    }
    void formatPlainTextKeepsProperFountain()
    {
        const auto text = QString::fromUtf8("EXT. ĐƯỜNG - ĐÊM\n\n"
                                            "Hai dòng hành động\n"
                                            "được ngắt cứng.\n\n"
                                            "QUANG\n"
                                            "Thoại dòng một\n"
                                            "thoại dòng hai\n\n"
                                            "Tối đen.");
        QCOMPARE(FountainTextNormalizer::formatPlainText(text),
                 FountainTextNormalizer::normalize(text));
    }
    void normalizeLine_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        auto row = [](const char* _name, const char* _input, const char* _expected) {
            QTest::newRow(_name) << QString::fromUtf8(_input) << QString::fromUtf8(_expected);
        };
        row("number-left", "1 EXT. ĐƯỜNG ĐÈO - ĐÊM", "EXT. ĐƯỜNG ĐÈO - ĐÊM");
        row("number-tab-both-sides", "12A\tINT. XE - ĐANG CHẠY - ĐÊM\t12A",
            "INT. XE - ĐANG CHẠY - ĐÊM");
        row("number-wide-gap-right", "7 EXT. SÂN - NGÀY      7", "EXT. SÂN - NGÀY");
        row("day-number-is-kept", "3 EXT. TRẠI - NGÀY 3", "EXT. TRẠI - NGÀY 3");
        row("int-ext", "4 INT./EXT. XE - ĐÊM", "INT./EXT. XE - ĐÊM");
        row("forced-heading", "5 .PHÒNG KHÁCH", ".PHÒNG KHÁCH");
        row("action-starting-with-number", "3 người bước vào.", "3 người bước vào.");
        row("vietnamese-heading", "NỘI. QUÁN CÀ PHÊ - NGÀY", ".NỘI. QUÁN CÀ PHÊ - NGÀY");
        row("vietnamese-heading-numbered", "2 NGOẠI. SÂN - CHIỀU", ".NGOẠI. SÂN - CHIỀU");
        row("vietnamese-int-ext", "NỘI/NGOẠI. XE - ĐÊM", ".NỘI/NGOẠI. XE - ĐÊM");
        row("word-starting-like-heading", "NỘI dung bức thư rất ngắn.",
            "NỘI dung bức thư rất ngắn.");
        row("ellipsis", "..xin lỗi con của ba.", "..xin lỗi con của ba.");
        row("non-breaking-space", "EXT.\xC2\xA0ĐƯỜNG - ĐÊM", "EXT. ĐƯỜNG - ĐÊM");
    }
    void normalizeLine()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(FountainTextNormalizer::normalize(input), expected);
    }

    void normalizeLineBreaks()
    {
        const QString input = QString::fromUtf8("\xEF\xBB\xBFTUẤN\r\nDòng một\x0B"
                                                "Dòng hai\r\n");
        QCOMPARE(FountainTextNormalizer::normalize(input),
                 QString::fromUtf8("TUẤN\nDòng một\nDòng hai\n"));
    }

    void importTextCopiedFromGoogleDocs()
    {
        const QString fountain = QStringLiteral(
            "1 EXT. ĐƯỜNG ĐÈO - ĐÊM\n"
            "\n"
            "Rừng thông đen kịt. Hai đốm đèn pha bò lên dốc.\n"
            "\n"
            "2 INT. XE TUẤN - ĐANG CHẠY - ĐÊM\n"
            "\n"
            "Tuấn lái xe, mắt đỏ hoe.\n"
            "\n"
            "TUẤN\n"
            "(thì thầm)\n"
            "Xin lỗi em...\n"
            "\n"
            "Anh chớp mắt.\n"
            "\n"
            "TUẤN\n"
            "..xin lỗi con.\n"
            "\n"
            "3 NỘI. NHÀ MAI - SÁNG\n"
            "\n"
            "Mai ngồi bên cửa sổ.\n"
            "\n"
            "..Ừ.\n");

        const auto result = import(fountain);

        QCOMPARE(textsOf(result, "scene_heading"),
                 QStringList({ "EXT. ĐƯỜNG ĐÈO - ĐÊM", "INT. XE TUẤN - ĐANG CHẠY - ĐÊM",
                               "NỘI. NHÀ MAI - SÁNG" }));
        QCOMPARE(textsOf(result, "character"), QStringList({ "TUẤN", "TUẤN" }));
        QCOMPARE(textsOf(result, "dialogue"), QStringList({ "Xin lỗi em...", "..xin lỗi con." }));
        QCOMPARE(textsOf(result, "parenthetical").size(), 1);
        QVERIFY(textsOf(result, "parenthetical").constFirst().contains("thì thầm"));
        QVERIFY(textsOf(result, "action").contains("Rừng thông đen kịt. Hai đốm đèn pha bò lên dốc."));
        QVERIFY(textsOf(result, "action").contains("..Ừ."));
    }
};

QTEST_MAIN(FountainImportTest)

#include "tst_fountain_import.moc"
