#include "qconvertorworker.h"
#include <QDomDocument>
#include <QDomNodeList>
#include <QStringList>
#include <QDomElement>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QObject>
#include <QDebug>
#include <QHash>
#include <QFile>
#include <QDir>


QConvertorWorker::QConvertorWorker(QObject *parent) :
    QObject(parent),
    m_abort(false)
{
}

void QConvertorWorker::abort()
{
    m_abort = true;
}

void QConvertorWorker::convert(QString url, QString destPath, QStringList revisions, QHash<QString, QString> userHash)
{
    bool hasMigrated = false;

    QProcess gitInit;
    gitInit.setWorkingDirectory(destPath);
    gitInit.start("git", QStringList() << "init");
    gitInit.waitForFinished(-1);

    foreach(const QString &revision, revisions)
    {
        if (m_abort)
        {
            emit progressText(tr("Aborted."));

            break;
        }

        emit startRevision(revision);

        QProcess process;

        emit progressText(tr("Start fetching revision info."));
        process.start("svn", QStringList() << "log" << "-v" << "-r" << revision << url << "--xml");
        process.waitForFinished(-1);
        emit progressText(tr("End fetching revision info."));

        if (process.exitCode() == 0)
        {
            QString xmlStr = process.readAllStandardOutput();

            QDomDocument xmlDoc;

            if (xmlDoc.setContent(xmlStr))
            {
                QDomElement rootElement = xmlDoc.documentElement(); // log
                QDomNode logEntryElement = rootElement.elementsByTagName("logentry").at(0);
                QString revision2 = logEntryElement.attributes().namedItem("revision").nodeValue();
                QString author = logEntryElement.firstChildElement("author").text();
                QString message = logEntryElement.firstChildElement("msg").text();
                QString date = logEntryElement.firstChildElement("date").text();

                QDomNodeList pathList = logEntryElement.firstChildElement("paths").elementsByTagName("path");

                if ((!hasMigrated)&&(isMigrationRevision(pathList)))
                {
                    emit progressText(tr("Migration revision found at rev %1.").arg(revision));

                    if (!url.endsWith('/'))
                    {
                        url.append('/');
                    }

                    url.append("trunk");

                    removeDir(destPath + "/.svn");

                    hasMigrated = true;

                    continue;
                }

                // Fetch from svn (svn checkout https://svn.code.sf.net/p/mkfusion/code@3 .)
                QProcess fetchFromSVN;
                fetchFromSVN.setWorkingDirectory(destPath);
                QString urlPathRev = url + "@" + revision;
                fetchFromSVN.start("svn", QStringList() << "checkout" << "--force" << urlPathRev << ".");
                fetchFromSVN.waitForFinished(-1);
                int exitCode = fetchFromSVN.exitCode();
                if (exitCode)
                {
                    emit progressText(tr("svn checkout exit code(%1), stdout:%2.").arg(exitCode).arg(QString::fromUtf8(fetchFromSVN.readAll())));

                    return;
                }

                if (rootElement.childNodes().count() == 0)
                {
                    emit progressText(tr("Ignoring empty revision %1.").arg(revision));

                    continue;
                }

                for(int c = 0; c < pathList.count(); c++)
                {
                    QDomNode pathNode = pathList.at(c);

                    QString action = pathNode.attributes().namedItem("action").nodeValue();
                    QString prop_mods = pathNode.attributes().namedItem("prop-mods").nodeValue();
                    QString text_mods = pathNode.attributes().namedItem("text-mods").nodeValue();
                    QString kind = pathNode.attributes().namedItem("kind").nodeValue();
                    QString path = pathNode.firstChild().nodeValue();

                    if (hasMigrated)
                    {
                        path = path.right(path.length() - 6); // remove "/trunk"
                    }

                    if (kind == "file")
                    {
                        if (action == "A")
                        {
                            gitAddFile(destPath, path.remove(0, 1));
                        }
                        else if (action == "M")
                        {
                            gitAddFile(destPath, path.remove(0, 1));
                        }
                        else if (action == "D")
                        {
                            // git rm one-of-the-directories
                            gitRemoveFile(destPath, path.remove(0, 1));
                        }
                        else
                        {
                            emit progressText(tr("Unknown item action:%1.").arg(action));

                            return;
                        }
                    }
                    else if (kind == "dir")
                    {
                        if (action == "A")
                        {
                            // Git can't add empty dirs, so we need to do this ugly hack
                            if (isEmptySvnDir(pathList, path))
                            {
                                createEmptyGitIgnoreFile(destPath+path);

                                gitAddFile(destPath, path.remove(0, 1) + "/.gitignore");
                            }
                        }
                        else if (action == "M")
                        {
                            if (prop_mods == "true")
                            {
                                if (!addGitIgnore(url + path, destPath+path, revision))
                                {
                                    return;
                                }

                                QString gitignore = path.remove(0, 1);

                                if (gitignore.isEmpty())
                                {
                                    gitignore = ".gitignore";
                                }
                                else
                                {
                                    gitignore.append("/.gitignore");
                                }

                                gitAddFile(destPath, gitignore);
                            }
                            else
                            {
                                emit progressText(tr("Warning: Modified dir:%1.").arg(path));
                            }
                        }
                        else if (action == "D")
                        {
                            if (QDir(destPath + path).exists())
                            {
                                gitRemoveDir(destPath, path.remove(0, 1));
                            }
                        }
                        else
                        {
                            emit progressText(tr("Unknown item action:%1.").arg(action));

                            return;
                        }
                    }
                    else if (kind == "")
                    {
                        // .gitignore
                        emit progressText(tr("Warning: Unimplemented .gitignore:%1.").arg(path));
                    }
                    else
                    {
                        emit progressText(tr("Unknown item kind:%1.").arg(kind));

                        return;
                    }
                }

                // git commit --date=<date> --author=<author> --message=<message>
                QProcess commitProc;
                QString commit_date, commit_author, commit_message;

                commit_date = "--date=\"" + date + "\"";
                commit_author = "--author=\"" + userHash[author] + "\"";
                if (message.isEmpty())
                {
                    message = "[no message]";
                }
                commit_message = "--message=" + encodeForStdIn(message);

                commitProc.setWorkingDirectory(destPath);
                commitProc.start("git", QStringList() << "commit" << commit_date << commit_author << commit_message);
                commitProc.waitForFinished(-1);

                if (commitProc.exitCode())
                {
                    emit progressText(tr("Error: File git commit exit code(%1), stdout:%2.").arg(commitProc.exitCode()).arg(QString::fromUtf8(commitProc.readAllStandardError())));

                    return;
                }
            }
            else
            {
                emit progressText(tr("Error parsing XML for revision %1.").arg(revision));
                break;
            }
        }
        else
        {
            emit progressText(tr("Error svn process exit code was %1.").arg(process.exitCode()));
            break;
        }
    }

    emit finish();
}

