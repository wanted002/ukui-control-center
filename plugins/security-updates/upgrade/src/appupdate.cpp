#include "appupdate.h"
#include <QPixmap>
#include <stdio.h>
#include <QLocale>
#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


#define JSON_FILE_PATH "/usr/share/kylin-update-desktop-config/data/"
#define CONFIG_FILE_PATH "/usr/share/ukui-control-center/upgrade/"

AppUpdateWid::AppUpdateWid(AppAllMsg msg,QWidget *parent):QWidget(parent)
{
    m_updateMutual = UpdateDbus::getInstance(this);
    qRegisterMetaType<AppMsg>("AppMsg"); //注册信号槽类型
    qRegisterMetaType<AppAllMsg>("AppAllMsg"); //注册信号槽类型
    appAllMsg = msg;
    timer = new QTimer();
    downloadPath = QString("%1%2/").arg(DOWN_CACHE_PATH).arg(appAllMsg.name);
    downloadProcess = new QProcess();
    updateAppUi(appAllMsg.name);
    this->setObjectName("AppUpdateWid");
    initConnect();
}

AppUpdateWid::~AppUpdateWid()
{
    qDebug() << "delete->"<< appAllMsg.name;
}

void AppUpdateWid::initConnect()
{
    connect(detaileInfo,&QPushButton::clicked,this,&AppUpdateWid::showDetails);
    connect(updatelogBtn,&QPushButton::clicked,this,&AppUpdateWid::showUpdateLog);
    connect(updateAPPBtn,&QPushButton::clicked,this,&AppUpdateWid::cancelOrUpdate);
    connect(m_updateMutual,&UpdateDbus::transferAptProgress,this,&AppUpdateWid::showInstallStatues);
    //绑定wget进程结束的信号getAppMessage
    connect(downloadProcess, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [=] (int exitCode, QProcess::ExitStatus exitStatus) {
        qDebug()  << "exitcode:" << exitCode << exitStatus;
        if(!isCancel) //当包存在依赖时，需要显示包的所有依赖以及自身包的总大小
        {
            if(exitCode == 0) //正常下载结束
            {
                qDebug() << "path:" << path;
                if(QFile::exists(path))
                {
                    connectTimes = 0;
                    slotDownloadPackages();
                }
                else
                    changeDownloadState(10); //下载过程中删除了该包，导致出现异常特殊处理
            }
            else //下载异常处理
                changeDownloadState(exitCode);
        }
        //            qDebug()  << "exitcode:" << exitCode << exitStatus;
    });
    connect(m_updateMutual, &UpdateDbus::copyFinish, this, &AppUpdateWid::startInstall);
    connect(timer, &QTimer::timeout, this, &AppUpdateWid::calculateSpeedProgress);
}

//函数：下载异常时调用该函数
void AppUpdateWid::changeDownloadState(int state)
{
    if(state == 3  || state == 1) //磁盘空间不够
    {
        isCancel = true;
        timer->stop();
        //        appVersion->setText(tr("磁盘空间不足！"));
        appVersion->setText(tr("Lack of local disk space!"));
        appVersion->setToolTip("");
        QIcon icon = QIcon::fromTheme("dialog-info");
        QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(14, 14)));
        appVersionIcon->setPixmap(pixmap);
        //        updateAPPBtn->setText(tr("更新"));
        updateAPPBtn->setText(tr("Update"));
        emit changeUpdateAllSignal(false);

    }
    else if(state == 4) //下载网络异常
    {
        if(connectTimes < 20) //判断重连次数，重连20次后退出下载
        {
            connectTimes += 1;
            wgetDownload(urlmsg,path);
            qDebug()  << "exitcode:" << state << "connectTimes:" << connectTimes;
        }
        else
        {
            isCancel = true;
            timer->stop();
            //            appVersion->setText(tr("网络异常！"));
            appVersion->setText(tr("Network abnormal!"));
            appVersion->setToolTip("");
            QIcon icon = QIcon::fromTheme("dialog-info");
            QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(16, 16)));
            appVersionIcon->setPixmap(pixmap);
            //            updateAPPBtn->setText(tr("更新"));
            updateAPPBtn->setText(tr("Update"));
            emit changeUpdateAllSignal(false);
        }
    }

    else if(state == 8) //自定义错误码，下载过程软件包被删除
    {
        isCancel = true;
        timer->stop();
        appVersion->setText(tr("Download failed!"));
        QString str = urlmsg.name;
        appVersion->setToolTip(str+tr("failed to get from the source!"));
        QIcon icon = QIcon::fromTheme("dialog-info");
        QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(16, 16)));
        appVersionIcon->setPixmap(pixmap);
        updateAPPBtn->hide();
        m_updateMutual->importantList.removeOne(appAllMsg.name);
        m_updateMutual->failedList.append(appAllMsg.name);
        emit hideUpdateBtnSignal(false);
    }
    else if(state == 10) //自定义错误码，下载过程软件包被删除
    {
        isCancel = true;
        timer->stop();
        //        appVersion->setText(tr("下载失败！"));
        appVersion->setText(tr("Download failed!"));
        //        appVersion->setToolTip(tr("下载缓存已被删除"));
        appVersion->setToolTip(tr("The download cache has been removed"));
        QIcon icon = QIcon::fromTheme("dialog-info");
        QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(16, 16)));
        appVersionIcon->setPixmap(pixmap);
        updateAPPBtn->hide();
        m_updateMutual->importantList.removeOne(appAllMsg.name);
        m_updateMutual->failedList.append(appAllMsg.name);
        emit hideUpdateBtnSignal(false);
    }
}

