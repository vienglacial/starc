#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <optional>

namespace ManagementLayer {
namespace AppUpdate {

/**
 * @brief Version of this fork, the GitHub release tag without the leading "v"
 * @note Raise it before building a new release. QApplication::applicationVersion() stays the
 *       upstream one, because the project database code compares it.
 */
inline constexpr char kVersion[] = "0.8.4-jenny.7";

/**
 * @brief A release that can be installed on Windows
 */
struct ReleaseInfo {
    QString version;
    QUrl downloadUrl;
    qint64 size = 0;
    QByteArray sha256Hex;
};

/**
 * @brief Version of the running app. The environment variable JENNY_SCRIPT_TEST_VERSION replaces it
 *        to test the update flow by pretending to be an older version
 */
QString currentVersion();

/**
 * @brief Is the candidate version newer than the current one. False if one of them is malformed
 */
bool isNewerVersion(const QString& _candidate, const QString& _current);

/**
 * @brief Read the answer of the GitHub "latest release" API
 * @return Nothing if the release cannot be installed safely: draft or prerelease, no Windows
 *         package, a download link outside of this repository, no SHA-256 digest
 */
std::optional<ReleaseInfo> parseLatestRelease(const QByteArray& _json);

} // namespace AppUpdate
} // namespace ManagementLayer
