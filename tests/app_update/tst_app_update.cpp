#include "update_info.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace ManagementLayer::AppUpdate;

namespace {

const QString kDigest = QStringLiteral("sha256:") + QString(64, 'a');
const QString kPackageName = QStringLiteral("JennyScript-v0.8.4-jenny.9-windows-x64.zip");
const QString kPackageUrl
    = QStringLiteral("https://github.com/vienglacial/starc/releases/download/v0.8.4-jenny.9/")
    + kPackageName;

QJsonObject asset(const QString& _name = kPackageName, const QString& _url = kPackageUrl,
                  const QJsonValue& _digest = kDigest, qint64 _size = 51060924)
{
    return QJsonObject{ { "name", _name },
                        { "browser_download_url", _url },
                        { "digest", _digest },
                        { "size", _size } };
}

QByteArray release(const QJsonArray& _assets, const QString& _tag = "v0.8.4-jenny.9",
                   bool _draft = false, bool _prerelease = false)
{
    return QJsonDocument(QJsonObject{ { "tag_name", _tag },
                                      { "draft", _draft },
                                      { "prerelease", _prerelease },
                                      { "assets", _assets } })
        .toJson();
}

} // namespace

class AppUpdateTest : public QObject
{
    Q_OBJECT

private slots:
    void versionConstantIsValid()
    {
        //
        // A malformed constant would silently turn the update check off
        //
        QVERIFY(isNewerVersion(QStringLiteral("99.0.0-jenny.0"), QString::fromLatin1(kVersion)));
    }

    void compareVersions_data()
    {
        QTest::addColumn<QString>("candidate");
        QTest::addColumn<QString>("current");
        QTest::addColumn<bool>("isNewer");

        const auto addRow = [](const char* _name, const char* _candidate, const char* _current,
                               bool _isNewer) {
            QTest::newRow(_name) << QString::fromLatin1(_candidate)
                                 << QString::fromLatin1(_current) << _isNewer;
        };
        addRow("next fork build", "0.8.4-jenny.3", "0.8.4-jenny.2", true);
        addRow("same", "0.8.4-jenny.3", "0.8.4-jenny.3", false);
        addRow("older", "0.8.4-jenny.2", "0.8.4-jenny.3", false);
        addRow("numbers, not text", "0.8.4-jenny.10", "0.8.4-jenny.9", true);
        addRow("newer upstream base", "0.8.5-jenny.1", "0.8.4-jenny.9", true);
        addRow("older upstream base", "0.8.3-jenny.9", "0.8.4-jenny.1", false);
        addRow("leading v", "v0.8.4-jenny.3", "0.8.4-jenny.2", true);
        addRow("upstream tag", "0.9.0", "0.8.4-jenny.2", false);
        addRow("garbage", "latest", "0.8.4-jenny.2", false);
        addRow("malformed current", "0.8.4-jenny.3", "", false);
    }
    void compareVersions()
    {
        QFETCH(QString, candidate);
        QFETCH(QString, current);
        QFETCH(bool, isNewer);
        QCOMPARE(isNewerVersion(candidate, current), isNewer);
    }

    void testVersionOverride()
    {
        qputenv("JENNY_SCRIPT_TEST_VERSION", "0.8.4-jenny.0");
        QCOMPARE(currentVersion(), QStringLiteral("0.8.4-jenny.0"));
        qunsetenv("JENNY_SCRIPT_TEST_VERSION");
        QCOMPARE(currentVersion(), QString::fromLatin1(kVersion));
    }

    void parsesRelease()
    {
        const auto otherAsset = asset(QStringLiteral("notes.txt"));
        const auto info = parseLatestRelease(release({ otherAsset, asset() }));
        QVERIFY(info.has_value());
        QCOMPARE(info->version, QStringLiteral("0.8.4-jenny.9"));
        QCOMPARE(info->downloadUrl, QUrl(kPackageUrl));
        QCOMPARE(info->size, qint64(51060924));
        QCOMPARE(info->sha256Hex, QByteArray(64, 'a'));
    }

    void parsesReleaseOfRenamedRepository()
    {
        const auto url
            = QStringLiteral("https://github.com/vienglacial/jenny-script/releases/download/"
                             "v0.8.4-jenny.9/")
            + kPackageName;
        const auto info = parseLatestRelease(release({ asset(kPackageName, url) }));
        QVERIFY(info.has_value());
        QCOMPARE(info->downloadUrl, QUrl(url));
    }

    void rejectsUnsafeRelease_data()
    {
        QTest::addColumn<QByteArray>("json");

        QTest::newRow("not json") << QByteArray("<html>rate limited</html>");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("draft") << release({ asset() }, "v0.8.4-jenny.9", true, false);
        QTest::newRow("prerelease") << release({ asset() }, "v0.8.4-jenny.9", false, true);
        QTest::newRow("foreign tag") << release({ asset() }, "v0.8.5");
        QTest::newRow("no assets") << release({});
        QTest::newRow("no windows package") << release({ asset(QStringLiteral("source.tar.gz")) });
        QTest::newRow("other repository")
            << release({ asset(kPackageName,
                               QStringLiteral("https://github.com/someone/starc/releases/download/"
                                              "v1/")
                                   + kPackageName) });
        QTest::newRow("other host")
            << release({ asset(kPackageName, QStringLiteral("https://example.com/") + kPackageName) });
        QTest::newRow("plain http")
            << release({ asset(kPackageName,
                               QStringLiteral("http://github.com/vienglacial/starc/releases/"
                                              "download/v1/")
                                   + kPackageName) });
        QTest::newRow("no digest") << release({ asset(kPackageName, kPackageUrl, QJsonValue()) });
        QTest::newRow("other digest")
            << release({ asset(kPackageName, kPackageUrl, QStringLiteral("md5:abc")) });
        QTest::newRow("short digest")
            << release({ asset(kPackageName, kPackageUrl, QStringLiteral("sha256:abc")) });
        QTest::newRow("no size") << release({ asset(kPackageName, kPackageUrl, kDigest, 0) });
    }
    void rejectsUnsafeRelease()
    {
        QFETCH(QByteArray, json);
        QVERIFY(!parseLatestRelease(json).has_value());
    }
};

QTEST_APPLESS_MAIN(AppUpdateTest)

#include "tst_app_update.moc"