//从列表中取出下载信息开始下载
void AppUpdateWid::slotDownloadPackages()
{
    //    qDebug() << "list:" << appAllMsg.msg.depList.length();
    if(appAllMsg.msg.depList.length() != 0)
    {
        QDir dir = downloadPath;
        if(!dir.exists())
        {
            //            qDebug() << "not exists:" <<QString("%1%2").arg(DOWN_CACHE_PATH).arg(appAllMsg.name);
            dir.mkpath(QString("%1%2").arg(DOWN_CACHE_PATH).arg(appAllMsg.name));
        }
        urlmsg = appAllMsg.msg.depList.at(0);
        path = QString("%1%2").arg(downloadPath).arg(urlmsg.fullname);
        if(!firstDownload) //判断是否为第一个下载的软件包，第一次下载时不需要从容器中移除软件包，下载完成后才需要移除。
        {
            priorSize += urlmsg.size;
            downloadPackages.append(path); //下载完成后将软件包路径存放到list中，发送到workthread线程，再遍历拷贝到archives目录下
            appAllMsg.msg.depList.erase(appAllMsg.msg.depList.begin()); //下载完一个包从列表中移除
            if(appAllMsg.msg.depList.length() != 0)
            {
                urlmsg = appAllMsg.msg.depList.at(0);
                path = QString("%1%2").arg(downloadPath).arg(urlmsg.fullname);
                if(!isCancel)
                    wgetDownload(urlmsg, path);
            }else
                downloadFinish = true; //下载完成调用dbus接口拷贝文件到/var/cache/apt/archives目录下
        }else{
            //            qDebug() << "first download";
            firstDownload = false;
            if(!isCancel)
                wgetDownload(urlmsg, path);
        }
    }else{
        startInstall(appAllMsg.name); //包已经存在于/var/cache/apt/archives目录下，直接安装
        appVersion->setText(tr("Ready to install"));
    }
}

//开启wget断点续传
void AppUpdateWid::wgetDownload(UrlMsg msg, QString path)
{
    QStringList args;
    args.append("-c");
    args.append("-P");
    args.append(QString("%1").arg(DOWN_CACHE_PATH));
    args.append(QString("%1").arg(msg.url.remove("'")));
    args.append("-O");
    args.append(QString("%1").arg(path));
    args.append("-T");
    args.append("10");
    //    args.append("--limit-rate 1"); //预留超时接口
    currentPackage = msg.fullname;
    qDebug() << "currentPackage" << currentPackage << "size:" << msg.size;
    downloadProcess->start("/usr/bin/wget", args);
}

//接收后台dbus信号开始安装软件包
void AppUpdateWid::startInstall(QString appName)
{
    if(appName == appAllMsg.name)
    {
        //        m_updateMutual->init_cache();
        updateAPPBtn->hide();
        m_updateMutual->installAndUpgrade(appAllMsg.name);
        workProcess = new QProcess();
        workProcess->start(QString("rm -r %1").arg(downloadPath)); //删除创建的目录保证缓存环境的干净
    }
}

/* remove enter */
void AppUpdateWid::remove_last_enter(char *p_src_in_out)
{
    if (p_src_in_out == NULL) {
        return;
    }

    char *p_tmp = p_src_in_out + strlen(p_src_in_out) - 1;
    if (*p_tmp == '\n') {
        *p_tmp = '\0';
    }

    return;
}

/* analysis config file */
QStringList AppUpdateWid::analysis_config_file(char *p_file_path)
{
    FILE *fd = NULL;
    char p_buf[1024];

    QStringList dst;
    dst.clear();

    memset(p_buf , 0x00 , sizeof(p_buf));

    //printf("Info : p_file_path = [%s]\n" , p_file_path);
    fd = fopen(p_file_path , "r");
    if (fd == NULL) {
        printf("Error : open reboot config file fail\n");
        return dst;
    }

    while (fgets(p_buf , sizeof(p_buf) , fd) != NULL) {
        remove_last_enter(p_buf);
        if (!strlen(p_buf)) {
            memset(p_buf , 0x00 , sizeof(p_buf));
            continue;
        }

        //printf("Info : config file data [%s]\n" , p_buf);

        dst << QString(p_buf);

        memset(p_buf , 0x00 , sizeof(p_buf));
    }

    return dst;
}