bool QConvertorWorker::updateFile(const QString &file, const QByteArray &fileContent)
{
    QFile f_handle(file);
    int fileSize;

    // ensure path exist.
    QFileInfo(file).absoluteDir().mkpath(".");

    if (!f_handle.open(QIODevice::WriteOnly))
    {
        emit progressText(tr("File open failed. %1.").arg(file));

        return false;
    }

    fileSize = fileContent.length();

    if (f_handle.write(fileContent.constData(), fileSize) != fileSize)
    {
        emit progressText(tr("File write failed. %1.").arg(file));

        return false;
    }

    f_handle.resize(fileSize);

    return true;
}

bool QConvertorWorker::gitAddFile(const QString &gitRoot, const QString &item)
{
    QProcess process;
    int exitCode;

    process.setWorkingDirectory(gitRoot);

    process.start("git", QStringList() << "add" << "-f" << item);
    process.waitForFinished(-1);
    exitCode = process.exitCode();

    if (exitCode)
    {
        QString out = QString::fromUtf8(process.readAll());

        emit progressText(tr("File git add exit code(%1), stdout:%2.").arg(exitCode).arg(out));

        return false;
    }

    return true;
}

bool QConvertorWorker::gitRemoveFile(const QString &gitRoot, const QString &item)
{
    QProcess process;
    int exitCode;

    process.setWorkingDirectory(gitRoot);

    process.start("git", QStringList() << "rm" << item);
    process.waitForFinished(-1);
    exitCode = process.exitCode();

    if (exitCode)
    {
        QString out = QString::fromUtf8(process.readAll());

        emit progressText(tr("File git rm exit code(%1), stdout:%2.").arg(exitCode).arg(out));

        return false;
    }

    return true;
}

