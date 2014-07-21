#include "qmaindialog.h"
#include "ui_qmaindialog.h"
#include <QDomDocument>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QDomNode>
#include <QDir>

QMainDialog::QMainDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QMainDialog),
    m_worker(NULL)
{
    ui->setupUi(this);

    m_workerThread.start();
}

QMainDialog::~QMainDialog()
{
    if (m_worker)
    {
        m_worker->abort();
    }
    m_workerThread.quit();
    m_workerThread.wait();

    delete ui;
}

void QMainDialog::on_toolButton_browse_clicked()
{
    const QString &dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty())
    {
        ui->lineEdit_DestPath->setText(dir);
    }
}

void QMainDialog::on_lineEdit_URL_textChanged(const QString &arg1)
{
    if (arg1.isEmpty())
    {
        ui->lineEdit_URL->setStyleSheet("background: red");
        ui->lineEdit_URL->setToolTip(tr("URL field is empty."));
        ui->pushButton_ScanConvert->setEnabled(false);
    }
    else
    {
        ui->lineEdit_URL->setStyleSheet("");
        ui->lineEdit_URL->setToolTip(tr(""));
        updateScanConvertButton();
    }
}

void QMainDialog::on_lineEdit_DestPath_textChanged(const QString &arg1)
{
    QDir dir(arg1);

    if (arg1.isEmpty())
    {
        ui->lineEdit_DestPath->setStyleSheet("background: red");
        ui->lineEdit_DestPath->setToolTip(tr("Dir field is empty."));
        ui->pushButton_ScanConvert->setEnabled(false);
    }
    else if (!dir.exists())
    {
        ui->lineEdit_DestPath->setStyleSheet("background: red");
        ui->lineEdit_DestPath->setToolTip(tr("Dir do not exist."));
        ui->pushButton_ScanConvert->setEnabled(false);
    }
    else if (dir.entryList().count() > 2) // '.' '..' are standard
    {
        ui->lineEdit_DestPath->setStyleSheet("background: red");
        ui->lineEdit_DestPath->setToolTip(tr("Dir is not empty."));
        ui->pushButton_ScanConvert->setEnabled(false);
    }
    else
    {
        ui->lineEdit_DestPath->setStyleSheet("");
        ui->lineEdit_DestPath->setToolTip("");
        updateScanConvertButton();
    }
}

void QMainDialog::updateScanConvertButton()
{
    if ((ui->lineEdit_URL->styleSheet().isEmpty())&&(ui->lineEdit_DestPath->styleSheet().isEmpty()))
    {
        ui->pushButton_ScanConvert->setEnabled(true);
    }
    else
    {
        ui->pushButton_ScanConvert->setEnabled(false);
    }
}

void QMainDialog::on_pushButton_ScanConvert_clicked()
{
    if (ui->pushButton_ScanConvert->text() == "&Scan")
    {
        ui->tableWidget_Revisions->setRowCount(0);

        QProcess *process = new QProcess(this);

        connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(process_finished(int, QProcess::ExitStatus)));
        connect(process, SIGNAL(error(QProcess::ProcessError)), SLOT(process_error(QProcess::ProcessError)));

        process->start("svn", QStringList() << "log" << ui->lineEdit_URL->text() << "--xml");

        ui->pushButton_ScanConvert->setEnabled(false);
    }
    else if (ui->pushButton_ScanConvert->text() == tr("&Convert"))
    {
        m_worker = new QConvertorWorker();
        m_worker->moveToThread(&m_workerThread);

        connect(this, SIGNAL(convert(QString, QString, QStringList, QHash<QString, QString>)), m_worker, SLOT(convert(QString, QString, QStringList, QHash<QString, QString>)));
        connect(m_worker, SIGNAL(startRevision(QString)), SLOT(worker_startRevision(QString)));
        connect(m_worker, SIGNAL(progressText(QString)), SLOT(worker_progressText(QString)));
        connect(m_worker, SIGNAL(finish()), SLOT(worker_finish()));

        QString url;
        QString destPath;
        QStringList revisions;
        QHash<QString, QString> userHash;

        url = ui->lineEdit_URL->text();
        destPath = ui->lineEdit_DestPath->text();

        for(int row = ui->tableWidget_Revisions->rowCount() - 1; row >= 0; row--)
        {
            const QString &revStr = ui->tableWidget_Revisions->item(row, 0)->text();

            revisions.append(revStr);
        }

        for(int row = 0; row < ui->tableWidget_Users->rowCount(); row++)
        {
            const QString &svn = ui->tableWidget_Users->item(row, 0)->text();
            const QString &git = ui->tableWidget_Users->item(row, 1)->text();

            userHash.insert(svn, git);
        }

        emit convert(url, destPath, revisions, userHash);

        ui->pushButton_ScanConvert->setText(tr("&Abort"));
    }
}

