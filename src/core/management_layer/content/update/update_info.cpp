#include "update_info.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <array>

namespace ManagementLayer {
namespace AppUpdate {

namespace {

const QString kDownloadUrlPrefix
    = QStringLiteral("https://github.com/vienglacial/starc/releases/download/");
//
// The repository may be renamed: GitHub redirects the API, but its links then have the new name
//
const QString kRenamedDownloadUrlPrefix
    = QStringLiteral("https://github.com/vienglacial/jenny-script/releases/download/");

/**
 * @brief "0.8.4-jenny.3" or "v0.8.4-jenny.3" as { 0, 8, 4, 3 }
 */
std::optional<std::array<int, 4>> versionParts(const QString& _version)
{
    static const QRegularExpression kVersionExpression(
        QStringLiteral(R"(^v?(\d{1,6})\.(\d{1,6})\.(\d{1,6})-jenny\.(\d{1,6})$)"));
    const auto match = kVersionExpression.match(_version);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    return std::array<int, 4>{ match.captured(1).toInt(), match.captured(2).toInt(),
                               match.captured(3).toInt(), match.captured(4).toInt() };
}

} // namespace


QString currentVersion()
{
    return qEnvironmentVariable("JENNY_SCRIPT_TEST_VERSION", QString::fromLatin1(kVersion));
}

bool isNewerVersion(const QString& _candidate, const QString& _current)
{
    const auto candidate = versionParts(_candidate);
    const auto current = versionParts(_current);
    return candidate.has_value() && current.has_value() && *candidate > *current;
}

std::optional<ReleaseInfo> parseLatestRelease(const QByteArray& _json)
{
    const auto release = QJsonDocument::fromJson(_json).object();
    if (release.value("draft").toBool() || release.value("prerelease").toBool()) {
        return std::nullopt;
    }

    const auto tag = release.value("tag_name").toString();
    if (!versionParts(tag).has_value()) {
        return std::nullopt;
    }

    static const QRegularExpression kDigestExpression(QStringLiteral("^sha256:([0-9a-f]{64})$"));
    const auto assets = release.value("assets").toArray();
    for (const auto& assetValue : assets) {
        const auto asset = assetValue.toObject();
        const auto name = asset.value("name").toString();
        if (!name.startsWith("JennyScript-") || !name.endsWith("-windows-x64.zip")) {
            continue;
        }

        const auto url = asset.value("browser_download_url").toString();
        const auto digest = kDigestExpression.match(asset.value("digest").toString());
        const auto size = asset.value("size").toInteger();
        const bool isTrustedUrl
            = url.startsWith(kDownloadUrlPrefix) || url.startsWith(kRenamedDownloadUrlPrefix);
        if (!isTrustedUrl || !digest.hasMatch() || size <= 0) {
            return std::nullopt;
        }

        ReleaseInfo info;
        info.version = tag.startsWith('v') ? tag.mid(1) : tag;
        info.downloadUrl = QUrl(url);
        info.size = size;
        info.sha256Hex = digest.captured(1).toLatin1();
        return info;
    }
    return std::nullopt;
}

} // namespace AppUpdate
} // namespace ManagementLayer
