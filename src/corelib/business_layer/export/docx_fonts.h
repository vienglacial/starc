#pragma once

#include <QByteArray>
#include <QString>

#include <corelib_global.h>

namespace BusinessLayer {

/**
 * @brief Jenny Script fork: set one font family in the WordprocessingML of a DOCX part, on every
 *        w:rFonts element of the runs and the styles (w:ascii, w:hAnsi, w:cs and w:eastAsia).
 *        Theme fonts (w:asciiTheme...) are left as they are.
 * @return number of replaced attributes, the XML is not touched when it is 0
 */
CORE_LIBRARY_EXPORT int replaceDocxXmlFonts(QByteArray& _xml, const QString& _fontFamily);

/**
 * @brief Jenny Script fork: the same for every XML part under word/ of a DOCX file. Used for
 *        Google Docs, which has neither the fonts installed on the computer nor the Vietnamese
 *        letters of its own Courier Prime.
 * @return false when the file was left unchanged because it could not be read or written
 */
CORE_LIBRARY_EXPORT bool replaceDocxFonts(const QString& _filePath, const QString& _fontFamily);

} // namespace BusinessLayer