void QMainDialog::process_finished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *process = (QProcess *)sender();

    if (exitStatus == QProcess::NormalExit)
    {
        if (exitCode == 0)
        {
            QByteArray outBA = process->readAllStandardOutput();

            if (!outBA.isEmpty())
            {
                QDomDocument xmlDoc;

                if (xmlDoc.setContent(outBA))
                {
                    QDomElement rootElement = xmlDoc.documentElement();
                    QStringList users;

                    QDomNodeList logentrys = rootElement.elementsByTagName("logentry");

                    for(int c = 0; c < logentrys.count(); c++)
                    {
                        const QDomNode &logentry = logentrys.at(c);

                        int row = ui->tableWidget_Revisions->rowCount();

                        ui->tableWidget_Revisions->insertRow(row);

                        const QString &revision = logentry.attributes().namedItem("revision").nodeValue();
                        const QString &author = logentry.firstChildElement("author").text();
                        const QString &date = logentry.firstChildElement("date").text();
                        const QString &message = logentry.firstChildElement("msg").text();

                        ui->tableWidget_Revisions->setItem(row, 0, new QTableWidgetItem(revision));
                        ui->tableWidget_Revisions->setItem(row, 1, new QTableWidgetItem(author));
                        ui->tableWidget_Revisions->setItem(row, 2, new QTableWidgetItem(date));
                        ui->tableWidget_Revisions->setItem(row, 3, new QTableWidgetItem(message));

                        if (!users.contains(author))
                        {
                            users.append(author);
                        }
                    }

                    users.sort();

                    ui->tableWidget_Users->setRowCount(users.count());

                    for(int c = 0; c < users.count(); c++)
                    {
                        const QString &user = users.at(c);
                        ui->tableWidget_Users->setItem(c, 0, new QTableWidgetItem(user));
                    }

                    ui->pushButton_ScanConvert->setText(tr("&Convert"));
                }
            }
        }
        else
        {
            QMessageBox::critical(this, tr("Error"), tr("Process terminated with exit code %1.").arg(exitCode));
        }
    }

    process->deleteLater();

    ui->pushButton_ScanConvert->setEnabled(true);
}

void QMainDialog::process_error(QProcess::ProcessError error)
{
    QProcess *process = (QProcess *)sender();
    QString errorStr;

    switch(error)
    {
    case QProcess::FailedToStart:
        errorStr = tr("Process failed to start.");
        break;
    case QProcess::Crashed:
        errorStr = tr("Process crashed.");
        break;
    case QProcess::Timedout:
        errorStr = tr("Process timeout.");
        break;
    case QProcess::ReadError:
        errorStr = tr("Process read error.");
        break;
    case QProcess::WriteError:
        errorStr = tr("Process write error.");
        break;
    default:
        errorStr = tr("Unknown process error.");
        break;
    }

    QMessageBox::critical(this, tr("Error"), errorStr);

    process->deleteLater();

    ui->pushButton_ScanConvert->setEnabled(true);
}

void QMainDialog::worker_startRevision(QString revision)
{
    ui->plainTextEdit->appendPlainText("[" + revision + "]");
}

void QMainDialog::worker_progressText(QString text)
{
    ui->plainTextEdit->appendPlainText(text);
}

void QMainDialog::worker_finish()
{
    ui->plainTextEdit->appendPlainText("----------------------");
    ui->plainTextEdit->appendPlainText(tr("Finished!"));
}