//修改界面状态值
void AppUpdateWid::showInstallStatues(QString status,QString appAptName, float progress ,QString errormsg)
{
    char p_path[1024];
    memset(p_path , 0x00 , sizeof(p_path));
    sprintf(p_path , "%s%s" , CONFIG_FILE_PATH , "need-reboot.conf");
    QStringList reboot = analysis_config_file(p_path);
    qDebug() << "Info : need reboot pkg :" << reboot;

    memset(p_path , 0x00 , sizeof(p_path));
    sprintf(p_path , "%s%s" , CONFIG_FILE_PATH , "need-logout.conf");
    QStringList logout = analysis_config_file(p_path);
    qDebug() << "Info : need logout pkg :" << logout;

    if(QString::compare(appAllMsg.name,appAptName) == 0)
    {
        if (this->execFun == false) {
            // this->execFun = true;
            return;
        }
        emit sendProgress(appAllMsg.name, 100, "download");
        emit sendProgress(appAllMsg.name, progress, "install");
        /* 临时解决方案 , 获取系统语言环境 , 英文加悬浮框 , 中文不加 */
        QLocale locale;

        int pgs = progress;
        //        appVersion->setText(tr("正在安装")+"("+QString::number(pgs)+"%)");
        if (!isUpdateAll) {
            appVersion->setText(tr("Being installed")+"("+QString::number(pgs)+"%)");
            appVersion->setToolTip("");
        } else {
            appVersion->setText(tr("Being installed"));
            appVersion->setToolTip("");
        }
        updateAPPBtn->hide();
        if(status == "apt_finish")
        {
            updateAPPBtn->hide();
            //            appVersion->setText(tr("更新成功！"));

            if (reboot.contains(appAptName)) {
                if (locale.language() == QLocale::Chinese) {
                    appVersion->setText(tr("Update succeeded , It is recommended that you restart later!"));
                } else {
                    appVersion->setText(tr("Update succeeded , It is recommended that you restart later!"));
                    appVersion->setToolTip(tr("Update succeeded , It is recommended that you restart later!"));
                }
            } else if (logout.contains(appAptName)) {
                if (locale.language() == QLocale::Chinese) {
                    appVersion->setText(tr("Update succeeded , It is recommended that you log out later and log in again!"));
                } else {
                    appVersion->setText(tr("Update succeeded , It is recommended that you log out later and log in again!"));
                    appVersion->setToolTip(tr("Update succeeded , It is recommended that you log out later and log in again!"));
                }
            } else {
                appVersion->setText(tr("Update succeeded!"));
            }

            QIcon icon = QIcon::fromTheme("ukui-dialog-success");
            QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(14, 14)));
            appVersionIcon->setPixmap(pixmap);
            m_updateMutual->importantList.removeOne(appAllMsg.name);
            m_updateMutual->failedList.removeOne(appAllMsg.name);
            //            QString message = QString("%1"+tr("更新成功！")).arg(dispalyName);
            QString message = QString("%1"+tr("Update succeeded!")).arg(dispalyName);
            m_updateMutual->onRequestSendDesktopNotify(message);
            detaileInfo->hide();
            largeWidget->hide();
            emit hideUpdateBtnSignal(true);

        }
        else if(status == "apt_error")
        {
            //            appVersion->setText(tr("更新失败！"));
            appVersion->setText(tr("Update failed!"));

            //            appVersion->setToolTip(tr("失败原因：")+(appNameLab->dealMessage(errormsg)));
            appVersion->setToolTip(tr("Failure reason:")+ "\r\n"+(appNameLab->dealMessage(errormsg)));
            m_updateMutual->importantList.removeOne(appAllMsg.name);
            m_updateMutual->failedList.append(appAllMsg.name);
            QIcon icon = QIcon::fromTheme("dialog-error");
            QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(14, 14)));
            appVersionIcon->setPixmap(pixmap);
            //            QString message = QString("%1"+tr("更新失败！")).arg(dispalyName);
            QString message = QString("%1"+tr("Update failed!")).arg(dispalyName);
            m_updateMutual->onRequestSendDesktopNotify(message);
            emit hideUpdateBtnSignal(false);
        }
    }
}

