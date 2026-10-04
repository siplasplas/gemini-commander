#include <gtest/gtest.h>

#include <QApplication>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUndoStack>

#include <qce/CodeEdit.h>
#include <qce/CodeEditArea.h>
#include <qce/SimpleTextDocument.h>

#include "editor/editor.h"

namespace {

QByteArray readBytes(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

void writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(bytes);
}

class EditorTest : public ::testing::Test {
protected:
    QTemporaryDir dir;
    QString path(const QString& name) const { return dir.filePath(name); }
};

// Loading and saving without edits must give back the same bytes.
void expectRoundTrip(const QString& file, const QByteArray& bytes)
{
    writeBytes(file, bytes);
    Editor editor;
    ASSERT_TRUE(editor.loadFile(file));
    EXPECT_FALSE(editor.isModified());
    QFile::remove(file);
    ASSERT_TRUE(editor.saveFile());
    EXPECT_EQ(readBytes(file), bytes);
}

} // namespace

TEST_F(EditorTest, KeepsLineEndingsEncodingAndBom)
{
    expectRoundTrip(path("lf.txt"), "one\ntwo\n");
    expectRoundTrip(path("crlf.txt"), "one\r\ntwo\r\n");
    expectRoundTrip(path("no-final-newline.txt"), "one\ntwo");
    expectRoundTrip(path("empty-last-line.txt"), "one\n\n");
    expectRoundTrip(path("empty.txt"), "");
    expectRoundTrip(path("newline-only.txt"), "\n");
    expectRoundTrip(path("bom.txt"), "\xEF\xBB\xBFzaż\xC3\xB3\xC5\x82\xC4\x87\n");
    expectRoundTrip(path("latin1.txt"), "caf\xE9 \xFF\n");
}

TEST_F(EditorTest, CrlfIsShownAsSeparateLines)
{
    writeBytes(path("crlf.txt"), "one\r\ntwo\r\n");
    Editor editor;
    ASSERT_TRUE(editor.loadFile(path("crlf.txt")));
    // The document holds lines; the line break after the last one is not an extra line.
    EXPECT_EQ(editor.document()->toPlainText(), QStringLiteral("one\ntwo"));
}

TEST_F(EditorTest, UndoBackToSavedStateClearsModified)
{
    writeBytes(path("a.txt"), "abc\n");
    Editor editor;
    ASSERT_TRUE(editor.loadFile(path("a.txt")));
    editor.insertAtCursor(QStringLiteral("X"));
    EXPECT_TRUE(editor.isModified());
    EXPECT_EQ(editor.document()->toPlainText(), QStringLiteral("Xabc"));
    editor.codeEdit()->area()->undo();
    EXPECT_FALSE(editor.isModified());
    EXPECT_EQ(editor.document()->toPlainText(), QStringLiteral("abc"));
}

TEST_F(EditorTest, SaveClearsModified)
{
    writeBytes(path("a.txt"), "abc\n");
    Editor editor;
    ASSERT_TRUE(editor.loadFile(path("a.txt")));
    editor.insertAtCursor(QStringLiteral("X"));
    ASSERT_TRUE(editor.saveFile());
    EXPECT_FALSE(editor.isModified());
    EXPECT_EQ(readBytes(path("a.txt")), QByteArray("Xabc\n"));
}

TEST_F(EditorTest, MissingFileGivesEmptyDocumentSavedUnderItsName)
{
    const QString file = path("new.txt");
    Editor editor;
    ASSERT_TRUE(editor.loadFile(file));
    EXPECT_EQ(editor.filePath(), file);
    EXPECT_FALSE(QFile::exists(file));
    editor.insertAtCursor(QStringLiteral("hello"));
    ASSERT_TRUE(editor.saveFile());
    EXPECT_EQ(readBytes(file), QByteArray("hello\n"));
}

TEST_F(EditorTest, ReloadsExternalChange)
{
    const QString file = path("a.txt");
    writeBytes(file, "first\n");
    Editor editor;
    ASSERT_TRUE(editor.loadFile(file));
    EXPECT_FALSE(editor.isChangedOnDisk());
    writeBytes(file, "second line\nmore\n");
    EXPECT_TRUE(editor.isChangedOnDisk());
    ASSERT_TRUE(editor.reloadFromDisk());
    EXPECT_FALSE(editor.isChangedOnDisk());
    EXPECT_FALSE(editor.isModified());
    EXPECT_EQ(editor.document()->toPlainText(), QStringLiteral("second line\nmore"));
}

TEST_F(EditorTest, ReadOnlyIgnoresInsert)
{
    writeBytes(path("a.txt"), "abc\n");
    Editor editor;
    ASSERT_TRUE(editor.loadFile(path("a.txt")));
    editor.setReadOnly(true);
    editor.insertAtCursor(QStringLiteral("X"));
    EXPECT_FALSE(editor.isModified());
    EXPECT_EQ(editor.document()->toPlainText(), QStringLiteral("abc"));
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // Keeps Config and the Kate data directory away from the user's files.
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
