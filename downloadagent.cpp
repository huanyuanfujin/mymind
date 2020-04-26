#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <stdio.h>

QT_BEGIN_NAMESPACE
class QSslError;
QT_END_NAMESPACE

QT_USE_NAMESPACE

#include "downloadagent.h"
#include "mainwindow.h"
#include "settings.h"
#include "vymmodel.h"

extern Main *mainWindow;
extern QString vymVersion;
extern QString vymPlatform;
extern QString tmpVymDir;
extern Settings settings;
extern bool debug;

DownloadAgent::DownloadAgent(const QUrl &u)
{
    finishedScriptModelID = 0;
    url = u;
    connect(&agent, SIGNAL(finished(QNetworkReply*)),
            SLOT(requestFinished(QNetworkReply*)));

    userAgent = QString("vym %1 ( %2)")
        .arg(vymVersion)
        .arg(vymPlatform).toUtf8();
}

QString  DownloadAgent::getDestination()
{
    return tmpFile.fileName();
}

bool DownloadAgent::isSuccess()
{
    return success;
}

QString DownloadAgent::getResultMessage()
{
    return resultMessage;
}

void DownloadAgent::setFinishedAction (VymModel *m, const QString &script)
{
    finishedScriptModelID = m->getModelID();
    finishedScript = script;
}

uint DownloadAgent::getFinishedScriptModelID()
{
    return finishedScriptModelID;
}

QString DownloadAgent::getFinishedScript ()
{
    return finishedScript;
}

void DownloadAgent::setUserAgent(const QString &s)
{
    userAgent = s.toLocal8Bit();
}

void DownloadAgent::doDownload(const QUrl &url) 
{
    QNetworkRequest request(url);
    if (!userAgent.isEmpty()) request.setRawHeader("User-Agent", userAgent);

    // Only send cookies if talking to my own domain
    bool useCookies = false;
    if (url.host().contains("insilmaril.de") ) useCookies = true;

    if (useCookies)
    {
        if (debug) qDebug() << "DownloadAgent::doDownload  Using cookies to download " << url.toString();
        QByteArray idCookieValue = settings.value("/downloads/cookies/vymID/value",QByteArray() ).toByteArray();
        //idCookieValue = QVariant("2000000002601").toByteArray(); //TESTING!!!
        //qDebug()<<"idCookie="<<idCookieValue;
        if (!idCookieValue.size() == 0 )
        {
            QNetworkCookie idCookie;
            idCookie.setPath("/");
    //        idCookie.setDomain("localhost");
            idCookie.setDomain("www.insilmaril.de");
            idCookie.setName("vymID");
            idCookie.setValue(idCookieValue);
            //idCookie.setExpirationDate( settings.value("/downloads/cookies/id/expires", QVariant(QDateTime::currentDateTime().addSecs(60) )).toDateTime() ); // testing
            idCookie.setExpirationDate( QDateTime( QDate(2099,1,1) ) ); 
            agent.cookieJar()->insertCookie(idCookie);

            QNetworkCookie platformCookie;
            platformCookie.setPath("/");
    //        platformCookie.setDomain("localhost");
            platformCookie.setDomain("www.insilmaril.de");
            platformCookie.setName("vymPlatform");
            platformCookie.setValue( QVariant(vymPlatform).toByteArray() );
            platformCookie.setExpirationDate( QDateTime( QDate(2099,1,1) ) ); 
            agent.cookieJar()->insertCookie(platformCookie);
        }
    }

    QNetworkReply *reply = agent.get(request);
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), SLOT(sslErrors(QList<QSslError>)));

    currentDownloads.append(reply);
}

bool DownloadAgent::saveToDisk(const QString &filename, const QString &data)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        fprintf(stderr, "Could not open %s for writing: %s\n",
                qPrintable(filename),
                qPrintable(file.errorString()));
        return false;
    }

    file.write(data.toLatin1() );
    file.close();

    return true;
}

void DownloadAgent::execute()
{
    doDownload(url);
}

void DownloadAgent::sslErrors(const QList<QSslError> &sslErrors)
{
#ifndef QT_NO_OPENSSL
    foreach (const QSslError &error, sslErrors)
        fprintf(stderr, "SSL error: %s\n", qPrintable(error.errorString()));
#endif
}

void DownloadAgent::requestFinished(QNetworkReply *reply)
{
    QUrl url = reply->url();
    if (reply->error()) 
    {
        success = false;
        resultMessage = reply->errorString();
        emit ( downloadFinished());
    }
    else {
        success = true;
        
        if (debug) qDebug()<<"\n* DownloadAgent::reqFinished: ";
        QList <QNetworkCookie> cookies =  reply->manager()->cookieJar()->cookiesForUrl(url);
        foreach (QNetworkCookie c, cookies)
        {
            if (debug)
            {
                qDebug() << "           url: " << url.toString();
                qDebug() << "   cookie name: " << c.name();
                qDebug() << "   cookie path: " << c.path();
                qDebug() << "  cookie value: " << c.value();
                qDebug() << " cookie domain: " << c.domain();
                qDebug() << " cookie exdate: " << c.expirationDate().toLocalTime().toString();
            }

            if (c.name() == "vymID" ) 
            {
                settings.setValue( "/downloads/cookies/vymID/value", c.value());
                // settings.setValue( "/downloads/cookies/vymID/expires", c.expirationDate());
            }
        }

        QString data = reply->readAll();
        if (!tmpFile.open() )
            QMessageBox::warning( 0, tr("Warning"), "Couldn't open tmpFile " + tmpFile.fileName());
        else
        {
            if (!saveToDisk(tmpFile.fileName(), data))
                QMessageBox::warning( 0, tr("Warning"), "Couldn't write to " + tmpFile.fileName());
            else
                resultMessage = QString ("saved to %1").arg(tmpFile.fileName());
        }
        emit ( downloadFinished());
    }

    currentDownloads.removeAll(reply);
    reply->deleteLater();
}