void AppUpdateWid::updateAppUi(QString name)
{
    iconNameLayout = new QHBoxLayout();
    smallHLayout = new QHBoxLayout();
    largeVLayout = new QVBoxLayout();
    appTitleWid = new QWidget();
    largeWidget = new QWidget();
    mainVLayout = new QVBoxLayout();
    QHBoxLayout *frameLayout = new QHBoxLayout();
    AppFrame = new QFrame(this);  //最外层窗口适配主题样式
    AppFrame->setFrameShape(QFrame::Box);

    frameLayout->addWidget(AppFrame);
    frameLayout->setContentsMargins(0,0,0,0);   //更新信息之间的间距
    frameLayout->setSpacing(0);
    this->setLayout(frameLayout);

    appIconName = new QLabel(this);
    appIconName->setMinimumWidth(160);
    appIconName->setMaximumWidth(600);
    appIcon = new QLabel(appIconName);
    appNameLab = new MyLabel(appIconName);
    //    appNameLab->setMinimumWidth(140);
    appIconName->setLayout(iconNameLayout);
    appIcon->setFixedSize(32,32);
    iconNameLayout->setAlignment(Qt::AlignLeft);
    iconNameLayout->addWidget(appIcon,1);
    iconNameLayout->setSpacing(0);
    iconNameLayout->addSpacing(8);
    iconNameLayout->addWidget(appNameLab,10);
    //    iconNameLayout->addStretch();

    appVersion = new QLabel(this);
    appVersionIcon = new QLabel(this);
    appVersionIcon->setFixedSize(16,16);
    appVersionIcon->setPixmap(QPixmap());
    appVersion->setMinimumWidth(180);
    //    QIcon icon = QIcon::fromTheme("dialog-error");
    //    QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(14, 14)));
    //    appVersionIcon->setPixmap(pixmap);

    detaileInfo = new QPushButton(this);
    //    detaileInfo->setText(tr("详情"));
    detaileInfo->setText(tr("details"));
    detaileInfo->setFixedSize(60,30);
    detaileInfo->setFlat(true);

    updateAPPBtn = new QPushButton(this);
    //    updateAPPBtn->setText(tr("更新"));
    updateAPPBtn->setText(tr("Update"));

    otherBtnLayout = new QHBoxLayout();  //版本号、详情、更细按钮布局
    otherBtnLayout->setSpacing(0);
    otherBtnLayout->setMargin(0);
    otherBtnLab = new QLabel(this);
    otherBtnLab->setMargin(0);
    otherBtnLab->setMinimumWidth(350);
    otherBtnLab->setMaximumWidth(500);
    otherBtnLab->setFixedHeight(60);

    otherBtnLayout->setAlignment(Qt::AlignLeft);
    otherBtnLayout->addWidget(appVersionIcon);
    otherBtnLayout->addWidget(appVersion);
    otherBtnLayout->addWidget(detaileInfo,1,Qt::AlignRight);
    otherBtnLayout->addSpacing(10);
    otherBtnLayout->addWidget(updateAPPBtn,0,Qt::AlignRight);
    otherBtnLab->setLayout(otherBtnLayout);
    smallHLayout->addWidget(appIconName,1);
    smallHLayout->addStretch(0);
    smallHLayout->addWidget(otherBtnLab,1);
    smallHLayout->setSpacing(0);
    smallHLayout->setContentsMargins(0,0,10,0);
    appTitleWid->setLayout(smallHLayout);

    mainVLayout->addWidget(appTitleWid);
    mainVLayout->setMargin(0);
    mainVLayout->setSpacing(0);
    someInfoEdit = new QTextEdit(this);   //部分changelog  缩略显示
    someInfoEdit->setReadOnly(true);
    someInfoEdit->verticalScrollBar()->setProperty("drawScrollBarGroove" , false);
    QPalette pl = someInfoEdit->palette();
    someInfoEdit->setFixedHeight(120);
    pl.setBrush(QPalette::Base,QBrush(QColor(255,0,0,0)));
    someInfoEdit->setPalette(pl);

    //当前语言环境
    QString locale = QLocale::system().name();
    if (locale == "zh_CN")
        environment=zh_cn;

    updatelogBtn = new QPushButton(this);
    //    updatelogBtn->setText(tr("更新日志"));
    updatelogBtn->setText(tr("Update log"));
    updatelogBtn->setFlat(true);
    updatelog1 = new UpdateLog(this);

    largeVLayout->addWidget(someInfoEdit);
    largeVLayout->addWidget(updatelogBtn,0,Qt::AlignLeft);
    largeVLayout->setSpacing(5);
    largeVLayout->setContentsMargins(50,0,50,5);
    largeWidget->setLayout(largeVLayout);
    //    largeWidget->setFixedHeight(80);
    mainVLayout->addWidget(largeWidget);
    largeWidget->hide();
    //    largeWidget->setFixedHeight(120);
    //    appTitleWid->setFixedHeight(60)
    //    this->setLayout(mainVLayout);
    AppFrame->setLayout(mainVLayout);
    QMap<QString, QString> map = getNameAndIconFromJson(name);
    if (!map.value("name").isNull())
        dispalyName = map.value("name");
    else
        dispalyName = translationVirtualPackage(name);

    appNameLab->setText(dispalyName);
    /*判断图标，优先级: JSON文件指定 > qrc资源文件中 > 主题 > 默认*/
    if (!map.value("icon").isNull()) {
        haveThemeIcon = true;
        appIcon->setPixmap(QPixmap(map.value("icon")));
    } else if (name.contains("kylin-update-desktop-")||name == "linux-generic") {
        haveThemeIcon = true;
        pkgIconPath = QString(":/img/plugins/upgrade/%1.png").arg(name);
        appIcon->setPixmap(QPixmap(pkgIconPath));
    } else if (QIcon::fromTheme(name).hasThemeIcon(name)) {    //判断是否有主题图标并输出
        haveThemeIcon = true;
        QIcon icon = QIcon::fromTheme(name);
        QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(32, 32)));
        appIcon->setPixmap(pixmap);
    } else {
        QIcon icon = QIcon::fromTheme("application-x-desktop");
        QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(32, 32)));
        appIcon->setPixmap(pixmap);
    }
    QString newStrMsg = appAllMsg.availableVersion;

    if(newStrMsg.size()>16)
    {
        //        appVersion->setText(tr("最新：")+newStrMsg);
        //        appVersion->setToolTip(tr("最新：")+newStrMsg);
        appVersion->setText(tr("Newest:")+newStrMsg);
        appVersion->setToolTip(tr("Newest:")+newStrMsg);
    }
    else
    {
        //        appVersion->setText(tr("最新：")+newStrMsg);
        appVersion->setText(tr("Newest:")+newStrMsg);
        appVersion->setToolTip("");
    }

    //获取并输出changelog
    chlog = setDefaultDescription(appAllMsg.longDescription);
    updatelog1->logContent->append(chlog);

    QTextCursor tmpCursor = updatelog1->logContent->textCursor();
    tmpCursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    updatelog1->logContent->setTextCursor(tmpCursor);

    //    updatelog1->logAppName->setText(dispalyName+tr("更新日志"));
    //    updatelog1->logAppVerson->setText(tr("最新:")+appAllMsg.availableVersion);
    updatelog1->logAppName->setText(dispalyName+tr("Update log"));
    updatelog1->logAppVerson->setText(tr("Newest:")+appAllMsg.availableVersion);


    QFontMetrics fontWidth(someInfoEdit->font());//得到每个字符的宽度
    QString StrMsg = fontWidth.elidedText(chlog, Qt::ElideRight,600);//最大宽度
    someInfoEdit->append(StrMsg);
    if(appAllMsg.msg.allSize == 0 || appAllMsg.packageSize == 0)
    {
        someInfoEdit->append(tr("Download completed"));
//        someInfoEdit->append(tr("Download size:")+QString(modifySizeUnit(appAllMsg.packageSize)));
        //        someInfoEdit->append(tr("下载大小：")+QString(modifySizeUnit(appAllMsg.packageSize)));
    }
    else
    {
        //        someInfoEdit->append(tr("下载大小：")+QString(modifySizeUnit(appAllMsg.msg.allSize)));
        someInfoEdit->append(tr("Download size:")+QString(modifySizeUnit(appAllMsg.msg.allSize)));
    }
    if (!map.value("icon").isNull()) {
        haveThemeIcon = true;
        updatelog1->logAppIcon->setPixmap(QPixmap(map.value("icon")));
    } else if(name.contains("kylin-update-desktop")||name == "linux-generic") {
        pkgIconPath = QString(":/img/plugins/upgrade/%1.png").arg(name);
        updatelog1->logAppIcon->setPixmap(QPixmap(pkgIconPath));
    } else {
        if(QIcon::fromTheme(name).hasThemeIcon(name))
        {
            QIcon icon = QIcon::fromTheme(name);
            QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(32, 32)));
            updatelog1->logAppIcon->setPixmap(pixmap);
        }
        else
        {
            QIcon icon = QIcon::fromTheme("application-x-desktop");
            QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(32, 32)));
            updatelog1->logAppIcon->setPixmap(pixmap);
        }
    }
    //获取并输出包大小、当前版本号
    QString currentVersion = appAllMsg.version;
    if(currentVersion != "")
    {
        //        someInfoEdit->append(tr("当前版本：")+currentVersion);
        someInfoEdit->append(tr("Current version:")+currentVersion);
    }

}


