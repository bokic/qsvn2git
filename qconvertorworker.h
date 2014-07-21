#ifndef QCONVERTORWORKER_H
#define QCONVERTORWORKER_H

#include <QDomNodeList>
#include <QStringList>
#include <QString>
#include <QObject>
#include <QHash>

class QConvertorWorker : public QObject
{
    Q_OBJECT
public:
    explicit QConvertorWorker(QObject *parent = 0);
    void abort();

signals:
    void startRevision(QString revision);
    void progressText(QString text);
    void finish();

public slots:
    void convert(QString url, QString destPath, QStringList revisions, QHash<QString, QString> userHash);

private:
    bool updateFile(const QString &file, const QByteArray &fileContent);
    bool gitAddFile(const QString &gitRoot, const QString &item);
    bool gitRemoveFile(const QString &gitRoot, const QString &item);
    bool gitRemoveDir(const QString &gitRoot, const QString &item);
    bool isMigrationRevision(const QDomNodeList &paths);
    bool addGitIgnore(const QString &url, const QString &path, const QString &revision);
    bool isEmptySvnDir(const QDomNodeList &items, const QString &path);
    bool createEmptyGitIgnoreFile(const QString &path);
    static bool removeDir(const QString & dirName);
    QString encodeForStdIn(const QString &text);

    volatile bool m_abort;
};

#endif // QCONVERTORWORKER_H