bool QConvertorWorker::gitRemoveDir(const QString &gitRoot, const QString &item)
{
    // git rm -r one-of-the-directories
    QProcess process;
    int exitCode;

    process.setWorkingDirectory(gitRoot);

    process.start("git", QStringList() << "rm" << "-r" << item);
    process.waitForFinished(-1);
    exitCode = process.exitCode();

    if (exitCode)
    {
        QString out = QString::fromUtf8(process.readAll());

        emit progressText(tr("File git rm -r exit code(%1), stdout:%2.").arg(exitCode).arg(out));

        return false;
    }

    return true;
}

bool QConvertorWorker::isMigrationRevision(const QDomNodeList &paths)
{
    for(int c = 0; c < paths.count(); c++)
    {
        QDomNode pathNode = paths.at(c);

        QString path = pathNode.firstChild().nodeValue();

        QString rootDir = path.remove(0, 1).split('/').first();

        if (rootDir == "trunk")
        {
            return true;
        }
    }

    return false;
}

bool QConvertorWorker::addGitIgnore(const QString &url, const QString &path, const QString &revision)
{
    // svn propget svn:ignore -r 2 https://svn.code.sf.net/p/mkfusion/code
    QProcess process;
    int exitCode;

    process.start("svn", QStringList() << "propget" << "svn:ignore" << url + "@" + revision);
    process.waitForFinished(-1);
    exitCode = process.exitCode();

    if (exitCode)
    {
        QString out = QString::fromUtf8(process.readAll());

        emit progressText(tr("Error: svn propget failed. Exit code(%1), stdout:%2.").arg(exitCode).arg(out));

        return false;
    }

    QByteArray newGitIgnoreContent = process.readAllStandardOutput();

    QString gitIgnoreFile;
    if (path.endsWith('/'))
    {
        gitIgnoreFile = path + ".gitignore";
    }
    else
    {
        gitIgnoreFile = path + "/.gitignore";
    }

    QFile handle(gitIgnoreFile);
    if (!handle.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        emit progressText(tr("Error: Can\'t open .gitignore: %1.").arg(gitIgnoreFile));

        return false;
    }

    handle.write(newGitIgnoreContent.constData(), newGitIgnoreContent.length());

    handle.close();

    return true;
}

bool QConvertorWorker::isEmptySvnDir(const QDomNodeList &items, const QString &path)
{

    for(int c = 0; c < items.count(); c++)
    {
        QDomNode pathNode = items.at(c);

        QString itempath = pathNode.firstChild().nodeValue();

        if ((itempath.startsWith(path))&&(itempath.length() > path.length()))
        {
            return false;
        }
    }

    return true;
}

bool QConvertorWorker::createEmptyGitIgnoreFile(const QString &path)
{
    QString gitIgnoreFile = path + "/.gitignore";

    QFile handle(gitIgnoreFile);

    if (!handle.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        emit progressText(tr("Error: Can\'t create .gitignore: %1.").arg(gitIgnoreFile));

        return false;
    }

    handle.close();

    return true;
}

bool QConvertorWorker::removeDir(const QString &dirName)
{
    bool result = true;
    QDir dir(dirName);

    if (dir.exists(dirName)) {
        Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
            if (info.isDir()) {
                result = removeDir(info.absoluteFilePath());
            }
            else {
                result = QFile::remove(info.absoluteFilePath());
            }

            if (!result) {
                return result;
            }
        }
        result = dir.rmdir(dirName);
    }
    return result;
}

QString QConvertorWorker::encodeForStdIn(const QString &text)
{
    QString ret;

    for(int c = 0; c < text.length(); c++)
    {
        QChar ch = text.at(c);

        if (ch == '"')
        {
            ret.append("\\\"");
        }
        else if (ch == '\t')
        {
            ret.append("\\t");
        }
        else if (ch == '\r')
        {
            continue;
        }
        else
        {
            ret.append(ch);
        }
    }

    return ret;
}
