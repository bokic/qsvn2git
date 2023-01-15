#ifndef QMAINDIALOG_OLD_H
#define QMAINDIALOG_OLD_H

#include "qconvertorworker.h"
#include <QProcess>
#include <QDialog>
#include <QThread>

namespace Ui {
class QMainDialog_Old;
}

class QMainDialog_Old : public QDialog
{
    Q_OBJECT

public:
    explicit QMainDialog_Old(QWidget *parent = 0);
    ~QMainDialog_Old();

private slots:
    void on_toolButton_browse_clicked();
    void on_lineEdit_URL_textChanged(const QString &arg1);
    void on_lineEdit_DestPath_textChanged(const QString &arg1);
    void on_pushButton_ScanConvert_clicked();
    void process_finished(int exitCode, QProcess::ExitStatus exitStatus);
    void process_error(QProcess::ProcessError error);
    void worker_startRevision(QString revision);
    void worker_progressText(QString text);
    void worker_finish();

signals:
    void convert(QString url, QString destPath, QStringList revisions, QHash<QString, QString> userHash);

private:
    void updateScanConvertButton();
    Ui::QMainDialog_Old *ui;

    QThread m_workerThread;
    QConvertorWorker *m_worker;
};

#endif // QMAINDIALOG_OLD_H