QMap<QString, QString> AppUpdateWid::getNameAndIconFromJson(QString pkgname)
{
    QMap <QString, QString> nameIconList;
    /*判断json文件是否存在*/
    QString filename = QString(JSON_FILE_PATH) +pkgname +".json";
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)){
        qDebug() << "JSON file open failed! ";
        return nameIconList;
    }
    QByteArray jsonData = file.readAll();

    QJsonParseError err_rpt;
    QJsonDocument  root_Doc = QJsonDocument::fromJson(jsonData, &err_rpt); // 字符串格式化为JSON

    if (!root_Doc.isNull() && (err_rpt.error == QJsonParseError::NoError)) {  // 解析未发生错误
        if (root_Doc.isObject()) { // JSON 文档为对象
            QJsonObject object = root_Doc.object();  // 转化为对象
            if (QLocale::system().name() == "zh_CN"){
                QString name  = object.value("name").toObject().value("zh_CN").toString();
                if (!name.isNull()) {
                    nameIconList.insert("name", name);
                }
            }else {
                QString name  = object.value("name").toObject().value("en_US").toString();
                if (!name.isNull()) {
                    nameIconList.insert("name", name);
                }
            }
            QString iconPath = object.value("icon").toString();
            if (!iconPath.isNull())
                nameIconList.insert("icon", iconPath);
        }
    }else{
        qDebug() << "JSON文件格式错误！";
        return nameIconList;
    }

    return nameIconList;
}

