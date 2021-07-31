/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2019 Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "sysdbusregister.h"

#include <QDebug>
#include <QSharedPointer>
#include <QRegExp>
#include <QProcess>
#include <stdlib.h>
#include <QDir>

/* qt会将glib里的signals成员识别为宏，所以取消该宏
 * 后面如果用到signals时，使用Q_SIGNALS代替即可
 **/
#ifdef signals
#undef signals
#endif

extern "C" {
#include <glib.h>
#include <gio/gio.h>

}
#include <QtConcurrent/QtConcurrent>

QStringList ddcProIdList;

SysdbusRegister::SysdbusRegister()
{
    mHibernateFile = "/etc/systemd/sleep.conf";
    mHibernateSet = new QSettings(mHibernateFile, QSettings::IniFormat, this);
    mHibernateSet->setIniCodec("UTF-8");
    runThreadFlag = false;
    getBrightnessInfo();
}

SysdbusRegister::~SysdbusRegister()
{
}

void SysdbusRegister::exitService() {
    qApp->exit(0);
}

QString SysdbusRegister::GetComputerInfo() {
    QByteArray ba;
    FILE * fp = NULL;
    char cmd[128];
    char buf[1024];
    snprintf(cmd, 128, "dmidecode -t system");

    if ((fp = popen(cmd, "r")) != NULL){
        rewind(fp);
        while (!feof(fp)) {
            fgets(buf, sizeof (buf), fp);
            ba.append(buf);
        }
        pclose(fp);
        fp = NULL;
    }
    return QString(ba);
}

//获取免密登录状态
QString SysdbusRegister::getNoPwdLoginStatus(){
    QByteArray ba;
    FILE * fp = NULL;
    char cmd[128];
    char buf[1024];
    snprintf(cmd, 128, "cat /etc/group |grep nopasswdlogin");
    if ((fp = popen(cmd, "r")) != NULL){
        rewind(fp);
        fgets(buf, sizeof (buf), fp);
        ba.append(buf);
        pclose(fp);
        fp = NULL;
    }else{
        qDebug()<<"popen文件打开失败"<<endl;
    }
    return QString(ba);
}

//设置免密登录状态
void SysdbusRegister::setNoPwdLoginStatus(bool status,QString username) {

    QString cmd;
    if(true == status){
         cmd = QString("gpasswd  -a %1 nopasswdlogin").arg(username);
    } else{
        cmd = QString("gpasswd  -d %1 nopasswdlogin").arg(username);
    }
    QProcess::execute(cmd);
}

// 设置自动登录状态
void SysdbusRegister::setAutoLoginStatus(QString username) {
    QString filename = "/etc/lightdm/lightdm.conf";
    QSharedPointer<QSettings>  autoSettings = QSharedPointer<QSettings>(new QSettings(filename, QSettings::IniFormat));
    autoSettings->beginGroup("SeatDefaults");

    autoSettings->setValue("autologin-user", username);

    autoSettings->endGroup();
    autoSettings->sync();
}

QString SysdbusRegister::getSuspendThenHibernate() {
    mHibernateSet->beginGroup("Sleep");

    QString time = mHibernateSet->value("HibernateDelaySec").toString();

    mHibernateSet->endGroup();
    mHibernateSet->sync();

    return time;
}

void SysdbusRegister::setSuspendThenHibernate(QString time) {
    mHibernateSet->beginGroup("Sleep");

    mHibernateSet->setValue("HibernateDelaySec", time);

    mHibernateSet->endGroup();
    mHibernateSet->sync();
}

void SysdbusRegister::setPasswdAging(int days, QString username) {
    QString cmd;

    cmd = QString("chage -M %1 %2").arg(days).arg(username);
    QProcess::execute(cmd);
}

int SysdbusRegister::changeOtherUserPasswd(QString username, QString pwd){

    std::string str1 = username.toStdString();
    const char * user_name = str1.c_str();

    std::string str2 = pwd.toStdString();
    const char * passwd = str2.c_str();

    QString output;

    char * cmd = g_strdup_printf("/usr/bin/changeotheruserpwd %s %s", user_name, passwd);

    FILE   *stream;
    char buf[256];

    if ((stream = popen(cmd, "r" )) == NULL){
        return -1;
    }

    while(fgets(buf, 256, stream) != NULL){
        output = QString(buf).simplified();
    }

    pclose(stream);

    return 1;

}

void SysdbusRegister::setDDCBrightness(QString brightness, QString type) {
    QString program = "/usr/sbin/i2ctransfer";
    QStringList arg;
    int br=brightness.toInt();
    QString light = "0x" + QString::number(br,16);
    QString c = "0x" + QString::number(168^br,16);
    arg << "-f" << "-y" << type << "w7@0x37" << "0x51" << "0x84" << "0x03"
        << "0x10" << "0x00" << light << c;
    QProcess *vcpPro = new QProcess(this);
//    vcpPro->start(program, arg);
//    vcpPro->waitForStarted();
//    vcpPro->waitForFinished();
    vcpPro->startDetached(program, arg);
}

