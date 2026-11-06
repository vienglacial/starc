#include "google_docs_manager.h"

#include "google_auth_manager.h"
#include "google_drive_client.h"
#include "../project/project_manager.h"

#include <business_layer/document/screenplay/text/screenplay_text_corrector.h>
#include <business_layer/document/screenplay/text/screenplay_text_document.h>
#include <business_layer/document/text/text_cursor.h>
#include <business_layer/export/screenplay/screenplay_docx_exporter.h>
#include <business_layer/export/screenplay/screenplay_dossier.h>
#include <business_layer/export/screenplay/screenplay_export_options.h>
#include <business_layer/import/fountain_text_normalizer.h>
#include <business_layer/import/import_options.h>
#include <business_layer/import/screenplay/screenplay_fountain_importer.h>
#include <business_layer/import/screenplay/screenplay_google_doc_importer.h>
#include <business_layer/model/screenplay/screenplay_information_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <utils/helpers/text_helper.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/dialog/standard_dialog.h>

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <utility>


namespace ManagementLayer {

namespace {

const QString kRootFolderName = QStringLiteral("Jenny Script");
const QString kDocxMimeType = QStringLiteral(
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
const QString kHtmlMimeType = QStringLiteral("text/html; charset=UTF-8");
constexpr int kChangesCheckIntervalMs = 60 * 1000;
const QString kDocumentUuidProperty = QStringLiteral("jsDocumentUuid");

/**
 * @brief Keys starting with "project" survive the settings reset
 */
QString settingsKey(const QUuid& _documentUuid, const QString& _name)
{
    return QString("project/google-docs/%1/%2")
        .arg(_documentUuid.toString(QUuid::WithoutBraces), _name);
}
const QString kFileIdKey = QStringLiteral("file-id");
const QString kTextHashKey = QStringLiteral("text-hash");
const QString kSyncedAtKey = QStringLiteral("synced-at");
const QString kFormatKey = QStringLiteral("format");
const QString kDocxFormat = QStringLiteral("docx");
const QString kSeenModifiedTimeKey = QStringLiteral("seen-modified-time");
const QString kOfferedHashKey = QStringLiteral("offered-hash");
const QString kDossierFileIdKey = QStringLiteral("dossier-file-id");
const QString kDossierHashKey = QStringLiteral("dossier-hash");

void forEachParagraph(const BusinessLayer::TextModelItem* _item,
                      const std::function<void(const BusinessLayer::TextModelTextItem*)>& _action)
{
    for (int row = 0; row < _item->childCount(); ++row) {
        const auto child = _item->childAt(row);
        if (child->type() == BusinessLayer::TextModelItemType::Text) {
            _action(static_cast<const BusinessLayer::TextModelTextItem*>(child));
        } else {
            forEachParagraph(child, _action);
        }
    }
}

QVector<BusinessLayer::DossierParagraph> paragraphsOf(BusinessLayer::AbstractModel* _model)
{
    QVector<BusinessLayer::DossierParagraph> paragraphs;
    if (const auto model = qobject_cast<BusinessLayer::TextModel*>(_model); model != nullptr) {
        forEachParagraph(model->itemForIndex({}),
                         [&paragraphs](const BusinessLayer::TextModelTextItem* _paragraph) {
                             paragraphs.append({ _paragraph->text(), false });
                         });
    }
    return paragraphs;
}

/**
 * @brief Headings and beats, it is what the treatment view of the app shows. A screenplay without
 *        beats has no treatment, only a list of its scenes.
 */
QVector<BusinessLayer::DossierParagraph> treatmentOf(BusinessLayer::ScreenplayTextModel* _model)
{
    using BusinessLayer::TextParagraphType;

    QVector<BusinessLayer::DossierParagraph> paragraphs;
    bool hasBeats = false;
    forEachParagraph(_model->itemForIndex({}),
                     [&paragraphs, &hasBeats](const BusinessLayer::TextModelTextItem* _paragraph) {
                         switch (_paragraph->paragraphType()) {
                         case TextParagraphType::ActHeading:
                         case TextParagraphType::SequenceHeading:
                         case TextParagraphType::SceneHeading: {
                             paragraphs.append({ _paragraph->text(), true });
                             break;
                         }
                         case TextParagraphType::BeatHeading: {
                             hasBeats = hasBeats || !_paragraph->text().trimmed().isEmpty();
                             paragraphs.append({ _paragraph->text(), false });
                             break;
                         }
                         default: {
                             break;
                         }
                         }
                     });
    return hasBeats ? paragraphs : QVector<BusinessLayer::DossierParagraph>();
}

/**
 * @brief Hash of a Doc text, insensitive to line endings and trailing spaces added by Google Docs
 */
QString textHash(const QString& _text)
{
    QStringList lines = BusinessLayer::FountainTextNormalizer::normalize(_text).split('\n');
    for (auto& line : lines) {
        while (!line.isEmpty() && line.back().isSpace()) {
            line.chop(1);
        }
    }
    const auto text = lines.join('\n').trimmed();
    return QString::fromLatin1(
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}

/**
 * @brief The Doc looks like the editor of the app: scene headings have the same grey band as there
 *        and a repeated character gets the same "(CONT'D)" (the DOCX export draws neither)
 *
 * Google Docs lays out the pages itself, so the lines for the page breaks of the app are left out:
 * they would end up in the middle of its pages and come back as text on pull.
 */
class GoogleDocExporter : public BusinessLayer::ScreenplayDocxExporter
{
protected:
    BusinessLayer::TextDocument* createDocument(
        const BusinessLayer::ExportOptions& _exportOptions) const override
    {
        Q_UNUSED(_exportOptions)

        auto document = new BusinessLayer::ScreenplayTextDocument;
        document->setCorrectionOptions(
            settingsValue(DataStorageLayer::kComponentsScreenplayEditorContinueDialogueKey)
                .toBool(),
            false);
        return document;
    }

    void processBlock(const BusinessLayer::TextCursor& _cursor,
                      const BusinessLayer::ExportOptions& _exportOptions,
                      QString& _documentXml) const override
    {
        using BusinessLayer::TextBlockStyle;
        using BusinessLayer::TextParagraphType;

        BusinessLayer::ScreenplayDocxExporter::processBlock(_cursor, _exportOptions, _documentXml);

        const auto blockType = TextBlockStyle::forCursor(_cursor);
        if (blockType == TextParagraphType::SceneHeading) {
            //
            // 12% of black on white, like the band in the editor
            //
            _documentXml.append(
                QStringLiteral("<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"E0E0E0\"/>"));
        } else if (blockType == TextParagraphType::Character) {
            const auto block = _cursor.block();
            const auto format = block.blockFormat();
            if (format.boolProperty(TextBlockStyle::PropertyIsCharacterContinued)
                && !format.boolProperty(TextBlockStyle::PropertyIsCorrection)) {
                auto cursor = _cursor;
                cursor.setPosition(block.position() + block.length() - 1);
                cursor.insertText(BusinessLayer::ScreenplayTextCorrector::continuedTerm(),
                                  TextHelper::fineBlockCharFormat(block));
            }
        }
    }
};

QUrl documentUrl(const QString& _fileId)
{
    return QUrl(QString("https://docs.google.com/document/d/%1/edit").arg(_fileId));
}

} // namespace


class GoogleDocsManager::Implementation
{
public:
    Implementation(GoogleDocsManager* _q, QWidget* _parentWidget,
                   ProjectManager* _projectManager);

    QWidget* dialogParent() const;
    void ask(const QString& _title, const QString& _text, const QVector<Dialog::ButtonInfo>& _buttons,
             const std::function<void(int)>& _callback);
    void inform(const QString& _title, const QString& _text);
    void showError(const QString& _title, const GoogleDriveError& _error);

    void withSignIn(const std::function<void()>& _action);
    BusinessLayer::ScreenplayTextModel* screenplayModel(const QUuid& _documentUuid) const;
    QByteArray exportDocx(BusinessLayer::ScreenplayTextModel* _model) const;

    void push(const QUuid& _documentUuid);
    void withProjectFolder(const std::function<void(const QString&)>& _action,
                           const std::function<void(const GoogleDriveError&)>& _fail);
    void uploadNew(const QUuid& _documentUuid, const QByteArray& _content);
    void updateExisting(const QUuid& _documentUuid, const QString& _fileId,
                        const QByteArray& _content);
    void storeSyncState(const QUuid& _documentUuid, const GoogleDriveFile& _file,
                        bool _showPushedMessage);
    QByteArray dossierHtml(const QUuid& _documentUuid) const;
    QString documentNameFor(const QUuid& _documentUuid, const QString& _suffix) const;
    /**
     * @brief Push the second Doc and call back with its name, empty when it was not pushed
     */
    void pushDossier(const QUuid& _documentUuid,
                     const std::function<void(const QString&)>& _done);
    void updateDossier(const QUuid& _documentUuid, const QString& _fileId,
                       const QByteArray& _content,
                       const std::function<void(const QString&)>& _done);
    void storeDossierState(const QUuid& _documentUuid, const GoogleDriveFile& _file,
                           const std::function<void(const QString&)>& _done);
    void showPushedMessage(const GoogleDriveFile& _file, const QString& _dossierName);
    void pull(const QUuid& _documentUuid);
    void pullDocx(const QUuid& _documentUuid, const QString& _fileId, const QString& _text);
    void addPulledDraft(const QUuid& _documentUuid, const QString& _screenplayXml,
                        const QString& _text);
    void openInBrowser(const QUuid& _documentUuid);
    void checkForChanges();
    void checkNextDocument(QVector<QUuid> _documents, int _generation);
    void offerPull(const QUuid& _documentUuid, const QString& _fileName);

    GoogleDocsManager* q = nullptr;
    QPointer<QWidget> parentWidget;
    ProjectManager* projectManager = nullptr;
    GoogleAuthManager* authManager = nullptr;
    GoogleDriveClient* driveClient = nullptr;
    QTimer changesTimer;
    std::function<void()> actionAfterSignIn;
    bool isBusy = false;
    bool isCheckingForChanges = false;
    bool isPullOffered = false;
    /**
     * @brief Raised by every push and pull, a running check for changes is outdated then
     */
    int syncGeneration = 0;
};

GoogleDocsManager::Implementation::Implementation(GoogleDocsManager* _q, QWidget* _parentWidget,
                                                  ProjectManager* _projectManager)
    : q(_q)
    , parentWidget(_parentWidget)
    , projectManager(_projectManager)
    , authManager(new GoogleAuthManager(_q))
    , driveClient(new GoogleDriveClient(authManager->networkAccessManager(), _q))
{
    driveClient->setAccessTokenProvider([this] { return authManager->accessToken(); });
    driveClient->setTokenRefresher(
        [this](std::function<void(bool)> _done) { authManager->refreshAccessToken(_done); });
}

QWidget* GoogleDocsManager::Implementation::dialogParent() const
{
    return parentWidget.isNull() ? nullptr : parentWidget->topLevelWidget();
}

void GoogleDocsManager::Implementation::ask(const QString& _title, const QString& _text,
                                            const QVector<Dialog::ButtonInfo>& _buttons,
                                            const std::function<void(int)>& _callback)
{
    auto dialog = new Dialog(dialogParent());
    dialog->showDialog(_title, _text, _buttons);
    QObject::connect(dialog, &Dialog::finished, q,
                     [dialog, _callback](const Dialog::ButtonInfo& _button) {
                         dialog->hideDialog();
                         _callback(_button.id);
                     });
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
}

void GoogleDocsManager::Implementation::inform(const QString& _title, const QString& _text)
{
    StandardDialog::information(dialogParent(), _title, _text);
}

void GoogleDocsManager::Implementation::showError(const QString& _title,
                                                  const GoogleDriveError& _error)
{
    QString text;
    switch (_error.type) {
    case GoogleDriveError::Type::Network: {
        text = tr("Can't connect to Google. Check the internet connection and try again.");
        break;
    }
    case GoogleDriveError::Type::Unauthorized: {
        //
        // An expired sign in was cleared already, so otherwise the refresh could not reach Google
        //
        text = authManager->isSignedIn()
            ? tr("Can't connect to Google. Check the internet connection and try again.")
            : tr("Google sign in has expired. Please sign in again.");
        break;
    }
    case GoogleDriveError::Type::Forbidden: {
        //
        // The token has no Drive scope (the permission was unticked on the consent screen)
        //
        if (_error.reason == QLatin1String("insufficientPermissions")) {
            authManager->signOut();
            text = tr("Jenny Script has no access to Google Drive. Push again, sign in and tick the "
                      "permission to see and edit the Google Drive files used with Jenny Script.");
            break;
        }
        text = tr("Google denied access to the file. It may be owned by another account or "
                  "the access was not granted.");
        break;
    }
    case GoogleDriveError::Type::NotFound: {
        text = tr("The Google Doc was not found. It may have been deleted.");
        break;
    }
    case GoogleDriveError::Type::RateLimited: {
        text = tr("Google is busy right now. Please try again in a minute.");
        break;
    }
    default: {
        text = tr("Google returned an error.");
        break;
    }
    }
    if (!_error.message.isEmpty()) {
        text += "\n\n" + _error.message;
    }
    inform(_title, text);
}

void GoogleDocsManager::Implementation::withSignIn(const std::function<void()>& _action)
{
    if (authManager->isSignedIn()) {
        _action();
        return;
    }

    if (!GoogleAuthManager::isConfigured()) {
        inform(tr("Google Docs"), tr("Google sign in is not available in this build."));
        return;
    }

    static constexpr int cancelButtonId = 0;
    static constexpr int signInButtonId = 1;
    ask(tr("Sign in to Google"),
        tr("The browser will open to sign in with your Google account. Jenny Script gets access "
           "only to the files it creates in your Google Drive."),
        { { cancelButtonId, tr("Cancel"), Dialog::RejectButton },
          { signInButtonId, tr("Sign in"), Dialog::AcceptButton } },
        [this, _action](int _buttonId) {
            if (_buttonId != signInButtonId) {
                return;
            }
            actionAfterSignIn = _action;
            authManager->signIn();
        });
}

BusinessLayer::ScreenplayTextModel* GoogleDocsManager::Implementation::screenplayModel(
    const QUuid& _documentUuid) const
{
    return qobject_cast<BusinessLayer::ScreenplayTextModel*>(
        projectManager->modelForDocument(_documentUuid));
}

QByteArray GoogleDocsManager::Implementation::exportDocx(
    BusinessLayer::ScreenplayTextModel* _model) const
{
    QTemporaryDir directory;
    if (!directory.isValid()) {
        return {};
    }

    BusinessLayer::ScreenplayExportOptions options;
    options.filePath = directory.filePath(QStringLiteral("screenplay.docx"));
    options.fileFormat = BusinessLayer::ExportFileFormat::Docx;
    //
    // Title page would come back as action on pull
    //
    options.includeTitlePage = false;
    options.includeText = true;
    options.includeFolders = true;
    options.includeInlineNotes = false;
    options.includeReviewMarks = false;
    options.showScenesNumbers = true;
    if (const auto information = _model->informationModel(); information != nullptr) {
        options.templateId = information->templateId();
        options.header = information->header();
        options.footer = information->footer();
    }

    const GoogleDocExporter docxExporter;
    const BusinessLayer::ScreenplayExporter& exporter = docxExporter;
    exporter.exportTo(_model, options);

    QFile file(options.filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void GoogleDocsManager::Implementation::push(const QUuid& _documentUuid)
{
    const auto model = screenplayModel(_documentUuid);
    if (model == nullptr) {
        return;
    }
    const auto content = exportDocx(model);
    if (content.isEmpty()) {
        inform(tr("Push to Google Docs"), tr("Can't prepare the screenplay for Google Docs."));
        return;
    }

    ++syncGeneration;
    const auto fileId = settingsValue(settingsKey(_documentUuid, kFileIdKey)).toString();
    if (fileId.isEmpty()) {
        uploadNew(_documentUuid, content);
        return;
    }

    //
    // Check that the Doc was not edited in Google Docs since the last sync
    //
    isBusy = true;
    driveClient->fileMetadata(fileId, [this, _documentUuid, fileId, content](
                                          const GoogleDriveFile& _file,
                                          const GoogleDriveError& _error) {
        if (_error.type == GoogleDriveError::Type::NotFound || (!_error.isError() && _file.trashed)) {
            uploadNew(_documentUuid, content);
            return;
        }
        if (_error.isError()) {
            isBusy = false;
            showError(tr("Push to Google Docs"), _error);
            return;
        }

        driveClient->exportFile(fileId, "text/plain", [this, _documentUuid, fileId, content](
                                                          const QByteArray& _text,
                                                          const GoogleDriveError& _error) {
            if (_error.isError()) {
                isBusy = false;
                showError(tr("Push to Google Docs"), _error);
                return;
            }

            const auto lastHash = settingsValue(settingsKey(_documentUuid, kTextHashKey)).toString();
            if (lastHash.isEmpty() || textHash(QString::fromUtf8(_text)) == lastHash) {
                updateExisting(_documentUuid, fileId, content);
                return;
            }

            isBusy = false;
            static constexpr int cancelButtonId = 0;
            static constexpr int pullButtonId = 1;
            static constexpr int overwriteButtonId = 2;
            ask(tr("The Google Doc was edited"),
                tr("Somebody changed the Google Doc after the last push or pull. Pull the changes "
                   "into a new draft first, or overwrite the Google Doc with the text from the "
                   "app?"),
                { { cancelButtonId, tr("Cancel"), Dialog::RejectButton },
                  { overwriteButtonId, tr("Overwrite"), Dialog::AcceptCriticalButton },
                  { pullButtonId, tr("Pull first"), Dialog::AcceptButton } },
                [this, _documentUuid, fileId, content](int _buttonId) {
                    if (_buttonId == pullButtonId) {
                        pull(_documentUuid);
                    } else if (_buttonId == overwriteButtonId) {
                        isBusy = true;
                        updateExisting(_documentUuid, fileId, content);
                    }
                });
        });
    });
}

void GoogleDocsManager::Implementation::withProjectFolder(
    const std::function<void(const QString&)>& _action,
    const std::function<void(const GoogleDriveError&)>& _fail)
{
    const QString projectName = projectManager->projectName();
    driveClient->findOrCreateFolder(
        kRootFolderName, {},
        [this, projectName, _action, _fail](const GoogleDriveFile& _rootFolder,
                                            const GoogleDriveError& _error) {
            if (_error.isError()) {
                _fail(_error);
                return;
            }
            driveClient->findOrCreateFolder(
                projectName.isEmpty() ? tr("Untitled project") : projectName, _rootFolder.id,
                [_action, _fail](const GoogleDriveFile& _projectFolder,
                                 const GoogleDriveError& _error) {
                    if (_error.isError()) {
                        _fail(_error);
                        return;
                    }
                    _action(_projectFolder.id);
                });
        });
}

QString GoogleDocsManager::Implementation::documentNameFor(const QUuid& _documentUuid,
                                                           const QString& _suffix) const
{
    const auto model = screenplayModel(_documentUuid);
    QStringList parts;
    if (const auto projectName = projectManager->projectName(); !projectName.isEmpty()) {
        parts.append(projectName);
    }
    if (!_suffix.isEmpty()) {
        parts.append(_suffix);
    } else if (model != nullptr && model->informationModel() != nullptr
               && !model->informationModel()->name().isEmpty()) {
        parts.append(model->informationModel()->name());
    }
    return parts.join(QStringLiteral(" - "));
}

void GoogleDocsManager::Implementation::uploadNew(const QUuid& _documentUuid,
                                                  const QByteArray& _content)
{
    isBusy = true;
    const auto fail = [this](const GoogleDriveError& _error) {
        isBusy = false;
        showError(tr("Push to Google Docs"), _error);
    };
    withProjectFolder(
        [this, _documentUuid, _content, fail](const QString& _folderId) {
            driveClient->createDocument(
                documentNameFor(_documentUuid, {}), _folderId, _content, kDocxMimeType,
                { { kDocumentUuidProperty, _documentUuid.toString(QUuid::WithoutBraces) } },
                [this, _documentUuid, fail](const GoogleDriveFile& _file,
                                            const GoogleDriveError& _error) {
                    if (_error.isError()) {
                        fail(_error);
                        return;
                    }
                    storeSyncState(_documentUuid, _file, true);
                });
        },
        fail);
}

void GoogleDocsManager::Implementation::updateExisting(const QUuid& _documentUuid,
                                                       const QString& _fileId,
                                                       const QByteArray& _content)
{
    driveClient->updateDocumentContent(
        _fileId, _content, kDocxMimeType,
        [this, _documentUuid](const GoogleDriveFile& _file, const GoogleDriveError& _error) {
            if (_error.isError()) {
                isBusy = false;
                showError(tr("Push to Google Docs"), _error);
                return;
            }
            storeSyncState(_documentUuid, _file, true);
        });
}

void GoogleDocsManager::Implementation::storeSyncState(const QUuid& _documentUuid,
                                                       const GoogleDriveFile& _file,
                                                       bool _showPushedMessage)
{
    ++syncGeneration;
    setSettingsValue(settingsKey(_documentUuid, kFileIdKey), _file.id);
    setSettingsValue(settingsKey(_documentUuid, kFormatKey), kDocxFormat);
    setSettingsValue(settingsKey(_documentUuid, kSyncedAtKey), QDateTime::currentDateTimeUtc());

    //
    // Remember the text as Google Docs returns it after the conversion, it is the base for the
    // next "edited in Google Docs" check
    //
    driveClient->exportFile(
        _file.id, "text/plain",
        [this, _documentUuid, _file, _showPushedMessage](const QByteArray& _text,
                                                         const GoogleDriveError& _error) {
            if (!_error.isError()) {
                setSettingsValue(settingsKey(_documentUuid, kTextHashKey),
                                 textHash(QString::fromUtf8(_text)));
            }
            if (!_showPushedMessage) {
                isBusy = false;
                return;
            }

            pushDossier(_documentUuid, [this, _file](const QString& _dossierName) {
                isBusy = false;
                showPushedMessage(_file, _dossierName);
            });
        });
}

QByteArray GoogleDocsManager::Implementation::dossierHtml(const QUuid& _documentUuid) const
{
    const auto model = screenplayModel(_documentUuid);
    if (model == nullptr) {
        return {};
    }

    QVector<BusinessLayer::DossierSection> sections;
    sections.append({ tr("Title page"),
                      paragraphsOf(projectManager->siblingModelForDocument(
                          _documentUuid, Domain::DocumentObjectType::ScreenplayTitlePage)) });
    if (const auto information = model->informationModel(); information != nullptr) {
        sections.append({ tr("Logline"), { { information->logline(), false } } });
    }
    sections.append({ tr("Synopsis"),
                      paragraphsOf(projectManager->siblingModelForDocument(
                          _documentUuid, Domain::DocumentObjectType::ScreenplaySynopsis)) });
    sections.append({ tr("Treatment"), treatmentOf(model) });
    return BusinessLayer::screenplayDossierHtml(sections);
}

void GoogleDocsManager::Implementation::pushDossier(
    const QUuid& _documentUuid, const std::function<void(const QString&)>& _done)
{
    const auto content = dossierHtml(_documentUuid);
    if (content.isEmpty()) {
        _done({});
        return;
    }

    const auto fail = [this, _done](const GoogleDriveError& _error) {
        showError(tr("Push to Google Docs"), _error);
        _done({});
    };
    const auto create = [this, _documentUuid, content, fail, _done] {
        withProjectFolder(
            [this, _documentUuid, content, fail, _done](const QString& _folderId) {
                driveClient->createDocument(
                    documentNameFor(_documentUuid, tr("Dossier")), _folderId, content,
                    kHtmlMimeType, {},
                    [this, _documentUuid, fail, _done](const GoogleDriveFile& _file,
                                                       const GoogleDriveError& _error) {
                        if (_error.isError()) {
                            fail(_error);
                            return;
                        }
                        storeDossierState(_documentUuid, _file, _done);
                    });
            },
            fail);
    };

    const auto fileId = settingsValue(settingsKey(_documentUuid, kDossierFileIdKey)).toString();
    if (fileId.isEmpty()) {
        create();
        return;
    }

    driveClient->fileMetadata(fileId, [this, _documentUuid, fileId, content, fail, create, _done](
                                          const GoogleDriveFile& _file,
                                          const GoogleDriveError& _error) {
        if (_error.type == GoogleDriveError::Type::NotFound || (!_error.isError() && _file.trashed)) {
            create();
            return;
        }
        if (_error.isError()) {
            fail(_error);
            return;
        }

        driveClient->exportFile(fileId, "text/plain", [this, _documentUuid, _file, fileId, content,
                                                       fail, _done](const QByteArray& _text,
                                                                    const GoogleDriveError& _error) {
            if (_error.isError()) {
                fail(_error);
                return;
            }

            const auto lastHash
                = settingsValue(settingsKey(_documentUuid, kDossierHashKey)).toString();
            if (lastHash.isEmpty() || textHash(QString::fromUtf8(_text)) == lastHash) {
                updateDossier(_documentUuid, fileId, content, _done);
                return;
            }

            //
            // This Doc is not pulled, so edits made there would be lost without a trace
            //
            static constexpr int keepButtonId = 0;
            static constexpr int overwriteButtonId = 1;
            isBusy = false;
            ask(tr("The Google Doc was edited"),
                tr("Somebody changed the Google Doc \"%1\". The title page, logline, synopsis and "
                   "treatment are not pulled into the app. Overwrite the changes with the text from "
                   "the app?")
                    .arg(_file.name),
                { { keepButtonId, tr("Keep the changes"), Dialog::RejectButton },
                  { overwriteButtonId, tr("Overwrite"), Dialog::AcceptCriticalButton } },
                [this, _documentUuid, fileId, content, _done](int _buttonId) {
                    isBusy = true;
                    if (_buttonId == overwriteButtonId) {
                        updateDossier(_documentUuid, fileId, content, _done);
                    } else {
                        _done({});
                    }
                });
        });
    });
}

void GoogleDocsManager::Implementation::updateDossier(
    const QUuid& _documentUuid, const QString& _fileId, const QByteArray& _content,
    const std::function<void(const QString&)>& _done)
{
    driveClient->updateDocumentContent(
        _fileId, _content, kHtmlMimeType,
        [this, _documentUuid, _done](const GoogleDriveFile& _file, const GoogleDriveError& _error) {
            if (_error.isError()) {
                showError(tr("Push to Google Docs"), _error);
                _done({});
                return;
            }
            storeDossierState(_documentUuid, _file, _done);
        });
}

void GoogleDocsManager::Implementation::storeDossierState(
    const QUuid& _documentUuid, const GoogleDriveFile& _file,
    const std::function<void(const QString&)>& _done)
{
    setSettingsValue(settingsKey(_documentUuid, kDossierFileIdKey), _file.id);
    driveClient->exportFile(_file.id, "text/plain",
                            [_documentUuid, _file, _done](const QByteArray& _text,
                                                          const GoogleDriveError& _error) {
                                if (!_error.isError()) {
                                    setSettingsValue(settingsKey(_documentUuid, kDossierHashKey),
                                                     textHash(QString::fromUtf8(_text)));
                                }
                                _done(_file.name);
                            });
}

void GoogleDocsManager::Implementation::showPushedMessage(const GoogleDriveFile& _file,
                                                          const QString& _dossierName)
{
    auto text = tr("The screenplay is in the Google Doc \"%1\" (folder \"%2\" in your Google "
                   "Drive).")
                    .arg(_file.name, kRootFolderName);
    if (!_dossierName.isEmpty()) {
        text += "\n\n"
            + tr("The title page, logline, synopsis and treatment are in the Google Doc \"%1\" in "
                 "the same folder. Changes made there are not pulled into the app.")
                  .arg(_dossierName);
    }

    static constexpr int closeButtonId = 0;
    static constexpr int openButtonId = 1;
    ask(tr("Pushed to Google Docs"), text,
        { { closeButtonId, tr("Close"), Dialog::RejectButton },
          { openButtonId, tr("Open in browser"), Dialog::AcceptButton } },
        [_file](int _buttonId) {
            if (_buttonId == openButtonId) {
                QDesktopServices::openUrl(documentUrl(_file.id));
            }
        });
}

void GoogleDocsManager::Implementation::pull(const QUuid& _documentUuid)
{
    const auto fileId = settingsValue(settingsKey(_documentUuid, kFileIdKey)).toString();
    if (fileId.isEmpty()) {
        inform(tr("Pull from Google Docs"),
               tr("This screenplay was not pushed to Google Docs yet. Push it first."));
        return;
    }

    isBusy = true;
    ++syncGeneration;
    driveClient->exportFile(fileId, "text/plain", [this, _documentUuid, fileId](
                                                      const QByteArray& _text,
                                                      const GoogleDriveError& _error) {
        if (_error.isError()) {
            isBusy = false;
            showError(tr("Pull from Google Docs"), _error);
            return;
        }

        const QString text = QString::fromUtf8(_text);
        if (text.trimmed().isEmpty()) {
            isBusy = false;
            inform(tr("Pull from Google Docs"), tr("The Google Doc is empty."));
            return;
        }

        if (settingsValue(settingsKey(_documentUuid, kFormatKey)).toString() == kDocxFormat) {
            pullDocx(_documentUuid, fileId, text);
            return;
        }

        //
        // Pushed by an older version: Fountain text. Leading new line prevents the first line from
        // being parsed as a title page key
        //
        isBusy = false;
        BusinessLayer::ScreenplayFountainImporter importer;
        addPulledDraft(_documentUuid, importer.screenplayText("\n" + text).text, text);
    });
}

void GoogleDocsManager::Implementation::pullDocx(const QUuid& _documentUuid, const QString& _fileId,
                                                 const QString& _text)
{
    driveClient->exportFile(
        _fileId, kDocxMimeType,
        [this, _documentUuid, _text](const QByteArray& _docx, const GoogleDriveError& _error) {
            isBusy = false;
            if (_error.isError()) {
                showError(tr("Pull from Google Docs"), _error);
                return;
            }

            QTemporaryDir directory;
            BusinessLayer::ImportOptions options;
            options.filePath = directory.filePath(QStringLiteral("screenplay.docx"));
            QFile file(options.filePath);
            if (!directory.isValid() || !file.open(QIODevice::WriteOnly)
                || file.write(_docx) != _docx.size()) {
                inform(tr("Pull from Google Docs"), tr("Can't read the Google Doc."));
                return;
            }
            file.close();

            BusinessLayer::ScreenplayGoogleDocImporter importer;
            importer.addKnownParagraphs(screenplayModel(_documentUuid));
            const auto screenplays = importer.importScreenplays(options);
            addPulledDraft(_documentUuid,
                           screenplays.isEmpty() ? QString() : screenplays.constFirst().text,
                           _text);
        });
}

void GoogleDocsManager::Implementation::addPulledDraft(const QUuid& _documentUuid,
                                                       const QString& _screenplayXml,
                                                       const QString& _text)
{
    if (_screenplayXml.isEmpty()) {
        inform(tr("Pull from Google Docs"), tr("Can't read the Google Doc."));
        return;
    }

    ++syncGeneration;
    const auto draftName
        = tr("Google Docs %1").arg(QDateTime::currentDateTime().toString("dd/MM HH:mm"));
    projectManager->addDocumentDraft(_documentUuid, draftName, _screenplayXml.toUtf8());
    setSettingsValue(settingsKey(_documentUuid, kTextHashKey), textHash(_text));
    setSettingsValue(settingsKey(_documentUuid, kSyncedAtKey), QDateTime::currentDateTimeUtc());
    inform(tr("Pull from Google Docs"),
           tr("The text from Google Docs was added as the new draft \"%1\". The current text "
              "was not changed.")
               .arg(draftName));
}

void GoogleDocsManager::Implementation::openInBrowser(const QUuid& _documentUuid)
{
    const auto fileId = settingsValue(settingsKey(_documentUuid, kFileIdKey)).toString();
    if (fileId.isEmpty()) {
        inform(tr("Open in Google Docs"),
               tr("This screenplay was not pushed to Google Docs yet. Push it first."));
        return;
    }
    QDesktopServices::openUrl(documentUrl(fileId));
}

void GoogleDocsManager::Implementation::checkForChanges()
{
    if (isBusy || isCheckingForChanges || isPullOffered || !authManager->isSignedIn()
        || QGuiApplication::applicationState() != Qt::ApplicationActive
        || QApplication::activeModalWidget() != nullptr) {
        return;
    }

    QVector<QUuid> documents;
    for (const auto& documentUuid : projectManager->screenplayDocuments()) {
        if (!settingsValue(settingsKey(documentUuid, kFileIdKey)).toString().isEmpty()) {
            documents.append(documentUuid);
        }
    }
    if (documents.isEmpty()) {
        return;
    }

    isCheckingForChanges = true;
    checkNextDocument(documents, syncGeneration);
}

void GoogleDocsManager::Implementation::checkNextDocument(QVector<QUuid> _documents,
                                                          int _generation)
{
    if (_documents.isEmpty() || _generation != syncGeneration) {
        isCheckingForChanges = false;
        return;
    }

    //
    // Errors are not shown: nobody asked for this check, and the app may be offline
    //
    const auto documentUuid = _documents.takeFirst();
    const auto fileId = settingsValue(settingsKey(documentUuid, kFileIdKey)).toString();
    driveClient->fileMetadata(fileId, [this, _documents, _generation, documentUuid, fileId](
                                          const GoogleDriveFile& _file,
                                          const GoogleDriveError& _error) {
        const auto modifiedTime = _file.modifiedTime.toUTC().toString(Qt::ISODateWithMs);
        if (_error.isError() || _file.trashed
            || modifiedTime
                == settingsValue(settingsKey(documentUuid, kSeenModifiedTimeKey)).toString()) {
            checkNextDocument(_documents, _generation);
            return;
        }

        driveClient->exportFile(
            fileId, "text/plain",
            [this, _documents, _generation, documentUuid, modifiedTime,
             _file](const QByteArray& _text, const GoogleDriveError& _error) {
                if (_error.isError() || _generation != syncGeneration) {
                    checkNextDocument(_documents, _generation);
                    return;
                }

                setSettingsValue(settingsKey(documentUuid, kSeenModifiedTimeKey), modifiedTime);
                const auto hash = textHash(QString::fromUtf8(_text));
                if (hash == settingsValue(settingsKey(documentUuid, kTextHashKey)).toString()
                    || hash
                        == settingsValue(settingsKey(documentUuid, kOfferedHashKey)).toString()) {
                    checkNextDocument(_documents, _generation);
                    return;
                }

                setSettingsValue(settingsKey(documentUuid, kOfferedHashKey), hash);
                isCheckingForChanges = false;
                offerPull(documentUuid, _file.name);
            });
    });
}

void GoogleDocsManager::Implementation::offerPull(const QUuid& _documentUuid,
                                                  const QString& _fileName)
{
    static constexpr int laterButtonId = 0;
    static constexpr int pullButtonId = 1;
    isPullOffered = true;
    ask(tr("The Google Doc was changed"),
        tr("Somebody changed the Google Doc \"%1\". Pull the changes into a new draft? The current "
           "text will not be changed.")
            .arg(_fileName),
        { { laterButtonId, tr("Later"), Dialog::RejectButton },
          { pullButtonId, tr("Pull"), Dialog::AcceptButton } },
        [this, _documentUuid](int _buttonId) {
            isPullOffered = false;
            //
            // The project could have been closed while the question was on the screen
            //
            if (_buttonId == pullButtonId && !isBusy
                && projectManager->screenplayDocuments().contains(_documentUuid)) {
                withSignIn([this, _documentUuid] { pull(_documentUuid); });
            }
        });
}


// ****


GoogleDocsManager::GoogleDocsManager(QObject* _parent, QWidget* _parentWidget,
                                     ProjectManager* _projectManager)
    : QObject(_parent)
    , d(new Implementation(this, _parentWidget, _projectManager))
{
    connect(d->authManager, &GoogleAuthManager::signedInChanged, this, [this](bool _isSignedIn) {
        if (_isSignedIn && d->actionAfterSignIn) {
            const auto action = std::exchange(d->actionAfterSignIn, {});
            action();
        }
    });
    connect(d->authManager, &GoogleAuthManager::signInFailed, this, [this](const QString& _error) {
        d->actionAfterSignIn = {};
        d->inform(tr("Sign in to Google"), tr("Google sign in failed.") + "\n\n" + _error);
    });
    connect(d->authManager, &GoogleAuthManager::reauthenticationRequired, this, [this] {
        d->inform(tr("Sign in to Google"),
                  tr("Google sign in has expired. You will be asked to sign in again on the next "
                     "push or pull."));
    });

    d->authManager->restoreSession();

    d->changesTimer.setInterval(kChangesCheckIntervalMs);
    connect(&d->changesTimer, &QTimer::timeout, this, [this] { d->checkForChanges(); });
    d->changesTimer.start();
}

GoogleDocsManager::~GoogleDocsManager() = default;

GoogleAuthManager* GoogleDocsManager::authManager() const
{
    return d->authManager;
}

void GoogleDocsManager::handleAction(int _action, const QUuid& _documentUuid)
{
    if (d->isBusy) {
        d->inform(tr("Google Docs"), tr("Previous Google Docs operation is still in progress."));
        return;
    }

    switch (_action) {
    case Push: {
        d->withSignIn([this, _documentUuid] { d->push(_documentUuid); });
        break;
    }
    case Pull: {
        d->withSignIn([this, _documentUuid] { d->pull(_documentUuid); });
        break;
    }
    case OpenInBrowser: {
        d->openInBrowser(_documentUuid);
        break;
    }
    default: {
        break;
    }
    }
}

} // namespace ManagementLayer