void AppUpdateWid::showDetails()
{
    if(largeWidget->isHidden())
    {
        largeWidget->show();
        //        detaileInfo->setText(tr("收起"));
        detaileInfo->setText(tr("back"));
    }
    else
    {
        largeWidget->hide();
        //        detaileInfo->setText(tr("详情"));
        detaileInfo->setText(tr("details"));
    }
}

void AppUpdateWid::showUpdateLog()
{
    //QRect availableGeometry = qApp->primaryScreen()->availableGeometry();
    //updatelog1->move((availableGeometry.width()-updatelog1->width())/2,(availableGeometry.height()- updatelog1->height())/2);
    updatelog1->exec();
}

void AppUpdateWid::cancelOrUpdate()
{
    if(updateAPPBtn->text() == tr("Update"))
    {
//        emit changeUpdateAllSignal(true);
        /*判断电量是否支持更新*/
        if (!get_battery()) {
            QMessageBox msgBox;
            msgBox.setText(tr("The battery is below 50% and the update cannot be downloaded"));
            msgBox.setIcon(QMessageBox::Information);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setButtonText(QMessageBox::Ok,tr("OK"));
            msgBox.exec();
            return ;
        }
        if (isAutoUpgrade) {
            updateOneApp();
            return ;
        }
        if(m_updateMutual->isPointOutNotBackup == true)
        {
            QMessageBox msgBox;
            msgBox.setText(tr("A single update will not automatically backup the system, if you want to backup, please click Update All."));
            msgBox.setWindowTitle(tr("Prompt information"));
            msgBox.setStandardButtons(QMessageBox::YesAll
                                      | QMessageBox::NoToAll|QMessageBox::Cancel);
            msgBox.setButtonText(QMessageBox::YesAll,tr("Do not backup, continue to update"));
            msgBox.setButtonText(QMessageBox::NoToAll,tr("Cancel"));
            msgBox.setButtonText(QMessageBox::Cancel,tr("Cancel update"));
            QCheckBox *cb = new QCheckBox(&msgBox);
            msgBox.setCheckBox(cb);
            msgBox.checkBox()->setText(tr("This time will no longer prompt"));
            msgBox.checkBox()->show();
            msgBox.button(QMessageBox::Cancel)->hide();
            int ret = msgBox.exec();
            if(msgBox.checkBox()->checkState() == Qt::Checked)
            {
                m_updateMutual->isPointOutNotBackup = false;
            }
            if(ret == QMessageBox::YesAll)
            {
                emit changeUpdateAllSignal(true);
                qDebug() << "立即更新!";
                updateOneApp();
            }
            else if(ret == QMessageBox::NoToAll)
            {
                emit changeUpdateAllSignal(false);
                m_updateMutual->isPointOutNotBackup = true;
                qDebug() << "不进行更新。";
            }
            else if(ret == QMessageBox::Cancel)
            {
                emit changeUpdateAllSignal(false);
                qDebug() << "不进行更新。";
                m_updateMutual->isPointOutNotBackup = true;

            }
            qDebug() << "m_updateMutual->isPointOutNotBackup = " << m_updateMutual->isPointOutNotBackup;
        }
        else
        {
            updateOneApp();
        }
    }
    else
    {
        isCancel = true;
        downloadProcess->terminate();
        timer->stop();
        //        updateAPPBtn->setText("更新");
        updateAPPBtn->setText(tr("Update"));
        QString newStrMsg = appAllMsg.availableVersion;
        if(newStrMsg.size()>16)
        {
            appVersion->setText(tr("Newest:")+newStrMsg);
            appVersion->setToolTip(tr("Newest:")+newStrMsg);
        }
        else
        {
            appVersion->setText(tr("Newest:")+newStrMsg);
            appVersion->setToolTip("");
        }
        emit changeUpdateAllSignal(false);
    }
}