int SysdbusRegister::getDDCBrightness(QString type) {
    QString program = "/usr/sbin/i2ctransfer";
    QStringList arg;
    arg<<"-f"<<"-y"<<type<<"w5@0x37"<<"0x51"<<"0x82"<<"0x01"<<"0x10"<<"0xac";
    QProcess *vcpPro = new QProcess();
    vcpPro->start(program, arg);
    vcpPro->waitForStarted();
    vcpPro->waitForFinished();
    arg.clear();
    arg<<"-f"<<"-y"<<type<<"r16@0x37";
    usleep(40000);
    vcpPro->start(program, arg);
    vcpPro->waitForStarted();
    vcpPro->waitForFinished();
    QString result = vcpPro->readAllStandardOutput().trimmed();
    if (result == "")
        return -1;
    QString bri=result.split(" ").at(9);
    bool ok;
    int bright=bri.toInt(&ok,16);
    if(ok && bright >= 0 && bright <= 100)
        return bright;

    return -1;
}

int SysdbusRegister::changeRTC() {
    QString cmd = "hwclock -w";
    return system(cmd.toLatin1().data());
}

bool SysdbusRegister::setNtpSerAddress(QString serverAddress)
{
    if (serverAddress == "default") {
        system("rm -rf /etc/systemd/timesyncd.conf.d/");
        system("timedatectl set-ntp false");
        system("timedatectl set-ntp true");
        return true;
    }

    QString dirName  = "/etc/systemd/timesyncd.conf.d/";
    QString fileName = "/etc/systemd/timesyncd.conf.d/kylin.conf";

    QDir  dir(dirName);
    QFile file(fileName);
    if (!dir.exists()) {
        if (dir.mkdir(dirName) == false) {
            return false;
        }
    }
    if (file.open(QIODevice::WriteOnly) == false) {
        return false;
    }
    file.write("[Time]\n");
    file.write("NTP = ");
    file.write(serverAddress.toLatin1().data());
    file.write("\n");
    file.close();
    system("timedatectl set-ntp false");
    system("timedatectl set-ntp true");
    return true;

}

void SysdbusRegister::getBrightnessInfo()
{
    if (true == runThreadFlag)
        return;

    QtConcurrent::run([=] {
        runThreadFlag = true;
        QString program = "/usr/bin/ddcutil";
        QStringList arg;
        arg << "detect";
        QProcess *vcpPro = new QProcess();
        vcpPro->start(program, arg);
        vcpPro->waitForStarted();
        vcpPro->waitForFinished();
        QByteArray arr=vcpPro->readAll();

        char *re=arr.data();
        char *p;
        QList<QString> l;
        while (*re) {
            p=strpbrk(re,"\n");
            *p=0;
            QString s=re;
            s=s.trimmed();
            l.append(s);
            re=++p;
            if(*re=='\n')
                re++;
        }
        
        for (int i=0; i < l.count(); i=i+9) {
            if (l.at(i).startsWith("Display") || l.at(i).startsWith("Invalid display")) {
                if (i+5 >= l.count())
                    break;
                QString bus=l.at(i+1).split(":").at(1).trimmed();
                QString serial=l.at(i+5).split(":").at(1).trimmed();
                QString busType = bus.split("-").at(1);
                bool existFlag = false;
                for (int i = 0; i < brightInfo_V.size(); i++) {
                    if (brightInfo_V[i].serialNum == serial) {
                        brightInfo_V[i].brightness = getDDCBrightness(busType);
                        existFlag = true;
                        break;
                    }
                }
                if (false == existFlag) {
                    struct brightInfo  mBrightInfo;
                    mBrightInfo.serialNum  = serial;
                    mBrightInfo.busType    = busType;
                    mBrightInfo.brightness = getDDCBrightness(busType);
                    brightInfo_V.push_back(mBrightInfo);
                }
            }
        }
        runThreadFlag = false;
    });
    return;
}

void SysdbusRegister::setDDCBrightnessUkui(QString brightness, QString serialNum)
{
    for (int i = 0; i < brightInfo_V.size(); i++) {
        if (brightInfo_V[i].serialNum == serialNum) {
            setDDCBrightness(brightness, brightInfo_V[i].busType);
            brightInfo_V[i].brightness = brightness.toInt();
            return;
        }
    }
}

int SysdbusRegister::getDDCBrightnessUkui(QString serialNum)
{
    for (int i = 0; i < brightInfo_V.size(); i++) {
        if (brightInfo_V[i].serialNum == serialNum && brightInfo_V[i].brightness >= 0 && brightInfo_V[i].brightness <= 100) {
            return brightInfo_V[i].brightness;
        }
    }

    getBrightnessInfo();
    return -2;   //表示没有信息，需要隔3～5秒再次获取
}