void AppUpdateWid::updateOneApp()
{
    if(appAllMsg.msg.getDepends == true)
    {
        this->execFun = true;
        if(checkSourcesType() != file){
            isCancel = false;
            firstDownload = true;
            slotDownloadPackages();
            timer->start(1000); //开启定时器用于计算下载速度
            //        updateAPPBtn->setText(tr("取消"));
            updateAPPBtn->setText(tr("Cancel"));
            appVersionIcon->setPixmap(QPixmap());
        }else{
            startInstall(appAllMsg.name); //本地源直接开始安装
            appVersion->setText(tr("Ready to install"));
        }
        QDir dir = downloadPath;
        if(!dir.isEmpty()){
            appVersion->setText(tr("Calculate the download progress"));
        }
    }
    else
    {
        updateAPPBtn->hide();
        //        appVersion->setText(tr("获取依赖失败！"));

        this->execFun = false;
        startInstall(appAllMsg.name);

        appVersion->setText(tr("Get depends failed!"));
        appVersion->setToolTip("");
        QIcon icon = QIcon::fromTheme("dialog-error");
        QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(14, 14)));
        appVersionIcon->setPixmap(pixmap);
        m_updateMutual->importantList.removeOne(appAllMsg.name);
        m_updateMutual->failedList.append(appAllMsg.name);
        QString message = QString("%1"+tr("Get depends failed!")).arg(dispalyName);
        m_updateMutual->onRequestSendDesktopNotify(message);
        emit hideUpdateBtnSignal(false);
    }
}
//转换包大小的单位
QString AppUpdateWid::modifySizeUnit(long size)
{
    double tmpSize = size/1024.0;
    if(tmpSize >= 1)
    {
        if((tmpSize/1024.0) >= 1)
        {
            if((tmpSize/1024.0/1024.0) >= 1)
                return QString("%1%2").arg((int)(tmpSize/1024.0/1024.0*100 + 0.5)/100.0).arg("GB");//保留小数点后两位，并四舍五入
            else
                return QString("%1%2").arg((int)(tmpSize/1024.0*100 + 0.5)/100.0).arg("MB");//保留小数点后两位，并四舍五入
        }
        else
            return QString("%1%2").arg((int)(tmpSize*100 + 0.5)/100.0).arg("KB");//保留小数点后两位，并四舍五入
    }
    else
        return QString("%1%2").arg(size).arg("B");
}

//当前下载速度单位换算
QString AppUpdateWid::modifySpeedUnit(long size, float time)
{
    if(size < 0)
        size = 0;
    size = size/time;
    if((size/1024) >= 1)
    {
        if((size/1024/1024) >= 1)
        {
            if((size/1024/1024/1024) >= 1)
                return QString("%1%2").arg((int)(size/1024/1024/1024)).arg("GB/S");
            else
                return QString("%1%2").arg((int)(size/1024/1024)).arg("MB/S");
        }
        else
            return QString("%1%2").arg((int)(size/1024)).arg("KB/S");
    }
    else
        return QString("%1%2").arg(size).arg("B/S");
}

void AppUpdateWid::showDownloadStatues(QString downloadSpeed, int progress)
{
    if (!isUpdateAll){
        appVersion->setText(tr("In the update")+"("+downloadSpeed+")"+QString::number(progress)+"%");
        appVersion->setToolTip("");
    } else {
        appVersion->setText(tr("In the update"));
        appVersion->setToolTip("");
    }
}


//定时器触发计算当前下载速度和下载进度
void AppUpdateWid::calculateSpeedProgress()
{
    QFile tmpFile(QString("%1%2").arg(downloadPath).arg(currentPackage));
    if(tmpFile.exists())
    {
        if(!downloadFinish)
            downSize = priorSize + tmpFile.size();
        else
            downSize = priorSize;
        QString speed = modifySpeedUnit(downSize-preDownSize, 1);
        int progress = (int)((downSize*100/appAllMsg.msg.allSize));
        qDebug() << "priorsize:" << priorSize
                 << "predownsize" << preDownSize
                 << "tmpsize" << tmpFile.size()
                 << "downsize" << downSize
                 << "allsize" << appAllMsg.msg.allSize
                 << "progress:" << progress
                 << "speed" << speed
                 << "name" << currentPackage;
        preDownSize = downSize;
        showDownloadStatues(speed,progress);
        emit sendProgress(appAllMsg.name, progress, "download");
        if(downSize == appAllMsg.msg.allSize) //确保完全下载完成后再停止定时器
        {
            qDebug() << "dowload over:" << priorSize;
            timer->stop();
            m_updateMutual->copyFinsh(downloadPackages, appAllMsg.name);
            if(m_updateMutual->fileLock() != false)
            {
                emit filelockedSignal();
            }
            appVersion->setText(tr("Ready to install"));
            appVersion->setToolTip("");
        }
    }
}

void AppUpdateWid::updateAllApp()
{

    //    updateAPPBtn->show();
    isUpdateAll = true;
    if(isCancel && m_updateMutual->failedList.indexOf(appAllMsg.name) == -1)
    {
        qDebug() << "全部更新信号发出，当前: " << appAllMsg.name;
        cancelOrUpdate();
    }
}

void AppUpdateWid::showUpdateBtn()
{
    if (!isUpdateAll)
        updateAPPBtn->show();
    updateAPPBtn->setText(tr("Update"));
}
void AppUpdateWid::hideOrShowUpdateBtnSlot(int result)
{
    if(result == 0)  //0表示备份还原开始
    {
        updateAPPBtn->hide();
    }
    else if(result == 99 ||result == -20)  //99时备份成功 -20时备份还原进程被中断
    {
        if (!isUpdateAll)
            updateAPPBtn->show();
    }

}

QString AppUpdateWid::translationVirtualPackage(QString str)
{
    if(QLocale::system().name()!="zh_CN")
        return str;
    if(str == "kylin-update-desktop-app")
        return "基本应用";
    if(str == "kylin-update-desktop-security")
        return "安全更新";
    if(str == "kylin-update-desktop-support")
        return "系统基础组件";
    if(str == "kylin-update-desktop-ukui")
        return "桌面环境组件";
    if(str == "linux-generic")
        return "系统内核组件";
    if(str == "kylin-update-desktop-kernel")
        return "系统内核组件";
    if(str == "kylin-update-desktop-kernel-3a4000")
        return "系统内核组件";
    if(str == "kylin-update-desktop-kydroid")
        return "kydroid补丁包";
    return str;
}
bool AppUpdateWid::eventFilter(QObject *watched, QEvent *event)
{
    if(watched->metaObject()->className() == QStringLiteral("QLabel"))
    {
        if(event->type() == QEvent :: ToolTip){
            QToolTip::hideText();
            event->ignore();
            return true;
        }
        else
        {
            return false;
        }
    }
}

QString AppUpdateWid::setDefaultDescription(QString str)
{
    if(str == "")
    {
        //        str = tr("暂无内容");
        str = tr("No content.");
    }
    return str;
}

type AppUpdateWid::checkSourcesType()
{
    QFile soucesFile(SOURCESLIST);
    soucesFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString result = soucesFile.readAll();
    if(result.contains("http://") || result.contains("https://")){
        qDebug() << "当前源为http源";
        return http;
    }else if(result.contains("ftp://")){
        qDebug() << "当前源为ftp源";
        return ftp;
    }else if(result.contains("file://")){
        qDebug() << "当前源为本地源";
        return file;
    }
    return http;
}

bool AppUpdateWid::get_battery()
{
    QStringList users;
    int battery_value = 0;
    QDBusInterface m_interface1( "org.freedesktop.UPower",
                                 "/org/freedesktop/UPower",
                                 "org.freedesktop.UPower",
                                 QDBusConnection::systemBus() );
    if (!m_interface1.isValid()) {
        qDebug() << "电源管理器dbus接口初始化失败";
        return true;
    }

    QDBusReply<QList<QDBusObjectPath>> obj_reply = m_interface1.call("EnumerateDevices");

    if (obj_reply.isValid()) {
        for (QDBusObjectPath op : obj_reply.value())
            users << op.path();
        if (users.size()==1 || users.isEmpty()) {
            qDebug()<<"无法获取电量值,判断此电脑为台式电脑";
            return true;
        }
        foreach (QString str, users) {
            if (str == users.at(0) || str == users.at(users.size() - 1)) {
                continue ;
            }
            QDBusInterface m_interface( "org.freedesktop.UPower",
                                        str,
                                        "org.freedesktop.DBus.Properties",
                                        QDBusConnection::systemBus());

            if (!m_interface.isValid()) {
                qDebug() << "电源管理器dbus接口初始化失败";
                return true;
            }

            QDBusReply<QVariant> obj_reply = m_interface.call("Get","org.freedesktop.UPower.Device","Percentage");
            int Ele_surplus = obj_reply.value().toInt();
            battery_value += Ele_surplus;
            qDebug() << "battery value : " << Ele_surplus;
        }
        return true;
    }
    /*如果电池总电量小于50不可升级*/
    if (battery_value < 50)
        return false;
    return true;
}
